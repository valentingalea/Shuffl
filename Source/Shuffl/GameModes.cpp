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

#include "GameModes.h"

#include "Math/UnrealMathUtility.h"

//#include "Net/UnrealNetwork.h"
//
//void AShuffGameState::BeginPlay()
//{
//	Super::BeginPlay();
//
//	if (GetWorld()->IsGameWorld() && GetLocalRole() == ROLE_Authority) {
//		GameType = static_cast<AShuffGameMode*>(AuthorityGameMode)->GameType;
//	}
//}

//void AShuffGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
//{
//	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
//
//	DOREPLIFETIME(AShuffGameState, GameType);
//}

void APracticeGameState::BeginPlay()
{
	Super::BeginPlay();

	InPlayTurn = EGameTurn::Player1;
	InPlayPuckColor = EPuckColor::Red;
}

void A2PlayersGameState::BeginPlay()
{
	Super::BeginPlay();

	InPlayPuckColor = FMath::RandBool() ? EPuckColor::Red : EPuckColor::Blue;
	InPlayTurn = InPlayPuckColor == EPuckColor::Red ? EGameTurn::Player1 : EGameTurn::Player2;
}

void A2PlayersGameState::NextTurn()
{
	InPlayPuckColor = InPlayPuckColor == EPuckColor::Blue ? EPuckColor::Red : EPuckColor::Blue;
	InPlayTurn = InPlayPuckColor == EPuckColor::Red ? EGameTurn::Player1 : EGameTurn::Player2;
}