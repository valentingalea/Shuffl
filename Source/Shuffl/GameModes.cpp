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

#include "EngineMinimal.h"
#include "Engine/Player.h"
#include "Math/UnrealMathUtility.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

#include "PlayerCtrl.h"

void AShufflPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShufflPlayerState, Color);
	DOREPLIFETIME(AShufflPlayerState, PucksToPlay);
}

void AShufflGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShufflGameState, RoundTurn);
}

void AShufflPracticeGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	NextTurn();
}

void AShufflPracticeGameMode::NextTurn()
{
	auto iterator = GetWorld()->GetPlayerControllerIterator(); 
	auto* controller = Cast<APlayerCtrl>(*iterator);
	controller->Client_NewThrow();
}

void AShuffl2PlayersGameMode::HandleMatchIsWaitingToStart()
{
	Super::HandleMatchIsWaitingToStart();

	auto iterator = GetWorld()->GetPlayerControllerIterator();
	auto* p1 = Cast<APlayerCtrl>(*iterator);

	// spawn a second player controller tied to the same local player
	auto* p2 = Cast<APlayerCtrl>(SpawnPlayerControllerCommon(
		ROLE_SimulatedProxy, //TODO: understand better the implications of this
		p1->K2_GetActorLocation(), p1->K2_GetActorRotation(),
		PlayerControllerClass));
	GetWorld()->AddController(p2);
	p2->Player = p1->Player;

	// give the player colors
	auto randColor = FMath::RandBool() ? EPuckColor::Red : EPuckColor::Blue;
	p1->GetPlayerState<AShufflPlayerState>()->Color = randColor;
	p2->GetPlayerState<AShufflPlayerState>()->Color = (randColor == EPuckColor::Blue)
		? EPuckColor::Red : EPuckColor::Blue;
}

void AShuffl2PlayersGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	// start player 1 by pretending there's a swap
	GetGameState<AShufflGameState>()->RoundTurn = ERoundTurn::Player2;
	NextTurn();
}

void AShuffl2PlayersGameMode::NextTurn()
{
	auto* state = GetGameState<AShufflGameState>();
	state->GlobalTurnCounter++;

	auto iterator = GetWorld()->GetPlayerControllerIterator();
	APlayerCtrl *pc, *prev_pc = nullptr;
	if (state->RoundTurn == ERoundTurn::Player1) {
		state->RoundTurn = ERoundTurn::Player2;
		prev_pc = Cast<APlayerCtrl>(*iterator);
		iterator++;
		pc = Cast<APlayerCtrl>(*iterator);
	} else {
		state->RoundTurn = ERoundTurn::Player1;
		pc = Cast<APlayerCtrl>(*iterator);
		iterator++;
		prev_pc = Cast<APlayerCtrl>(*iterator);
	}

	//TODO: handle round end
	//auto* prev_ps = prev_pc->GetPlayerState<AShufflPlayerState>();
	//auto* ps = pc->GetPlayerState<AShufflPlayerState>();
	//if ((prev_ps->PucksToPlay + ps->PucksToPlay) == 3 + 3) {
	//	pc->Client_EnterScoreCounting();
	//	return;
	//}
	//ps->PucksToPlay--;

	//TODO: manage score getting to 21

	prev_pc->Player->SwitchController(pc);
	pc->Client_NewThrow();
}