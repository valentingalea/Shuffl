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
#include "TimerManager.h"
#include "Components/InputComponent.h"
#include "Components/ArrowComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "DrawDebugHelpers.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/ScopeExit.h"

#include "Shuffl.h"
#include "GameSubSys.h"
#include "GameModes.h"
#include "ScoringVolume.h"
#include "SceneProps.h"
#include "UI.h"

//#define PRINT_THROW

inline FHitResult ProjectScreenPoint(const APlayerCtrl* ctrl, const FVector2D& location)
{
	FHitResult hitResult;
	ctrl->GetHitResultAtScreenPosition(location, ECC_Visibility,
		false/*trace complex*/, hitResult);
	return hitResult;
}

APlayerCtrl::APlayerCtrl()
{
	LoadConfig(APlayerCtrl::StaticClass());

	bShowMouseCursor = true;
}

void APlayerCtrl::BeginPlay()
{
	Super::BeginPlay();

	auto iter = TActorIterator<ASceneProps>(GetWorld());
	make_sure(*iter);
	SceneProps = *iter;
	make_sure(SceneProps->DetailViewCamera);
	make_sure(SceneProps->StartingPoint);
	make_sure(SceneProps->KillingVolume);

	make_sure(PawnClass);

	StartingPoint = SceneProps->StartingPoint->GetActorLocation();

#ifdef TOUCH_HISTORY
	TouchHistory.Reserve(64);
#endif
	PlayMode = EPlayerCtrlMode::Setup;
}

void APlayerCtrl::SetupInputComponent() //TODO: extract the action names into shared def
{
	Super::SetupInputComponent();

	InputComponent->BindAction("Rethrow", IE_Released, this, &APlayerCtrl::RequestNewThrow);

	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &APlayerCtrl::ConsumeTouchOn);
	InputComponent->BindTouch(EInputEvent::IE_Repeat, this, &APlayerCtrl::ConsumeTouchRepeat);
	InputComponent->BindTouch(EInputEvent::IE_Released, this, &APlayerCtrl::ConsumeTouchOff);
}

void APlayerCtrl::RequestNewThrow()
{
	if (PlayMode == EPlayerCtrlMode::Setup && !SceneProps->ARTable) return;

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

	HandleTutorial();
}

// "OK" to be statics are these are used only for the duration of touch events
static FHitResult TouchStartHitResult;
static bool ProhibitFurtherTouch = false; // to handle all exit-conditions at TouchOn

void APlayerCtrl::ConsumeTouchOn(const ETouchIndex::Type fingerIndex, const FVector location)
{
	ProhibitFurtherTouch = ARSetup || !GetPuck();
	if (ProhibitFurtherTouch) return;

	HandleTutorial(false/*show*/);

	if (fingerIndex != ETouchIndex::Touch1) {
		GetPuck()->ThrowMode = EPuckThrowMode::WithSpin;
		return;
	}

	ThrowStartTime = GetWorld()->GetRealTimeSeconds();

	if (PlayMode == EPlayerCtrlMode::Spin) {
		SpinStartPoint = FVector2D(location);
		return;
	}

	if (PlayMode != EPlayerCtrlMode::Setup) return;
	PlayMode = EPlayerCtrlMode::Throw; // depending on vertical dir this could turn into Slingshot

	ThrowStartPoint = FVector2D(location);
	TouchStartHitResult = ProjectScreenPoint(this, ThrowStartPoint);

#ifdef TOUCH_HISTORY
	TouchHistory.Reset();
	TouchHistory.Add(ThrowStartPoint);
#endif
}

void APlayerCtrl::ConsumeTouchRepeat(const ETouchIndex::Type fingerIndex, const FVector location)
{
	if (ProhibitFurtherTouch) return;

	// iOS seems to receive random Repeat touch events with no starting On
	if (ThrowStartTime == 0.f) return;

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

#ifdef TOUCH_HISTORY
	TouchHistory.Add(FVector2D(location));
#endif
}

