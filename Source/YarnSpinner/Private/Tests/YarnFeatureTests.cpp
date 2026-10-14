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

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Border.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundBase.h"
#include "Tests/AutomationCommon.h"
#include "YarnAssetProvider.h"
#include "YarnEffects.h"
#include "YarnLineAdvancer.h"
#include "YarnDialogueRunner.h"
#include "YarnNodeReference.h"
#include "YarnVariableStorage.h"
#include "YarnProgram.h"
#include "YarnSpinnerCore.h"

namespace
{
	UYarnProject* MakeProjectWithNode(const FString& NodeName)
	{
		UYarnProject* Project = NewObject<UYarnProject>();
		FYarnNode Node;
		Node.Name = NodeName;
		Project->Program.Nodes.Add(Node.Name, Node);
		Project->NodeNames.Add(Node.Name);
		return Project;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnNodeReferenceTest, "YarnSpinner.NodeReference.Validity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnNodeReferenceTest::RunTest(const FString& Parameters)
{
	UYarnProject* Project = MakeProjectWithNode(TEXT("Start"));

	TestTrue(TEXT("a reference to a real node is valid"), FYarnNodeReference(Project, TEXT("Start")).IsValid());
	TestFalse(TEXT("a reference with no project is invalid"), FYarnNodeReference(nullptr, TEXT("Start")).IsValid());
	TestFalse(TEXT("a reference with no node name is invalid"), FYarnNodeReference(Project, FString()).IsValid());
	TestFalse(TEXT("a reference to a missing node is invalid"), FYarnNodeReference(Project, TEXT("Nope")).IsValid());
	TestFalse(TEXT("a differently-cased node name is invalid"), FYarnNodeReference(Project, TEXT("start")).IsValid());

	TestEqual(TEXT("the library reports the node name"),
		UYarnNodeReferenceLibrary::GetNodeReferenceName(FYarnNodeReference(Project, TEXT("Start"))), FString(TEXT("Start")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnAssetProviderTest, "YarnSpinner.Assets.ProjectAssetProvider",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnAssetProviderTest::RunTest(const FString& Parameters)
{
	UYarnProject* Project = MakeProjectWithNode(TEXT("Start"));
	Project->BaseLanguage = TEXT("en");
	Project->LineMetadata.Add(TEXT("line:tagged"), TEXT("audio:/Game/VO/Hello lastline"));
	Project->LineMetadata.Add(TEXT("line:shadowed"), TEXT("audio:/Game/VO/Source"));

	FYarnLocalization Localization;
	Localization.AssetsPath = TEXT("/Game/VO/en");
	Project->Localizations.Add(TEXT("en"), Localization);

	UYarnProjectAssetProvider* Provider = NewObject<UYarnProjectAssetProvider>();
	Provider->SetAssetContext(Project, TEXT("en"));

	TestEqual(TEXT("localised path drops the line: prefix"),
		Provider->MakeLocalisedAssetPath(TEXT("line:Start-0")), FString(TEXT("/Game/VO/en/Start-0")));
	TestEqual(TEXT("an unknown locale falls back to nothing"),
		NewObject<UYarnProjectAssetProvider>()->MakeLocalisedAssetPath(TEXT("line:Start-0")), FString());

	FYarnLocalization RelativeLocalization;
	RelativeLocalization.AssetsPath = TEXT("Audio/VO");
	Project->Localizations.Add(TEXT("fr"), RelativeLocalization);
	UYarnProjectAssetProvider* FrenchProvider = NewObject<UYarnProjectAssetProvider>();
	FrenchProvider->SetAssetContext(Project, TEXT("fr"));
	TestEqual(TEXT("a path that is not a content path is ignored"),
		FrenchProvider->MakeLocalisedAssetPath(TEXT("line:Start-0")), FString());

	FYarnLocalizedLine TaggedLine;
	TaggedLine.RawLine.LineID = TEXT("line:tagged");
	TestNull(TEXT("a line whose tagged asset does not exist resolves to nothing"),
		IYarnAssetProvider::Execute_GetAssetForLine(Provider, TaggedLine, USoundBase::StaticClass()));

	FYarnLocalizedLine PlainLine;
	PlainLine.RawLine.LineID = TEXT("line:plain");
	TestNull(TEXT("a line with no asset at all resolves to nothing"),
		IYarnAssetProvider::Execute_GetAssetForLine(Provider, PlainLine, USoundBase::StaticClass()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnEffectsTest, "YarnSpinner.Effects.ImmediateAndZeroDuration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnEffectsTest::RunTest(const FString& Parameters)
{
	UBorder* Widget = NewObject<UBorder>();

	UYarnEffectsLibrary::SetWidgetOpacity(Widget, 0.25f);
	TestEqual(TEXT("opacity is set immediately"), Widget->GetRenderOpacity(), 0.25f);

	UYarnEffectsLibrary::SetWidgetScale(Widget, 2.0f);
	TestEqual(TEXT("scale is set immediately"), static_cast<float>(Widget->GetRenderTransform().Scale.X), 2.0f);

	UYarnEffectsLibrary::SetWidgetOffset(Widget, FVector2D(4.0f, 0.0f));
	TestEqual(TEXT("offset is set immediately"), static_cast<float>(Widget->GetRenderTransform().Translation.X), 4.0f);

	UYarnFadeWidgetEffect* Fade = UYarnFadeWidgetEffect::YarnFadeWidget(Widget, 0.0f, 1.0f, 0.0f, FYarnLineCancellationToken());
	Fade->Activate();
	TestEqual(TEXT("a zero-duration fade lands on its end value straight away"), Widget->GetRenderOpacity(), 1.0f);

	UYarnFadeWidgetEffect* NullFade = UYarnFadeWidgetEffect::YarnFadeWidgetOut(nullptr, 1.0f, FYarnLineCancellationToken());
	NullFade->Activate();
	TestTrue(TEXT("fading a widget that is not there does not crash"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnLineAdvancerTest, "YarnSpinner.LineAdvancer.TracksContent",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnLineAdvancerTest::RunTest(const FString& Parameters)
{
	UYarnLineAdvancer* Advancer = NewObject<UYarnLineAdvancer>();

	TestFalse(TEXT("nothing is presenting to begin with"), Advancer->IsPresentingLine());
	TestFalse(TEXT("no options are presenting to begin with"), Advancer->IsPresentingOptions());

	FYarnLocalizedLine Line;
	Line.RawLine.LineID = TEXT("line:a");
	Advancer->RunLine_Implementation(Line, true);
	TestTrue(TEXT("a line is presenting once one arrives"), Advancer->IsPresentingLine());
	TestFalse(TEXT("options are not presenting during a line"), Advancer->IsPresentingOptions());

	FYarnOptionSet Options;
	Advancer->RunOptions_Implementation(Options);
	TestTrue(TEXT("options are presenting once they arrive"), Advancer->IsPresentingOptions());
	TestFalse(TEXT("the line stops presenting when options arrive"), Advancer->IsPresentingLine());

	Advancer->OnDialogueComplete_Implementation();
	TestFalse(TEXT("nothing presents once the dialogue is over"), Advancer->IsPresentingLine());
	TestFalse(TEXT("no options present once the dialogue is over"), Advancer->IsPresentingOptions());

	Advancer->RequestNextLine();
	Advancer->RequestLineHurryUp();
	Advancer->RequestOptionHurryUp();
	Advancer->RequestDialogueCancellation();
	TestTrue(TEXT("requests without a runner are ignored rather than crashing"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnNameCaseTest, "YarnSpinner.Commands.NamesAreCaseSensitive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnNameCaseTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	AActor* Actor = WorldWrapper.GetTestWorld()->SpawnActor<AActor>();
	UYarnInMemoryVariableStorage* Storage = NewObject<UYarnInMemoryVariableStorage>(Actor);
	Storage->RegisterComponent();

	UYarnDialogueRunner* Runner = NewObject<UYarnDialogueRunner>(Actor);
	Runner->VariableStorage = TScriptInterface<IYarnVariableStorage>(Storage);
	Runner->RegisterComponent();

	Runner->AddFunction(TEXT("score"), [](const TArray<FYarnValue>&) { return FYarnValue(1.0f); }, 0);
	Runner->AddFunction(TEXT("Score"), [](const TArray<FYarnValue>&) { return FYarnValue(2.0f); }, 2);

	UYarnProject* Project = NewObject<UYarnProject>();
	FYarnNode Node;
	Node.Name = TEXT("Start");
	Node.Instructions.Add(FYarnInstruction::PushFloat(0.0f));
	Node.Instructions.Add(FYarnInstruction::CallFunction(TEXT("score")));
	Node.Instructions.Add(FYarnInstruction::StoreVariable(TEXT("$result")));
	Node.Instructions.Add(FYarnInstruction::Pop());
	Node.Instructions.Add(FYarnInstruction::Stop());
	Project->Program.Nodes.Add(Node.Name, Node);

	Runner->SetYarnProject(Project);
	Runner->StartDialogue(TEXT("Start"));

	FYarnValue Result;
	const bool bFound = IYarnVariableStorage::Execute_TryGetValue(Storage, TEXT("$result"), Result);
	TestTrue(TEXT("calling 'score' reaches a function"), bFound);
	TestEqual(TEXT("'score' and 'Score' are separate functions, so the no-argument one ran"),
		Result.ConvertToNumber(), 1.0f);

	WorldWrapper.DestroyTestWorld(true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnSaliencyQueryTest, "YarnSpinner.Saliency.RunnerQueriesUseBuiltInOperators",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnSaliencyQueryTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	AActor* Actor = WorldWrapper.GetTestWorld()->SpawnActor<AActor>();
	UYarnInMemoryVariableStorage* Storage = NewObject<UYarnInMemoryVariableStorage>(Actor);
	Storage->RegisterComponent();

	UYarnDialogueRunner* Runner = NewObject<UYarnDialogueRunner>(Actor);
	Runner->VariableStorage = TScriptInterface<IYarnVariableStorage>(Storage);
	Runner->RegisterComponent();

	UYarnProject* Project = NewObject<UYarnProject>();

	FYarnNode Hub;
	Hub.Name = TEXT("Greet");
	Hub.Headers.Add(FYarnHeader(TEXT("$Yarn.Internal.NodeGroupHub"), TEXT("Greet")));
	Project->Program.Nodes.Add(Hub.Name, Hub);

	const FString ConditionName = TEXT("$Yarn.Internal.Greet.Stranger.Condition0");
	FYarnNode Member;
	Member.Name = TEXT("Greet.Stranger");
	Member.Headers.Add(FYarnHeader(TEXT("$Yarn.Internal.NodeGroup"), TEXT("Greet")));
	Member.Headers.Add(FYarnHeader(TEXT("$Yarn.Internal.ContentSaliencyVariables"), ConditionName));
	Member.Headers.Add(FYarnHeader(TEXT("$Yarn.Internal.ContentSaliencyComplexity"), TEXT("1")));
	Project->Program.Nodes.Add(Member.Name, Member);

	FYarnNode Condition;
	Condition.Name = ConditionName;
	Condition.Instructions.Add(FYarnInstruction::PushVariable(TEXT("$met")));
	Condition.Instructions.Add(FYarnInstruction::PushFloat(1.0f));
	Condition.Instructions.Add(FYarnInstruction::CallFunction(TEXT("Bool.Not")));
	Condition.Instructions.Add(FYarnInstruction::Stop());
	Project->Program.Nodes.Add(Condition.Name, Condition);

	FYarnNode Plain;
	Plain.Name = TEXT("Shop");
	Project->Program.Nodes.Add(Plain.Name, Plain);

	Runner->SetYarnProject(Project);

	IYarnVariableStorage::Execute_SetValue(Storage, TEXT("$met"), FYarnValue(false));
	TestTrue(TEXT("a condition using 'not' passes before dialogue starts"), Runner->HasSalientContent(TEXT("Greet")));
	TArray<FYarnSaliencyCandidate> Candidates = Runner->GetSaliencyOptionsForNodeGroup(TEXT("Greet"));
	TestEqual(TEXT("the group has one member"), Candidates.Num(), 1);
	if (Candidates.Num() == 1)
	{
		TestEqual(TEXT("the member is reported by name"), Candidates[0].ContentID, FString(TEXT("Greet.Stranger")));
		TestEqual(TEXT("its condition passes"), Candidates[0].PassingConditionCount, 1);
		TestEqual(TEXT("and none fail"), Candidates[0].FailingConditionCount, 0);
	}

	IYarnVariableStorage::Execute_SetValue(Storage, TEXT("$met"), FYarnValue(true));
	TestFalse(TEXT("the group has nothing to run once its only condition fails"), Runner->HasSalientContent(TEXT("Greet")));
	Candidates = Runner->GetSaliencyOptionsForNodeGroup(TEXT("Greet"));
	TestTrue(TEXT("the failing member is still listed"), Candidates.Num() == 1 && Candidates[0].FailingConditionCount == 1);

	TestTrue(TEXT("a plain node always has content"), Runner->HasSalientContent(TEXT("Shop")));
	TestEqual(TEXT("a plain node is its own single option"), Runner->GetSaliencyOptionsForNodeGroup(TEXT("Shop")).Num(), 1);

	AddExpectedMessagePlain(TEXT("is not a valid node name"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2);
	TestFalse(TEXT("an unknown group has no content"), Runner->HasSalientContent(TEXT("Nope")));
	TestEqual(TEXT("an unknown group has no options"), Runner->GetSaliencyOptionsForNodeGroup(TEXT("Nope")).Num(), 0);

	WorldWrapper.DestroyTestWorld(true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnCommandTokenisationTest, "YarnSpinner.Commands.TokenisationEdges",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnCommandTokenisationTest::RunTest(const FString& Parameters)
{
	{
		const FYarnCommand Command(TEXT("move\tplayer  north"));
		TestEqual(TEXT("any whitespace splits arguments"), Command.CommandName, TEXT("move"));
		TestEqual(TEXT("repeated whitespace does not make empty arguments"), Command.Parameters.Num(), 2);
	}
	{
		const FYarnCommand Command(TEXT("say \"hello\\nthere\""));
		TestEqual(TEXT("backslash-n stays literal inside quotes"), Command.Parameters.Num(), 1);
		if (Command.Parameters.Num() == 1)
		{
			TestFalse(TEXT("no real newline is produced"), Command.Parameters[0].Contains(TEXT("\n")));
		}
	}
	{
		const FYarnCommand Command(TEXT("say 'hello there'"));
		TestEqual(TEXT("single quotes do not group arguments"), Command.Parameters.Num(), 2);
	}
	{
		const FYarnCommand Command(TEXT("wave"));
		TestEqual(TEXT("a command with no arguments parses"), Command.CommandName, TEXT("wave"));
		TestEqual(TEXT("and has no parameters"), Command.Parameters.Num(), 0);
	}

	return true;
}

#endif
