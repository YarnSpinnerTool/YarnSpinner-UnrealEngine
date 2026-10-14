#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Tests/AutomationCommon.h"
#include "YarnDialogueRunner.h"
#include "YarnProgram.h"
#include "YarnTestCommandHandler.h"
#include "YarnVariableStorage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnEarlyBlockingCompletionTest, "YarnSpinner.Commands.HandlerObjectCanCompleteItsOwnBlockingCommand",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnEarlyBlockingCompletionTest::RunTest(const FString& Parameters)
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

	UYarnTestCommandHandler* Handler = NewObject<UYarnTestCommandHandler>(Runner);
	Handler->Runner = Runner;
	Runner->CommandHandlerObjects.Add(Handler);
	Runner->RegisterComponent();

	UYarnProject* Project = NewObject<UYarnProject>();
	FYarnNode Node;
	Node.Name = TEXT("Start");
	Node.Instructions.Add(FYarnInstruction::RunCommand(TEXT("finish_at_once"), 0));
	Node.Instructions.Add(FYarnInstruction::PushBool(true));
	Node.Instructions.Add(FYarnInstruction::StoreVariable(TEXT("$after")));
	Node.Instructions.Add(FYarnInstruction::Pop());
	Node.Instructions.Add(FYarnInstruction::Stop());
	Project->Program.Nodes.Add(Node.Name, Node);

	Runner->SetYarnProject(Project);
	Runner->StartDialogue(TEXT("Start"));

	for (int32 Frame = 0; Frame < 3; ++Frame)
	{
		WorldWrapper.TickTestWorld(0.01f);
	}

	FYarnValue After;
	const bool bFound = IYarnVariableStorage::Execute_TryGetValue(Storage, TEXT("$after"), After);
	TestTrue(TEXT("the dialogue carried on past the command"), bFound && After.ConvertToBool());
	TestFalse(TEXT("and finished"), Runner->IsDialogueRunning());

	WorldWrapper.DestroyTestWorld(true);
	return true;
}

#endif