void APlayerCtrl::ConsumeTouchOff(const ETouchIndex::Type fingerIndex, const FVector location)
{
	if (ProhibitFurtherTouch) return;

	if (fingerIndex != ETouchIndex::Touch1) return;
	if (PlayMode == EPlayerCtrlMode::Observe) return;

	// make sure a TouchOff is paired with a TouchOn, as UI presses can still trigger us
	if (ThrowStartTime == 0.f) return;

	float deltaTime = GetWorld()->GetRealTimeSeconds() - ThrowStartTime;
	FVector2D gestureEndPoint = FVector2D(location);
	FVector2D gestureVector = gestureEndPoint - ThrowStartPoint;
	float distance = gestureVector.Size();
	float velocity = distance / deltaTime;
	float angle = FMath::Atan2(-gestureVector.X, -gestureVector.Y) + PI / 2.f;
		// need to rotate otherwise it's 0 when drawing straight throw (screen lenghtwise)

	// reset for next cycle of checks
	ON_SCOPE_EXIT{ ThrowStartTime = 0.f; };

	if (PlayMode == EPlayerCtrlMode::Spin) {
		CalculateSpin(location);
		ExitSpinMode(velocity);
		return;
	}

	GetPuck()->HideSlingshotPreview();
	if (PlayMode == EPlayerCtrlMode::Slingshot 
			&& (deltaTime > .1f/*sec*/)
			&& (SlingshotDir.Size() > GetPuck()->Radius)){
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

FVector2D APlayerCtrl::ThrowPuck(FVector2D gestureVector, float velocity)
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
#ifdef PRINT_THROW
	ShufflLog(TEXT("Vel %4.2f px/sec -- (%3.1f, %3.1f)"), velocity, X, Y);
#endif

	return FVector2D(X, Y);
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
#ifdef PRINT_THROW
	ShufflLog(TEXT("Spin %3.1f"), SpinAmount);
#endif

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

FVector2D APlayerCtrl::DoSlingshot()
{
	auto f = FVector2D(SlingshotDir) * SlingshotForceScaling;
	auto len = f.Size();
	f.Normalize();
	f *= FMath::Min(len, ThrowForceMax);
	
	GetPuck()->ApplyThrow(f);
#ifdef PRINT_THROW
	ShufflLog(TEXT("Sling %3.1f %3.1f"), f.X, f.Y);
#endif

	return f;
}

FVector APlayerCtrl::MovePuckOnTouchPosition(FVector2D touchLocation)
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

		GetPuck()->MoveTo(location);
		return location;
	} else {
		return StartingPoint;
	}
}

void APlayerCtrl::SwitchToDetailView()
{
	if (!SceneProps.IsValid()) return;
	SetViewTargetWithBlend(SceneProps->DetailViewCamera, 0.f);
}

void APlayerCtrl::SwitchToPlayView()
{
	if (!GetPuck()) return;
	SetViewTargetWithBlend(GetPuck(), 0.f);
}

void APlayerCtrl::HandleScoreCounting(EPuckColor winnerColor,
	int winnerTotalScore, int winnerRoundScore)
{
	auto sys = UGameSubSys::Get(this);
	sys->ScoreChanged.Broadcast(winnerColor, winnerTotalScore, winnerRoundScore);
}

void APlayerCtrl::SetupBowling()
{
	make_sure(SceneProps->BowlingPinClass);

	// cleanup previous pins
	for (auto i = TActorIterator<AActor>(GetWorld()); i; ++i) {
		if ((*i)->ActorHasTag(TEXT("Bowling"))) {
			(*i)->Destroy();
		}
	}

	// rack'em up!
	FVector center = SceneProps->BowlingPinsCenter->GetActorLocation();
	const float space = SceneProps->BowlingPinsSpacing;
	FVector pins[] = {
		center + FVector(space, -space * 1.5f, 0), // 7
		center + FVector(space, -space / 2, 0), // 8
		center + FVector(space, +space / 2, 0), // 9
		center + FVector(space, +space * 1.5f, 0), // 10
		center + FVector(0, -space, 0), // 4
		center, // 5  
		center + FVector(0, +space, 0), // 6
		center + FVector(-space, -space / 2, 0), // 2
		center + FVector(-space, +space / 2, 0), // 3
		center + FVector(-2 * space, 0, 0), // 1
	};
	for (const auto& p : pins) {
		GetWorld()->SpawnActor(SceneProps->BowlingPinClass, &p);
	}
}

void APlayerCtrl::HandleTutorial(bool show)
{
	if (ARSetup || SceneProps->ARTable) return;
	if (GetWorld()->WorldType == EWorldType::PIE) return; // gets tiresome in editor

	make_sure(GetHUD());
	if (show) {
		GetHUD<ABoardPlayHUD>()->HandleTutorial();
	} else {
		GetHUD<ABoardPlayHUD>()->HideTutorial();
	}
}

//
// AI Controller experiment
//

void AAIPlayerCtrl::SetupInputComponent()
{
	APlayerController::SetupInputComponent();

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
