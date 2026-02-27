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

#include "YarnDialogueUITemplates.h"
#include "YarnDialogueRunner.h"
#include "YarnProgram.h"
#include "YarnSpinnerModule.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

// ============================================================================
// UYarnSimpleDialogueWidget
// ============================================================================
// A basic dialogue widget with text and options. Good for prototyping or
// simple games that don't need portraits or styling.

void UYarnSimpleDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Hide until we're waiting for player input
    if (ContinueIndicator)
    {
        ContinueIndicator->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UYarnSimpleDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Typewriter effect: reveal text one character at a time
    if (bIsTypewriting && DialogueText)
    {
        TypewriterTime += InDeltaTime;
        float CharsPerSecond = TypewriterSpeed;
        int32 TargetIndex = FMath::FloorToInt(TypewriterTime * CharsPerSecond);

        if (TargetIndex > TypewriterIndex)
        {
            TypewriterIndex = FMath::Min(TargetIndex, CurrentFullText.Len());
            DialogueText->SetText(FText::FromString(CurrentFullText.Left(TypewriterIndex)));

            if (TypewriterIndex >= CurrentFullText.Len())
            {
                bIsTypewriting = false;
                OnTypewriterComplete.Broadcast();
            }
        }
    }
}

void UYarnSimpleDialogueWidget::ShowLine(const FString& SpeakerName, const FString& Text)
{
    if (SpeakerNameText)
    {
        if (SpeakerName.IsEmpty())
        {
            SpeakerNameText->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            SpeakerNameText->SetText(FText::FromString(SpeakerName));
            SpeakerNameText->SetVisibility(ESlateVisibility::Visible);
        }
    }

    CurrentFullText = Text;

    if (bUseTypewriter && DialogueText)
    {
        TypewriterIndex = 0;
        TypewriterTime = 0.0f;
        bIsTypewriting = true;
        DialogueText->SetText(FText::GetEmpty());
    }
    else if (DialogueText)
    {
        DialogueText->SetText(FText::FromString(Text));
        OnTypewriterComplete.Broadcast();
    }

    SetContinueIndicatorVisible(false);
    ClearOptions();
}

void UYarnSimpleDialogueWidget::ShowOptions(const TArray<FYarnOption>& Options)
{
    ClearOptions();

    if (!OptionsContainer)
    {
        return;
    }

    for (int32 i = 0; i < Options.Num(); i++)
    {
        const FYarnOption& Option = Options[i];

        UUserWidget* OptionWidget = nullptr;

        if (OptionButtonClass)
        {
            OptionWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), OptionButtonClass);
        }
        else
        {
            UE_LOG(LogYarnSpinner, Warning, TEXT("No OptionButtonClass set, options won't display properly"));
            continue;
        }

        if (OptionWidget)
        {
            if (UTextBlock* OptionText = Cast<UTextBlock>(OptionWidget->GetWidgetFromName(TEXT("OptionText"))))
            {
                FString OptionString = FString::Printf(TEXT("%d. %s"), i + 1, *Option.Line.Text.ToString());
                OptionText->SetText(FText::FromString(OptionString));
            }

            // Button click binding is handled by the custom widget class, which
            // is responsible for calling back with the option index.
            OptionsContainer->AddChild(OptionWidget);
        }
    }

    OptionsContainer->SetVisibility(ESlateVisibility::Visible);
}

