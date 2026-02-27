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

#include "YarnActionMarkupHandler.h"
#include "YarnSpinnerModule.h"
#include "Kismet/GameplayStatics.h"

// ============================================================================
// UYarnActionMarkupHandlerComponent
// ============================================================================

void UYarnActionMarkupHandlerComponent::OnPrepareForLine_Implementation(const FYarnMarkupParseResult& ParseResult)
{
	CurrentParseResult = ParseResult;
}

void UYarnActionMarkupHandlerComponent::OnLineDisplayBegin_Implementation(const FYarnMarkupParseResult& ParseResult)
{
	// Default does nothing - override in subclasses
}

float UYarnActionMarkupHandlerComponent::OnCharacterWillAppear_Implementation(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken)
{
	// Default returns no delay
	return 0.0f;
}

void UYarnActionMarkupHandlerComponent::OnLineDisplayComplete_Implementation()
{
	// Default does nothing
}

void UYarnActionMarkupHandlerComponent::OnLineWillDismiss_Implementation()
{
	// Default does nothing
}

// ============================================================================
// UYarnPauseEventProcessor
// ============================================================================

UYarnPauseEventProcessor::UYarnPauseEventProcessor()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UYarnPauseEventProcessor::OnPrepareForLine_Implementation(const FYarnMarkupParseResult& ParseResult)
{
	Super::OnPrepareForLine_Implementation(ParseResult);

	// Find all pause markers and record their positions
	PausePositions.Empty();

	for (const FYarnMarkupAttribute& Attr : ParseResult.Attributes)
	{
		if (Attr.Name.Equals(PauseMarkerName, ESearchCase::IgnoreCase))
		{
			// Self-closing pause tags have Length = 0, position is where pause occurs
			int32 PausePos = Attr.Position;

			// Get duration from properties, or use default
			float Duration = DefaultPauseDuration;

			// Check for duration property (in seconds)
			FString DurationStr = Attr.GetProperty(TEXT("duration"));
			if (!DurationStr.IsEmpty())
			{
				Duration = FCString::Atof(*DurationStr);
			}

			// Also check for shorthand "pause" property (e.g., [pause=1.5/])
			FString PauseStr = Attr.GetProperty(TEXT("pause"));
			if (!PauseStr.IsEmpty())
			{
				Duration = FCString::Atof(*PauseStr);
			}

			// Also check for "ms" property (milliseconds)
			FString MsStr = Attr.GetProperty(TEXT("ms"));
			if (!MsStr.IsEmpty())
			{
				Duration = FCString::Atof(*MsStr) / 1000.0f;
			}

			PausePositions.Add(PausePos, Duration);
		}
	}
}

float UYarnPauseEventProcessor::OnCharacterWillAppear_Implementation(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken)
{
	// Check if we're being hurried - skip pauses
	if (CancellationToken.IsHurryUpRequested())
	{
		return 0.0f;
	}

	// Check if there's a pause at this position
	if (const float* Duration = PausePositions.Find(CharacterIndex))
	{
		return *Duration;
	}

	return 0.0f;
}

// ============================================================================
// UYarnMarkupEventHandler
// ============================================================================

