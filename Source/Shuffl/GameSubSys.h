// Copyright (C) 2020 Valentin Galea
//
// This program is free software : you can redistribute it and /or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "CoreMinimal.h"
#include "Engine\Public\Subsystems\GameInstanceSubsystem.h"

#include "Def.h"

#include "GameSubSys.generated.h"

// only handled in C++
DECLARE_MULTICAST_DELEGATE_OneParam(FEvent_PuckResting, class APuck *);

// handled in C++/Blueprint
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEvent_ScoreChanged, int, NewScoreValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEvent_PlayersChangeTurn, EPuckColor, NewColor);

UCLASS()
class UGameSubSys : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UGameSubSys* Get(const UObject* ContextObject);

	FEvent_PuckResting PuckResting;

	UPROPERTY(BlueprintAssignable)
	FEvent_ScoreChanged ScoreChanged;

	UPROPERTY(BlueprintAssignable)
	FEvent_PlayersChangeTurn PlayersChangeTurn;
};
