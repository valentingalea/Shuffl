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
#include "TimerManager.h"
#include "Components/InputComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "DrawDebugHelpers.h"
#include "Math/UnrealMathUtility.h"
#include "Components/ArrowComponent.h"

#include "Shuffl.h"
#include "GameSubSys.h"
#include "GameModes.h"
#include "ScoringVolume.h"
#include "SceneProps.h"

template <typename FmtType, typename... Types>
inline void DebugPrint(const FmtType& fmt, Types... args)
{
#if 0
	static uint64 id = 0;
	GEngine->AddOnScreenDebugMessage(id++, 3/*sec*/, FColor::Green,
		FString::Printf(fmt, args...));
#endif
}

#include <cstring>
//NOTE: https://en.cppreference.com/w/cpp/numeric/bit_cast
template <class To, class From>
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

inline FHitResult ProjectScreenPoint(const APlayerCtrl *ctrl, const FVector2D& location)
{
	FHitResult hitResult;
	ctrl->GetHitResultAtScreenPosition(location, ECC_Visibility,
		false/*trace complex*/, hitResult);
	return hitResult;
}

APlayerCtrl::APlayerCtrl()
{
	bShowMouseCursor = true;
}

void APlayerCtrl::BeginPlay()
{
	Super::BeginPlay();

// inviolable contracts
	{
		make_sure(PawnClass);

		auto iter = TActorIterator<ASceneProps>(GetWorld());
		make_sure(*iter);
		SceneProps = *iter;
		make_sure(SceneProps->DetailViewCamera);
		make_sure(SceneProps->StartingPoint);
		make_sure(SceneProps->KillingVolume);

		StartingPoint = SceneProps->StartingPoint->GetActorLocation();
	}

	TouchHistory.Reserve(64);
	PlayMode = EPlayerCtrlMode::Setup;
}

void APlayerCtrl::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindAction("Quit", IE_Released, this, &APlayerCtrl::OnQuit);

	constexpr auto dv = "DetailView";
	InputComponent->BindAction(dv, IE_Pressed, this, &APlayerCtrl::SwitchToDetailView);
	InputComponent->BindAction(dv, IE_Released, this, &APlayerCtrl::SwitchToPlayView);

	InputComponent->BindAction("Rethrow", IE_Released, this, &APlayerCtrl::RequestNewThrow);

	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &APlayerCtrl::ConsumeTouchOn);
	InputComponent->BindTouch(EInputEvent::IE_Repeat, this, &APlayerCtrl::ConsumeTouchRepeat);
	InputComponent->BindTouch(EInputEvent::IE_Released, this, &APlayerCtrl::ConsumeTouchOff);
}

void APlayerCtrl::OnQuit()
{
	FPlatformMisc::RequestExit(false);
}

void APlayerCtrl::RequestNewThrow()
{
	if (PlayMode == EPlayerCtrlMode::Setup) return;
	//TODO: allow XMMPPlayer to request at any time?

	if (XMPP) {
		auto sys = UGameSubSys::Get(this);
		auto* gameState = Cast<AShufflGameState>(GetWorld()->GetGameState());
		sys->XmppSend(FString::Printf(TEXT("/turn %i"), gameState->GlobalTurnCounter));
	}
	GetWorld()->GetAuthGameMode<AShufflCommonGameMode>()->NextTurn();
}

void APlayerCtrl::HandleNewThrow()
{
	if (PlayMode == EPlayerCtrlMode::Spin) return;

	if (SceneProps->ARTable) {
		StartingPoint = static_cast<UArrowComponent*>(SceneProps->ARTable->
			GetComponentByClass(UArrowComponent::StaticClass()))->GetComponentLocation();
	}
	const FVector location = StartingPoint;
	APuck* new_puck = static_cast<APuck*>(GetWorld()->SpawnActor(PawnClass, &location));
	if (!new_puck) { // if null most probably there is a previous one in the way
		TArray<APuck*, TInlineAllocator<32>> pucks;
		for (auto i = TActorIterator<APuck>(GetWorld()); i; ++i) {
			pucks.Add(*i);
		}
		FBox killVol = SceneProps->KillingVolume->GetBounds().GetBox();
		for (auto* i : pucks) {
			FBox puckVol = i->GetBoundingBox();
			if (killVol.Intersect(puckVol)) {
				GetWorld()->DestroyActor(i);
			}
		}
		new_puck = static_cast<APuck*>(GetWorld()->SpawnActor(PawnClass, &location));
	}
	make_sure(new_puck);

	new_puck->SetColor(GetPlayerState<AShufflPlayerState>()->Color);
	new_puck->ThrowMode = EPuckThrowMode::Simple;
	auto* gameState = Cast<AShufflGameState>(GetWorld()->GetGameState());
	new_puck->TurnId = gameState->GlobalTurnCounter;
	Possess(new_puck);

	SpinAmount = 0.f;
	SlingshotDir = FVector::ZeroVector;
	PlayMode = EPlayerCtrlMode::Setup;

	auto sys = UGameSubSys::Get(this);
	sys->PlayersChangeTurn.Broadcast(new_puck->Color);

	if (SceneProps->ARTable) {
		SwitchToDetailView();
	}
}

