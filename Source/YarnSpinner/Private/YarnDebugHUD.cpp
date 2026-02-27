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

#include "YarnDebugHUD.h"
#include "YarnDialogueRunner.h"
#include "YarnSpinnerModule.h"
#include "YarnVariableStorage.h"
#include "YarnSpinnerCore.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "GameFramework/PlayerController.h"

// ============================================================================
// UYarnDebugWidget
// ============================================================================
// The widget displayed on screen showing current dialogue state, variables, and
// execution history. Subclass in Blueprint to customise the look and feel.

void UYarnDebugWidget::SetupDebugWidget(UYarnDialogueRunner* InDialogueRunner)
{
    DialogueRunner = InDialogueRunner;
    RefreshDisplay();
}

void UYarnDebugWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UYarnDebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    TimeSinceLastRefresh += InDeltaTime;
    if (TimeSinceLastRefresh >= RefreshInterval)
    {
        TimeSinceLastRefresh = 0.0f;
        RefreshDisplay();
    }
}

void UYarnDebugWidget::RefreshDisplay()
{
    if (!DialogueRunner)
    {
        CurrentNodeName = TEXT("(No DialogueRunner)");
        CurrentLineID = TEXT("");
        bIsDialogueRunning = false;
        return;
    }

    bIsDialogueRunning = DialogueRunner->IsDialogueRunning();
    CurrentNodeName = DialogueRunner->GetCurrentNodeName();

    // The line ID gets updated when AddHistoryEntry is called with a LINE entry,
    // since the runner doesn't track it directly.
}

void UYarnDebugWidget::AddHistoryEntry(const FYarnDebugHistoryEntry& Entry)
{
    ExecutionHistory.Add(Entry);

    // Use a while loop in case MaxHistoryEntries was reduced at runtime
    while (ExecutionHistory.Num() > MaxHistoryEntries)
    {
        ExecutionHistory.RemoveAt(0);
    }

    if (Entry.EntryType == TEXT("LINE"))
    {
        CurrentLineID = Entry.LineID;
    }
}

void UYarnDebugWidget::ClearHistory()
{
    ExecutionHistory.Empty();
}

TArray<FString> UYarnDebugWidget::GetFormattedVariables() const
{
    TArray<FString> Result;

    if (!DialogueRunner)
    {
        Result.Add(TEXT("(No DialogueRunner)"));
        return Result;
    }

    // The interface doesn't support enumerating all variables, so we cast
    // to the concrete type that has GetAllVariablesAsMap.
    UYarnInMemoryVariableStorage* InMemoryStorage = Cast<UYarnInMemoryVariableStorage>(DialogueRunner->VariableStorage.GetObject());
    if (!InMemoryStorage)
    {
        Result.Add(TEXT("(No InMemoryVariableStorage)"));
        return Result;
    }

    TMap<FString, FYarnValue> AllVariables;
    InMemoryStorage->GetAllVariablesAsMap(AllVariables);

    for (const auto& Pair : AllVariables)
    {
        FString ValueStr;
        switch (Pair.Value.Type)
        {
        case EYarnValueType::String:
            ValueStr = FString::Printf(TEXT("\"%s\""), *Pair.Value.GetStringValue());
            break;
        case EYarnValueType::Number:
            ValueStr = FString::Printf(TEXT("%.2f"), Pair.Value.GetNumberValue());
            break;
        case EYarnValueType::Bool:
            ValueStr = Pair.Value.GetBoolValue() ? TEXT("true") : TEXT("false");
            break;
        default:
            ValueStr = TEXT("(unknown)");
            break;
        }

        Result.Add(FString::Printf(TEXT("%s = %s"), *Pair.Key, *ValueStr));
    }

    if (Result.Num() == 0)
    {
        Result.Add(TEXT("(No variables)"));
    }

    Result.Sort();
    return Result;
}

// ============================================================================
// UYarnDebugHUDComponent
// ============================================================================
// Manages the debug HUD lifecycle and handles input for toggling it on and off.

UYarnDebugHUDComponent::UYarnDebugHUDComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UYarnDebugHUDComponent::BeginPlay()
{
    Super::BeginPlay();

    // Auto-detect a dialogue runner on the same actor if none was explicitly set
    if (!DialogueRunner)
    {
        DialogueRunner = GetOwner()->FindComponentByClass<UYarnDialogueRunner>();
    }

    SetupEventBindings();

    if (bShowByDefault)
    {
        ShowDebugHUD();
    }
}

void UYarnDebugHUDComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    HideDebugHUD();
    Super::EndPlay(EndPlayReason);
}

void UYarnDebugHUDComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    CheckToggleInput();
}

void UYarnDebugHUDComponent::SetupEventBindings()
{
    if (!DialogueRunner || !bAutoLogEvents)
    {
        return;
    }

    DialogueRunner->OnNodeStart.AddDynamic(this, &UYarnDebugHUDComponent::OnNodeStarted);
    DialogueRunner->OnNodeComplete.AddDynamic(this, &UYarnDebugHUDComponent::OnNodeCompleted);
    DialogueRunner->OnDialogueStart.AddDynamic(this, &UYarnDebugHUDComponent::OnDialogueStarted);
    DialogueRunner->OnDialogueComplete.AddDynamic(this, &UYarnDebugHUDComponent::OnDialogueCompleted);
}

void UYarnDebugHUDComponent::CheckToggleInput()
{
    // In multiplayer this only checks the first player, which is acceptable
    // for a debug tool.
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC && PC->WasInputKeyJustPressed(ToggleKey))
    {
        ToggleDebugHUD();
    }
}

