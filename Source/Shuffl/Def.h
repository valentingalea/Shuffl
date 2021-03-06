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

enum class EPuckState : uint8
{
	Setup,
	Traveling,
	Traveling_WithSpin,
	Resting
};

enum class EPuckThrowMode : uint8
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

enum class EPlayerCtrlMode : uint8
{
	Setup,
	Throw,
	Slingshot,
	Spin,
	Observe
};

namespace ERound
{
	static constexpr int PucksPerPlayer = 4;
	static constexpr int TotalThrows = PucksPerPlayer * 2;
	static constexpr int WinningScore = 21;
};

namespace MatchState
{
	// all our new states are extensions of this state: `AGameMode::MatchState::InProgress`
	extern const FName Round_Player1;
	extern const FName Round_Player2;
	extern const FName Round_XMPPSync; /* only during online matches via XMPP chat */
	extern const FName Round_End;
	extern const FName Round_WinnerDeclared;
};