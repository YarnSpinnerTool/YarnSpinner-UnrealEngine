// ============================================================================
//
//  Yarn Spinner for Unreal Engine
//
//  Copyright (c) Yarn Spinner Pty. Ltd. All Rights Reserved.
//
//  Yarn Spinner is a trademark of Secret Lab Pty. Ltd., used under license.
//
//  This code is subject to the terms and conditions of the license found in
//  the LICENSE.md file in the root of this repository.
//
//  For help, support, and more information, visit:
//    https://yarnspinner.dev
//    https://docs.yarnspinner.dev
//
// ============================================================================

#pragma once

// ============================================================================
// YarnActionMarkupHandler.h
// ============================================================================
//
// The action markup handler system allows you to register callbacks that fire
// during typewriter text display when specific markup tags are encountered.
// For example, [wave], [shake], [pause], or custom effects.
//
// Usage:
// 1. Implement IYarnActionMarkupHandler interface on a component
// 2. Add the component to your dialogue actor
// 3. Register it with the line presenter or typewriter
//
// For simple cases, use UYarnMarkupEventHandler with Blueprint delegates.
// For complex effects, derive from UYarnActionMarkupHandlerComponent.

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Components/ActorComponent.h"
#include "Sound/SoundBase.h"
#include "YarnMarkup.h"
#include "YarnCancellationToken.h"
#include "YarnActionMarkupHandler.generated.h"

// Forward declarations
class UYarnActionMarkupHandlerRegistry;

// ============================================================================
// Delegates for Blueprint-friendly markup handling
// ============================================================================

/** Called when preparing to display a line (before typewriter starts) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnYarnPrepareForLine, const FYarnMarkupParseResult&, ParseResult);

/** Called when line display begins (first character about to appear) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnYarnLineDisplayBegin, const FYarnMarkupParseResult&, ParseResult);

/** Called for each character as it appears, returns delay in seconds */
DECLARE_DYNAMIC_DELEGATE_RetVal_ThreeParams(float, FOnYarnCharacterWillAppear, int32, CharacterIndex, const FYarnMarkupParseResult&, ParseResult, const FYarnLineCancellationToken&, CancellationToken);

/** Called when all characters have been displayed */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnLineDisplayComplete);

/** Called when the line is about to be dismissed */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnYarnLineWillDismiss);

/** Called when a specific markup attribute is encountered during typewriter */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnYarnMarkupAttributeEncountered, const FString&, AttributeName, int32, CharacterIndex, const FYarnMarkupAttribute&, Attribute);

/** Called when entering a markup range during typewriter (start of [tag]...[/tag]) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnYarnMarkupRangeEnter, const FString&, AttributeName, int32, CharacterIndex, const FYarnMarkupAttribute&, Attribute);

/** Called when exiting a markup range during typewriter (end of [tag]...[/tag]) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnYarnMarkupRangeExit, const FString&, AttributeName, int32, CharacterIndex, const FYarnMarkupAttribute&, Attribute);

// ============================================================================
// IYarnActionMarkupHandler Interface
// ============================================================================

/**
 * Interface for handling markup effects during typewriter presentation.
 *
 * Action markup handlers are called at specific points during line presentation:
 * - When a line is about to be displayed (OnPrepareForLine)
 * - When display begins, before first character (OnLineDisplayBegin)
 * - For each character as it appears (OnCharacterWillAppear)
 * - When all characters are displayed (OnLineDisplayComplete)
 * - When the line is about to be dismissed (OnLineWillDismiss)
 *
 * Use this to trigger effects, sounds, or animations at specific text positions.
 */
UINTERFACE(MinimalAPI, Blueprintable, BlueprintType)
class UYarnActionMarkupHandler : public UInterface
{
	GENERATED_BODY()
};