void UYarnDebugHUDComponent::CreateDebugWidget()
{
    if (DebugWidget)
    {
        return;
    }

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
    {
        return;
    }

    if (DebugWidgetClass)
    {
        DebugWidget = CreateWidget<UYarnDebugWidget>(PC, DebugWidgetClass);
    }
    else
    {
        DebugWidget = CreateWidget<UYarnDebugWidget>(PC, UYarnDebugWidget::StaticClass());
    }

    if (DebugWidget)
    {
        DebugWidget->SetupDebugWidget(DialogueRunner);
    }
}

void UYarnDebugHUDComponent::ToggleDebugHUD()
{
    if (bIsVisible)
    {
        HideDebugHUD();
    }
    else
    {
        ShowDebugHUD();
    }
}

void UYarnDebugHUDComponent::ShowDebugHUD()
{
    if (bIsVisible)
    {
        return;
    }

    CreateDebugWidget();

    if (DebugWidget)
    {
        // High z-order so the debug HUD appears on top of other UI elements
        DebugWidget->AddToViewport(100);
        bIsVisible = true;

        UE_LOG(LogYarnSpinner, Log, TEXT("Yarn Debug HUD shown"));
    }
}

void UYarnDebugHUDComponent::HideDebugHUD()
{
    if (!bIsVisible || !DebugWidget)
    {
        return;
    }

    DebugWidget->RemoveFromParent();
    bIsVisible = false;

    UE_LOG(LogYarnSpinner, Log, TEXT("Yarn Debug HUD hidden"));
}

bool UYarnDebugHUDComponent::IsDebugHUDVisible() const
{
    return bIsVisible;
}

void UYarnDebugHUDComponent::LogLine(const FString& CharacterName, const FString& Text, const FString& LineID)
{
    // Create the widget if needed, even when the HUD is hidden, so we
    // capture the history regardless of visibility.
    if (!DebugWidget)
    {
        CreateDebugWidget();
    }

    if (DebugWidget)
    {
        FYarnDebugHistoryEntry Entry;
        Entry.EntryType = TEXT("LINE");
        Entry.CharacterName = CharacterName;
        Entry.Content = Text;
        Entry.LineID = LineID;
        Entry.Timestamp = GetWorld()->GetTimeSeconds();
        DebugWidget->AddHistoryEntry(Entry);
    }
}

void UYarnDebugHUDComponent::LogOptionSelected(int32 OptionIndex, const FString& OptionText)
{
    if (!DebugWidget)
    {
        CreateDebugWidget();
    }

    if (DebugWidget)
    {
        FYarnDebugHistoryEntry Entry;
        Entry.EntryType = TEXT("OPTION");
        Entry.Content = FString::Printf(TEXT("[%d] %s"), OptionIndex, *OptionText);
        Entry.Timestamp = GetWorld()->GetTimeSeconds();
        DebugWidget->AddHistoryEntry(Entry);
    }
}

void UYarnDebugHUDComponent::LogCommand(const FString& CommandText)
{
    if (!DebugWidget)
    {
        CreateDebugWidget();
    }

    if (DebugWidget)
    {
        FYarnDebugHistoryEntry Entry;
        Entry.EntryType = TEXT("COMMAND");
        Entry.Content = CommandText;
        Entry.Timestamp = GetWorld()->GetTimeSeconds();
        DebugWidget->AddHistoryEntry(Entry);
    }
}

void UYarnDebugHUDComponent::LogNodeStart(const FString& NodeName)
{
    if (!DebugWidget)
    {
        CreateDebugWidget();
    }

    if (DebugWidget)
    {
        FYarnDebugHistoryEntry Entry;
        Entry.EntryType = TEXT("NODE_START");
        Entry.Content = NodeName;
        Entry.Timestamp = GetWorld()->GetTimeSeconds();
        DebugWidget->AddHistoryEntry(Entry);
    }
}

void UYarnDebugHUDComponent::LogNodeEnd(const FString& NodeName)
{
    if (!DebugWidget)
    {
        CreateDebugWidget();
    }

    if (DebugWidget)
    {
        FYarnDebugHistoryEntry Entry;
        Entry.EntryType = TEXT("NODE_END");
        Entry.Content = NodeName;
        Entry.Timestamp = GetWorld()->GetTimeSeconds();
        DebugWidget->AddHistoryEntry(Entry);
    }
}

void UYarnDebugHUDComponent::OnNodeStarted(const FString& NodeName)
{
    LogNodeStart(NodeName);
}

void UYarnDebugHUDComponent::OnNodeCompleted(const FString& NodeName)
{
    LogNodeEnd(NodeName);
}

void UYarnDebugHUDComponent::OnDialogueStarted()
{
    if (!DebugWidget)
    {
        CreateDebugWidget();
    }

    if (DebugWidget)
    {
        FYarnDebugHistoryEntry Entry;
        Entry.EntryType = TEXT("DIALOGUE_START");
        Entry.Content = TEXT("Dialogue Started");
        Entry.Timestamp = GetWorld()->GetTimeSeconds();
        DebugWidget->AddHistoryEntry(Entry);
    }
}

void UYarnDebugHUDComponent::OnDialogueCompleted()
{
    if (!DebugWidget)
    {
        CreateDebugWidget();
    }

    if (DebugWidget)
    {
        FYarnDebugHistoryEntry Entry;
        Entry.EntryType = TEXT("DIALOGUE_END");
        Entry.Content = TEXT("Dialogue Completed");
        Entry.Timestamp = GetWorld()->GetTimeSeconds();
        DebugWidget->AddHistoryEntry(Entry);
    }
}
