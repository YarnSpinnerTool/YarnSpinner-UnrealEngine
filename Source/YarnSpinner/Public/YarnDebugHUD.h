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

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "YarnSpinnerCore.h"
#include "YarnDebugHUD.generated.h"

// Forward declarations
class UYarnDialogueRunner;
class UTextBlock;
class UVerticalBox;
class UScrollBox;
class UBorder;

/**
 * Entry in the dialogue execution history.
 */
USTRUCT(BlueprintType)
struct YARNSPINNER_API FYarnDebugHistoryEntry
{
    GENERATED_BODY()

    /** Type of entry: "LINE", "OPTION", "COMMAND", "NODE_START", "NODE_END" */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Debug")
    FString EntryType;

    /** The content (line text, command text, node name, etc.) */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Debug")
    FString Content;

    /** Character name if applicable */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Debug")
    FString CharacterName;

    /** Line ID if applicable */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Debug")
    FString LineID;

    /** Timestamp when this occurred */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Debug")
    float Timestamp = 0.0f;
};

/**
 * Debug widget that displays current dialogue state.
 *
 * Shows:
 * - Current node name
 * - Current line ID and text
 * - All yarn variables and their values
 * - Execution history (breadcrumb trail)
 *
 * Can be toggled on/off at runtime with a key press.
 */
UCLASS(BlueprintType, Blueprintable)
class YARNSPINNER_API UYarnDebugWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Set up the debug widget with a dialogue runner to monitor */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    void SetupDebugWidget(UYarnDialogueRunner* InDialogueRunner);

    /** Refresh all displayed information */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    void RefreshDisplay();

    /** Add an entry to the execution history */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    void AddHistoryEntry(const FYarnDebugHistoryEntry& Entry);

    /** Clear execution history */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    void ClearHistory();

    /** Get all current variables as formatted strings */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    TArray<FString> GetFormattedVariables() const;

    /** Get execution history */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    TArray<FYarnDebugHistoryEntry> GetHistory() const { return ExecutionHistory; }

    /** The dialogue runner being monitored */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Debug")
    UYarnDialogueRunner* DialogueRunner;

    /** Maximum history entries to keep */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Debug")
    int32 MaxHistoryEntries = 100;

    /** Current node name (updated by RefreshDisplay) */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Debug")
    FString CurrentNodeName;

    /** Current line ID (updated by RefreshDisplay) */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Debug")
    FString CurrentLineID;

    /** Whether dialogue is currently running */
    UPROPERTY(BlueprintReadOnly, Category = "Yarn Spinner|Debug")
    bool bIsDialogueRunning = false;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Execution history */
    UPROPERTY()
    TArray<FYarnDebugHistoryEntry> ExecutionHistory;

    // Refresh interval in seconds; dialogue state doesn't change rapidly enough
    // to warrant per-frame updates.
    float RefreshInterval = 0.1f;

    float TimeSinceLastRefresh = 0.0f;
};

/**
 * Component that manages the debug HUD for a dialogue runner.
 *
 * Attach this to the same actor as your dialogue runner.
 * Press the toggle key (default: F3) to show/hide the debug display.
 *
 * The HUD shows:
 * - Current dialogue state (running/stopped)
 * - Current node and line
 * - All yarn variables
 * - Recent execution history
 */
UCLASS(ClassGroup = (YarnSpinner), meta = (BlueprintSpawnableComponent))
class YARNSPINNER_API UYarnDebugHUDComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UYarnDebugHUDComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Toggle the debug HUD visibility */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    void ToggleDebugHUD();

    /** Show the debug HUD */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    void ShowDebugHUD();

    /** Hide the debug HUD */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    void HideDebugHUD();

    /** Check if debug HUD is visible */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    bool IsDebugHUDVisible() const;

    /** Log a line to the debug history */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    void LogLine(const FString& CharacterName, const FString& Text, const FString& LineID);

    /** Log an option selection to history */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    void LogOptionSelected(int32 OptionIndex, const FString& OptionText);

    /** Log a command execution to history */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    void LogCommand(const FString& CommandText);

    /** Log a node start to history */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    void LogNodeStart(const FString& NodeName);

    /** Log a node end to history */
    UFUNCTION(BlueprintCallable, Category = "Yarn Spinner|Debug")
    void LogNodeEnd(const FString& NodeName);

    /** The dialogue runner to monitor (auto-detected if on same actor) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Debug")
    UYarnDialogueRunner* DialogueRunner;

    /** Widget class to use for the debug display */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Debug")
    TSubclassOf<UYarnDebugWidget> DebugWidgetClass;

    /** Key to toggle debug HUD (default: F3) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Debug")
    FKey ToggleKey = EKeys::F3;

    /** Whether to show the debug HUD by default */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Debug")
    bool bShowByDefault = false;

    /** Whether to auto-log dialogue events */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Debug")
    bool bAutoLogEvents = true;

    /** Screen position anchor (0,0 = top-left, 1,1 = bottom-right) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Yarn Spinner|Debug")
    FVector2D ScreenAnchor = FVector2D(0.0f, 0.0f);

protected:
    UPROPERTY()
    UYarnDebugWidget* DebugWidget;

    bool bIsVisible = false;

    void CreateDebugWidget();
    void SetupEventBindings();
    void CheckToggleInput();
    UFUNCTION()
    void OnNodeStarted(const FString& NodeName);

    UFUNCTION()
    void OnNodeCompleted(const FString& NodeName);

    UFUNCTION()
    void OnDialogueStarted();

    UFUNCTION()
    void OnDialogueCompleted();
};
