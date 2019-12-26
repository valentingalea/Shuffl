// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "ShufflGameModeBase.h"
#include "PlayerCtrl.h"

#include "UObject/ConstructorHelpers.h"

AShufflGameModeBase::AShufflGameModeBase()
{
	DefaultPawnClass = nullptr; // the PC below will handle this

	PlayerControllerClass = APlayerCtrl::StaticClass();
}