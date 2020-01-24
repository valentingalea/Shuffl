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

#include "Engine.h"
#include "EngineUtils.h"
#include "Components/InputComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "DrawDebugHelpers.h"
#include "Math/UnrealMathUtility.h"

#include "Shuffl.h"
#include "GameSubSys.h"
#include "ScoringVolume.h"

ASceneProps::ASceneProps()
{
	auto root = CreateDefaultSubobject<USceneComponent>(TEXT("Dummy"));
	RootComponent = root;
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
	}

	{
		auto iter = TActorIterator<ASceneProps>(GetWorld());
		make_sure(*iter);
		SceneProps = *iter;
		make_sure(SceneProps->DetailViewCamera);
		make_sure(SceneProps->StartingPoint);
		make_sure(SceneProps->KillingVolume);

		StartingPoint = (SceneProps->StartingPoint)->GetActorLocation() - StartingLine / 2.f;
	}

	{
		auto sys = UGameSubSys::Get(this);
		make_sure(sys);
		sys->PuckResting.BindUObject(this, &APlayerCtrl::OnPuckResting);
	}

	TouchHistory.Reserve(64);
	PlayMode = EPlayerCtrlMode::Setup;
	SetupNewThrow();
}

void APlayerCtrl::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindAction("Quit", IE_Released, this, &APlayerCtrl::OnQuit);

	constexpr auto dv = "DetailView";
	InputComponent->BindAction(dv, IE_Pressed, this, &APlayerCtrl::SwitchToDetailView);
	InputComponent->BindAction(dv, IE_Released, this, &APlayerCtrl::SwitchToPlayView);

	InputComponent->BindAction("Rethrow", IE_Released, this, &APlayerCtrl::SetupNewThrow);

	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &APlayerCtrl::ConsumeTouchOn);
	InputComponent->BindTouch(EInputEvent::IE_Repeat, this, &APlayerCtrl::ConsumeTouchRepeat);
	InputComponent->BindTouch(EInputEvent::IE_Released, this, &APlayerCtrl::ConsumeTouchOff);
}

void APlayerCtrl::OnQuit()
{
	FPlatformMisc::RequestExit(false);
}

void APlayerCtrl::OnPuckResting(APuck *puck)
{
	if (PlayMode == EPlayerCtrlMode::Setup) return; // player restarted on their own
	make_sure(puck);

	FBox killVol = SceneProps->KillingVolume->GetBounds().GetBox();
	FBox puckVol = puck->GetBoundingBox();
	if (killVol.Intersect(puckVol)) {
		puck->Destroy();
	}

	SetupNewThrow();
}

void APlayerCtrl::SetupNewThrow()
{
	if (PlayMode == EPlayerCtrlMode::Spin) return;

	const FVector location = StartingPoint + StartingLine / 2.f;
	APuck* new_puck = static_cast<APuck*>(GetWorld()->SpawnActor(PawnClass, &location));
	make_sure(new_puck);

	auto* gameState = Cast<ACommonGameState>(GetWorld()->GetGameState());
	make_sure(gameState);
	//TODO: needs to be RPC to GameMode
	gameState->NextTurn();

	Possess(new_puck);
	PlayMode = EPlayerCtrlMode::Setup;
	GetPuck()->SetColor(gameState->InPlayPuckColor);
	GetPuck()->ThrowMode = EPuckThrowMode::Simple;
	SpinAmount = 0.f;
}

void APlayerCtrl::ConsumeTouchOn(const ETouchIndex::Type fingerIndex, const FVector location)
{
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
	TouchHistory.Reset();
	TouchHistory.Add(ThrowStartPoint);
}

void APlayerCtrl::ConsumeTouchRepeat(const ETouchIndex::Type fingerIndex, const FVector location)
{
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
		FHitResult hitResult;
		GetHitResultAtScreenPosition(FVector2D(location), ECC_Visibility,
			false/*trace complex*/, hitResult);

		if (hitResult.bBlockingHit) {
			const auto I = hitResult.ImpactPoint;
			const auto P = GetPuck()->GetActorLocation();
			SlingshotDir = P - FVector(I.X, I.Y, P.Z);
			auto color = FColor(128 + (int(SlingshotDir.Size() * SlingshotForceScaling) % 128), 15, 15);
			GetPuck()->ShowSlingshotPreview(SlingshotDir, color);

			PlayMode = EPlayerCtrlMode::Slingshot;
		}
	} else {
		PlayMode = EPlayerCtrlMode::Throw;
	}

	TouchHistory.Add(FVector2D(location));
}

