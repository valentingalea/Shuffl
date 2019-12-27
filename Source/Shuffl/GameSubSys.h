// Copyright (C) 2020 Valentin Galea

#pragma once

#include "CoreMinimal.h"
#include "Engine\Public\Subsystems\GameInstanceSubsystem.h"
#include "GameSubSys.generated.h"

DECLARE_DELEGATE_OneParam(FEvent_AwardPoints, int);

UCLASS()
class UGameSubSys : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//virtual void Initialize(FSubsystemCollectionBase& Collection) override {}
	//virtual void Deinitialize() override {}

	static UGameSubSys* Get(const UObject* ContextObject);

	FEvent_AwardPoints AwardPoints;
};
