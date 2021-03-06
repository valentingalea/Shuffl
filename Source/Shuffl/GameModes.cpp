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
#include "GameSubSys.h"
#include "XMPP.h"

namespace MatchState
{
	const FName Round_Player1 = TEXT("Round_P1");
	const FName Round_Player2 = TEXT("Round_P2");
	const FName Round_XMPPSync = TEXT("Round_XMPPSync");
	const FName Round_End = TEXT("Round_End");
	const FName Round_WinnerDeclared = TEXT("Round_Winner");
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

	// reset scores after a winning round
	if (GetMatchState() == MatchState::Round_WinnerDeclared) {
		for (auto i = GetWorld()->GetPlayerControllerIterator(); i; ++i) {
			auto* pc = Cast<APlayerCtrl>(i->Get());
			pc->GetPlayerState<AShufflPlayerState>()->SetScore(0);
		}
	}
	
	// for fairness swap the players each round (except the starting one)
	if (GetGameState<AShufflGameState>()->GlobalTurnCounter) {
		Swap(PlayOrder[0], PlayOrder[1]);
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
	winnerColor = EPuckColor::Red; // choose a default, BP will handle it properly
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

	// only 1 player
	auto* game_state = GetGameState<AShufflGameState>();
	game_state->ActiveLocalPlayerCtrlIndex = 0;
	PlayOrder[0] = GetWorld()->GetPlayerControllerIterator()->Get();

	if (AutoTurnStart) {
		NextTurn();
	}
}

void AShufflPracticeGameMode::NextTurn()
{
	auto* game_state = GetGameState<AShufflGameState>();
	game_state->GlobalTurnCounter++;

	auto iterator = GetWorld()->GetPlayerControllerIterator(); 
	auto* controller = Cast<APlayerCtrl>(*iterator);
	controller->HandleNewThrow();
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
	RealPlayer = p2->Player = p1->Player;
	p2->MyHUD = p1->MyHUD;

	// give the player their colors
	p1->GetPlayerState<AShufflPlayerState>()->Color = EPuckColor::Red;
	p2->GetPlayerState<AShufflPlayerState>()->Color = EPuckColor::Blue;

	PlayOrder[0] = p1;
	PlayOrder[1] = p2;
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
	make_sure(RealPlayer);
	auto *iterator = PlayOrder;
	APlayerCtrl *next_player, *curr_player = nullptr;
	decltype(MatchState) desiredState;

	if (GetMatchState() == MatchState::Round_Player1) {
		desiredState = MatchState::Round_Player2;
		curr_player = Cast<APlayerCtrl>(*iterator);
		iterator++;
		next_player = Cast<APlayerCtrl>(*iterator);
	} else { // P2 or Round_End's
		if (GetMatchState() != MatchState::Round_Player2) {
			SetupRound();
		}
		desiredState = MatchState::Round_Player1;
		next_player = Cast<APlayerCtrl>(*iterator);
		iterator++;
		curr_player = Cast<APlayerCtrl>(*iterator);
	}

	auto* curr_player_state = curr_player->GetPlayerState<AShufflPlayerState>();
	auto* next_player_state = next_player->GetPlayerState<AShufflPlayerState>();

	if ((curr_player_state->PucksToPlay + next_player_state->PucksToPlay) == 0) {
		curr_player_state->PucksToPlay = ERound::PucksPerPlayer;
		next_player_state->PucksToPlay = ERound::PucksPerPlayer;

		EPuckColor winner_color;
		int round_score;
		CalculateRoundScore(winner_color, round_score);
		AShufflPlayerState* winner_player = winner_color == curr_player_state->Color ?
			curr_player_state : next_player_state;
		winner_player->SetScore(winner_player->GetScore() + round_score);

		if (winner_player->GetScore() >= UGameSubSys::ShufflGetWinningScore()) {
			SetMatchState(MatchState::Round_WinnerDeclared);
		} else {
			SetMatchState(MatchState::Round_End);
		}

		curr_player->HandleScoreCounting(winner_color,
			winner_player->GetScore(), round_score);
		return;
	}

	next_player_state->PucksToPlay--;
	auto* game_state = GetGameState<AShufflGameState>();
	game_state->GlobalTurnCounter++;
	game_state->ActiveLocalPlayerCtrlIndex = 
		desiredState == MatchState::Round_Player1 ? 0 : 1;
	SetMatchState(desiredState);

	RealPlayer->SwitchController(next_player);
	next_player->HandleNewThrow();
}

void AShufflAgainstAIGameMode::HandleMatchIsWaitingToStart()
{
	Super::Super::HandleMatchIsWaitingToStart();

	auto iterator = GetWorld()->GetPlayerControllerIterator();
	auto* p1 = Cast<APlayerCtrl>(*iterator);

	// spawn a second player controller tied to the same local player
	auto* p2 = Cast<APlayerCtrl>(SpawnPlayerControllerCommon(
		ROLE_SimulatedProxy, // so it gets localplayer flag
		p1->K2_GetActorLocation(), p1->K2_GetActorRotation(),
		ReplaySpectatorPlayerControllerClass)); //NOTE: use this as transport for the AI pc
	GetWorld()->AddController(p2);
	RealPlayer = p2->Player = p1->Player;

	// give the player their colors
	p1->GetPlayerState<AShufflPlayerState>()->Color = EPuckColor::Red;
	p2->GetPlayerState<AShufflPlayerState>()->Color = EPuckColor::Blue;

	PlayOrder[0] = p1;
	PlayOrder[1] = p2;
}

void AShufflXMPPGameMode::HandleMatchIsWaitingToStart()
{
	Super::Super::HandleMatchIsWaitingToStart();

	auto iterator = GetWorld()->GetPlayerControllerIterator();
	auto* p1 = Cast<APlayerCtrl>(*iterator);

	// spawn a second player controller tied to the same local player
	auto* p2 = Cast<APlayerCtrl>(SpawnPlayerControllerCommon(
		ROLE_SimulatedProxy, // so it gets localplayer flag
		p1->K2_GetActorLocation(), p1->K2_GetActorRotation(),
		ReplaySpectatorPlayerControllerClass)); //NOTE: use this as transport for second type of controller
	GetWorld()->AddController(p2);
	RealPlayer = p2->Player = p1->Player;
	p2->MyHUD = p1->MyHUD;

	make_sure(UGameplayStatics::HasOption(OptionsString, XMPPGameMode::Option_PuckColor));
	auto startPuckColor = StringToPuckColor(
		*UGameplayStatics::ParseOption(OptionsString, XMPPGameMode::Option_PuckColor));

	if (UGameplayStatics::HasOption(OptionsString, XMPPGameMode::Option_Host)) {
		p1->GetPlayerState<AShufflPlayerState>()->Color = startPuckColor;
		p2->GetPlayerState<AShufflPlayerState>()->Color = OppositePuckColor(startPuckColor);
	}
	if (UGameplayStatics::HasOption(OptionsString, XMPPGameMode::Option_Invitee)) {
		p1->GetPlayerState<AShufflPlayerState>()->Color = OppositePuckColor(startPuckColor);
		p2->GetPlayerState<AShufflPlayerState>()->Color = startPuckColor;
	}

	PlayOrder[0] = p1;
	PlayOrder[1] = p2;

	auto sys = UGameSubSys::Get(this);
	make_sure(sys);
	sys->OnXMPPChatReceived.AddUObject(this, &AShufflXMPPGameMode::OnReceiveChat);
}

void AShufflXMPPGameMode::NextTurn()
{
	if (UGameplayStatics::HasOption(OptionsString, XMPPGameMode::Option_Host)) {
		Super::NextTurn();

		if (GetMatchState() == MatchState::Round_End
			|| GetMatchState() == MatchState::Round_WinnerDeclared) {
			EPuckColor winner_color;
			int round_score;
			CalculateRoundScore(winner_color, round_score);

			SyncPuck(-1); // send across all puck positions
			auto sys = UGameSubSys::Get(this);
			sys->XMPP.SendChat(FString::Printf(TEXT("/score-sync %s %i"), 
				PuckColorToString(winner_color), round_score));
		}
	} else {
		if (GetMatchState() == MatchState::Round_XMPPSync) {
			return; // wait to receive above msg from host
		}

		int pucks_remaining = 0;
		for (auto* i : PlayOrder) {
			auto *ps = i->GetPlayerState<AShufflPlayerState>();
			pucks_remaining += ps->PucksToPlay;
		}

		if (pucks_remaining == 0) {
			SetMatchState(MatchState::Round_XMPPSync);
		} else {
			Super::NextTurn();
		}
	}
}

void AShufflXMPPGameMode::OnReceiveChat(FString msg)
{
	if (msg.Contains("/score-sync")) {
		if (UGameplayStatics::HasOption(OptionsString, XMPPGameMode::Option_Invitee)) {
			ensure(GetMatchState() == MatchState::Round_XMPPSync);

			TArray<FString> args;
			msg.ParseIntoArrayWS(args);
			EPuckColor winner_color = StringToPuckColor(*args[1]);
			int round_score = FCString::Atoi(*args[2]);

			AShufflPlayerState* winner_player = nullptr;
			for (auto* i : PlayOrder) {
				auto* ps = i->GetPlayerState<AShufflPlayerState>();
				if (ps->Color == winner_color) {
					winner_player = ps;
				}
				ps->PucksToPlay = ERound::PucksPerPlayer;
			}

			winner_player->SetScore(winner_player->GetScore() + round_score);
			if (winner_player->GetScore() >= UGameSubSys::ShufflGetWinningScore()) {
				SetMatchState(MatchState::Round_WinnerDeclared);
			}
			else {
				SetMatchState(MatchState::Round_End);
			}

			auto* pc = Cast<APlayerCtrl>(RealPlayer->PlayerController);
			pc->HandleScoreCounting(winner_color, winner_player->GetScore(), round_score);
		}
	}
}

void AShufflXMPPGameMode::SyncPuck(int turnId)
{
	// sync only in one direction i.e. the Host has (arbitrary) authority
	if (!UGameplayStatics::HasOption(OptionsString, XMPPGameMode::Option_Host)) return;
	
	for (auto *i : PlayOrder) {
		if (auto* pc = Cast<AXMPPPlayerCtrl>(i)) {
			pc->SendSync(turnId);
			return;
		}
	}
}