class YARNSPINNER_API IYarnActionMarkupHandler
{
	GENERATED_BODY()

public:
	/**
	 * Called when preparing to display a line.
	 * Use this to set up any state needed for the line (cache pause positions, etc).
	 *
	 * This is called BEFORE any part of the line is visible.
	 *
	 * @param ParseResult The parsed markup for the line.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Markup")
	void OnPrepareForLine(const FYarnMarkupParseResult& ParseResult);

	/**
	 * Called when line display begins (before first character appears).
	 *
	 * @param ParseResult The parsed markup for the line.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Markup")
	void OnLineDisplayBegin(const FYarnMarkupParseResult& ParseResult);

	/**
	 * Called for each character as it's about to appear during typewriter effect.
	 *
	 * Return a delay in seconds to pause before showing this character.
	 * Return 0 to show immediately.
	 *
	 * @param CharacterIndex The index of the character about to appear (0-based).
	 * @param ParseResult The parsed markup for the line.
	 * @param CancellationToken Token to check for hurry-up/cancel requests.
	 * @return Delay in seconds before showing this character.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Markup")
	float OnCharacterWillAppear(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken);

	/**
	 * Called when all characters have been displayed.
	 * Use this to finalize any effects or clean up state.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Markup")
	void OnLineDisplayComplete();

	/**
	 * Called when the line is about to be dismissed.
	 * The presenter has finished and control is returning to the dialogue runner.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Markup")
	void OnLineWillDismiss();
};

// ============================================================================
// UYarnActionMarkupHandlerComponent
// ============================================================================

/**
 * Base class for action markup handlers implemented as components.
 *
 * Derive from this to create custom markup handlers that can be attached
 * to actors and configured in the editor.
 *
 * This is used by line presenters to have handlers that can be connected via the inspector.
 */
UCLASS(Abstract, ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnActionMarkupHandlerComponent : public UActorComponent, public IYarnActionMarkupHandler
{
	GENERATED_BODY()

public:
	// IYarnActionMarkupHandler interface
	virtual void OnPrepareForLine_Implementation(const FYarnMarkupParseResult& ParseResult) override;
	virtual void OnLineDisplayBegin_Implementation(const FYarnMarkupParseResult& ParseResult) override;
	virtual float OnCharacterWillAppear_Implementation(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken) override;
	virtual void OnLineDisplayComplete_Implementation() override;
	virtual void OnLineWillDismiss_Implementation() override;

protected:
	/** The current line's parse result - cached for subclass use */
	UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Markup")
	FYarnMarkupParseResult CurrentParseResult;
};

// ============================================================================
// UYarnPauseEventProcessor
// ============================================================================

/**
 * Pause event processor - handles [pause] markup tags.
 *
 * When the typewriter encounters a [pause] tag, this handler returns
 * a delay to pause text display.
 *
 * Usage in Yarn:
 *   NPC: Wait for it...[pause duration=1.5/] ...there!
 *   NPC: Dramatic[pause/]pause! (defaults to 1 second)
 *   NPC: Short pause[pause ms=500/] here (milliseconds)
 *
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnPauseEventProcessor : public UYarnActionMarkupHandlerComponent
{
	GENERATED_BODY()

public:
	UYarnPauseEventProcessor();

	/** Default pause duration in seconds when no duration is specified */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Pause")
	float DefaultPauseDuration = 1.0f;

	/** The name of the pause marker to look for */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Pause")
	FString PauseMarkerName = TEXT("pause");

	// IYarnActionMarkupHandler interface
	virtual void OnPrepareForLine_Implementation(const FYarnMarkupParseResult& ParseResult) override;
	virtual float OnCharacterWillAppear_Implementation(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken) override;

private:
	/** Positions where pauses should occur, mapped to duration in seconds */
	TMap<int32, float> PausePositions;
};

// ============================================================================
// UYarnMarkupEventHandler
// ============================================================================

/**
 * A flexible markup handler that fires Blueprint events for specific markup attributes.
 *
 * Use this when you want to respond to specific markup tags without creating
 * a custom handler class. Register attribute names and bind to the events.
 *
 * Usage:
 * 1. Add this component to your dialogue actor
 * 2. Add attribute names to AttributesToWatch (e.g., "wave", "shake", "sfx")
 * 3. Bind to OnMarkupAttributeEncountered in Blueprint
 * 4. Handle the event with custom logic
 *
 * Example in Yarn:
 *   NPC: This is [wave]wavy text[/wave] and [shake]shaky text[/shake]!
 *   NPC: Play a [sfx name="beep"/] sound effect!
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnMarkupEventHandler : public UYarnActionMarkupHandlerComponent
{
	GENERATED_BODY()

public:
	UYarnMarkupEventHandler();

	/**
	 * List of attribute names to watch for.
	 * When any of these are encountered, the events will fire.
	 * Leave empty to fire for ALL markup attributes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup Events")
	TArray<FString> AttributesToWatch;

	/**
	 * Whether to fire events for self-closing tags (e.g., [pause/]).
	 * When true, self-closing tags fire OnMarkupAttributeEncountered.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup Events")
	bool bFireForSelfClosingTags = true;

	/**
	 * Whether to fire events for range tags (e.g., [wave]...[/wave]).
	 * When true, range tags fire OnMarkupRangeEnter and OnMarkupRangeExit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Markup Events")
	bool bFireForRangeTags = true;

	// Events

	/** Fired when the line is about to be displayed */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Markup Events")
	FOnYarnPrepareForLine OnPrepareForLineEvent;

