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

#include "Puck.h"

#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Kismet/GameplayStatics.h"

#include "Shuffl.h"
#include "GameModes.h"
#include "ScoringVolume.h"
#include "SceneProps.h"

APuck::APuck()
{
	//
	// NOTE: expects to be subclassed in Blueprint with the following hierarchy
	//
	// + Puck-component-class (root)
	// |   |
	// |   +-- Spring Arm component
	// |       |
	// |       +-- Camera component
	// |
	// +-- high detail meshes
	// |   |
	// |   + ...
	// |
	// +-- Arrow component (slingshot preview)
	//
	// the reason is done like this is to have one source of thruth and for simplicity

	// Docs quote: "Determines which PlayerController, if any, should automatically possess the pawn
	// when the level starts or when the pawn is spawned"
	// disable so the PC will spawn us, but this is a good shortcut for tests
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	PrimaryActorTick.bCanEverTick = true;
}

UStaticMeshComponent* APuck::GetPuck()
{
	return static_cast<UStaticMeshComponent*>(GetRootComponent());
}

void APuck::Tick(float deltaTime)
{
	if (State == EPuckState::Setup) return;

	Lifetime += deltaTime;
	FVector vel = GetPuck()->GetPhysicsLinearVelocity();

	if (State == EPuckState::Traveling_WithSpin) {
		const auto spin_angle = Impulse.Y;
		const auto spin_vel = Impulse.Z;

		if (SpinAccumulator <= FMath::Abs(spin_angle * Radius)) {
			auto s = spin_vel * deltaTime;
			SpinAccumulator += s;
			GetPuck()->AddImpulse(FVector(0, s * (spin_angle >= 0 ? 1 : -1), 0));
		}
	}

	if (State == EPuckState::Resting) {
		if (Lifetime > TimeResting) {
			OnResting();
		}
		return; 
	}

	if ((vel.SizeSquared() < .0001f) && Lifetime > ThresholdToResting) {
		State = EPuckState::Resting;

		bool scored = false;
		for (auto i = TActorIterator<AScoringVolume>(GetWorld()); i; ++i) {
			FBox scoreVol = i->GetBounds().GetBox();
			if (scoreVol.IsInside(GetActorLocation())) {
				scored = true;
				break;
			}
		}
		if (scored) {
			Lifetime = 0.f; // recyle to count resting time and go again
		} else {
			OnResting(); // change turn immediately
		}
	}
}

void APuck::OnResting()
{
	SetActorTickEnabled(false); // don't bother updating anymore once fully rested

	auto iter = TActorIterator<ASceneProps>(GetWorld());
	make_sure(*iter);
	auto *SceneProps = Cast<ASceneProps>(*iter);

	FBox killVol = SceneProps->KillingVolume->GetBounds().GetBox();
	FBox puckVol = GetBoundingBox();
	if (killVol.Intersect(puckVol)) {
		Destroy();
	} else {
		//HACK: send a sync for this puck (the game mode will choose which side
		//to send to, and redirect the request via the Player Ctrl)
		if (auto* gm = GetWorld()->GetAuthGameMode<AShufflXMPPGameMode>()) {
			gm->SyncPuck(TurnId);
		}
	}

	// abort if next turn already happened
	auto* gameState = Cast<AShufflGameState>(GetWorld()->GetGameState());
	if (TurnId < gameState->GlobalTurnCounter) return;
	// abort if we're showing the end of round results - next one needs to be manual
	if (gameState->GetMatchState() == MatchState::Round_End ||
		gameState->GetMatchState() == MatchState::Round_WinnerDeclared) return;

	// otherwise force next turn/throw
	GetWorld()->GetAuthGameMode<AShufflCommonGameMode>()->NextTurn();
}

void APuck::ApplyThrow(FVector2D force)
{
	Impulse = FVector(force.X, force.Y, 0);
	GetPuck()->AddImpulse(Impulse);
	State = EPuckState::Traveling;
}

void APuck::MoveTo(FVector location)
{
	GetPuck()->SetWorldLocationAndRotation(location, FRotator::ZeroRotator,
		false/*sweep*/, nullptr/*hit result*/,
		ETeleportType::TeleportPhysics); //NOTE: ResetPhysics causes problems
	State = EPuckState::Setup;
}

inline UStaticMeshComponent* FindCap(EPuckColor color, AActor* parent)
{
	auto name = color == EPuckColor::Red ? "Puck_Cap_Red" : "Puck_Cap_Blue";
	TInlineComponentArray<UStaticMeshComponent*> compList(parent);
	for (auto* c : compList) {
		if (c->GetName() == name) return c;
	}
	return nullptr;
}

void APuck::SetColor(EPuckColor newColor)
{
	auto prev = OppositePuckColor(newColor);
	if (auto mesh = FindCap(prev, this)) {
		mesh->SetHiddenInGame(true);
	}
	Color = newColor;
}

void APuck::PreviewSpin(float spinAmount)
{
	if (auto mesh = FindCap(Color, this)) {
		mesh->SetRelativeRotation(FRotator(0, FMath::RadiansToDegrees(spinAmount), 0));
	}
}

static float NormalArmLength;

void APuck::OnEnterSpin()
{
	auto* springArm = static_cast<USpringArmComponent*>(
		GetComponentByClass(USpringArmComponent::StaticClass()));
	make_sure(springArm);
	NormalArmLength = springArm->TargetArmLength;
	springArm->TargetArmLength = SpringArmLenghtOnZoom;
	springArm->bEnableCameraLag = false;
}

void APuck::OnExitSpin()
{
	auto* springArm = static_cast<USpringArmComponent*>(
		GetComponentByClass(USpringArmComponent::StaticClass()));
	make_sure(springArm);
	springArm->TargetArmLength = NormalArmLength;
	springArm->bEnableCameraLag = true;
}

void APuck::ApplySpin(float spinAmount, float fingerVelocity)
{
	//     .X set previously during the throw part
	Impulse.Y = spinAmount;
	Impulse.Z = fingerVelocity;
	State = EPuckState::Traveling_WithSpin;
	SpinAccumulator = 0.f;
	GetPuck()->AddAngularImpulseInRadians(FVector(0, 0, PI * Radius * 2.f * spinAmount));
}

void APuck::ShowSlingshotPreview(FVector rot, FColor color)
{
	auto* arrow = static_cast<UArrowComponent*>(GetComponentByClass(UArrowComponent::StaticClass()));
	make_sure(arrow);
	arrow->SetRelativeRotation(rot.Rotation());
	arrow->SetArrowColor(color);
	arrow->SetHiddenInGame(false);
}

void APuck::HideSlingshotPreview()
{
	auto* arrow = static_cast<UArrowComponent*>(GetComponentByClass(UArrowComponent::StaticClass()));
	make_sure(arrow);
	arrow->SetHiddenInGame(true);
}

FBox APuck::GetBoundingBox()
{
	FVector _min, _max;
	GetPuck()->GetLocalBounds(_min, _max);
	return FBox(_min, _max).MoveTo(GetPuck()->GetComponentLocation());
}