UYarnMarkupEventHandler::UYarnMarkupEventHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UYarnMarkupEventHandler::ShouldHandleAttribute(const FString& AttributeName) const
{
	// If no specific attributes are configured, handle all
	if (AttributesToWatch.Num() == 0)
	{
		return true;
	}

	// Check if this attribute is in our watch list
	for (const FString& WatchedName : AttributesToWatch)
	{
		if (WatchedName.Equals(AttributeName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

void UYarnMarkupEventHandler::OnPrepareForLine_Implementation(const FYarnMarkupParseResult& ParseResult)
{
	Super::OnPrepareForLine_Implementation(ParseResult);

	// Cache attribute positions for efficient lookup during typewriter
	SelfClosingAttributes.Empty();
	RangeStartPositions.Empty();
	RangeEndPositions.Empty();

	for (const FYarnMarkupAttribute& Attr : ParseResult.Attributes)
	{
		if (!ShouldHandleAttribute(Attr.Name))
		{
			continue;
		}

		if (Attr.Length == 0)
		{
			// Self-closing tag
			if (bFireForSelfClosingTags)
			{
				SelfClosingAttributes.Add(Attr.Position, Attr);
			}
		}
		else
		{
			// Range tag
			if (bFireForRangeTags)
			{
				RangeStartPositions.Add(Attr.Position, Attr);
				RangeEndPositions.Add(Attr.Position + Attr.Length, Attr);
			}
		}
	}

	// Fire prepare event
	OnPrepareForLineEvent.Broadcast(ParseResult);
}

void UYarnMarkupEventHandler::OnLineDisplayBegin_Implementation(const FYarnMarkupParseResult& ParseResult)
{
	Super::OnLineDisplayBegin_Implementation(ParseResult);
	OnLineDisplayBeginEvent.Broadcast(ParseResult);
}

float UYarnMarkupEventHandler::OnCharacterWillAppear_Implementation(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken)
{
	// Check for self-closing attributes at this position
	if (const FYarnMarkupAttribute* Attr = SelfClosingAttributes.Find(CharacterIndex))
	{
		OnMarkupAttributeEncountered.Broadcast(Attr->Name, CharacterIndex, *Attr);
	}

	// Check for range starts at this position
	if (const FYarnMarkupAttribute* Attr = RangeStartPositions.Find(CharacterIndex))
	{
		OnMarkupRangeEnter.Broadcast(Attr->Name, CharacterIndex, *Attr);
	}

	// Check for range ends at this position
	if (const FYarnMarkupAttribute* Attr = RangeEndPositions.Find(CharacterIndex))
	{
		OnMarkupRangeExit.Broadcast(Attr->Name, CharacterIndex, *Attr);
	}

	return 0.0f; // This handler doesn't add delays
}

void UYarnMarkupEventHandler::OnLineDisplayComplete_Implementation()
{
	Super::OnLineDisplayComplete_Implementation();
	OnLineDisplayCompleteEvent.Broadcast();
}

void UYarnMarkupEventHandler::OnLineWillDismiss_Implementation()
{
	Super::OnLineWillDismiss_Implementation();
	OnLineWillDismissEvent.Broadcast();
}

// ============================================================================
// UYarnSoundEffectHandler
// ============================================================================

UYarnSoundEffectHandler::UYarnSoundEffectHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UYarnSoundEffectHandler::OnPrepareForLine_Implementation(const FYarnMarkupParseResult& ParseResult)
{
	Super::OnPrepareForLine_Implementation(ParseResult);

	// Cache sound positions
	SoundPositions.Empty();

	for (const FYarnMarkupAttribute& Attr : ParseResult.Attributes)
	{
		if (Attr.Name.Equals(SoundMarkerName, ESearchCase::IgnoreCase))
		{
			SoundPositions.Add(Attr.Position, Attr);
		}
	}
}

float UYarnSoundEffectHandler::OnCharacterWillAppear_Implementation(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken)
{
	// Check if there's a sound to play at this position
	if (const FYarnMarkupAttribute* Attr = SoundPositions.Find(CharacterIndex))
	{
		// Get the sound name from the attribute
		FString SoundName = Attr->GetProperty(TEXT("name"));
		if (SoundName.IsEmpty())
		{
			// Try the attribute's own name property (for [sfx=soundname/] syntax)
			SoundName = Attr->GetProperty(SoundMarkerName);
		}

		if (!SoundName.IsEmpty())
		{
			// Look up the sound
			USoundBase* Sound = GetSoundForName(SoundName);

			if (Sound)
			{
				// Play the sound
				UGameplayStatics::PlaySound2D(this, Sound, VolumeMultiplier, PitchMultiplier);
			}
			else
			{
				// Fire event so Blueprint can handle it
				OnSoundNotFound.Broadcast(SoundName, CharacterIndex, *Attr);
			}
		}
	}

	return 0.0f; // Sound effects don't add delays by default
}

USoundBase* UYarnSoundEffectHandler::GetSoundForName_Implementation(const FString& SoundName)
{
	// Look up in the sound map
	if (USoundBase** Found = SoundMap.Find(SoundName))
	{
		return *Found;
	}

	return nullptr;
}

// ============================================================================
// UYarnActionMarkupHandlerRegistry
// ============================================================================

UYarnActionMarkupHandlerRegistry::UYarnActionMarkupHandlerRegistry()
{
}

void UYarnActionMarkupHandlerRegistry::RegisterHandler(TScriptInterface<IYarnActionMarkupHandler> Handler)
{
	if (!Handler.GetInterface())
	{
		return;
	}

	// Avoid duplicates
	if (!GlobalHandlers.Contains(Handler))
	{
		GlobalHandlers.Add(Handler);
	}
}

void UYarnActionMarkupHandlerRegistry::RegisterHandlerForAttribute(const FString& AttributeName, TScriptInterface<IYarnActionMarkupHandler> Handler)
{
	if (!Handler.GetInterface() || AttributeName.IsEmpty())
	{
		return;
	}

	TArray<TScriptInterface<IYarnActionMarkupHandler>>& Handlers = AttributeHandlers.FindOrAdd(AttributeName.ToLower());

	// Avoid duplicates
	if (!Handlers.Contains(Handler))
	{
		Handlers.Add(Handler);
	}
}

void UYarnActionMarkupHandlerRegistry::UnregisterHandler(TScriptInterface<IYarnActionMarkupHandler> Handler)
{
	GlobalHandlers.Remove(Handler);

	// Remove from all attribute handlers
	for (auto& Pair : AttributeHandlers)
	{
		Pair.Value.Remove(Handler);
	}
}

void UYarnActionMarkupHandlerRegistry::ClearAllHandlers()
{
	GlobalHandlers.Empty();
	AttributeHandlers.Empty();
}

TArray<TScriptInterface<IYarnActionMarkupHandler>> UYarnActionMarkupHandlerRegistry::GetHandlersForPosition(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult) const
{
	TArray<TScriptInterface<IYarnActionMarkupHandler>> Result;

	// Always include global handlers
	Result.Append(GlobalHandlers);

	// Find which attributes are active at this position
	for (const FYarnMarkupAttribute& Attr : ParseResult.Attributes)
	{
		// Check if this position is at the start of the attribute, at the end, or inside it
		bool bIsRelevant = false;

		if (Attr.Length == 0)
		{
			// Self-closing tag - only relevant at exact position
			bIsRelevant = (CharacterIndex == Attr.Position);
		}
		else
		{
			// Range tag - relevant at start, end, or anywhere inside
			bIsRelevant = (CharacterIndex >= Attr.Position && CharacterIndex < Attr.Position + Attr.Length);
		}

		if (bIsRelevant)
		{
			// Look up handlers for this attribute name
			if (const TArray<TScriptInterface<IYarnActionMarkupHandler>>* Handlers = AttributeHandlers.Find(Attr.Name.ToLower()))
			{
				for (const TScriptInterface<IYarnActionMarkupHandler>& Handler : *Handlers)
				{
					// Avoid duplicates
					if (!Result.Contains(Handler))
					{
						Result.Add(Handler);
					}
				}
			}
		}
	}

	return Result;
}

void UYarnActionMarkupHandlerRegistry::DispatchPrepareForLine(const FYarnMarkupParseResult& ParseResult)
{
	// Dispatch to all global handlers
	for (const TScriptInterface<IYarnActionMarkupHandler>& Handler : GlobalHandlers)
	{
		if (Handler.GetInterface())
		{
			IYarnActionMarkupHandler::Execute_OnPrepareForLine(Handler.GetObject(), ParseResult);
		}
	}

	// Dispatch to attribute-specific handlers (for any attribute in the parse result)
	TSet<UObject*> NotifiedObjects;
	for (const FYarnMarkupAttribute& Attr : ParseResult.Attributes)
	{
		if (const TArray<TScriptInterface<IYarnActionMarkupHandler>>* Handlers = AttributeHandlers.Find(Attr.Name.ToLower()))
		{
			for (const TScriptInterface<IYarnActionMarkupHandler>& Handler : *Handlers)
			{
				if (Handler.GetInterface() && !NotifiedObjects.Contains(Handler.GetObject()))
				{
					IYarnActionMarkupHandler::Execute_OnPrepareForLine(Handler.GetObject(), ParseResult);
					NotifiedObjects.Add(Handler.GetObject());
				}
			}
		}
	}
}

void UYarnActionMarkupHandlerRegistry::DispatchLineDisplayBegin(const FYarnMarkupParseResult& ParseResult)
{
	// Dispatch to all global handlers
	for (const TScriptInterface<IYarnActionMarkupHandler>& Handler : GlobalHandlers)
	{
		if (Handler.GetInterface())
		{
			IYarnActionMarkupHandler::Execute_OnLineDisplayBegin(Handler.GetObject(), ParseResult);
		}
	}

	// Dispatch to attribute-specific handlers
	TSet<UObject*> NotifiedObjects;
	for (const FYarnMarkupAttribute& Attr : ParseResult.Attributes)
	{
		if (const TArray<TScriptInterface<IYarnActionMarkupHandler>>* Handlers = AttributeHandlers.Find(Attr.Name.ToLower()))
		{
			for (const TScriptInterface<IYarnActionMarkupHandler>& Handler : *Handlers)
			{
				if (Handler.GetInterface() && !NotifiedObjects.Contains(Handler.GetObject()))
				{
					IYarnActionMarkupHandler::Execute_OnLineDisplayBegin(Handler.GetObject(), ParseResult);
					NotifiedObjects.Add(Handler.GetObject());
				}
			}
		}
	}
}

float UYarnActionMarkupHandlerRegistry::DispatchCharacterWillAppear(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken)
{
	float MaxDelay = 0.0f;

	// Get all relevant handlers for this position
	TArray<TScriptInterface<IYarnActionMarkupHandler>> Handlers = GetHandlersForPosition(CharacterIndex, ParseResult);

	for (const TScriptInterface<IYarnActionMarkupHandler>& Handler : Handlers)
	{
		if (Handler.GetInterface())
		{
			float Delay = IYarnActionMarkupHandler::Execute_OnCharacterWillAppear(Handler.GetObject(), CharacterIndex, ParseResult, CancellationToken);
			MaxDelay = FMath::Max(MaxDelay, Delay);
		}
	}

	return MaxDelay;
}

void UYarnActionMarkupHandlerRegistry::DispatchLineDisplayComplete()
{
	// Dispatch to all global handlers
	for (const TScriptInterface<IYarnActionMarkupHandler>& Handler : GlobalHandlers)
	{
		if (Handler.GetInterface())
		{
			IYarnActionMarkupHandler::Execute_OnLineDisplayComplete(Handler.GetObject());
		}
	}

	// Dispatch to all attribute handlers (they all need to know the line is done)
	TSet<UObject*> NotifiedObjects;
	for (const auto& Pair : AttributeHandlers)
	{
		for (const TScriptInterface<IYarnActionMarkupHandler>& Handler : Pair.Value)
		{
			if (Handler.GetInterface() && !NotifiedObjects.Contains(Handler.GetObject()))
			{
				IYarnActionMarkupHandler::Execute_OnLineDisplayComplete(Handler.GetObject());
				NotifiedObjects.Add(Handler.GetObject());
			}
		}
	}
}

void UYarnActionMarkupHandlerRegistry::DispatchLineWillDismiss()
{
	// Dispatch to all global handlers
	for (const TScriptInterface<IYarnActionMarkupHandler>& Handler : GlobalHandlers)
	{
		if (Handler.GetInterface())
		{
			IYarnActionMarkupHandler::Execute_OnLineWillDismiss(Handler.GetObject());
		}
	}

	// Dispatch to all attribute handlers
	TSet<UObject*> NotifiedObjects;
	for (const auto& Pair : AttributeHandlers)
	{
		for (const TScriptInterface<IYarnActionMarkupHandler>& Handler : Pair.Value)
		{
			if (Handler.GetInterface() && !NotifiedObjects.Contains(Handler.GetObject()))
			{
				IYarnActionMarkupHandler::Execute_OnLineWillDismiss(Handler.GetObject());
				NotifiedObjects.Add(Handler.GetObject());
			}
		}
	}
}

TArray<TScriptInterface<IYarnActionMarkupHandler>> UYarnActionMarkupHandlerRegistry::GetAllHandlers() const
{
	TArray<TScriptInterface<IYarnActionMarkupHandler>> Result = GlobalHandlers;

	// Add all attribute handlers, avoiding duplicates
	for (const auto& Pair : AttributeHandlers)
	{
		for (const TScriptInterface<IYarnActionMarkupHandler>& Handler : Pair.Value)
		{
			if (!Result.Contains(Handler))
			{
				Result.Add(Handler);
			}
		}
	}

	return Result;
}

// ============================================================================
// UYarnMarkupHandlerLibrary
// ============================================================================

bool UYarnMarkupHandlerLibrary::IsPositionInAttribute(int32 CharacterIndex, const FYarnMarkupAttribute& Attribute)
{
	if (Attribute.Length == 0)
	{
		// Self-closing tag - only matches exact position
		return CharacterIndex == Attribute.Position;
	}

	// Range tag
	return CharacterIndex >= Attribute.Position && CharacterIndex < Attribute.Position + Attribute.Length;
}

TArray<FYarnMarkupAttribute> UYarnMarkupHandlerLibrary::GetAttributesAtPosition(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult)
{
	TArray<FYarnMarkupAttribute> Result;

	for (const FYarnMarkupAttribute& Attr : ParseResult.Attributes)
	{
		if (IsPositionInAttribute(CharacterIndex, Attr))
		{
			Result.Add(Attr);
		}
	}

	return Result;
}

TArray<FYarnMarkupAttribute> UYarnMarkupHandlerLibrary::GetSelfClosingAttributesAtPosition(int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult)
{
	TArray<FYarnMarkupAttribute> Result;

	for (const FYarnMarkupAttribute& Attr : ParseResult.Attributes)
	{
		if (Attr.Length == 0 && Attr.Position == CharacterIndex)
		{
			Result.Add(Attr);
		}
	}

	return Result;
}

bool UYarnMarkupHandlerLibrary::IsAtRangeStart(int32 CharacterIndex, const FString& AttributeName, const FYarnMarkupParseResult& ParseResult, FYarnMarkupAttribute& OutAttribute)
{
	for (const FYarnMarkupAttribute& Attr : ParseResult.Attributes)
	{
		if (Attr.Length > 0 && Attr.Position == CharacterIndex && Attr.Name.Equals(AttributeName, ESearchCase::IgnoreCase))
		{
			OutAttribute = Attr;
			return true;
		}
	}

	return false;
}

bool UYarnMarkupHandlerLibrary::IsAtRangeEnd(int32 CharacterIndex, const FString& AttributeName, const FYarnMarkupParseResult& ParseResult, FYarnMarkupAttribute& OutAttribute)
{
	for (const FYarnMarkupAttribute& Attr : ParseResult.Attributes)
	{
		if (Attr.Length > 0 && (Attr.Position + Attr.Length) == CharacterIndex && Attr.Name.Equals(AttributeName, ESearchCase::IgnoreCase))
		{
			OutAttribute = Attr;
			return true;
		}
	}

	return false;
}

float UYarnMarkupHandlerLibrary::GetFloatProperty(const FYarnMarkupAttribute& Attribute, const FString& PropertyName, float DefaultValue)
{
	FString Value = Attribute.GetProperty(PropertyName);
	if (Value.IsEmpty())
	{
		return DefaultValue;
	}

	return FCString::Atof(*Value);
}

int32 UYarnMarkupHandlerLibrary::GetIntProperty(const FYarnMarkupAttribute& Attribute, const FString& PropertyName, int32 DefaultValue)
{
	FString Value = Attribute.GetProperty(PropertyName);
	if (Value.IsEmpty())
	{
		return DefaultValue;
	}

	return FCString::Atoi(*Value);
}

bool UYarnMarkupHandlerLibrary::GetBoolProperty(const FYarnMarkupAttribute& Attribute, const FString& PropertyName, bool DefaultValue)
{
	FString Value = Attribute.GetProperty(PropertyName);
	if (Value.IsEmpty())
	{
		return DefaultValue;
	}

	// Check for various true/false representations
	if (Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("1"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	if (Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("0"), ESearchCase::IgnoreCase) ||
		Value.Equals(TEXT("no"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	return DefaultValue;
}
