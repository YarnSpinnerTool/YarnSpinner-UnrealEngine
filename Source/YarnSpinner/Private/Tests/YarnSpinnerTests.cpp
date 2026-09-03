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
#include "YarnMarkup.h"
#include "YarnProgram.h"
#include "YarnSmartVariables.h"
#include "YarnSpinnerCore.h"
#include "YarnVariableStorage.h"
#include "YarnVirtualMachine.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FYarnVMTestHarness
	{
		FYarnVirtualMachine VM;
		TArray<FString> Lines;
		TArray<FString> CompletedNodes;
		bool bDialogueComplete = false;

		explicit FYarnVMTestHarness(const FYarnProgram& Program)
		{
			VM.SetProgram(Program);
			VM.LineHandler.BindLambda([this](const FYarnLine& Line)
			{
				Lines.Add(Line.LineID);
				VM.SignalContentComplete();
			});
			VM.NodeCompleteHandler.BindLambda([this](const FString& NodeName)
			{
				CompletedNodes.Add(NodeName);
			});
			VM.DialogueCompleteHandler.BindLambda([this]()
			{
				bDialogueComplete = true;
			});

			VM.OptionsHandler.BindLambda([](const FYarnOptionSet&) {});
			VM.CommandHandler.BindLambda([this](const FYarnCommand&)
			{
				VM.SignalContentComplete();
			});
			VM.NodeStartHandler.BindLambda([](const FString&) {});
			VM.PrepareForLinesHandler.BindLambda([](const TArray<FString>&) {});
			VM.CallFunctionHandler.BindLambda([](const FString&, const TArray<FYarnValue>&) -> FYarnValue
			{
				return FYarnValue(0.0f);
			});
			VM.FunctionExistsHandler.BindLambda([](const FString&) { return true; });
			VM.FunctionParamCountHandler.BindLambda([](const FString&) { return -1; });
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnVMLinesTest, "YarnSpinner.VM.LinesRunInOrder",
	 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnVMLinesTest::RunTest(const FString& Parameters)
{
	FYarnProgram Program;
	FYarnNode Node;
	Node.Name = TEXT("Start");
	Node.Instructions.Add(FYarnInstruction::RunLine(TEXT("line:a"), 0));
	Node.Instructions.Add(FYarnInstruction::RunLine(TEXT("line:b"), 0));
	Node.Instructions.Add(FYarnInstruction::Stop());
	Program.Nodes.Add(Node.Name, Node);

	FYarnVMTestHarness Harness(Program);
	TestTrue(TEXT("SetNode succeeds"), Harness.VM.SetNode(TEXT("Start")));
	Harness.VM.Continue();

	TestEqual(TEXT("two lines ran"), Harness.Lines.Num(), 2);
	if (Harness.Lines.Num() == 2)
	{
		TestEqual(TEXT("first line"), Harness.Lines[0], TEXT("line:a"));
		TestEqual(TEXT("second line"), Harness.Lines[1], TEXT("line:b"));
	}
	TestTrue(TEXT("dialogue completed"), Harness.bDialogueComplete);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnVMOptionsTest, "YarnSpinner.VM.OptionsSelectAndFallthrough",
	 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnVMOptionsTest::RunTest(const FString& Parameters)
{
	FYarnProgram Program;
	FYarnNode Node;
	Node.Name = TEXT("Start");
	Node.Instructions.Add(FYarnInstruction::AddOption(TEXT("line:opt1"), 6, 0, false));
	Node.Instructions.Add(FYarnInstruction::ShowOptions());
	Node.Instructions.Add(FYarnInstruction::JumpIfFalse(9));
	Node.Instructions.Add(FYarnInstruction::Pop());
	Node.Instructions.Add(FYarnInstruction::PeekAndJump());
	Node.Instructions.Add(FYarnInstruction::Stop());
	Node.Instructions.Add(FYarnInstruction::Pop());
	Node.Instructions.Add(FYarnInstruction::RunLine(TEXT("line:selected"), 0));
	Node.Instructions.Add(FYarnInstruction::Stop());
	Node.Instructions.Add(FYarnInstruction::Pop());
	Node.Instructions.Add(FYarnInstruction::RunLine(TEXT("line:fellthrough"), 0));
	Node.Instructions.Add(FYarnInstruction::Stop());
	Program.Nodes.Add(Node.Name, Node);

	{
		FYarnVMTestHarness Harness(Program);
		int32 OptionCount = 0;
		Harness.VM.OptionsHandler.BindLambda([&OptionCount](const FYarnOptionSet& Options)
		{
			OptionCount = Options.Options.Num();
		});
		Harness.VM.SetNode(TEXT("Start"));
		Harness.VM.Continue();
		TestEqual(TEXT("one option shown"), OptionCount, 1);

		Harness.VM.SetSelectedOption(0);
		Harness.VM.Continue();
		TestEqual(TEXT("selection ran the option branch"), Harness.Lines.Num(), 1);
		if (Harness.Lines.Num() == 1)
		{
			TestEqual(TEXT("selected line"), Harness.Lines[0], TEXT("line:selected"));
		}
	}

	{
		FYarnVMTestHarness Harness(Program);
		Harness.VM.OptionsHandler.BindLambda([](const FYarnOptionSet&) {});
		Harness.VM.SetNode(TEXT("Start"));
		Harness.VM.Continue();

		Harness.VM.SetSelectedOption(YarnNoOptionSelected);
		Harness.VM.Continue();
		TestEqual(TEXT("fall-through ran the after-options branch"), Harness.Lines.Num(), 1);
		if (Harness.Lines.Num() == 1)
		{
			TestEqual(TEXT("fell through"), Harness.Lines[0], TEXT("line:fellthrough"));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnVMDetourTest, "YarnSpinner.VM.DetourAndReturn",
	 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnVMDetourTest::RunTest(const FString& Parameters)
{
	FYarnProgram Program;

	FYarnNode Start;
	Start.Name = TEXT("Start");
	Start.Instructions.Add(FYarnInstruction::DetourToNode(TEXT("Sub")));
	Start.Instructions.Add(FYarnInstruction::RunLine(TEXT("line:back"), 0));
	Start.Instructions.Add(FYarnInstruction::Stop());
	Program.Nodes.Add(Start.Name, Start);

	FYarnNode Sub;
	Sub.Name = TEXT("Sub");
	Sub.Instructions.Add(FYarnInstruction::RunLine(TEXT("line:sub"), 0));
	Sub.Instructions.Add(FYarnInstruction::Return());
	Program.Nodes.Add(Sub.Name, Sub);

	FYarnVMTestHarness Harness(Program);
	Harness.VM.SetNode(TEXT("Start"));
	Harness.VM.Continue();

	TestEqual(TEXT("both lines ran"), Harness.Lines.Num(), 2);
	if (Harness.Lines.Num() == 2)
	{
		TestEqual(TEXT("detour line first"), Harness.Lines[0], TEXT("line:sub"));
		TestEqual(TEXT("resumed after return"), Harness.Lines[1], TEXT("line:back"));
	}
	TestTrue(TEXT("sub node completed"), Harness.CompletedNodes.Contains(TEXT("Sub")));
	TestTrue(TEXT("dialogue completed"), Harness.bDialogueComplete);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnVMFunctionArityTest, "YarnSpinner.VM.FunctionArityUnknownSkipsCheck",
	 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnVMFunctionArityTest::RunTest(const FString& Parameters)
{
	FYarnProgram Program;
	FYarnNode Node;
	Node.Name = TEXT("Start");
	Node.Instructions.Add(FYarnInstruction::PushFloat(1.0f));
	Node.Instructions.Add(FYarnInstruction::PushFloat(2.0f));
	Node.Instructions.Add(FYarnInstruction::PushFloat(2.0f));
	Node.Instructions.Add(FYarnInstruction::CallFunction(TEXT("fn")));
	Node.Instructions.Add(FYarnInstruction::Pop());
	Node.Instructions.Add(FYarnInstruction::Stop());
	Program.Nodes.Add(Node.Name, Node);

	FYarnVMTestHarness Harness(Program);
	int32 ReceivedParamCount = -1;
	Harness.VM.FunctionExistsHandler.BindLambda([](const FString&) { return true; });
	Harness.VM.FunctionParamCountHandler.BindLambda([](const FString&) { return -1; });
	Harness.VM.CallFunctionHandler.BindLambda([&ReceivedParamCount](const FString&, const TArray<FYarnValue>& Params) -> FYarnValue
	{
		ReceivedParamCount = Params.Num();
		return FYarnValue(0.0f);
	});
	Harness.VM.SetNode(TEXT("Start"));
	Harness.VM.Continue();

	TestEqual(TEXT("function received both parameters"), ReceivedParamCount, 2);
	TestTrue(TEXT("dialogue completed (no arity halt)"), Harness.bDialogueComplete);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnCommandParseTest, "YarnSpinner.Commands.ParseQuotedText",
	 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnCommandParseTest::RunTest(const FString& Parameters)
{
	const FYarnCommand Command(TEXT("play_sound \"big \\\"boom\\\"\" 3.5"));
	TestEqual(TEXT("command name"), Command.CommandName, TEXT("play_sound"));
	TestEqual(TEXT("parameter count"), Command.Parameters.Num(), 2);
	if (Command.Parameters.Num() == 2)
	{
		TestEqual(TEXT("quoted parameter keeps spaces and escaped quotes"), Command.Parameters[0], TEXT("big \"boom\""));
		TestEqual(TEXT("plain parameter"), Command.Parameters[1], TEXT("3.5"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnMarkupCharacterTest, "YarnSpinner.Markup.CharacterAndSelect",
	 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnMarkupCharacterTest::RunTest(const FString& Parameters)
{
	const FYarnMarkupParseResult Result = UYarnMarkupLibrary::ParseMarkupFull(
		TEXT("Mae: I choose [select value=2 1=one 2=two /]."), TEXT("en"), true);

	TestEqual(TEXT("character name extracted"), Result.CharacterName, TEXT("Mae"));
	TestTrue(TEXT("select replaced with matching property"), Result.Text.Contains(TEXT("two")));
	TestFalse(TEXT("select markup consumed"), Result.Text.Contains(TEXT("select")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnMarkupSplitMergeTest, "YarnSpinner.Markup.MisnestedTagsRemerge",
	 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnMarkupSplitMergeTest::RunTest(const FString& Parameters)
{
	const FYarnMarkupParseResult Result = UYarnMarkupLibrary::ParseMarkupFull(
		TEXT("[a]xx[b]yy[/a]zz[/b]"), TEXT("en"), false);

	TestEqual(TEXT("plain text"), Result.Text, TEXT("xxyyzz"));

	const FYarnMarkupAttribute* A = Result.FindAttribute(TEXT("a"));
	const FYarnMarkupAttribute* B = Result.FindAttribute(TEXT("b"));
	TestNotNull(TEXT("attribute a present"), A);
	TestNotNull(TEXT("attribute b present"), B);
	if (A)
	{
		TestEqual(TEXT("a position"), A->Position, 0);
		TestEqual(TEXT("a length"), A->Length, 4);
	}
	if (B)
	{
		TestEqual(TEXT("b position"), B->Position, 2);
		TestEqual(TEXT("b length"), B->Length, 4);
	}
	TestEqual(TEXT("exactly two attributes after merge"), Result.Attributes.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnVMVisitedTrackingTest, "YarnSpinner.VM.VisitedTrackingIncrements",
	 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnVMVisitedTrackingTest::RunTest(const FString& Parameters)
{
	FYarnProgram Program;
	FYarnNode Node;
	Node.Name = TEXT("Start");
	Node.Headers.Add(FYarnHeader(TEXT("$Yarn.Internal.TrackingVariable"), TEXT("$visited_start")));
	Node.Instructions.Add(FYarnInstruction::RunLine(TEXT("line:a"), 0));
	Node.Instructions.Add(FYarnInstruction::Stop());
	Program.Nodes.Add(Node.Name, Node);

	UYarnInMemoryVariableStorage* Storage = NewObject<UYarnInMemoryVariableStorage>();

	FYarnVMTestHarness Harness(Program);
	Harness.VM.VariableStorage = Storage;
	Harness.VM.SetNode(TEXT("Start"));
	Harness.VM.Continue();

	FYarnValue Count;
	TestTrue(TEXT("tracking variable exists"), IYarnVariableStorage::Execute_TryGetValue(Storage, TEXT("$visited_start"), Count));
	TestEqual(TEXT("visited once"), Count.ConvertToNumber(), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnVMVariableRoundTripTest, "YarnSpinner.VM.StoreAndPushVariables",
	 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnVMVariableRoundTripTest::RunTest(const FString& Parameters)
{
	FYarnProgram Program;
	Program.InitialValues.Add(TEXT("$initial"), FYarnValue(5.0f));
	FYarnNode Node;
	Node.Name = TEXT("Start");
	Node.Instructions.Add(FYarnInstruction::PushFloat(42.0f));
	Node.Instructions.Add(FYarnInstruction::StoreVariable(TEXT("$x")));
	Node.Instructions.Add(FYarnInstruction::Pop());
	Node.Instructions.Add(FYarnInstruction::PushVariable(TEXT("$x")));
	Node.Instructions.Add(FYarnInstruction::StoreVariable(TEXT("$y")));
	Node.Instructions.Add(FYarnInstruction::Pop());
	Node.Instructions.Add(FYarnInstruction::PushVariable(TEXT("$initial")));
	Node.Instructions.Add(FYarnInstruction::StoreVariable(TEXT("$z")));
	Node.Instructions.Add(FYarnInstruction::Pop());
	Node.Instructions.Add(FYarnInstruction::Stop());
	Program.Nodes.Add(Node.Name, Node);

	UYarnInMemoryVariableStorage* Storage = NewObject<UYarnInMemoryVariableStorage>();

	FYarnVMTestHarness Harness(Program);
	Harness.VM.VariableStorage = Storage;
	Harness.VM.SetNode(TEXT("Start"));
	Harness.VM.Continue();

	FYarnValue Value;
	TestTrue(TEXT("$y exists"), IYarnVariableStorage::Execute_TryGetValue(Storage, TEXT("$y"), Value));
	TestEqual(TEXT("$y round-tripped"), Value.ConvertToNumber(), 42.0f);
	TestTrue(TEXT("$z exists"), IYarnVariableStorage::Execute_TryGetValue(Storage, TEXT("$z"), Value));
	TestEqual(TEXT("$z from initial values"), Value.ConvertToNumber(), 5.0f);
	TestTrue(TEXT("dialogue completed"), Harness.bDialogueComplete);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnVMSubstitutionTest, "YarnSpinner.VM.CommandSubstitutionsLastOccurrence",
	 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnVMSubstitutionTest::RunTest(const FString& Parameters)
{
	{
		TArray<FString> Subs = { TEXT("X") };
		TestEqual(TEXT("repeated marker keeps earlier occurrence"),
			FYarnVirtualMachine::ExpandSubstitutions(TEXT("cmd {0} {0}"), Subs), TEXT("cmd {0} X"));
	}
	{
		TArray<FString> Subs = { TEXT("X"), TEXT("{0}") };
		TestEqual(TEXT("substitution value containing a marker is not re-expanded"),
			FYarnVirtualMachine::ExpandSubstitutions(TEXT("a {0} b {1}"), Subs), TEXT("a X b {0}"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnSmartVariableTest, "YarnSpinner.SmartVariables.EvaluateExpressionNode",
	 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnSmartVariableTest::RunTest(const FString& Parameters)
{
	FYarnProgram Program;
	FYarnNode SmartNode;
	SmartNode.Name = TEXT("$smart");
	SmartNode.Instructions.Add(FYarnInstruction::PushFloat(7.0f));
	SmartNode.Instructions.Add(FYarnInstruction::Stop());
	Program.Nodes.Add(SmartNode.Name, SmartNode);

	UYarnInMemoryVariableStorage* Storage = NewObject<UYarnInMemoryVariableStorage>();

	FOnSmartVariableCallFunction CallFn;
	CallFn.BindLambda([](const FString&, const TArray<FYarnValue>&) -> FYarnValue { return FYarnValue(0.0f); });

	FYarnValue Result;
	TestTrue(TEXT("smart variable evaluates"),
		FYarnSmartVariableEvaluationVM::TryGetSmartVariable(TEXT("$smart"), Program, Storage, CallFn, Result));
	TestEqual(TEXT("smart variable value"), Result.ConvertToNumber(), 7.0f);

	FYarnValue Missing;
	TestFalse(TEXT("unknown smart variable fails fast"),
		FYarnSmartVariableEvaluationVM::TryGetSmartVariable(TEXT("$nope"), Program, Storage, CallFn, Missing));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnVMNestedContinueTest, "YarnSpinner.VM.NestedContinueDoesNotResume",
	 EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnVMNestedContinueTest::RunTest(const FString& Parameters)
{
	FYarnProgram Program;
	FYarnNode Node;
	Node.Name = TEXT("Start");
	Node.Instructions.Add(FYarnInstruction::RunLine(TEXT("line:a"), 0));
	Node.Instructions.Add(FYarnInstruction::RunLine(TEXT("line:b"), 0));
	Node.Instructions.Add(FYarnInstruction::Stop());
	Program.Nodes.Add(Node.Name, Node);

	FYarnVMTestHarness Harness(Program);
	TArray<FString> Lines;
	Harness.VM.LineHandler.BindLambda([&](const FYarnLine& Line)
	{
		Lines.Add(Line.LineID);
		Harness.VM.Continue();
	});
	Harness.VM.SetNode(TEXT("Start"));
	Harness.VM.Continue();

	TestEqual(TEXT("only the first line ran"), Lines.Num(), 1);
	TestEqual(TEXT("waiting for continue"), (int32)Harness.VM.GetExecutionState(), (int32)EYarnExecutionState::WaitingForContinue);

	Harness.VM.Continue();
	TestEqual(TEXT("second line ran after external continue"), Lines.Num(), 2);
	return true;
}

#endif
