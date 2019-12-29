// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "ShufflGameModeBase.h"
#include "PlayerCtrl.h"

#include "UObject/ConstructorHelpers.h"

AShufflGameModeBase::AShufflGameModeBase()
{
	DefaultPawnClass = nullptr; // the PC below will handle this

	static ConstructorHelpers::FClassFinder<APlayerCtrl> PC(TEXT("/Game/BPC_PlayerCtrl"));
	PlayerControllerClass = PC.Class;
}