static FHitResult TouchStartHitResult;

void APlayerCtrl::ConsumeTouchOn(const ETouchIndex::Type fingerIndex, const FVector location)
{
	if (ARSetup) return;
	make_sure(GetPuck());

	if (fingerIndex != ETouchIndex::Touch1) {
		GetPuck()->ThrowMode = EPuckThrowMode::WithSpin;
		return;
	}

	if (PlayMode == EPlayerCtrlMode::Spin) {
		SpinStartPoint = FVector2D(location);
		return;
	}

	if (PlayMode != EPlayerCtrlMode::Setup) return;
	PlayMode = EPlayerCtrlMode::Throw; // depending on vertical dir this could turn into Slingshot

	ThrowStartTime = GetWorld()->GetRealTimeSeconds();
	ThrowStartPoint = FVector2D(location);
	TouchStartHitResult = ProjectScreenPoint(this, ThrowStartPoint);

	TouchHistory.Reset();
	TouchHistory.Add(ThrowStartPoint);
}

void APlayerCtrl::ConsumeTouchRepeat(const ETouchIndex::Type fingerIndex, const FVector location)
{
	if (ARSetup) return;
	make_sure(GetPuck());

	if (fingerIndex != ETouchIndex::Touch1) {
		GetPuck()->ThrowMode = EPuckThrowMode::WithSpin;
		return;
	}

	if (PlayMode == EPlayerCtrlMode::Spin) {
		float preview = CalculateSpin(location);
		GetPuck()->PreviewSpin(preview);
		return;
	}

	if (PlayMode == EPlayerCtrlMode::Observe) return;

	if (location.Y > ThrowStartPoint.Y) {
		PreviewSlingshot(location);
		PlayMode = EPlayerCtrlMode::Slingshot;
	} else {
		PlayMode = EPlayerCtrlMode::Throw;
	}

	TouchHistory.Add(FVector2D(location));
}

void APlayerCtrl::ConsumeTouchOff(const ETouchIndex::Type fingerIndex, const FVector location)
{
	if (ARSetup) return;
	make_sure(GetPuck());

	if (fingerIndex != ETouchIndex::Touch1) return;
	if (PlayMode == EPlayerCtrlMode::Observe) return;

	float deltaTime = GetWorld()->GetRealTimeSeconds() - ThrowStartTime;
	FVector2D gestureEndPoint = FVector2D(location);
	FVector2D gestureVector = gestureEndPoint - ThrowStartPoint;
	float distance = gestureVector.Size();
	float velocity = distance / deltaTime;
	float angle = FMath::Atan2(-gestureVector.X, -gestureVector.Y) + PI / 2.f;
		// need to rotate otherwise it's 0 when drawing straight throw (screen lenghtwise)

	if (PlayMode == EPlayerCtrlMode::Spin) {
		CalculateSpin(location);
		ExitSpinMode(velocity);
		return;
	}

	GetPuck()->HideSlingshotPreview();
	if (PlayMode == EPlayerCtrlMode::Slingshot 
			&& (deltaTime > .1f/*sec*/)
			&& !SlingshotDir.IsNearlyZero()) {
		DoSlingshot();
		PlayMode = EPlayerCtrlMode::Observe;
		return;
	}

	if (velocity < EscapeVelocity || 
			angle < 0.349f/*20 deg*/ || angle > 2.793f/*160 deg*/) {
		MovePuckOnTouchPosition(gestureEndPoint);
		PlayMode = EPlayerCtrlMode::Setup;
	} else {
		ThrowPuck(gestureVector, velocity);
		PlayMode = GetPuck()->ThrowMode == EPuckThrowMode::Simple ?
			EPlayerCtrlMode::Observe : EPlayerCtrlMode::Spin;
	}
}

