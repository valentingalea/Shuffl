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
#include "EngineUtils.h"
#include "Engine/Player.h"
#include "Math/UnrealMathUtility.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Algo/Sort.h"

#include "Shuffl.h"
#include "Puck.h"
#include "PlayerCtrl.h"
#include "ScoringVolume.h"

namespace MatchState
{
	const FName Round_Player1 = TEXT("Round_P1");
	const FName Round_Player2 = TEXT("Round_P2");
	const FName Round_End = TEXT("Round_End");
};

void AShufflPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShufflPlayerState, Color);
	DOREPLIFETIME(AShufflPlayerState, PucksToPlay);
}

void AShufflGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShufflGameState, GlobalTurnCounter);
}

void AShufflCommonGameMode::SetupRound()
{
	// clean prev pucks when restarting round
	TArray<APuck*, TInlineAllocator<32>> pucks;
	for (auto i = TActorIterator<APuck>(GetWorld()); i; ++i) {
		pucks.Add(*i);
	}
	for (auto* i : pucks) {
		GetWorld()->DestroyActor(i);
	}
}

void AShufflCommonGameMode::CalculateRoundScore(EPuckColor &winnerColor, int &totalScore)
{
	// get a list of the scoring areas
	TArray<AScoringVolume*, TInlineAllocator<4>> volumes;
	for (auto i = TActorIterator<AScoringVolume>(GetWorld()); i; ++i) {
		volumes.Add(*i);
	}

	// get the pucks and sort them by closest to edge (by furthest X position)
	TArray<APuck*, TInlineAllocator<32>> pucks;
	for (auto i = TActorIterator<APuck>(GetWorld()); i; ++i) {
		pucks.Add(*i);
	}
	Algo::Sort(pucks, [](APuck* p1, APuck* p2) {
		return p1->GetActorLocation().X > p2->GetActorLocation().X;
	});

	auto GetPointsForPuck = [&volumes](APuck* p) {
		for (AScoringVolume* vol : volumes) {
			FBox scoreVol = vol->GetBounds().GetBox();
			if (scoreVol.IsInside(p->GetActorLocation())) {
				return vol->PointsAwarded;
			}
		}
		return int(0);
	};

	// iterate on pucks and get their score
	totalScore = 0;
	bool foundWinner = false;
	winnerColor = EPuckColor::Red; //TODO: handle draw
	for (APuck* p : pucks) {
		int score = GetPointsForPuck(p);

		if (score > 0 && !foundWinner) {
			foundWinner = true;
			winnerColor = p->Color;
			totalScore = score;
			continue;
		}

		// only count the color of the winner if unobstructed by the other color pucks 
		if (score > 0 && foundWinner) {
			if (p->Color == winnerColor) {
				totalScore += score;
			} else {
				break;
			}
		}
	}
}

void AShufflPracticeGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	if (AutoTurnStart) {
		NextTurn();
	}
}

void AShufflPracticeGameMode::NextTurn()
{
	auto iterator = GetWorld()->GetPlayerControllerIterator(); 
	auto* controller = Cast<APlayerCtrl>(*iterator);
	controller->Server_NewThrow();
}

void AShuffl2PlayersGameMode::HandleMatchIsWaitingToStart()
{
	Super::HandleMatchIsWaitingToStart();

	auto iterator = GetWorld()->GetPlayerControllerIterator();
	auto* p1 = Cast<APlayerCtrl>(*iterator);

	// spawn a second player controller tied to the same local player
	auto* p2 = Cast<APlayerCtrl>(SpawnPlayerControllerCommon(
		ROLE_SimulatedProxy, // so it gets localplayer flag
		p1->K2_GetActorLocation(), p1->K2_GetActorRotation(),
		PlayerControllerClass));
	GetWorld()->AddController(p2);
	p2->Player = p1->Player;

	// give the player their colors
	p1->GetPlayerState<AShufflPlayerState>()->Color = EPuckColor::Red;
	p2->GetPlayerState<AShufflPlayerState>()->Color = EPuckColor::Blue;
}

void AShuffl2PlayersGameMode::StartMatch()
{
	Super::StartMatch();

	MatchState = MatchState::Round_End;
	if (AutoTurnStart) {
		NextTurn();
	}
}

