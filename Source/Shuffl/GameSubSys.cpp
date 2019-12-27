// Copyright (C) 2020 Valentin Galea

#include "GameSubSys.h"
#include "EngineMinimal.h"
#include "Engine\GameInstance.h"

UGameSubSys* UGameSubSys::Get(const UObject* ContextObject)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(ContextObject, 
		EGetWorldErrorMode::LogAndReturnNull)) {
		return UGameInstance::GetSubsystem<UGameSubSys>(World->GetGameInstance());
	}

	return nullptr;
}