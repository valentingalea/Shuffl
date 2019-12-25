// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "ShufflGameModeBase.h"
#include "LookAtPawn.h"

AShufflGameModeBase::AShufflGameModeBase()
{
	DefaultPawnClass = ALookAtPawn::StaticClass();
}