# Yarn Spinner for Unreal Engine — Changelog

## Alpha 9 (in progress)

Alpha 9 reworks a few parts of how the plugin fits into Unreal, guided by community feedback on the component workflow, amongst other things. Thanks to everyone who provided feedback, bug reports, and more! We REALLY really appreicate you. The dialogue runner's Blueprint and C++ surface should largely be unchanged, so if you only used the runner, presenters, and events, your project may just work as before. Action markup handlers changed a fair bit and a handful of script behaviours got stricter, so please read the migration list below as you work through things..

### Migrating from an earlier Alpha

1. **Everyone**: recompile! thHe dialogue runner now delegates its
   internals to a dialogue instance object. Nothing changes in how you
   use it and no Blueprint changes are needed for the runner itself.
2. **If you added markup handler components** (pause processor, markup
   event handler, sound effect handler, or your own subclass etc. etc.) to actors:
   these classes are no longer components. Before upgrading, note down
   each component's settings. After upgrading, remove the now-missing
   components, then open your line presenter's **Action Markup Handlers**
   array (Details panel, Yarn Spinner|Markup) and add entries there
   instead, picking the handler type inline and re-entering the settings.
   A pause processor is already in the array by default, so `[pause/]`
   works without any setup now; remove it from the array if you don't
   want it.
3. **If you used the sound effect handler**: its sound map now stores
   soft references keyed by name, and sounds stream in as needed instead
   of loading with the level. Old map entries do not carry over; re-add
   your name-to-sound entries on the new array entry. Nothing changes in
   your `.yarn` files.
4. **If you subclassed a handler in C++**: re-parent from the old
   component base to `UYarnActionMarkupHandler`, update the method
   signatures (they now receive the presenter as the first parameter),
   and add your handler to the array instead of the actor. In Bllueprint,
   re-parent to `UYarnBlueprintActionMarkupHandler` and reconnect the
   `Receive...` events.
5. **If your commands rely on quoting or escapes**: tokenisation now
   splits on any whitespace (including tabs), and inside quotes only
   `\\` and `\"` are escapes; `\n` and `\t` are no longer converted to
   real newline/tab characters. Check any command lines using those.
6. **If your scripts feed unparseable text to `number()` or `bool()`,
   call `format()` with markers beyond `{0}`, or divide by zero with
   `%`**: these now stop the dialogue with an error instead of quietly
   returning a default. Fix the scripts; the errors name the function.
7. **If you display large or tiny numbers in dialogue**: values outside
   the range 0.0001 to 1,000,000,000 now print in scientific notation
   (`1E+09`), matching the other Yarn Spinner runtimes...
8. **If your project has node names differing only by case** ("Start"
   and "start"): imports now fail with an error naming both. Rename one, please!
9. **If you pass an option's `Option ID` back to Select Option**: that
   property now holds thee option's place in the list it arrived in (0, 1,
   2, ...) rather than an internal jump target. Blueprints that used Option ID 
   straight into Select Option will work fine still, and now pick the right option lol; 
   anything that did something with Option ID should use the line ID instead now.
10. **If you call Start Dialogue or Has Node with a differently-cased
   node name** ("start" for a node called "Start"): these are case
   sensitive now, matching every other Yarn Spinner runtime, so they no
   longer find the node. Use the name as it appears in the script.

### Added

- **Line Advancer** component. You should use with dialogue runner to turn
  a key press into the right request: hurry the line that's still typing
  itself out, move on to the next one when it's finished, hurry up options,
  or cancel the dialogue outright, etc. One control can do both hurry-up and
  advance (the usual "tap to skip, tap again to continue" thing), or you can
  give them seperate keys. Set Input Mode to Manual and call the Request
  functions yourself if your game already has an input setup... There
  is also an option to cancel the dialogue after the player mashes advance a
  few times on one line.
- **Widget effects**: Fade Widget (plus Fade In and Fade Out), Punch Scale,
  and Shake, as Blueprint async nodes that finish early when a cancellation
  token is cancelled or the player hurries the line along. Handy for the
  small flourishes etc. There are immediate versions too (Set Widget Opacity, Scale,
  Offset).
