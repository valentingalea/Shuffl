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
#include "Engine/Engine.h"
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
	static constexpr TCHAR* NextTurn = TEXT("/turn");
	static constexpr TCHAR* Throw = TEXT("/throw");
	static constexpr TCHAR* Move = TEXT("/move");
}

template <typename FmtType, typename... Types>
inline void DebugPrint(const FmtType& fmt, Types... args)
{
	static uint64 id = 0;
	GEngine->AddOnScreenDebugMessage(id++, 5/*sec*/, FColor::Green,
		FString::Printf(fmt, args...), false/*newer on top*/, FVector2D(2, 2)/*scale*/);
}

void AXMPPPlayerCtrl::BeginPlay()
{
	Super::BeginPlay();

	if (auto sys = UGameSubSys::Get(this)) {
		XMPP = &sys->XMPP;
	}
}

void AXMPPPlayerCtrl::SetupInputComponent()
{
	if (XMPPState == EXMPPMultiplayerState::Broadcast) {
		Super::SetupInputComponent();
	} else {
		APlayerController::SetupInputComponent();
		InputComponent->BindAction("Quit", IE_Released, this, &AXMPPPlayerCtrl::OnQuit);
		InputComponent->BindAction("Rethrow", IE_Released, this, &AXMPPPlayerCtrl::RequestNewThrow);
	}
}

void AXMPPPlayerCtrl::RequestNewThrow()
{
	if (PlayMode == EPlayerCtrlMode::Setup) return;

	if (XMPPState == EXMPPMultiplayerState::Broadcast) {
		auto* gameState = Cast<AShufflGameState>(GetWorld()->GetGameState());
		XMPP->SendChat(FString::Printf(TEXT("%s %i"),
			ChatCmd::NextTurn, gameState->GlobalTurnCounter));
	}

	GetWorld()->GetAuthGameMode<AShufflCommonGameMode>()->NextTurn();
}

FVector AXMPPPlayerCtrl::MovePuckOnTouchPosition(FVector2D touchLocation)
{
	auto location = MovePuckOnTouchPosition(touchLocation);

	if (XMPPState == EXMPPMultiplayerState::Broadcast) {
		XMPP->SendChat(FString::Printf(TEXT("%s %i %i %i"),
			ChatCmd::Move, bit_cast(location.X), bit_cast(location.Y), bit_cast(location.Z)));
	}

	return location;
}

FVector2D AXMPPPlayerCtrl::ThrowPuck(FVector2D gestureVector, float velocity)
{
	auto force = Super::ThrowPuck(gestureVector, velocity);

	if (XMPPState == EXMPPMultiplayerState::Broadcast) {
		XMPP->SendChat(FString::Printf(TEXT("%s %i %i"),
			ChatCmd::Throw, bit_cast(force.X), bit_cast(force.Y)));
	}

	return force;
}

FVector2D AXMPPPlayerCtrl::DoSlingshot()
{
	auto force = Super::DoSlingshot();

	if (XMPPState == EXMPPMultiplayerState::Broadcast) {
		XMPP->SendChat(FString::Printf(TEXT("%s %i %i"),
			ChatCmd::Throw, bit_cast(force.X), bit_cast(force.Y)));
	}

	return force;
}

void AXMPPPlayerCtrl::OnReceiveChat(const FString& msg)
{
	TArray<FString> args;
	msg.ParseIntoArrayWS(args);
	make_sure(args.Num() > 1);
	const FString &cmd = args[0];

	ensure(XMPPState == EXMPPMultiplayerState::Spectate);

	if (cmd == ChatCmd::NextTurn) {
		auto* gameState = Cast<AShufflGameState>(GetWorld()->GetGameState());
		int turnId = FCString::Atoi(*args[1]);
		DebugPrint(TEXT("%s %i"), *cmd, turnId);

		if (turnId == gameState->GlobalTurnCounter) {
			GetWorld()->GetAuthGameMode<AShufflCommonGameMode>()->NextTurn();
		} else {
			UE_LOG(LogShuffl, Error, TEXT("Received bad turn %i vs %i"),
				turnId, gameState->GlobalTurnCounter);
		}

		return;
	}

	if (cmd == ChatCmd::Move) {
		float X = bit_cast(FCString::Atoi(*args[1]));
		float Y = bit_cast(FCString::Atoi(*args[2]));
		float Z = bit_cast(FCString::Atoi(*args[3]));
		DebugPrint(TEXT("%s (%f) (%f) (%f)"), *cmd, X, Y, Z);

		GetPuck()->MoveTo(FVector(X, Y, Z));
		PlayMode = EPlayerCtrlMode::Setup;

		return;
	}

	if (cmd == ChatCmd::Throw) {
		float X = bit_cast(FCString::Atoi(*args[1]));
		float Y = bit_cast(FCString::Atoi(*args[2]));
		DebugPrint(TEXT("%s (%f) (%f)"), *cmd, X, Y);

		if (X > ThrowForceMax || Y > ThrowForceMax) {
			DebugPrint(TEXT("ERROR!"));
			return;
		}

		GetPuck()->ApplyThrow(FVector2D(X, Y));
		PlayMode = EPlayerCtrlMode::Observe;

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
			GEngine->AddOnScreenDebugMessage(id++, 30/*sec*/, FColor::Green, dbg);
			UE_LOG(LogShuffl, Warning, TEXT("%s"), *dbg);
		}

		return;
	}
}

void AXMPPPlayerCtrl::Debug()
{
	FString out = TEXT("/debug ?");
	int num = 0;

	for (auto i = TActorIterator<APuck>(GetWorld()); i; ++i) {
		FVector p = i->GetActorLocation();
		out += FString::Printf(TEXT(" %i %i %i %i"),
			i->TurnId, bit_cast(p.X), bit_cast(p.Y), bit_cast(p.Z));
		num++;
	}

	out = out.Replace(TEXT("?"), *FString::Printf(TEXT("%i"), num));

	auto sys = UGameSubSys::Get(this);
	sys->XMPP.SendChat(out);
}