// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "ShufflGameModeBase.h"
#include "PlayerCtrl.h"

#include "UObject/ConstructorHelpers.h"

AShufflGameModeBase::AShufflGameModeBase()
{
	static ConstructorHelpers::FClassFinder<APawn> Pawn(TEXT("/Game/BPC_Pawn"));
	ensure(Pawn.Class);
	DefaultPawnClass = Pawn.Class;

	PlayerControllerClass = APlayerCtrl::StaticClass();
}