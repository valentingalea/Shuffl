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
#include "Blueprint/UserWidget.h"
#include "LevelSequence/Public/LevelSequenceActor.h"

#include "Shuffl.h"
#include "GameSubSys.h"
#include "ScoringVolume.h"

//#define DEBUG_DRAW_TOUCH

ASceneProps::ASceneProps()
{
	auto root = CreateDefaultSubobject<USceneComponent>(TEXT("Dummy"));
	RootComponent = root;
}

void APlayerCtrl::BeginPlay()
{
	Super::BeginPlay();

	// inviolable contracts
	{
		make_sure(PawnClass);

		make_sure(HUDClass);
		auto widget = CreateWidget<UUserWidget>(this, HUDClass);
		widget->AddToViewport();
	}

	{
		auto iter = TActorIterator<ASceneProps>(GetWorld());
		make_sure(*iter);
		SceneProps = *iter;
		make_sure(SceneProps->DetailViewCamera);
		make_sure(SceneProps->StartingPoint);
		make_sure(SceneProps->Cinematic);
		make_sure(SceneProps->KillingVolume);

		StartingPoint = (SceneProps->StartingPoint)->GetActorLocation() - StartingLine / 2.f;

		SceneProps->Cinematic->SequencePlayer->Play();
		SetViewTarget(SceneProps->DetailViewCamera);
	}

	{
		auto sys = UGameSubSys::Get(this);
		make_sure(sys);
		sys->AwardPoints.BindUObject(this, &APlayerCtrl::OnAwardPoints);
		sys->PuckResting.BindUObject(this, &APlayerCtrl::OnPuckResting);
	}

	TouchHistory.Reserve(64);
	PlayMode = EPlayMode::Setup;
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

void APlayerCtrl::OnAwardPoints(int points)
{
	PlayerState->Score += points;

	auto sys = UGameSubSys::Get(this);
	sys->ScoreChanged.Broadcast(PlayerState->Score);
}

void APlayerCtrl::OnPuckResting(APuck *puck)
{
	if (PlayMode == EPlayMode::Setup) return; // player restarted on their own
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
	if (PlayMode == EPlayMode::Spin) return;

	const FVector location = StartingPoint + StartingLine / 2.f;
	APuck* new_puck = static_cast<APuck*>(GetWorld()->SpawnActor(PawnClass, &location));
	make_sure(new_puck);

	Possess(new_puck);
	PlayMode = EPlayMode::Setup;
	GetPuck()->ThrowMode = EThrowMode::SimpleThrow;
	SpinAmount = 0.f;
}

void APlayerCtrl::ConsumeTouchOn(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (FingerIndex != ETouchIndex::Touch1) {
		GetPuck()->ThrowMode = EThrowMode::ThrowAndSpin;
		return;
	}

	if (PlayMode == EPlayMode::Spin) {
		SpinStartPoint = FVector2D(Location);
		return;
	}

	if (PlayMode != EPlayMode::Setup) return;
	PlayMode = EPlayMode::Throw;

	ThrowStartTime = GetWorld()->GetRealTimeSeconds();
	ThrowStartPoint = FVector2D(Location);
	TouchHistory.Reset();
	TouchHistory.Add(ThrowStartPoint);
}

void APlayerCtrl::ConsumeTouchRepeat(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (FingerIndex != ETouchIndex::Touch1) {
		GetPuck()->ThrowMode = EThrowMode::ThrowAndSpin;
		return;
	}

	if (PlayMode == EPlayMode::Spin) {
		SpinAmount = (FVector2D(Location) - SpinStartPoint).Y;
		GetPuck()->PreviewSpin(SpinAmount);
		return;
	}

	if (PlayMode != EPlayMode::Throw) return;
	TouchHistory.Add(FVector2D(Location));
}

void APlayerCtrl::ConsumeTouchOff(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (FingerIndex != ETouchIndex::Touch1) return;

	if (PlayMode == EPlayMode::Spin) {
		SpinAmount = (FVector2D(Location) - SpinStartPoint).Y;
		ExitSpinMode();
		return;
	}
	if (PlayMode != EPlayMode::Throw) return;

	float deltaTime = GetWorld()->GetRealTimeSeconds() - ThrowStartTime;
	FVector2D gestureEndPoint = FVector2D(Location);
	FVector2D gestureVector = gestureEndPoint - ThrowStartPoint;
	float distance = gestureVector.Size();
	float velocity = distance / deltaTime;

	if (velocity < EscapeVelocity) {
		MovePuckOnTouchPosition(gestureEndPoint);
		PlayMode = EPlayMode::Setup;
	} else { 
		ThrowPuck(gestureVector, velocity);
		PlayMode = GetPuck()->ThrowMode == EThrowMode::SimpleThrow ?
			EPlayMode::Observe : EPlayMode::Spin;
	}
}

void APlayerCtrl::ThrowPuck(FVector2D gestureVector, float velocity)
{
	make_sure(GetPuck());

	gestureVector.Normalize();
	gestureVector *= velocity / ThrowForceScaling;

	auto X = FMath::Clamp(FMath::Abs(gestureVector.Y), 0.f, ThrowForceMax);
	auto Y = FMath::Clamp(gestureVector.X, -ThrowForceMax, ThrowForceMax);
	if (GetPuck()->ThrowMode == EThrowMode::ThrowAndSpin) {
		Y = 0.f;
		GetWorldTimerManager().SetTimer(SpinTimer, this, &APlayerCtrl::EnterSpinMode,
			1.f / 60.f, false);
	}
	GetPuck()->ApplyThrow(FVector2D(X, Y));

	static uint64 id = 0;
	GEngine->AddOnScreenDebugMessage(id++, 3/*sec*/, FColor::Green,
		FString::Printf(TEXT("Vel %4.2f px/sec -- (%3.1f, %3.1f)"), velocity, X, Y));
}

void APlayerCtrl::EnterSpinMode()
{
	PlayMode = EPlayMode::Spin;

	GetPuck()->OnEnterSpin();

	GetWorldSettings()->SetTimeDilation(SpinSlowMoFactor);

	GetWorldTimerManager().SetTimer(SpinTimer, this, &APlayerCtrl::ExitSpinMode,
		SpinTime * SpinSlowMoFactor, false);
}

void APlayerCtrl::ExitSpinMode()
{
	if (PlayMode != EPlayMode::Spin) return; // can be triggered by timer or user, choose earliest
	PlayMode = EPlayMode::Observe;

	GetPuck()->OnExitSpin();
	static uint64 id = 1000;
	GEngine->AddOnScreenDebugMessage(id++, 3/*sec*/, FColor::Green,
		FString::Printf(TEXT("spin %3.1f"), SpinAmount));
	GetPuck()->ApplySpin(SpinAmount / ThrowForceScaling);

	GetWorldSettings()->SetTimeDilation(1.f);
}

void APlayerCtrl::MovePuckOnTouchPosition(FVector2D ScreenSpaceLocation)
{
	FHitResult HitResult;
	GetHitResultAtScreenPosition(ScreenSpaceLocation, ECC_Visibility,
		false/*trace complex*/, HitResult);

	if (HitResult.bBlockingHit)
	{
		// project the touched point onto the start line
		const auto &P = HitResult.ImpactPoint;
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

//TODO: move elsewhere
void ACustomHUD::DrawHUD()
{
	Super::DrawHUD();

#ifdef DEBUG_DRAW_TOUCH
	const auto& pc = *static_cast<APlayerCtrl*>(this->GetOwningPlayerController());
	for (int i = 0; i < pc.TouchHistory.Num() - 1; ++i) {
		const FVector2D& a = pc.TouchHistory[i];
		const FVector2D& b = pc.TouchHistory[i + 1];
		constexpr float d = 3.f;
		DrawRect(FColor::Yellow, a.X - d / 2, a.Y - d / 2, d, d);
		DrawLine(a.X, a.Y, b.X, b.Y, FColor::Red);
	}
#endif
}