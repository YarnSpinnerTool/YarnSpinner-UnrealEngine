#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "YarnDialogueRunner.h"
#include "YarnTestCommandHandler.generated.h"

UCLASS(Transient, NotBlueprintable, HideDropdown)
class UYarnTestCommandHandler : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UYarnDialogueRunner> Runner;

	UFUNCTION()
	bool finish_at_once()
	{
		if (Runner)
		{
			Runner->CompleteBlockingCommand();
		}
		return true;
	}
};
