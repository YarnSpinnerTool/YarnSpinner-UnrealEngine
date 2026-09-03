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
#include "YarnDialoguePresenter.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/AssetManager.h"

// ============================================================================

void UYarnBlueprintActionMarkupHandler::OnPrepareForLine(UYarnDialoguePresenter* Presenter, const FYarnMarkupParseResult& ParseResult)
{
	ReceiveOnPrepareForLine(Presenter, ParseResult);
}

void UYarnBlueprintActionMarkupHandler::OnLineDisplayBegin(UYarnDialoguePresenter* Presenter, const FYarnMarkupParseResult& ParseResult)
{
	ReceiveOnLineDisplayBegin(Presenter, ParseResult);
}

float UYarnBlueprintActionMarkupHandler::OnCharacterWillAppear(UYarnDialoguePresenter* Presenter, int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken)
{
	return ReceiveOnCharacterWillAppear(Presenter, CharacterIndex, ParseResult, CancellationToken);
}

void UYarnBlueprintActionMarkupHandler::OnLineDisplayComplete(UYarnDialoguePresenter* Presenter)
{
	ReceiveOnLineDisplayComplete(Presenter);
}

void UYarnBlueprintActionMarkupHandler::OnLineWillDismiss(UYarnDialoguePresenter* Presenter)
{
	ReceiveOnLineWillDismiss(Presenter);
}

// ============================================================================
// UYarnPauseEventProcessor
// ============================================================================

void UYarnPauseEventProcessor::OnPrepareForLine(UYarnDialoguePresenter* Presenter, const FYarnMarkupParseResult& ParseResult)
{
	PausePositions.Empty();

	for (const FYarnMarkupAttribute& Attr : ParseResult.Attributes)
	{
		if (Attr.Name.Equals(PauseMarkerName, ESearchCase::IgnoreCase))
		{
			int32 PausePos = Attr.Position;

			float Duration = DefaultPauseDuration;

			FString DurationStr = Attr.GetProperty(TEXT("duration"));
			if (!DurationStr.IsEmpty())
			{
				Duration = FCString::Atof(*DurationStr);
			}

			FString PauseStr = Attr.GetProperty(TEXT("pause"));
			if (!PauseStr.IsEmpty())
			{
				Duration = FCString::Atof(*PauseStr);
			}

			FString MsStr = Attr.GetProperty(TEXT("ms"));
			if (!MsStr.IsEmpty())
			{
				Duration = FCString::Atof(*MsStr) / 1000.0f;
			}

			PausePositions.Add(PausePos, Duration);
		}
	}
}

float UYarnPauseEventProcessor::OnCharacterWillAppear(UYarnDialoguePresenter* Presenter, int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken)
{
	if (CancellationToken.IsHurryUpRequested())
	{
		return 0.0f;
	}

	if (const float* Duration = PausePositions.Find(CharacterIndex))
	{
		const float Result = *Duration;
		PausePositions.Remove(CharacterIndex);
		return Result;
	}

	return 0.0f;
}

// ============================================================================
// UYarnMarkupEventHandler
// ============================================================================

