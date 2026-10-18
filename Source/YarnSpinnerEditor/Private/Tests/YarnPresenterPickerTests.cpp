#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Tests/AutomationCommon.h"
#include "YarnDialogueRunner.h"
#include "YarnLinePresenter.h"
#include "YarnOptionsPresenter.h"
#include "../YarnPresenterReferenceCustomization.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYarnPresenterPickerTest, "YarnSpinner.Editor.PresenterPickerListsPresentersInBlueprintAndLevel",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FYarnPresenterPickerTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), GetTransientPackage(), TEXT("BP_PresenterPickerTest"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!TestNotNull(TEXT("Blueprint created"), Blueprint))
	{
		return false;
	}

	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	USCS_Node* RunnerNode = SCS->CreateNode(UYarnDialogueRunner::StaticClass(), TEXT("Runner"));
	SCS->AddNode(RunnerNode);
	USCS_Node* LineNode = SCS->CreateNode(UYarnLinePresenter::StaticClass(), TEXT("LinePresenter"));
	SCS->AddNode(LineNode);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UYarnDialogueRunner* RunnerTemplate = Cast<UYarnDialogueRunner>(RunnerNode->ComponentTemplate);
	if (!TestNotNull(TEXT("Runner template"), RunnerTemplate))
	{
		return false;
	}
	TArray<FString> TemplateNames = FYarnPresenterReferenceCustomization::GetPresenterNames(RunnerTemplate);
	TestTrue(TEXT("Blueprint editor lists the Blueprint's Presenter"), TemplateNames.Contains(TEXT("LinePresenter")));

	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Editor))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	AActor* Actor = WorldWrapper.GetTestWorld()->SpawnActor<AActor>(Blueprint->GeneratedClass);
	if (!TestNotNull(TEXT("Actor spawned"), Actor))
	{
		return false;
	}

	UYarnOptionsPresenter* Extra = NewObject<UYarnOptionsPresenter>(Actor, TEXT("ExtraPresenter"));
	Actor->AddInstanceComponent(Extra);
	Extra->RegisterComponent();

	UYarnDialogueRunner* Runner = Actor->FindComponentByClass<UYarnDialogueRunner>();
	if (!TestNotNull(TEXT("Runner on the placed Actor"), Runner))
	{
		return false;
	}
	TArray<FString> LevelNames = FYarnPresenterReferenceCustomization::GetPresenterNames(Runner);
	TestTrue(TEXT("Level copy lists the Blueprint's Presenter"), LevelNames.Contains(TEXT("LinePresenter")));
	TestTrue(TEXT("Level copy lists a Presenter added to the copy"), LevelNames.Contains(TEXT("ExtraPresenter")));

	FComponentReference ByProperty;
	ByProperty.ComponentProperty = TEXT("LinePresenter");
	TestTrue(TEXT("A picked Blueprint Presenter resolves at runtime"), Cast<UYarnLinePresenter>(ByProperty.GetComponent(Actor)) != nullptr);

	FComponentReference ByPath;
	ByPath.PathToComponent = TEXT("ExtraPresenter");
	TestTrue(TEXT("A picked instance Presenter resolves at runtime"), ByPath.GetComponent(Actor) == Extra);

	return true;
}

#endif