void UYarnSimpleDialogueWidget::ClearOptions()
{
    if (OptionsContainer)
    {
        OptionsContainer->ClearChildren();
        OptionsContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UYarnSimpleDialogueWidget::SetContinueIndicatorVisible(bool bVisible)
{
    if (ContinueIndicator)
    {
        ContinueIndicator->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UYarnSimpleDialogueWidget::SkipTypewriter()
{
    if (bIsTypewriting && DialogueText)
    {
        bIsTypewriting = false;
        DialogueText->SetText(FText::FromString(CurrentFullText));
        OnTypewriterComplete.Broadcast();
    }
}

void UYarnSimpleDialogueWidget::OnOptionButtonClicked(int32 Index)
{
    OnOptionSelected.Broadcast(Index);
}

// ============================================================================
// UYarnRPGDialogueWidget
// ============================================================================
// An RPG-style dialogue widget with character portraits and coloured name
// plates. Good for JRPGs, WRPGs, and story-driven games.

void UYarnRPGDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (ContinueIndicator)
    {
        ContinueIndicator->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UYarnRPGDialogueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bIsTypewriting && DialogueText)
    {
        TypewriterTime += InDeltaTime;
        int32 TargetIndex = FMath::FloorToInt(TypewriterTime * TypewriterSpeed);

        if (TargetIndex > TypewriterIndex)
        {
            TypewriterIndex = FMath::Min(TargetIndex, CurrentFullText.Len());
            DialogueText->SetText(FText::FromString(CurrentFullText.Left(TypewriterIndex)));

            if (TypewriterIndex >= CurrentFullText.Len())
            {
                bIsTypewriting = false;
                OnTypewriterComplete.Broadcast();
            }
        }
    }
}

void UYarnRPGDialogueWidget::ShowLine(const FString& SpeakerName, const FString& Text, const FString& Emotion)
{
    if (SpeakerNameText)
    {
        SpeakerNameText->SetText(FText::FromString(SpeakerName));
    }

    // Colour the name plate per-character
    if (NamePlateBorder)
    {
        FLinearColor* Color = CharacterColors.Find(SpeakerName);
        NamePlateBorder->SetBrushColor(Color ? *Color : DefaultNameColor);
    }

    SetPortrait(SpeakerName, Emotion);

    CurrentFullText = Text;

    if (bUseTypewriter && DialogueText)
    {
        TypewriterIndex = 0;
        TypewriterTime = 0.0f;
        bIsTypewriting = true;
        DialogueText->SetText(FText::GetEmpty());
    }
    else if (DialogueText)
    {
        DialogueText->SetText(FText::FromString(Text));
        OnTypewriterComplete.Broadcast();
    }

    SetContinueIndicatorVisible(false);
    ClearOptions();
}

void UYarnRPGDialogueWidget::SetPortrait(const FString& CharacterName, const FString& Emotion)
{
    if (!PortraitImage)
    {
        return;
    }

    UTexture2D* Portrait = nullptr;

    // Fall back to the default portrait if we don't have one for this character
    if (UTexture2D** Found = CharacterPortraits.Find(CharacterName))
    {
        Portrait = *Found;
    }
    else
    {
        Portrait = DefaultPortrait;
    }

    // The Emotion parameter could be used to select different portrait
    // expressions with a more complex per-character texture setup.
    SetPortraitTexture(Portrait);
}

void UYarnRPGDialogueWidget::SetPortraitTexture(UTexture2D* Texture)
{
    if (PortraitImage)
    {
        if (Texture)
        {
            PortraitImage->SetBrushFromTexture(Texture);
            PortraitImage->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            PortraitImage->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void UYarnRPGDialogueWidget::ShowOptions(const TArray<FYarnOption>& Options)
{
    ClearOptions();

    if (!OptionsContainer)
    {
        return;
    }

    for (int32 i = 0; i < Options.Num(); i++)
    {
        const FYarnOption& Option = Options[i];

        UUserWidget* OptionWidget = nullptr;
        if (OptionButtonClass)
        {
            OptionWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), OptionButtonClass);
        }

        if (OptionWidget)
        {
            if (UTextBlock* OptionText = Cast<UTextBlock>(OptionWidget->GetWidgetFromName(TEXT("OptionText"))))
            {
                FString OptionString = FString::Printf(TEXT("%d. %s"), i + 1, *Option.Line.Text.ToString());
                OptionText->SetText(FText::FromString(OptionString));
            }

            OptionsContainer->AddChild(OptionWidget);
        }
    }

    OptionsContainer->SetVisibility(ESlateVisibility::Visible);
}

void UYarnRPGDialogueWidget::ClearOptions()
{
    if (OptionsContainer)
    {
        OptionsContainer->ClearChildren();
        OptionsContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UYarnRPGDialogueWidget::SetContinueIndicatorVisible(bool bVisible)
{
    if (ContinueIndicator)
    {
        ContinueIndicator->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UYarnRPGDialogueWidget::SkipTypewriter()
{
    if (bIsTypewriting && DialogueText)
    {
        bIsTypewriting = false;
        DialogueText->SetText(FText::FromString(CurrentFullText));
        OnTypewriterComplete.Broadcast();
    }
}

void UYarnRPGDialogueWidget::OnOptionButtonClicked(int32 Index)
{
    OnOptionSelected.Broadcast(Index);
}

// ============================================================================
// UYarnSubtitleWidget
// ============================================================================
// A minimal subtitle widget that displays text at the bottom of the screen.
// Good for games with voice acting where you don't want a full dialogue box.

void UYarnSubtitleWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetRenderOpacity(0.0f);
    bIsVisible = false;
}

void UYarnSubtitleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Fade in
    if (bIsFadingIn)
    {
        CurrentOpacity += InDeltaTime / FMath::Max(FadeDuration, 0.01f);
        if (CurrentOpacity >= 1.0f)
        {
            CurrentOpacity = 1.0f;
            bIsFadingIn = false;
        }
        SetRenderOpacity(CurrentOpacity);
    }
    // Fade out
    else if (bIsFadingOut)
    {
        CurrentOpacity -= InDeltaTime / FMath::Max(FadeDuration, 0.01f);
        if (CurrentOpacity <= 0.0f)
        {
            CurrentOpacity = 0.0f;
            bIsFadingOut = false;
            bIsVisible = false;
            // Notify listeners that the subtitle has finished hiding
            OnSubtitleHidden.Broadcast();
        }
        SetRenderOpacity(CurrentOpacity);
    }

    // Auto-hide timer
    if (bIsVisible && !bIsFadingOut && AutoHideDuration > 0.0f && TimeRemaining > 0.0f)
    {
        TimeRemaining -= InDeltaTime;
        if (TimeRemaining <= 0.0f)
        {
            HideSubtitle();
        }
    }
}

void UYarnSubtitleWidget::ShowSubtitle(const FString& SpeakerName, const FString& Text)
{
    FString FormattedText;

    if (bShowSpeakerName && !SpeakerName.IsEmpty())
    {
        FormattedText = SpeakerFormat;
        FormattedText = FormattedText.Replace(TEXT("{0}"), *SpeakerName);
        FormattedText = FormattedText.Replace(TEXT("{1}"), *Text);
    }
    else
    {
        FormattedText = Text;
    }

    if (SubtitleText)
    {
        SubtitleText->SetText(FText::FromString(FormattedText));
    }

    bIsVisible = true;
    bIsFadingIn = true;
    bIsFadingOut = false;
    TimeRemaining = AutoHideDuration;
}

void UYarnSubtitleWidget::HideSubtitle()
{
    // Don't do anything if already hidden or fading out
    if (!bIsVisible || bIsFadingOut)
    {
        return;
    }

    bIsFadingIn = false;
    bIsFadingOut = true;
}

// ============================================================================
// AYarnSimpleDialogueDemo
// ============================================================================
// A drop-in demo actor that sets up a complete dialogue system with the simple
// widget. Drag this into your level, set the YarnProject, and hit Play.

AYarnSimpleDialogueDemo::AYarnSimpleDialogueDemo()
{
    // Ticking isn't needed for this actor but doesn't hurt
    PrimaryActorTick.bCanEverTick = true;
}

void AYarnSimpleDialogueDemo::BeginPlay()
{
    Super::BeginPlay();

    SetupDialogueRunner();
    SetupUI();

    if (bAutoStart && YarnProject)
    {
        StartDialogue();
    }
}

void AYarnSimpleDialogueDemo::SetupDialogueRunner()
{
    DialogueRunner = NewObject<UYarnDialogueRunner>(this);
    DialogueRunner->RegisterComponent();
    DialogueRunner->YarnProject = YarnProject;

    // This demo hooks up events directly rather than using the presenter
    // system. In a real game you'd use the proper presenter components.
}

void AYarnSimpleDialogueDemo::SetupUI()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
    {
        return;
    }

    if (CustomWidgetClass)
    {
        DialogueWidget = CreateWidget<UYarnSimpleDialogueWidget>(PC, CustomWidgetClass);
    }
    else
    {
        DialogueWidget = CreateWidget<UYarnSimpleDialogueWidget>(PC, UYarnSimpleDialogueWidget::StaticClass());
    }

    if (DialogueWidget)
    {
        DialogueWidget->AddToViewport();
        DialogueWidget->OnOptionSelected.AddDynamic(this, &AYarnSimpleDialogueDemo::OnOptionSelected);
    }
}

void AYarnSimpleDialogueDemo::StartDialogue()
{
    if (DialogueRunner && YarnProject)
    {
        DialogueRunner->StartDialogue(StartNode);
    }
}

void AYarnSimpleDialogueDemo::OnLineReceived(const FYarnLocalizedLine& Line)
{
    if (DialogueWidget)
    {
        // Extract character name from "Character: text" format
        FString CharacterName;
        FString Text = Line.Text.ToString();

        int32 ColonIndex;
        if (Text.FindChar(TEXT(':'), ColonIndex) && ColonIndex < 30)
        {
            CharacterName = Text.Left(ColonIndex).TrimStartAndEnd();
            Text = Text.Mid(ColonIndex + 1).TrimStart();
        }

        DialogueWidget->ShowLine(CharacterName, Text);
    }
}

void AYarnSimpleDialogueDemo::OnOptionsReceived(const FYarnOptionSet& Options)
{
    if (DialogueWidget)
    {
        DialogueWidget->ShowOptions(Options.Options);
    }
}

void AYarnSimpleDialogueDemo::OnDialogueComplete()
{
    if (DialogueWidget)
    {
        DialogueWidget->RemoveFromParent();
    }
}

void AYarnSimpleDialogueDemo::OnOptionSelected(int32 OptionIndex)
{
    if (DialogueRunner)
    {
        DialogueRunner->SelectOption(OptionIndex);
    }
}

// ============================================================================
// AYarnRPGDialogueDemo
// ============================================================================
// Demo actor for the RPG-style dialogue widget with portraits and colours.

AYarnRPGDialogueDemo::AYarnRPGDialogueDemo()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AYarnRPGDialogueDemo::BeginPlay()
{
    Super::BeginPlay();

    SetupDialogueRunner();
    SetupUI();

    if (bAutoStart && YarnProject)
    {
        StartDialogue();
    }
}

void AYarnRPGDialogueDemo::SetupDialogueRunner()
{
    DialogueRunner = NewObject<UYarnDialogueRunner>(this);
    DialogueRunner->RegisterComponent();
    DialogueRunner->YarnProject = YarnProject;
}

void AYarnRPGDialogueDemo::SetupUI()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
    {
        return;
    }

    if (CustomWidgetClass)
    {
        DialogueWidget = CreateWidget<UYarnRPGDialogueWidget>(PC, CustomWidgetClass);
    }
    else
    {
        DialogueWidget = CreateWidget<UYarnRPGDialogueWidget>(PC, UYarnRPGDialogueWidget::StaticClass());
    }

    if (DialogueWidget)
    {
        // Copy character portrait and colour data to the widget
        DialogueWidget->CharacterPortraits = CharacterPortraits;
        DialogueWidget->CharacterColors = CharacterColors;

        DialogueWidget->AddToViewport();
        DialogueWidget->OnOptionSelected.AddDynamic(this, &AYarnRPGDialogueDemo::OnOptionSelected);
    }
}

void AYarnRPGDialogueDemo::StartDialogue()
{
    if (DialogueRunner && YarnProject)
    {
        DialogueRunner->StartDialogue(StartNode);
    }
}

void AYarnRPGDialogueDemo::OnLineReceived(const FYarnLocalizedLine& Line)
{
    if (DialogueWidget)
    {
        FString CharacterName;
        FString Text = Line.Text.ToString();

        int32 ColonIndex;
        if (Text.FindChar(TEXT(':'), ColonIndex) && ColonIndex < 30)
        {
            CharacterName = Text.Left(ColonIndex).TrimStartAndEnd();
            Text = Text.Mid(ColonIndex + 1).TrimStart();
        }

        DialogueWidget->ShowLine(CharacterName, Text);
    }
}

void AYarnRPGDialogueDemo::OnOptionsReceived(const FYarnOptionSet& Options)
{
    if (DialogueWidget)
    {
        DialogueWidget->ShowOptions(Options.Options);
    }
}

void AYarnRPGDialogueDemo::OnDialogueComplete()
{
    if (DialogueWidget)
    {
        DialogueWidget->RemoveFromParent();
    }
}

void AYarnRPGDialogueDemo::OnOptionSelected(int32 OptionIndex)
{
    if (DialogueRunner)
    {
        DialogueRunner->SelectOption(OptionIndex);
    }
}

// ============================================================================
// AYarnSubtitleDemo
// ============================================================================
// Demo actor for the subtitle widget. Good for games with voiceover where
// you want dialogue to appear without blocking gameplay.

AYarnSubtitleDemo::AYarnSubtitleDemo()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AYarnSubtitleDemo::BeginPlay()
{
    Super::BeginPlay();

    SetupDialogueRunner();
    SetupUI();

    if (bAutoStart && YarnProject)
    {
        StartDialogue();
    }
}

void AYarnSubtitleDemo::SetupDialogueRunner()
{
    DialogueRunner = NewObject<UYarnDialogueRunner>(this);
    DialogueRunner->RegisterComponent();
    DialogueRunner->YarnProject = YarnProject;
}

void AYarnSubtitleDemo::SetupUI()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
    {
        return;
    }

    if (CustomWidgetClass)
    {
        SubtitleWidget = CreateWidget<UYarnSubtitleWidget>(PC, CustomWidgetClass);
    }
    else
    {
        SubtitleWidget = CreateWidget<UYarnSubtitleWidget>(PC, UYarnSubtitleWidget::StaticClass());
    }

    if (SubtitleWidget)
    {
        SubtitleWidget->AutoHideDuration = SubtitleDuration;
        SubtitleWidget->AddToViewport();
    }
}

void AYarnSubtitleDemo::StartDialogue()
{
    if (DialogueRunner && YarnProject)
    {
        DialogueRunner->StartDialogue(StartNode);
    }
}

void AYarnSubtitleDemo::OnLineReceived(const FYarnLocalizedLine& Line)
{
    if (SubtitleWidget)
    {
        FString CharacterName;
        FString Text = Line.Text.ToString();

        int32 ColonIndex;
        if (Text.FindChar(TEXT(':'), ColonIndex) && ColonIndex < 30)
        {
            CharacterName = Text.Left(ColonIndex).TrimStartAndEnd();
            Text = Text.Mid(ColonIndex + 1).TrimStart();
        }

        SubtitleWidget->ShowSubtitle(CharacterName, Text);

        // Auto-advance after the subtitle duration for a voice-over experience
        if (SubtitleDuration > 0.0f)
        {
            GetWorld()->GetTimerManager().SetTimer(
                AutoAdvanceTimer,
                [this]()
                {
                    if (DialogueRunner)
                    {
                        DialogueRunner->Continue();
                    }
                },
                SubtitleDuration,
                false  // Don't loop
            );
        }
    }
}

void AYarnSubtitleDemo::OnOptionsReceived(const FYarnOptionSet& Options)
{
    // Subtitles don't support options, so auto-select the first one
    if (DialogueRunner && Options.Options.Num() > 0)
    {
        DialogueRunner->SelectOption(0);
    }
}

void AYarnSubtitleDemo::OnDialogueComplete()
{
    if (SubtitleWidget)
    {
        SubtitleWidget->HideSubtitle();
    }
}