void APlayerCtrl::ThrowPuck(FVector2D gestureVector, float velocity)
{
	gestureVector.Normalize();
	gestureVector *= velocity / (50.f - ThrowForceScaling);
		//TODO: HACK: inverted it so it makes more sense: low scale -> slow flick push

	auto X = FMath::Clamp(FMath::Abs(gestureVector.Y), 0.f, ThrowForceMax);
	auto Y = FMath::Clamp(gestureVector.X, -ThrowForceMax, ThrowForceMax);
	if (GetPuck()->ThrowMode == EPuckThrowMode::WithSpin) {
		Y = 0.f;
		GetWorldTimerManager().SetTimer(SpinTimer, this, &APlayerCtrl::EnterSpinMode,
			1.f / 60.f, false);
	}
	GetPuck()->ApplyThrow(FVector2D(X, Y));

	DebugPrint(TEXT("Vel %4.2f px/sec -- (%3.1f, %3.1f)"), velocity, X, Y);

	if (XMPP) {
		auto sys = UGameSubSys::Get(this);
		sys->XmppSend(FString::Printf(TEXT("/throw %i %i"), bit_cast(X), bit_cast(Y)));
	}
}

float APlayerCtrl::CalculateSpin(FVector touchLocation)
{
	// get the vector of: touch point - the original start point
	const auto P = FVector2D(SpinStartPoint.X - touchLocation.X, SpinStartPoint.Y);
	SpinAmount = FMath::Atan2(P.X, P.Y);	// reversed args due to coordinate frame of "X" 
									// axis being the length of the portrait screen
	return SpinAmount;
}

void APlayerCtrl::EnterSpinMode()
{
	PlayMode = EPlayerCtrlMode::Spin;

	GetPuck()->OnEnterSpin();
	GetWorldSettings()->SetTimeDilation(SpinSlowMoFactor);

	GetWorldTimerManager().SetTimer(SpinTimer, [this]() { ExitSpinMode(0.f); },
		SpinTime * SpinSlowMoFactor, false);
}

void APlayerCtrl::ExitSpinMode(float fingerVelocity)
{
	if (PlayMode != EPlayerCtrlMode::Spin) return; // can be triggered by timer or user, choose earliest
	PlayMode = EPlayerCtrlMode::Observe;

	GetPuck()->OnExitSpin();
	GetPuck()->ApplySpin(SpinAmount, fingerVelocity);
	DebugPrint(TEXT("Spin %3.1f"), SpinAmount);

	GetWorldSettings()->SetTimeDilation(1.f);
}

void APlayerCtrl::PreviewSlingshot(FVector touchLocation)
{
	auto h = ProjectScreenPoint(this, FVector2D(touchLocation));
	if (!h.bBlockingHit) return;

	auto d = TouchStartHitResult.ImpactPoint - h.ImpactPoint;
	d.Z = 0;
	auto a = FMath::Atan2(d.X, d.Y);
	if (a >= .785f && a <= 2.356f) // 45-135 degrees
	{
		SlingshotDir = d;
		auto color = FColor(127 + (int(SlingshotDir.Size() * SlingshotForceScaling) % 128), 64, 64);
		GetPuck()->ShowSlingshotPreview(SlingshotDir, color);
	}
}

void APlayerCtrl::DoSlingshot()
{
	auto f = FVector2D(SlingshotDir) * SlingshotForceScaling;
	auto len = f.Size();
	f.Normalize();
	f *= FMath::Min(len, ThrowForceMax);
	GetPuck()->ApplyThrow(f);
	DebugPrint(TEXT("Sling %3.1f %3.1f"), f.X, f.Y);

	if (XMPP) {
		auto sys = UGameSubSys::Get(this);
		sys->XmppSend(FString::Printf(TEXT("/throw %i %i"), bit_cast(f.X), bit_cast(f.Y)));
	}
}