	/** Fired when line display begins */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Markup Events")
	FOnYarnLineDisplayBegin OnLineDisplayBeginEvent;

	/** Fired when line display completes */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Markup Events")
	FOnYarnLineDisplayComplete OnLineDisplayCompleteEvent;

	/** Fired when line is about to dismiss */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Markup Events")
	FOnYarnLineWillDismiss OnLineWillDismissEvent;

	/** Fired when a watched markup attribute is encountered (self-closing tags) */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Markup Events")
	FOnYarnMarkupAttributeEncountered OnMarkupAttributeEncountered;

	/** Fired when entering a markup range (start of [tag]...[/tag]) */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Markup Events")
	FOnYarnMarkupRangeEnter OnMarkupRangeEnter;

	/** Fired when exiting a markup range (end of [tag]...[/tag]) */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Markup Events")
	FOnYarnMarkupRangeExit OnMarkupRangeExit;

	// IYarnActionMarkupHandler interface
	virtual void OnPrepareForLine_Implementation(const FYarnMarkupParseResult& ParseResult) override;
	virtual void OnLineDisplayBegin_Implementation(const FYarnMarkupParseResult& ParseResult) override;
	virtual float OnCharacterWillAppear_Implementation(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken) override;
	virtual void OnLineDisplayComplete_Implementation() override;
	virtual void OnLineWillDismiss_Implementation() override;

protected:
	/** Check if an attribute name should be handled */
	bool ShouldHandleAttribute(const FString& AttributeName) const;

private:
	/** Cached self-closing attributes indexed by their position */
	TMap<int32, FYarnMarkupAttribute> SelfClosingAttributes;

	/** Cached range start positions for watched attributes */
	TMap<int32, FYarnMarkupAttribute> RangeStartPositions;

	/** Cached range end positions for watched attributes */
	TMap<int32, FYarnMarkupAttribute> RangeEndPositions;
};

// ============================================================================
// UYarnSoundEffectHandler
// ============================================================================

