#include "YarnPresenterReferenceCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "YarnDialoguePresenter.h"
#include "YarnDialogueRunner.h"

#define LOCTEXT_NAMESPACE "YarnPresenterReferenceCustomization"

bool FYarnPresenterPickerIdentifier::IsPropertyTypeCustomized(const IPropertyHandle& PropertyHandle) const
{
	return PropertyHandle.HasMetaData(TEXT("YarnPresenterPicker"));
}

TSharedRef<IPropertyTypeCustomization> FYarnPresenterReferenceCustomization::MakeInstance()
{
	return MakeShared<FYarnPresenterReferenceCustomization>();
}

void FYarnPresenterReferenceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructHandle = PropertyHandle;
	PropertyNameHandle = PropertyHandle->GetChildHandle(TEXT("ComponentProperty"));
	PathHandle = PropertyHandle->GetChildHandle(TEXT("PathToComponent"));
	OtherActorHandle = PropertyHandle->GetChildHandle(TEXT("OtherActor"));

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(220.0f)
	[
		SNew(SComboButton)
		.OnGetMenuContent(this, &FYarnPresenterReferenceCustomization::MakeMenu)
		.ContentPadding(FMargin(4.0f, 2.0f))
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &FYarnPresenterReferenceCustomization::GetCurrentText)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	];
}

void FYarnPresenterReferenceCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FYarnPresenterReferenceCustomization::AddCandidate(TArray<FCandidate>& OutCandidates, FName Property, const FString& Path)
{
	const FString Label = Property.IsNone() ? Path : Property.ToString();
	if (Label.IsEmpty())
	{
		return;
	}
	for (const FCandidate& Existing : OutCandidates)
	{
		if (Existing.Label == Label)
		{
			return;
		}
	}
	OutCandidates.Add({ Property, Property.IsNone() ? Path : FString(), Label });
}

void FYarnPresenterReferenceCustomization::AddCandidatesFromActor(AActor* Actor, TArray<FCandidate>& OutCandidates)
{
	TArray<UYarnDialoguePresenter*> Presenters;
	Actor->GetComponents(Presenters);
	for (UYarnDialoguePresenter* Presenter : Presenters)
	{
		if (!Presenter)
		{
			continue;
		}
		const FName VariableName = FComponentEditorUtils::FindVariableNameGivenComponentInstance(Presenter);
		AddCandidate(OutCandidates, VariableName, Presenter->GetName());
	}
}

void FYarnPresenterReferenceCustomization::AddCandidatesFromClass(UClass* ActorClass, TArray<FCandidate>& OutCandidates)
{
	if (!ActorClass)
	{
		return;
	}
	if (AActor* DefaultActor = Cast<AActor>(ActorClass->GetDefaultObject()))
	{
		AddCandidatesFromActor(DefaultActor, OutCandidates);
	}
	for (UClass* Class = ActorClass; Class; Class = Class->GetSuperClass())
	{
		UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Class);
		if (!GeneratedClass || !GeneratedClass->SimpleConstructionScript)
		{
			continue;
		}
		for (USCS_Node* Node : GeneratedClass->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<UYarnDialoguePresenter>())
			{
				AddCandidate(OutCandidates, Node->GetVariableName(), FString());
			}
		}
	}
}

void FYarnPresenterReferenceCustomization::AddCandidatesForRunner(UYarnDialogueRunner* Runner, TArray<FCandidate>& OutCandidates)
{
	if (!Runner)
	{
		return;
	}
	AActor* Owner = Runner->GetOwner();
	if (Owner && !Owner->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		AddCandidatesFromActor(Owner, OutCandidates);
	}
	else if (Owner)
	{
		AddCandidatesFromClass(Owner->GetClass(), OutCandidates);
	}
	else
	{
		AddCandidatesFromClass(Runner->GetTypedOuter<UClass>(), OutCandidates);
	}
}

TArray<FString> FYarnPresenterReferenceCustomization::GetPresenterNames(UYarnDialogueRunner* Runner)
{
	TArray<FCandidate> Candidates;
	AddCandidatesForRunner(Runner, Candidates);
	TArray<FString> Names;
	for (const FCandidate& Candidate : Candidates)
	{
		Names.Add(Candidate.Label);
	}
	return Names;
}

TArray<FYarnPresenterReferenceCustomization::FCandidate> FYarnPresenterReferenceCustomization::GatherCandidates() const
{
	TArray<FCandidate> Candidates;
	TArray<UObject*> Outers;
	StructHandle->GetOuterObjects(Outers);
	for (UObject* Outer : Outers)
	{
		AddCandidatesForRunner(Cast<UYarnDialogueRunner>(Outer), Candidates);
	}
	return Candidates;
}

FText FYarnPresenterReferenceCustomization::GetCurrentText() const
{
	FName Property;
	FString Path;
	if (PropertyNameHandle.IsValid())
	{
		if (PropertyNameHandle->GetValue(Property) == FPropertyAccess::MultipleValues)
		{
			return LOCTEXT("MultipleValues", "Multiple Values");
		}
	}
	if (PathHandle.IsValid())
	{
		PathHandle->GetValue(Path);
	}
	if (!Property.IsNone())
	{
		return FText::FromName(Property);
	}
	if (!Path.IsEmpty())
	{
		return FText::FromString(Path);
	}
	return LOCTEXT("None", "None");
}

TSharedRef<SWidget> FYarnPresenterReferenceCustomization::MakeMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ClearEntry", "None"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &FYarnPresenterReferenceCustomization::Select, FName(), FString())));

	const TArray<FCandidate> Candidates = GatherCandidates();
	if (Candidates.Num() == 0)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NoPresenters", "No Presenters on this Actor"),
			LOCTEXT("NoPresentersTooltip", "Add a Presenter component to this Actor, then compile the Blueprint."),
			FSlateIcon(),
			FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })));
	}
	for (const FCandidate& Candidate : Candidates)
	{
		MenuBuilder.AddMenuEntry(
			FText::FromString(Candidate.Label),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FYarnPresenterReferenceCustomization::Select, Candidate.Property, Candidate.Path)));
	}
	return MenuBuilder.MakeWidget();
}

void FYarnPresenterReferenceCustomization::Select(FName Property, FString Path)
{
	if (OtherActorHandle.IsValid())
	{
		OtherActorHandle->SetValueFromFormattedString(TEXT("None"));
	}
	if (PathHandle.IsValid())
	{
		PathHandle->SetValue(Path);
	}
	if (PropertyNameHandle.IsValid())
	{
		PropertyNameHandle->SetValue(Property);
	}
}

#undef LOCTEXT_NAMESPACE
