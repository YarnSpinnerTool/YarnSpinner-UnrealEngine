# Yarn Spinner for Unreal Engine — Changelog

## Alpha 5 (in progress)

Alpha 5 reworks a few parts of how the plugin fits into Unreal, guided by community feedback on the component workflow, amongst other things. Thanks to everyone who provided feedback, bug reports, and more! We really appreicate you. The dialogue runner's Blueprint and C++ surface should largely be unchanged, so if you only used the runner, presenters, and events, your project may just work as before. Action markup handlers changed a fair bit and a handful of script behaviours got stricter, so please read the migration list below as you work through things..

### Migrating from Alpha 4

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
- Node names and initial variable values that differ only by case are now
  rejected at import with a clear error naming both. Unreal keys these maps
  case-insensitively, so a project with both a "Start" and a "start" node
  previously imported without complaint and one silently overwrote the
  other.
- The last-line preview shown alongside options now actually truncates at
  the `[lastline]` marker. The marker and the constant that named it were
  already there, but nothing read them! Hah.
- Least-recently-viewed saliency now rounds view counts the same way
  everywhere. Some code paths truncated and others rounded, so a node's
  view count could read differently depending on which one touched it.
- Modulo by zero now stops the dialogue with a clear error instead of
  returning 0. Note the divisor converts to an integer first, so a
  divisor smaller than 1 also counts as zero.

### Changed

- Action markup handlers are no longer actor components!! They're now
  lightweight objects you add directly to a presenter's Action Markup
  Handlers array in the Details panel, where each entry unfolds inline
  for editing. This removes the add-a-component-and-wire-it-up dance for
  every object that reacts to dialogue markup. Blueprint handlers
  subclass `UYarnBlueprintActionMarkupHandler` and implement its
  `Receive...` events. See the migration list above, please.
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
- Command tokenisation now matches the C# runtime: any whitespace character
  splits arguments, not just the space character. NOTE: this is a breaking
  change for existing scripts - `\n` and `\t` inside a quoted command
  argument are no longer converted into real newline/tab characters; only
  `\\` and `\"` are recognised as escapes inside quotes.

### Documentation

- The quick start now describes presenter wiring properly, oops: the editor
  property is the Dialogue Presenters component-reference array, with a
  runtime fallback for cases where the component picker won't cooperate.
  It previously described adding entries to a property that isn't editable.
- The quick start's ysc install command now matches the README (3.2.2 for now)!
- Clarified in code that the baked command registry covers native C++
  classes only, and that Blueprint commands register at runtime through
  the command library.