void APlayerCtrl::ConsumeTouchOff(const ETouchIndex::Type fingerIndex, const FVector location)
{
	if (fingerIndex != ETouchIndex::Touch1) return;

	if (PlayMode == EPlayerCtrlMode::Spin) {
		CalculateSpin(location);
		ExitSpinMode();
		return;
	}

	if (PlayMode == EPlayerCtrlMode::Observe) return;
	GetPuck()->HideSlingshotPreview();

	float deltaTime = GetWorld()->GetRealTimeSeconds() - ThrowStartTime;
	FVector2D gestureEndPoint = FVector2D(location);
	FVector2D gestureVector = gestureEndPoint - ThrowStartPoint;
	float distance = gestureVector.Size();
	float velocity = distance / deltaTime;

	const float limit = PlayMode == EPlayerCtrlMode::Slingshot ? 
		(EscapeVelocity / SlingshotForceScaling) : EscapeVelocity;

	if (velocity < limit) {
		MovePuckOnTouchPosition(gestureEndPoint);
		PlayMode = EPlayerCtrlMode::Setup;
	} else {
		if (PlayMode == EPlayerCtrlMode::Slingshot) {
			GetPuck()->ApplyThrow(FVector2D(SlingshotDir) * SlingshotForceScaling);
			PlayMode = EPlayerCtrlMode::Observe;
			return;
		}
		ThrowPuck(gestureVector, velocity);
		PlayMode = GetPuck()->ThrowMode == EPuckThrowMode::Simple ?
			EPlayerCtrlMode::Observe : EPlayerCtrlMode::Spin;
	}
}

void APlayerCtrl::ThrowPuck(FVector2D gestureVector, float velocity)
{
	make_sure(GetPuck());

	gestureVector.Normalize();
	gestureVector *= velocity / ThrowForceScaling;

	auto X = FMath::Clamp(FMath::Abs(gestureVector.Y), 0.f, ThrowForceMax);
	auto Y = FMath::Clamp(gestureVector.X, -ThrowForceMax, ThrowForceMax);
	if (GetPuck()->ThrowMode == EPuckThrowMode::WithSpin) {
		Y = 0.f;
		GetWorldTimerManager().SetTimer(SpinTimer, this, &APlayerCtrl::EnterSpinMode,
			1.f / 60.f, false);
	}
	GetPuck()->ApplyThrow(FVector2D(X, Y));

	static uint64 id = 0;
	GEngine->AddOnScreenDebugMessage(id++, 3/*sec*/, FColor::Green,
		FString::Printf(TEXT("Vel %4.2f px/sec -- (%3.1f, %3.1f)"), velocity, X, Y));
}

float APlayerCtrl::CalculateSpin(FVector touchLocation)
{
	SpinAmount = (SpinStartPoint - FVector2D(touchLocation)).X / ThrowForceScaling;
	auto preview = SpinAmount * 2.f;
	//SpinAmount /= ThrowForceScaling;
	return preview;
}

void APlayerCtrl::EnterSpinMode()
{
	PlayMode = EPlayerCtrlMode::Spin;

	GetPuck()->OnEnterSpin();
	GetWorldSettings()->SetTimeDilation(SpinSlowMoFactor);

	GetWorldTimerManager().SetTimer(SpinTimer, this, &APlayerCtrl::ExitSpinMode,
		SpinTime * SpinSlowMoFactor, false);
}

void APlayerCtrl::ExitSpinMode()
{
	if (PlayMode != EPlayerCtrlMode::Spin) return; // can be triggered by timer or user, choose earliest
	PlayMode = EPlayerCtrlMode::Observe;

	GetPuck()->OnExitSpin();
	GetPuck()->ApplySpin(SpinAmount);

	GetWorldSettings()->SetTimeDilation(1.f);

	static uint64 id = 1000;
	GEngine->AddOnScreenDebugMessage(id++, 3/*sec*/, FColor::Green,
		FString::Printf(TEXT("spin %3.1f"), SpinAmount));
}

void APlayerCtrl::MovePuckOnTouchPosition(FVector2D screenSpaceLocation)
{
	FHitResult hitResult;
	GetHitResultAtScreenPosition(screenSpaceLocation, ECC_Visibility,
		false/*trace complex*/, hitResult);

	if (hitResult.bBlockingHit)
	{
		// project the touched point onto the start line
		const auto &P = hitResult.ImpactPoint;
		//DrawDebugSphere(GetWorld(), P, 5, 4, FColor::Green, false, 3);
		const FVector AP = P - StartingPoint;
		FVector AB = StartingLine;
		AB.Normalize();
		const float d = FVector::DotProduct(AP, AB);
		const FVector location = StartingPoint + FVector(0, d, 0);
		//DrawDebugSphere(GetWorld(), location, 5, 4, FColor::Red, false, 3);

		if (auto p = GetPuck()) {
			p->MoveTo(location);
		}
	}
}

void APlayerCtrl::SwitchToDetailView()
{
	if (!SceneProps.IsValid()) return;
	SetViewTargetWithBlend(SceneProps->DetailViewCamera, .5f);
}

void APlayerCtrl::SwitchToPlayView()
{
	SetViewTargetWithBlend(GetPuck(), .25f);
}