/**
 * A markup handler that plays sound effects when [sfx] tags are encountered.
 *
 * Usage in Yarn:
 *   NPC: *knock knock* [sfx name="door_knock"/] Who's there?
 *
 * The handler looks up sounds by name in the SoundMap or can be extended
 * to use a custom sound lookup system.
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class YARNSPINNER_API UYarnSoundEffectHandler : public UYarnActionMarkupHandlerComponent
{
	GENERATED_BODY()

public:
	UYarnSoundEffectHandler();

	/** The marker name to respond to (default: "sfx") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Sound Effects")
	FString SoundMarkerName = TEXT("sfx");

	/** Map of sound names to sound assets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Sound Effects")
	TMap<FString, USoundBase*> SoundMap;

	/** Volume multiplier for all sounds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Sound Effects", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VolumeMultiplier = 1.0f;

	/** Pitch multiplier for all sounds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Sound Effects", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float PitchMultiplier = 1.0f;

	/**
	 * Blueprint event fired when a sound should play but wasn't found in SoundMap.
	 * Implement this to use a custom sound lookup system.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Yarn Spinner|Sound Effects")
	FOnYarnMarkupAttributeEncountered OnSoundNotFound;

	// IYarnActionMarkupHandler interface
	virtual void OnPrepareForLine_Implementation(const FYarnMarkupParseResult& ParseResult) override;
	virtual float OnCharacterWillAppear_Implementation(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken) override;

	/**
	 * Look up a sound by name.
	 * Override this in Blueprint to implement custom sound lookup.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Yarn Spinner|Sound Effects")
	USoundBase* GetSoundForName(const FString& SoundName);

protected:
	/** Sound positions and their attribute data */
	TMap<int32, FYarnMarkupAttribute> SoundPositions;
};

// ============================================================================
// UYarnActionMarkupHandlerRegistry
// ============================================================================

/**
 * A registry that manages multiple action markup handlers.
 *
 * This allows you to:
 * - Register handlers by attribute name
 * - Automatically dispatch events to the appropriate handlers
 * - Add/remove handlers at runtime
 *
 * Use this in your line presenter to manage all markup handlers in one place.
 */
UCLASS(BlueprintType)
class YARNSPINNER_API UYarnActionMarkupHandlerRegistry : public UObject
{
	GENERATED_BODY()

public:
	UYarnActionMarkupHandlerRegistry();

	/**
	 * Register a handler for all markup events.
	 * This handler will receive events for all markup, not filtered by name.
	 *
	 * @param Handler The handler to register.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup Registry")
	void RegisterHandler(TScriptInterface<IYarnActionMarkupHandler> Handler);

	/**
	 * Register a handler for a specific markup attribute name.
	 * The handler will only receive events when that attribute is present.
	 *
	 * @param AttributeName The attribute name to handle (e.g., "wave", "pause").
	 * @param Handler The handler to register.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup Registry")
	void RegisterHandlerForAttribute(const FString& AttributeName, TScriptInterface<IYarnActionMarkupHandler> Handler);

	/**
	 * Unregister a handler.
	 *
	 * @param Handler The handler to unregister.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup Registry")
	void UnregisterHandler(TScriptInterface<IYarnActionMarkupHandler> Handler);

	/**
	 * Remove all registered handlers.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup Registry")
	void ClearAllHandlers();

	// Dispatch methods - call these from your line presenter

	/** Dispatch OnPrepareForLine to all registered handlers */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup Registry")
	void DispatchPrepareForLine(const FYarnMarkupParseResult& ParseResult);

	/** Dispatch OnLineDisplayBegin to all registered handlers */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup Registry")
	void DispatchLineDisplayBegin(const FYarnMarkupParseResult& ParseResult);

	/**
	 * Dispatch OnCharacterWillAppear to all registered handlers.
	 * Returns the maximum delay requested by any handler.
	 */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup Registry")
	float DispatchCharacterWillAppear(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken);

	/** Dispatch OnLineDisplayComplete to all registered handlers */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup Registry")
	void DispatchLineDisplayComplete();

	/** Dispatch OnLineWillDismiss to all registered handlers */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup Registry")
	void DispatchLineWillDismiss();

	/** Get all registered handlers */
	UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Markup Registry")
	TArray<TScriptInterface<IYarnActionMarkupHandler>> GetAllHandlers() const;

private:
	/** Global handlers that receive all events */
	UPROPERTY()
	TArray<TScriptInterface<IYarnActionMarkupHandler>> GlobalHandlers;