- **Asset providers**. `Get Asset For Line` and `Prepare Assets For Lines`
  are now behind an interface, so finding the voice over clip (or portrait,
  or anything else keyed to a line) can be swapped out without touching teh
  presenters. The dialogue runner makes a default one if you don't supply
  yours: it reads an asset path from a line's metadata tag (`#audio:...`),
  then falls back to the localisation's assets folder for the current locale,
  and follows shadow lines to their source. The voice over presenter asks the
  provider first and keeps its old behaviour as a fallback.
- **Yarn Node Reference**, a project-plus-node pair that shows a dropdown of
  the project's nodes in the Details panel instead of a text box. Feed one to 
  the new Start Dialogue From Reference. Matches teh node picker the Godot and 
  Unity versions have.
- **Has Salient Content** and **Get Saliency Options For Node Group** on the
  dialogue runner, matching the Unity names. They check a node group's
  `when:` conditions with the runner's own functions, including `not`,
  `once`, `<` and `and`, so you don't need to pass anything else in. Both
  work from Blueprints and C++. An unknown node name logs a warning and
  gives back false or an empty list.

### Fixed

- Smart variables (`<<declare $x = <expression>>>`) can now be read in
  ordinary expressions like `<<if $x>>`. The variable storage never
  consulted the compiled expression evaluator, so any smart variable used
  outside a saliency condition halted the dialogue with a "variable not
  found" error. Oops. Sorry.
- String and enum comparisons in Yarn expressions are now case-sensitive,
  matching Yarn Spinner for Unity. Unreal's `FString` comparison ignores
  case by default, so `"Alice" == "alice"` was quietly true here and false
  everywhere else. Node header lookups and variable change listeners now
  compare case-sensitively as well.
- Calling an unknown function, or calling a function with the wrong number
  of arguments, now stops the dialogue with a clear error naming the
  function and both argument counts. Previously the VM carried on with a
  corrupted value stack, which surfaced later as unrelated wrong values.
- Showing options with no dialogue presenters registered now now ends the
  dialogue with an error instead of waiting forever for a selection that
  nothing can make.
- Replacing the Yarn project on a virtual machine no longer leaves a stale
  pointer to a node from the old program.
- The debug HUD component no longer ticks every frame when it has no
  dialogue runner to observe and no toggle key to poll.
- `number()`, `bool()`, and `format()` now stop dialogue with a clear error
  when they're given input they can't make sense of, instead of quietly
  returning a default value. `number("abc")` halts rather than returning
  `0`; `bool("yes")` halts rather than returning `false`; `format()` halts
  if the format string references an argument that wasn't supplied, rather
  than leaving the literal `{1}` placeholder in the displayed text.
- Node names that differ only by case are now rejected at import with a
  clear error naming both... Unreal keys these maps case-insensitively, so a
  project with both a "Start" and a "start" node previously imported
  without issue. Whoops. Sorry!
- The voice over presenter now gets its clips from the asset provider rather
  than its own copy of the same lookup rules, so there is one place where
  "which asset belongs to this line" is decided. Behaviour is is unchanged:
  the default provider does metadata tag, then localised assets folder, then
  shadow source.
- Variables that differ only by case now log a warning at import. They
  share a single value in Unreal and have two separate values in the other
  runtimes, so if you're comign from Unity/Godot, be aware..
- The last-line preview shown alongside options now actually truncates at
  the `[lastline]` marker. The marker and the constant that named it were
  already there, but nothing read them! Hah.
- Least-recently-viewed saliency now rounds view counts the same way
  everywhere. Some code paths truncated and others rounded, so a node's
  view count could read differently depending on which one touched it.
- Modulo by zero now stops the dialogue with a clear error instead of
  returning 0. Note the divisor converts to an integer first, so a
  divisor smaller than 1 also counts as zero.
- An option's `Option ID` is now the option's position in the set it came
  in with, matching the Unity C# runtime, instead of the instruction the VM
  jumps to when that option is chosen! Selecting an option by its ID could
  pick a different option entirely, or fail with an "invalid option index"
  error, depending on what the compiler happened to emit. The jump target
  moved to its own field that the VM reads.