void AShuffl2PlayersGameMode::NextTurn()
{
	auto iterator = GetWorld()->GetPlayerControllerIterator();
	APlayerCtrl *next_player, *curr_player = nullptr;
	decltype(MatchState) desiredState;
	if (GetMatchState() == MatchState::Round_Player1) {
		desiredState = MatchState::Round_Player2;
		curr_player = Cast<APlayerCtrl>(*iterator);
		iterator++;
		next_player = Cast<APlayerCtrl>(*iterator);
	} else { // P2 or Round_End
		if (GetMatchState() == MatchState::Round_End) {
			SetupRound();
		}
		desiredState = MatchState::Round_Player1;
		next_player = Cast<APlayerCtrl>(*iterator);
		iterator++;
		curr_player = Cast<APlayerCtrl>(*iterator);
	}

	auto* curr_player_state = curr_player->GetPlayerState<AShufflPlayerState>();
	auto* next_player_state = next_player->GetPlayerState<AShufflPlayerState>();
	int totalScore = int(curr_player_state->Score) + int(next_player_state->Score);
	if (totalScore == 21) {
		EndMatch(); //TODO: manage properly end of match
		return;
	}
	if ((curr_player_state->PucksToPlay + next_player_state->PucksToPlay) == 0) {
		SetMatchState(MatchState::Round_End);

		curr_player_state->PucksToPlay = ERound::PucksPerPlayer;
		next_player_state->PucksToPlay = ERound::PucksPerPlayer;

		EPuckColor winner_color;
		int winner_score;
		CalculateRoundScore(winner_color, winner_score);
		AShufflPlayerState* winner_player = winner_color == curr_player_state->Color ?
			curr_player_state : next_player_state;
		winner_player->Score += winner_score;
 		curr_player->Client_EnterScoreCounting(winner_color, winner_player->Score);
		
		return;
	}

	next_player_state->PucksToPlay--;
	auto* game_state = GetGameState<AShufflGameState>();
	game_state->GlobalTurnCounter++;
	SetMatchState(desiredState);

	curr_player->Player->SwitchController(next_player);
	next_player->Server_NewThrow();
}

void AShufflNetworkGameMode::PostLogin(APlayerController *new_player)
{
	Super::PostLogin(new_player);

	if (NumPlayers <= 1) {
		P1 = static_cast<decltype(P1)>(new_player);
		P1->GetPlayerState<AShufflPlayerState>()->Color = EPuckColor::Red;
	} else {
		P2 = static_cast<decltype(P2)>(new_player);
		P2->GetPlayerState<AShufflPlayerState>()->Color = EPuckColor::Blue;
	}
}

void AShufflNetworkGameMode::StartMatch()
{
	Super::StartMatch();

	MatchState = MatchState::Round_End;
	if (AutoTurnStart) {
		NextTurn();
	}
}

void AShufflNetworkGameMode::NextTurn()
{
	make_sure(P1 && P2);

	APlayerCtrl *next_player, *prev_player;
	decltype(MatchState) desiredState;

	if (GetMatchState() == MatchState::Round_Player1) {
		desiredState = MatchState::Round_Player2;
		prev_player = P1;
		next_player = P2;
	} else { // P2 or Round_End
		if (GetMatchState() == MatchState::Round_End) {
			SetupRound();
		}
		desiredState = MatchState::Round_Player1;
		prev_player = P2;
		next_player = P1;
	}

	auto* prev_player_state = prev_player->GetPlayerState<AShufflPlayerState>();
	auto* next_player_state = next_player->GetPlayerState<AShufflPlayerState>();
	int totalScore = int(prev_player_state->Score) + int(next_player_state->Score);
	if (totalScore == 21) {
		EndMatch(); //TODO: manage properly end of match
		return;
	}
	if ((prev_player_state->PucksToPlay + next_player_state->PucksToPlay) == 0) {
		SetMatchState(MatchState::Round_End);

		prev_player_state->PucksToPlay = ERound::PucksPerPlayer;
		next_player_state->PucksToPlay = ERound::PucksPerPlayer;

		EPuckColor winner_color;
		int winner_score;
		CalculateRoundScore(winner_color, winner_score);
		AShufflPlayerState* winner_player = winner_color == prev_player_state->Color ?
			prev_player_state : next_player_state;
		winner_player->Score += winner_score;
		prev_player->Client_EnterScoreCounting(winner_color, winner_player->Score);
		next_player->Client_EnterScoreCounting(winner_color, winner_player->Score); //DIFF

		return;
	}

	next_player_state->PucksToPlay--;
	auto* game_state = GetGameState<AShufflGameState>();
	game_state->GlobalTurnCounter++;
	SetMatchState(desiredState);

	//curr_player->Player->SwitchController(next_player); DIFF
	next_player->Server_NewThrow();
	next_player->Client_NewThrow();
}