	/** Handlers registered for specific attribute names */
	TMap<FString, TArray<TScriptInterface<IYarnActionMarkupHandler>>> AttributeHandlers;

	/** Find handlers that should process a specific character position */
	TArray<TScriptInterface<IYarnActionMarkupHandler>> GetHandlersForPosition(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult) const;
};

// ============================================================================
// UYarnMarkupHandlerLibrary
// ============================================================================

/**
 * Blueprint function library for markup handler utilities.
 */
UCLASS()
class YARNSPINNER_API UYarnMarkupHandlerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Check if a character position is within a specific markup attribute's range.
	 *
	 * @param CharacterIndex The position to check.
	 * @param Attribute The markup attribute.
	 * @return True if the position is within the attribute's range.
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Markup")
	static bool IsPositionInAttribute(int32 CharacterIndex, const FYarnMarkupAttribute& Attribute);

	/**
	 * Find all attributes that contain a specific character position.
	 *
	 * @param CharacterIndex The position to check.
	 * @param ParseResult The parsed markup.
	 * @return Array of attributes that contain the position.
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Markup")
	static TArray<FYarnMarkupAttribute> GetAttributesAtPosition(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult);

	/**
	 * Find all self-closing attributes (length 0) at a specific position.
	 *
	 * @param CharacterIndex The position to check.
	 * @param ParseResult The parsed markup.
	 * @return Array of self-closing attributes at the position.
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Markup")
	static TArray<FYarnMarkupAttribute> GetSelfClosingAttributesAtPosition(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult);

	/**
	 * Check if a character position is at the start of a markup range.
	 *
	 * @param CharacterIndex The position to check.
	 * @param AttributeName The attribute name to check for.
	 * @param ParseResult The parsed markup.
	 * @param OutAttribute If found, outputs the attribute.
	 * @return True if this position is the start of the named attribute.
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Markup")
	static bool IsAtRangeStart(int32 CharacterIndex, const FString& AttributeName, const FYarnMarkupParseResult& ParseResult, FYarnMarkupAttribute& OutAttribute);

	/**
	 * Check if a character position is at the end of a markup range.
	 *
	 * @param CharacterIndex The position to check.
	 * @param AttributeName The attribute name to check for.
	 * @param ParseResult The parsed markup.
	 * @param OutAttribute If found, outputs the attribute.
	 * @return True if this position is the end of the named attribute.
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Markup")
	static bool IsAtRangeEnd(int32 CharacterIndex, const FString& AttributeName, const FYarnMarkupParseResult& ParseResult, FYarnMarkupAttribute& OutAttribute);

	/**
	 * Get a float property from a markup attribute with a default value.
	 *
	 * @param Attribute The attribute to get the property from.
	 * @param PropertyName The property name.
	 * @param DefaultValue The default value if not found or not a number.
	 * @return The property value as a float.
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Markup")
	static float GetFloatProperty(const FYarnMarkupAttribute& Attribute, const FString& PropertyName, float DefaultValue = 0.0f);

	/**
	 * Get an integer property from a markup attribute with a default value.
	 *
	 * @param Attribute The attribute to get the property from.
	 * @param PropertyName The property name.
	 * @param DefaultValue The default value if not found or not a number.
	 * @return The property value as an integer.
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Markup")
	static int32 GetIntProperty(const FYarnMarkupAttribute& Attribute, const FString& PropertyName, int32 DefaultValue = 0);

	/**
	 * Get a boolean property from a markup attribute with a default value.
	 *
	 * @param Attribute The attribute to get the property from.
	 * @param PropertyName The property name.
	 * @param DefaultValue The default value if not found.
	 * @return The property value as a boolean.
	 */
	UFUNCTION(BlueprintPure, Category = "Yarn Spinner|Markup")
	static bool GetBoolProperty(const FYarnMarkupAttribute& Attribute, const FString& PropertyName, bool DefaultValue = false);
};