bool UYarnMarkupEventHandler::ShouldHandleAttribute(const FString& AttributeName) const
{
	if (AttributesToWatch.Num() == 0)
	{
		return true;
	}

	for (const FString& WatchedName : AttributesToWatch)
	{
		if (WatchedName.Equals(AttributeName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

void UYarnMarkupEventHandler::OnPrepareForLine(UYarnDialoguePresenter* Presenter, const FYarnMarkupParseResult& ParseResult)
{
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
			if (bFireForSelfClosingTags)
			{
				SelfClosingAttributes.Add(Attr.Position, Attr);
			}
		}
		else
		{
			if (bFireForRangeTags)
			{
				RangeStartPositions.Add(Attr.Position, Attr);
				RangeEndPositions.Add(Attr.Position + Attr.Length, Attr);
			}
		}
	}

	OnPrepareForLineEvent.Broadcast(ParseResult);
}

void UYarnMarkupEventHandler::OnLineDisplayBegin(UYarnDialoguePresenter* Presenter, const FYarnMarkupParseResult& ParseResult)
{
	OnLineDisplayBeginEvent.Broadcast(ParseResult);
}

float UYarnMarkupEventHandler::OnCharacterWillAppear(UYarnDialoguePresenter* Presenter, int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken)
{
	if (const FYarnMarkupAttribute* Attr = SelfClosingAttributes.Find(CharacterIndex))
	{
		OnMarkupAttributeEncountered.Broadcast(Attr->Name, CharacterIndex, *Attr);
	}

	if (const FYarnMarkupAttribute* Attr = RangeStartPositions.Find(CharacterIndex))
	{
		OnMarkupRangeEnter.Broadcast(Attr->Name, CharacterIndex, *Attr);
	}

	if (const FYarnMarkupAttribute* Attr = RangeEndPositions.Find(CharacterIndex))
	{
		OnMarkupRangeExit.Broadcast(Attr->Name, CharacterIndex, *Attr);
	}

	return 0.0f;
}

void UYarnMarkupEventHandler::OnLineDisplayComplete(UYarnDialoguePresenter* Presenter)
{
	OnLineDisplayCompleteEvent.Broadcast();
}

void UYarnMarkupEventHandler::OnLineWillDismiss(UYarnDialoguePresenter* Presenter)
{
	OnLineWillDismissEvent.Broadcast();
}

// ============================================================================
// UYarnSoundEffectHandler
// ============================================================================

void UYarnSoundEffectHandler::OnPrepareForLine(UYarnDialoguePresenter* Presenter, const FYarnMarkupParseResult& ParseResult)
{
	SoundPositions.Empty();

	for (const FYarnMarkupAttribute& Attr : ParseResult.Attributes)
	{
		if (Attr.Name.Equals(SoundMarkerName, ESearchCase::IgnoreCase))
		{
			SoundPositions.Add(Attr.Position, Attr);
		}
	}

	for (const TPair<int32, FYarnMarkupAttribute>& Pair : SoundPositions)
	{
		FString SoundName = Pair.Value.GetProperty(TEXT("name"));
		if (SoundName.IsEmpty())
		{
			SoundName = Pair.Value.GetProperty(SoundMarkerName);
		}

		if (!SoundName.IsEmpty())
		{
			RequestSound(FName(*SoundName));
		}
	}
}

float UYarnSoundEffectHandler::OnCharacterWillAppear(UYarnDialoguePresenter* Presenter, int32 CharacterIndex, const FYarnMarkupParseResult& ParseResult, const FYarnLineCancellationToken& CancellationToken)
{
	if (const FYarnMarkupAttribute* Attr = SoundPositions.Find(CharacterIndex))
	{
		FString SoundName = Attr->GetProperty(TEXT("name"));
		if (SoundName.IsEmpty())
		{
			SoundName = Attr->GetProperty(SoundMarkerName);
		}

		if (!SoundName.IsEmpty())
		{
			PlaySoundByName(Presenter, FName(*SoundName), CharacterIndex, *Attr);
		}
	}

	return 0.0f;
}

TSharedPtr<FStreamableHandle> UYarnSoundEffectHandler::RequestSound(FName SoundName)
{
	const TSoftObjectPtr<USoundBase>* SoftSound = SoundMap.Find(SoundName);
	if (!SoftSound || SoftSound->IsNull())
	{
		return nullptr;
	}

	if (SoftSound->IsValid())
	{
		return nullptr;
	}

	if (TSharedPtr<FStreamableHandle>* Existing = LoadHandles.Find(SoundName))
	{
		return *Existing;
	}

	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftSound->ToSoftObjectPath());
	LoadHandles.Add(SoundName, Handle);
	return Handle;
}

void UYarnSoundEffectHandler::PlaySoundByName(UYarnDialoguePresenter* Presenter, FName SoundName, int32 CharacterIndex, const FYarnMarkupAttribute& Attribute)
{
	const TSoftObjectPtr<USoundBase>* SoftSound = SoundMap.Find(SoundName);
	if (!SoftSound || SoftSound->IsNull())
	{
		USoundBase* FallbackSound = GetSoundForName(SoundName.ToString());
		if (FallbackSound)
		{
			UGameplayStatics::PlaySound2D(Presenter, FallbackSound, VolumeMultiplier, PitchMultiplier);
		}
		else
		{
			OnSoundNotFound.Broadcast(SoundName.ToString(), CharacterIndex, Attribute);
		}
		return;
	}

	if (USoundBase* LoadedSound = SoftSound->Get())
	{
		UGameplayStatics::PlaySound2D(Presenter, LoadedSound, VolumeMultiplier, PitchMultiplier);
		return;
	}

	TSharedPtr<FStreamableHandle> Handle = RequestSound(SoundName);
	if (!Handle.IsValid())
	{
		OnSoundNotFound.Broadcast(SoundName.ToString(), CharacterIndex, Attribute);
		return;
	}

	TWeakObjectPtr<UYarnSoundEffectHandler> WeakThis(this);
	TWeakObjectPtr<UYarnDialoguePresenter> WeakPresenter(Presenter);
	const float Volume = VolumeMultiplier;
	const float Pitch = PitchMultiplier;
	FSoftObjectPath SoundPath = SoftSound->ToSoftObjectPath();

	Handle->BindCompleteDelegate(FStreamableDelegate::CreateLambda([WeakThis, WeakPresenter, SoundPath, Volume, Pitch]()
	{
		if (!WeakThis.IsValid() || !WeakPresenter.IsValid())
		{
			return;
		}

		if (USoundBase* Sound = Cast<USoundBase>(SoundPath.ResolveObject()))
		{
			UGameplayStatics::PlaySound2D(WeakPresenter.Get(), Sound, Volume, Pitch);
		}
	}));
}

USoundBase* UYarnSoundEffectHandler::GetSoundForName_Implementation(const FString& SoundName)
{
	if (const TSoftObjectPtr<USoundBase>* SoftSound = SoundMap.Find(FName(*SoundName)))
	{
		return SoftSound->Get();
	}

	return nullptr;
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