- Dialogue text is normalised (NFC) before markup is parsed. Text that arrives 
  with decomposed accents produced markup positionand comparisions that coudl be odd. 
  So, e.g. an "é" written as "e" plus a combining accent is now the same single 
  character everywhere. The normalisation comes from UE's own internationalisation
  data, same as `[plural]` and `[ordinal]` do.. Projects that build with ICU disabled 
  get their text through unnormalised rather than failing...
- Command and function names are now matched case sensitively, as they are in
  Yarn Spinner for Unity. Unreal's string maps ignore case, so registering
  handlers for `move` and `Move` used to leave you with whichever came last,
  and `<<Move>>` would happily run a handler registered as `move`. This covers 
  handlers registered in C++ and Blueprint, functions, and and commands dispatched
  to an actor or component by name...
- Looking a node up by name is case sensitive, so `Has Node` and `Start
  Dialogue` no longer accept "start" for a node called "Start". Unreal's
  string maps ignore case, sorry.
- The options presenter now skips its fade-in when options hurry-up is
  requested, so the line advancer's Option Hurry Up Key and Request Hurry
  Up Option work. It only listened for line hurry-up before.
- Calling Complete Blocking Command from inside a command handler object or
  actor function that returns true no longer hangs the dialogue. The
  completion arrived before the runner marked the command as pending, so it
  was ignored. Registered blocking handlers already handled this.

### Changed

- Action markup handlers are no longer actor components!! They're now
  lightweight objects you add directly to a presenter's Action Markup
  Handlers array in the Details panel, where each entry unfolds inline
  for editing. This removes the add-a-component-and-wire-it-up dance for
  every object that reacts to dialogue markup! Blueprint handlers
  subclass `UYarnBlueprintActionMarkupHandler` and implement its
  `Receive...` events. See the migration list above, please!!
- The sound effect handler's sound map uses soft references and streams
  sounds in on demand. Previously every mapped sound was hard-referenced
  and stayed loaded for the life of the component whether used or not.
- The dialogue runner's internals moved into a dedicated dialogue
  instance object owned by the runner. The runner's Blueprint and C++
  surface is unchanged; this is groundwork that also makes the
  orchestration logic testable on its own.
- `[plural]` and `[ordinal]` markup now get their plural rules from the
  engine's own ICU culture data (`FCulture::GetPluralForm`) instead of a
  hand-written table. This extends correct plural handling to every
  language the engine knows, keeps the rules current with engine updates,
  and removes about a a thousand lines of frozen CLDR transcription. Note:
  projects that strip ICU data in packaging (for example "English only")
  get correspondingly reduced plural forms for the stripped locales.
- 28 query-only Blueprint functions are now Blueprint Pure, so they no
  longer need an execution pin in your graphs (variable getters, node and
  line queries, presenter state checks, metadata lookups).
- Every Blueprint category now sits under "Yarn Spinner", and the
  localisation categories use one spelling. Properties and functions that
  used to appear under bare "Markup", "Style", "Voice Over", "Localization"
  and similar top-level categories have moved.
- Numbers written into dialogue text now format the same way as the C#
  runtime: very small or very large values switch to scientific notation
  (for example, `0.00001` prints as `1E-05`, and `1000000000` prints as
  `1E+09`), everything else prints as plain decimal digits. Previously
  every number printed as plain digits regardless of size.
- The plugin's automation tests now run test plans. Each of the 34 test case
  drives a dialogue runner through each part of plans! Aw yeah.
- Command tokenisation now matches the C# runtime: any whitespace character
  splits arguments, not just the space character. NOTE: this is a breaking
  change for existing scripts - `\n` and `\t` inside a quoted command
  argument are no longer converted into real newline/tab characters; only
  `\\` and `\"` are recognised as escapes inside quotes...

### Documentation

- The quick start now describes presenter wiring properly, oops: the editor
  property is the Dialogue Presenters component-reference array, with a
  runtime fallback for cases where the component picker won't cooperate.
  It previously described adding entries to a property that isn't editable.
- The quick start's ysc install command now matches the README (3.2.2 for now)!
- Clarified in code that the baked command registry covers native C++
  classes only, and that Blueprint commands register at runtime through
  the command library.
