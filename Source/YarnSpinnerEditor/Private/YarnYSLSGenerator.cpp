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

#include "YarnYSLSGenerator.h"
#include "YarnSpinnerModule.h"
#include "YarnCommandLibrary.h"
#include "YarnCommandRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace
{
	using FEntryMap = TMap<FString, TSharedPtr<FJsonObject>>;

	FString PropertyToYarnType(const FProperty* Property)
	{
		if (!Property)
		{
			return TEXT("any");
		}
		if (Property->IsA<FBoolProperty>())
		{
			return TEXT("bool");
		}
		if (Property->IsA<FNumericProperty>())
		{
			return TEXT("number");
		}
		if (Property->IsA<FStrProperty>() || Property->IsA<FNameProperty>() || Property->IsA<FTextProperty>())
		{
			return TEXT("string");
		}
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			if (ObjectProperty->PropertyClass && ObjectProperty->PropertyClass->IsChildOf(AActor::StaticClass()))
			{
				return TEXT("node");
			}
			return TEXT("instance");
		}
		return TEXT("any");
	}

	bool IsGeneratedClassArtifact(const UClass* Class)
	{
		const FString Name = Class->GetName();
		return Name.StartsWith(TEXT("SKEL_"))
			|| Name.StartsWith(TEXT("REINST_"))
			|| Name.StartsWith(TEXT("TRASHCLASS_"))
			|| Name.StartsWith(TEXT("HOTRELOADED_"))
			|| Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists);
	}

	TSharedPtr<FJsonObject> MakeBaseEntry(const FString& YarnName, const FString& DefinitionName, const FString& FileName, const FString& Language)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("yarnName"), YarnName);
		Entry->SetStringField(TEXT("definitionName"), DefinitionName);
		if (!FileName.IsEmpty())
		{
			Entry->SetStringField(TEXT("fileName"), FileName);
		}
		Entry->SetStringField(TEXT("language"), Language);
		return Entry;
	}

	TArray<TSharedPtr<FJsonValue>> BuildParameters(UFunction* Function)
	{
		TArray<TSharedPtr<FJsonValue>> Params;

		for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm; ++It)
		{
			TSharedPtr<FJsonObject> Param = MakeShared<FJsonObject>();
			Param->SetStringField(TEXT("name"), It->GetName());

			if (CastField<FArrayProperty>(*It))
			{
				Param->SetStringField(TEXT("type"), TEXT("any"));
				Param->SetBoolField(TEXT("isParamsArray"), true);
			}
			else
			{
				Param->SetStringField(TEXT("type"), PropertyToYarnType(*It));

				const FString DefaultValue = Function->GetMetaData(*FString::Printf(TEXT("CPP_Default_%s"), *It->GetName()));
				if (!DefaultValue.IsEmpty())
				{
					Param->SetStringField(TEXT("defaultValue"), DefaultValue);
				}
			}

			Params.Add(MakeShared<FJsonValueObject>(Param));
		}

		return Params;
	}

	void CollectFromReflection(FEntryMap& Commands, FEntryMap& Functions, TArray<FYarnBakedActionEntry>& OutBakedEntries)
	{
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			UClass* Class = *ClassIt;
			if (IsGeneratedClassArtifact(Class))
			{
				continue;
			}

			if (Class->HasAnyClassFlags(CLASS_Native))
			{
				UYarnCommandLibrary::CollectYarnActionsFromClass(Class, OutBakedEntries, /*bIncludeInherited=*/false);
			}

			for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
			{
				UFunction* Function = *FuncIt;
				const bool bIsCommand = Function->HasMetaData(TEXT("YarnCommand"));
				const bool bIsFunction = Function->HasMetaData(TEXT("YarnFunction"));
				if (!bIsCommand && !bIsFunction)
				{
					continue;
				}

				const FString FileName = FPaths::GetCleanFilename(Class->GetMetaData(TEXT("ModuleRelativePath")));

				FString Documentation = Function->GetMetaData(TEXT("ToolTip"));
				Documentation.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

				if (bIsCommand)
				{
					FString YarnName = Function->GetMetaData(TEXT("YarnCommand"));
					if (YarnName.IsEmpty())
					{
						YarnName = Function->GetName();
					}

					TSharedPtr<FJsonObject> Entry = MakeBaseEntry(YarnName, Function->GetName(), FileName, TEXT("c++"));
					if (!Documentation.IsEmpty())
					{
						Entry->SetStringField(TEXT("documentation"), Documentation);
					}
					Entry->SetArrayField(TEXT("parameters"), BuildParameters(Function));
					Entry->SetBoolField(TEXT("async"), false);
					Commands.Add(YarnName, Entry);
				}

				if (bIsFunction)
				{
					FString YarnName = Function->GetMetaData(TEXT("YarnFunction"));
					if (YarnName.IsEmpty())
					{
						YarnName = Function->GetName();
					}

					TSharedPtr<FJsonObject> Entry = MakeBaseEntry(YarnName, Function->GetName(), FileName, TEXT("c++"));
					if (!Documentation.IsEmpty())
					{
						Entry->SetStringField(TEXT("documentation"), Documentation);
					}
					Entry->SetArrayField(TEXT("parameters"), BuildParameters(Function));

					TSharedPtr<FJsonObject> Return = MakeShared<FJsonObject>();
					Return->SetStringField(TEXT("type"), PropertyToYarnType(Function->GetReturnProperty()));
					Entry->SetObjectField(TEXT("return"), Return);
					Functions.Add(YarnName, Entry);
				}
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> BuildPlaceholderParameters(int32 Count)
	{
		TArray<TSharedPtr<FJsonValue>> Params;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			TSharedPtr<FJsonObject> Param = MakeShared<FJsonObject>();
			Param->SetStringField(TEXT("name"), FString::Printf(TEXT("arg%d"), Index));
			Param->SetStringField(TEXT("type"), TEXT("any"));
			Params.Add(MakeShared<FJsonValueObject>(Param));
		}
		return Params;
	}

	void CollectFromBlueprints(FEntryMap& Commands, FEntryMap& Functions)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		TArray<FAssetData> BlueprintAssets;
		AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BlueprintAssets, /*bSearchSubClasses=*/true);

		const FName YarnRuntimePackage(TEXT("/Script/YarnSpinner"));

		for (const FAssetData& AssetData : BlueprintAssets)
		{
			TArray<FName> Dependencies;
			AssetRegistry.GetDependencies(AssetData.PackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
			if (!Dependencies.Contains(YarnRuntimePackage))
			{
				continue;
			}

			UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
			if (!Blueprint)
			{
				continue;
			}

			TArray<UEdGraph*> Graphs;
			Blueprint->GetAllGraphs(Graphs);

			for (UEdGraph* Graph : Graphs)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
					if (!CallNode || CallNode->FunctionReference.GetMemberParentClass() != UYarnCommandLibrary::StaticClass())
					{
						continue;
					}

					const FName MemberName = CallNode->FunctionReference.GetMemberName();
					const bool bIsCommand = MemberName == GET_FUNCTION_NAME_CHECKED(UYarnCommandLibrary, RegisterCommandHandler);
					const bool bIsBlockingCommand = MemberName == GET_FUNCTION_NAME_CHECKED(UYarnCommandLibrary, RegisterBlockingCommandHandler);
					const bool bIsFunction = MemberName == GET_FUNCTION_NAME_CHECKED(UYarnCommandLibrary, RegisterFunctionHandler);
					if (!bIsCommand && !bIsBlockingCommand && !bIsFunction)
					{
						continue;
					}

					UEdGraphPin* NamePin = CallNode->FindPin(bIsFunction ? TEXT("FunctionName") : TEXT("CommandName"));
					if (!NamePin)
					{
						continue;
					}
					if (NamePin->LinkedTo.Num() > 0)
					{
						UE_LOG(LogYarnSpinner, Log, TEXT("YSLS: skipping a registration in %s - the name is computed at runtime, not a literal"), *AssetData.AssetName.ToString());
						continue;
					}

					const FString YarnName = NamePin->DefaultValue.TrimStartAndEnd();
					if (YarnName.IsEmpty())
					{
						continue;
					}

					const FString FileName = AssetData.AssetName.ToString();

					if (bIsFunction)
					{
						if (Functions.Contains(YarnName))
						{
							continue;
						}

						int32 ParameterCount = 0;
						if (UEdGraphPin* CountPin = CallNode->FindPin(TEXT("ParameterCount")))
						{
							if (CountPin->LinkedTo.Num() == 0)
							{
								ParameterCount = FCString::Atoi(*CountPin->DefaultValue);
							}
						}

						TSharedPtr<FJsonObject> Entry = MakeBaseEntry(YarnName, YarnName, FileName, TEXT("blueprint"));
						Entry->SetArrayField(TEXT("parameters"), BuildPlaceholderParameters(ParameterCount));

						TSharedPtr<FJsonObject> Return = MakeShared<FJsonObject>();
						Return->SetStringField(TEXT("type"), TEXT("any"));
						Entry->SetObjectField(TEXT("return"), Return);
						Functions.Add(YarnName, Entry);
					}
					else
					{
						if (Commands.Contains(YarnName))
						{
							continue;
						}

						TSharedPtr<FJsonObject> Entry = MakeBaseEntry(YarnName, YarnName, FileName, TEXT("blueprint"));
						Entry->SetArrayField(TEXT("parameters"), TArray<TSharedPtr<FJsonValue>>());
						Entry->SetBoolField(TEXT("async"), bIsBlockingCommand);
						Commands.Add(YarnName, Entry);
					}
				}
			}
		}
	}

	FString BuildYSLSJson()
	{
		FEntryMap Commands;
		FEntryMap Functions;
		TArray<FYarnBakedActionEntry> BakedEntries;
		CollectFromReflection(Commands, Functions, BakedEntries);
		FYarnYSLSGenerator::WriteBakedRegistry(MoveTemp(BakedEntries));
		CollectFromBlueprints(Commands, Functions);

		Commands.KeySort(TLess<FString>());
		Functions.KeySort(TLess<FString>());

		TArray<TSharedPtr<FJsonValue>> CommandsArray;
		for (const auto& Pair : Commands)
		{
			CommandsArray.Add(MakeShared<FJsonValueObject>(Pair.Value));
		}

		TArray<TSharedPtr<FJsonValue>> FunctionsArray;
		for (const auto& Pair : Functions)
		{
			FunctionsArray.Add(MakeShared<FJsonValueObject>(Pair.Value));
		}

		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("version"), 1);
		Root->SetArrayField(TEXT("commands"), CommandsArray);
		Root->SetArrayField(TEXT("functions"), FunctionsArray);

		FString Json;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return Json;
	}

	bool SaveYSLS(const FString& YarnProjectPath, const FString& Json)
	{
		FString ProjectPath = FPaths::ConvertRelativePathToFull(YarnProjectPath);
		FPaths::NormalizeFilename(ProjectPath);

		if (!FPaths::FileExists(ProjectPath))
		{
			UE_LOG(LogYarnSpinner, Warning, TEXT("YSLS: cannot generate for '%s' - the .yarnproject file does not exist"), *ProjectPath);
			return false;
		}

		const FString OutputPath = FPaths::ChangeExtension(ProjectPath, TEXT("ysls.json"));

		FString Existing;
		if (FFileHelper::LoadFileToString(Existing, *OutputPath) && Existing == Json)
		{
			return true;
		}

		if (!FFileHelper::SaveStringToFile(Json, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogYarnSpinner, Error, TEXT("YSLS: failed to write '%s'"), *OutputPath);
			return false;
		}

		UE_LOG(LogYarnSpinner, Log, TEXT("YSLS: generated '%s'"), *OutputPath);
		return true;
	}
}

