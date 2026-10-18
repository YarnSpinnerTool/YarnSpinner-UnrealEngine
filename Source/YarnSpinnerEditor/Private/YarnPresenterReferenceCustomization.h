#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyEditorModule.h"

class UYarnDialogueRunner;

class FYarnPresenterPickerIdentifier : public IPropertyTypeIdentifier
{
public:
	virtual bool IsPropertyTypeCustomized(const IPropertyHandle& PropertyHandle) const override;
};

class FYarnPresenterReferenceCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	static TArray<FString> GetPresenterNames(UYarnDialogueRunner* Runner);

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	struct FCandidate
	{
		FName Property;
		FString Path;
		FString Label;
	};

	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> PropertyNameHandle;
	TSharedPtr<IPropertyHandle> PathHandle;
	TSharedPtr<IPropertyHandle> OtherActorHandle;

	TArray<FCandidate> GatherCandidates() const;
	static void AddCandidatesForRunner(UYarnDialogueRunner* Runner, TArray<FCandidate>& OutCandidates);
	static void AddCandidatesFromActor(AActor* Actor, TArray<FCandidate>& OutCandidates);
	static void AddCandidatesFromClass(UClass* ActorClass, TArray<FCandidate>& OutCandidates);
	static void AddCandidate(TArray<FCandidate>& OutCandidates, FName Property, const FString& Path);
	FText GetCurrentText() const;
	TSharedRef<SWidget> MakeMenu();
	void Select(FName Property, FString Path);
};