void APlayerCtrl::MovePuckOnTouchPosition(FVector2D touchLocation)
{
	FHitResult hitResult = ProjectScreenPoint(this, touchLocation);
	if (hitResult.bBlockingHit)
	{
		// project the touched point onto the start line
		const auto &P = hitResult.ImpactPoint;
		const FVector A = StartingPoint - StartingLine / 2.f;
		const FVector AP = P - A;
		FVector AB = StartingLine;
		AB.Normalize();
		const float d = FVector::DotProduct(AP, AB);
		const FVector location = A + FVector(0, FMath::Clamp(d, 0.f, StartingLine.Y), 0);

		//DrawDebugSphere(GetWorld(), P, 5, 4, FColor::Green, false, 3);
		//DrawDebugSphere(GetWorld(), location, 5, 4, FColor::Red, false, 3);

		GetPuck()->MoveTo(location);

		if (XMPP) {
			auto sys = UGameSubSys::Get(this);
			sys->XmppSend(FString::Printf(TEXT("/move %i %i %i"),
				bit_cast(location.X), bit_cast(location.Y), bit_cast(location.Z)));
		}
	}
}

void APlayerCtrl::SwitchToDetailView()
{
	if (XMPP) {
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
		sys->XmppSend(out);
	}

	if (!SceneProps.IsValid()) return;
	SetViewTargetWithBlend(SceneProps->DetailViewCamera, 0.f);
}

void APlayerCtrl::SwitchToPlayView()
{
	SetViewTargetWithBlend(GetPuck(), 0.f);
}

void APlayerCtrl::HandleScoreCounting(EPuckColor winnerColor,
	int winnerTotalScore, int winnerRoundScore)
{
	SwitchToDetailView();

	auto sys = UGameSubSys::Get(this);
	sys->ScoreChanged.Broadcast(winnerColor, winnerTotalScore, winnerRoundScore);
}

void AAIPlayerCtrl::SetupInputComponent()
{
	APlayerController::SetupInputComponent();

	InputComponent->BindAction("Quit", IE_Released, this, &AAIPlayerCtrl::OnQuit);
	InputComponent->BindAction("Rethrow", IE_Released, this, &AAIPlayerCtrl::RequestNewThrow);
}

void AAIPlayerCtrl::HandleNewThrow()
{
	Super::HandleNewThrow();

	static FTimerHandle MoveTimer;
	GetWorldTimerManager().SetTimer(MoveTimer, 
		[this]() {
			FVector p = StartingPoint - StartingLine / 2.f;
			p.Y += FMath::RandRange(0.f, StartingLine.Y);
			GetPuck()->MoveTo(p);
		},
		1.f/*sec*/, false);

	static FTimerHandle ThrowTimer;
	GetWorldTimerManager().SetTimer(ThrowTimer,
		[this]() {
			float force = ThrowForceMax / FMath::RandRange(1.f, 5.f);
			GetPuck()->ApplyThrow(FVector2D(force, 0.f));
		},
		2.f/*sec*/, false);
}

void AXMPPPlayerCtrl::SetupInputComponent()
{
	APlayerController::SetupInputComponent();

	InputComponent->BindAction("Quit", IE_Released, this, &AXMPPPlayerCtrl::OnQuit);
	InputComponent->BindAction("Rethrow", IE_Released, this, &AXMPPPlayerCtrl::RequestNewThrow);
}

void AXMPPPlayerCtrl::OnReceiveChat(const FString& msg)
{
	TArray<FString> args;
	msg.ParseIntoArrayWS(args);
	make_sure(args.Num() > 1);

	if (args[0] == TEXT("/turn")) {
		auto* gameState = Cast<AShufflGameState>(GetWorld()->GetGameState());
		int turnId = FCString::Atoi(*args[1]);
		if (turnId == gameState->GlobalTurnCounter) {
			GetWorld()->GetAuthGameMode<AShufflCommonGameMode>()->NextTurn();
		} else {
			UE_LOG(LogShuffl, Error, TEXT("Received bad turn %i vs %i"),
				turnId, gameState->GlobalTurnCounter);
		}
		return;
	}

	if (args[0] == TEXT("/move")) {
		float X = bit_cast(FCString::Atoi(*args[1]));
		float Y = bit_cast(FCString::Atoi(*args[2]));
		float Z = bit_cast(FCString::Atoi(*args[3]));
		GetPuck()->MoveTo(FVector(X, Y, Z));
		return;
	}

	if (args[0] == TEXT("/throw")) {
		float X = bit_cast(FCString::Atoi(*args[1]));
		float Y = bit_cast(FCString::Atoi(*args[2]));
		GetPuck()->ApplyThrow(FVector2D(X, Y));
		return;
	}

	if (args[0] == TEXT("/debug")) {
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