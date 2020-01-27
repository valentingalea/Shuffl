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
#include "Def.generated.h"

enum class EPuckState
{
	Setup,
	Traveling,
	Resting
};

enum class EPuckThrowMode
{
	Simple,
	WithSpin
};

UENUM(BlueprintType)
enum class EPuckColor : uint8
{
	Red,
	Blue
};

enum class EPlayerCtrlMode
{
	Setup,
	Throw,
	Slingshot,
	Spin,
	Observe
};

enum class EGameTurn
{
	Player1,
	Player2,
	CountingPoints
};