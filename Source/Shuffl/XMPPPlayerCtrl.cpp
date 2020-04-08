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

#include "PlayerCtrl.h"
#include "EngineUtils.h"
#include "Components/InputComponent.h"
#include "Math/UnrealMathUtility.h"
#include <cstring>

#include "Shuffl.h"
#include "GameSubSys.h"
#include "GameModes.h"
#include "XMPP.h"

//
// https://twitter.com/valentin_galea/status/1245054381583728641
//
template<class To, class From>
inline To bit_cast_generic(From src) noexcept
{
	static_assert(sizeof(To) == sizeof(From), "invalid bit cast");
	To dst;
	std::memcpy(&dst, &src, sizeof(To));
	return dst;
}

inline int32 bit_cast(float f) noexcept
{
	return bit_cast_generic<int32>(f);
}

inline float bit_cast(int32 i) noexcept
{
	return bit_cast_generic<float>(i);
}

namespace ChatCmd
{
	static constexpr auto NextTurn = TEXT("/turn");
	static constexpr auto Throw = TEXT("/throw");
	static constexpr auto Move = TEXT("/move");
}

static void RequestDebug(UWorld* world, FShufflXMPPService* xmpp)
{
	FString out = TEXT("/debug ?");
	int num = 0;

	for (auto i = TActorIterator<APuck>(world); i; ++i) {
		FVector p = i->GetActorLocation();
		out += FString::Printf(TEXT(" %i %i %i %i"),
			i->TurnId, bit_cast(p.X), bit_cast(p.Y), bit_cast(p.Z));
		num++;
	}

	out = out.Replace(TEXT("?"), *FString::Printf(TEXT("%i"), num));
	xmpp->SendChat(out);
}

void AXMPPPlayerCtrl::BeginPlay()
{
	Super::BeginPlay();

	if (auto sys = UGameSubSys::Get(this)) {
		XMPP = &sys->XMPP;
	}
}

void AXMPPPlayerCtrl::RequestNewThrow()
{
	if (PlayMode == EPlayerCtrlMode::Setup) return;

	auto* gameState = Cast<AShufflGameState>(GetWorld()->GetGameState());
	XMPP->SendChat(FString::Printf(TEXT("%s %i"),
		ChatCmd::NextTurn, gameState->GlobalTurnCounter));

	GetWorld()->GetAuthGameMode<AShufflCommonGameMode>()->NextTurn();
}

FVector AXMPPPlayerCtrl::MovePuckOnTouchPosition(FVector2D touchLocation)
{
	auto location = Super::MovePuckOnTouchPosition(touchLocation);

	XMPP->SendChat(FString::Printf(TEXT("%s %i %i %i"),
		ChatCmd::Move, bit_cast(location.X), bit_cast(location.Y), bit_cast(location.Z)));

	return location;
}

FVector2D AXMPPPlayerCtrl::ThrowPuck(FVector2D gestureVector, float velocity)
{
	auto force = Super::ThrowPuck(gestureVector, velocity);

	XMPP->SendChat(FString::Printf(TEXT("%s %i %i"),
		ChatCmd::Throw, bit_cast(force.X), bit_cast(force.Y)));

	return force;
}

FVector2D AXMPPPlayerCtrl::DoSlingshot()
{
	auto force = Super::DoSlingshot();

	XMPP->SendChat(FString::Printf(TEXT("%s %i %i"),
		ChatCmd::Throw, bit_cast(force.X), bit_cast(force.Y)));

	return force;
}

void AXMPPPlayerCtrl::SwitchToDetailView()
{
	Super::SwitchToDetailView();

	RequestDebug(GetWorld(), XMPP);
}

void AXMPPPlayerSpectator::BeginPlay()
{
	Super::BeginPlay();

	if (auto sys = UGameSubSys::Get(this)) {
		XMPP = &sys->XMPP;
		sys->OnXMPPChatReceived.AddUObject(this, &AXMPPPlayerSpectator::OnReceiveChat);
	}
}