bool FYarnYSLSGenerator::GenerateForProject(const FString& YarnProjectPath)
{
	return SaveYSLS(YarnProjectPath, BuildYSLSJson());
}

bool FYarnYSLSGenerator::GenerateForProjects(const TArray<FString>& YarnProjectPaths)
{
	if (YarnProjectPaths.Num() == 0)
	{
		return true;
	}

	const FString Json = BuildYSLSJson();

	bool bAllSucceeded = true;
	for (const FString& ProjectPath : YarnProjectPaths)
	{
		bAllSucceeded &= SaveYSLS(ProjectPath, Json);
	}
	return bAllSucceeded;
}

void FYarnYSLSGenerator::RefreshCommandRegistry()
{
	TArray<FYarnBakedActionEntry> Entries;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		if (!ClassIt->HasAnyClassFlags(CLASS_Native) || IsGeneratedClassArtifact(*ClassIt))
		{
			continue;
		}
		UYarnCommandLibrary::CollectYarnActionsFromClass(*ClassIt, Entries, /*bIncludeInherited=*/false);
	}
	WriteBakedRegistry(MoveTemp(Entries));
}

void FYarnYSLSGenerator::WriteBakedRegistry(TArray<FYarnBakedActionEntry> Entries)
{
	Entries.Sort([](const FYarnBakedActionEntry& A, const FYarnBakedActionEntry& B)
	{
		if (A.YarnName != B.YarnName)
		{
			return A.YarnName < B.YarnName;
		}
		if (A.OwningClass.ToString() != B.OwningClass.ToString())
		{
			return A.OwningClass.ToString() < B.OwningClass.ToString();
		}
		return A.FunctionName.LexicalLess(B.FunctionName);
	});

	UYarnCommandRegistrySettings* Settings = GetMutableDefault<UYarnCommandRegistrySettings>();

	const bool bUnchanged = Settings->BakedActions.Num() == Entries.Num()
		&& [&]()
		{
			for (int32 i = 0; i < Entries.Num(); i++)
			{
				const FYarnBakedActionEntry& A = Settings->BakedActions[i];
				const FYarnBakedActionEntry& B = Entries[i];
				if (A.YarnName != B.YarnName || A.OwningClass != B.OwningClass
					|| A.FunctionName != B.FunctionName || A.bIsFunction != B.bIsFunction)
				{
					return false;
				}
			}
			return true;
		}();

	if (bUnchanged)
	{
		return;
	}

	Settings->BakedActions = MoveTemp(Entries);
	if (!Settings->TryUpdateDefaultConfigFile())
	{
		UE_LOG(LogYarnSpinner, Warning, TEXT("YSLS: could not write the command registry to DefaultGame.ini (read-only or not checked out?) - packaged builds may miss metadata-registered commands"));
		return;
	}
	UE_LOG(LogYarnSpinner, Log, TEXT("YSLS: baked %d command registry entries to config"), Settings->BakedActions.Num());
}