void AXMPPPlayerSpectator::SetupInputComponent()
{
	// circumvent parent as to not inherit touch input
	APlayerController::SetupInputComponent();

	InputComponent->BindAction("Quit", IE_Released, this, &AXMPPPlayerSpectator::OnQuit);
	InputComponent->BindAction("Rethrow", IE_Released, this, &AXMPPPlayerSpectator::RequestNewThrow);
}

void AXMPPPlayerSpectator::SwitchToDetailView()
{
	Super::SwitchToDetailView();

	RequestDebug(GetWorld(), XMPP);
}

void AXMPPPlayerSpectator::OnReceiveChat(FString msg)
{
	TArray<FString> args;
	msg.ParseIntoArrayWS(args);
	make_sure(args.Num() > 1);
	const FString &cmd = args[0];

	if (cmd == ChatCmd::NextTurn) {
		auto* gameState = Cast<AShufflGameState>(GetWorld()->GetGameState());
		int turnId = FCString::Atoi(*args[1]);
		ShufflLog(TEXT("%s %i"), *cmd, turnId);

		if (turnId == gameState->GlobalTurnCounter) {
			GetWorld()->GetAuthGameMode<AShufflCommonGameMode>()->NextTurn();
		} else {
			ShufflErr(TEXT("Received bad turn %i vs %i"),
				turnId, gameState->GlobalTurnCounter); //TODO: add all this log info on screen as well
		}

		return;
	}

	if (cmd == ChatCmd::Move) {
		float X = bit_cast(FCString::Atoi(*args[1]));
		float Y = bit_cast(FCString::Atoi(*args[2]));
		float Z = bit_cast(FCString::Atoi(*args[3]));
		ShufflLog(TEXT("%s (%f) (%f) (%f)"), *cmd, X, Y, Z);

		if (GetPuck()) {
			GetPuck()->MoveTo(FVector(X, Y, Z));
			PlayMode = EPlayerCtrlMode::Setup;
		} else {
			ShufflErr(TEXT("received Move cmd when puck not spawned!"));
			//TODO: possibly save this and apply later when appropiate?
		}

		return;
	}

	if (cmd == ChatCmd::Throw) {
		float X = bit_cast(FCString::Atoi(*args[1]));
		float Y = bit_cast(FCString::Atoi(*args[2]));
		ShufflLog(TEXT("%s (%f) (%f)"), *cmd, X, Y);

		if (X > ThrowForceMax || Y > ThrowForceMax) {
			ShufflLog(TEXT("received invalid force!"));
			return;
		}

		if (GetPuck()) {
			GetPuck()->ApplyThrow(FVector2D(X, Y));
			PlayMode = EPlayerCtrlMode::Observe;
		} else {
			ShufflErr(TEXT("received Throw cmd when puck not spawned!"));
		}

		return;
	}

	if (cmd == TEXT("/debug")) {
		TMap<int, FVector> other_pos;
		int num = FCString::Atoi(*args[1]);

		for (int i = 2; i < 2 + num * 4;) {
			int turnId = FCString::Atoi(*args[i++]);
			float x = bit_cast(FCString::Atoi(*args[i++]));
			float y = bit_cast(FCString::Atoi(*args[i++]));
			float z = bit_cast(FCString::Atoi(*args[i++]));
			other_pos.Add(turnId, FVector(x, y, z));
		}

		static uint64 id = 0;
		for (auto i = TActorIterator<APuck>(GetWorld()); i; ++i) {
			if (!other_pos.Contains(i->TurnId)) continue;

			const FVector p = i->GetActorLocation();
			const FVector other_p = *other_pos.Find(i->TurnId);
			const FVector d = p - other_p;

			FString dbg = FString::Printf(TEXT("%i: %1.6f | %1.6f | %1.6f"),
				i->TurnId, FMath::Abs(d.X), FMath::Abs(d.Y), FMath::Abs(d.Z));
			ShufflLog(TEXT("%s"), *dbg);
		}

		return;
	}
}
