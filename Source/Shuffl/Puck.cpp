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
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/ArrowComponent.h"
#include "Net/UnrealNetwork.h"

#include "Shuffl.h"
#include "GameSubSys.h"
#include "GameModes.h"
#include "ScoringVolume.h"

// Sets default values
APuck::APuck()
{
	//TODO: this should be taken from a .ini config
	// official documentation warns against this practice https://docs.unrealengine.com/en-US/Resources/SampleGames/ARPG/BalancingBlueprintAndCPP/index.html
	// here it's used to make sure the the "look at" class is fixed and save recreating it
	static ConstructorHelpers::FClassFinder<UStaticMeshComponent> puck(TEXT("/Game/BPC_Puck"));
	PuckMeshClass = puck.Class;
	make_sure(PuckMeshClass);

	// this is the best way to create a component from a custom UClass (Blueprint one in this case)
	// calling NewObject() forces to declare an 'outer' and that's a rabbit hole
	ThePuck = static_cast<UStaticMeshComponent *>(CreateDefaultSubobject("Puck", PuckMeshClass, 
		PuckMeshClass, /*bIsRequired =*/ true, /*bTransient*/ false));
	RootComponent = ThePuck;

	// create the class and give some decent defaults as it's hard to navigate and
	// see the overrides in a Blueprint'able subclass
	// this has the disadvantage that now we will multiple defaults :(
	TheSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	TheSpringArm->SetupAttachment(RootComponent);
	TheSpringArm->SetUsingAbsoluteRotation(true); // not affected by rotation of target
	TheSpringArm->SetRelativeRotation(FRotator(0.f, -20.f, 0.f));
	TheSpringArm->TargetArmLength = 300.f;
	TheSpringArm->bEnableCameraLag = true;
	TheSpringArm->bDoCollisionTest = false;

	MainCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	MainCamera->SetupAttachment(TheSpringArm, USpringArmComponent::SocketName);

	// Docs quote: "Determines which PlayerController, if any, should automatically possess the pawn
	// when the level starts or when the pawn is spawned"
	// disable so the PC will spawn us, but this is a good shortcut for tests
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	PrimaryActorTick.bCanEverTick = true;
}

void APuck::BeginPlay()
{
	Super::BeginPlay();
}

void APuck::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APuck, Color);
	DOREPLIFETIME(APuck, ThrowMode); // just for initial setup, the client side will keep modifying it
	DOREPLIFETIME(APuck, TurnId);
}

void APuck::ApplyThrow(FVector2D force)
{
	Velocity = force;
	RPC_ApplyThrow(force);
	State = EPuckState::Traveling;
}

void APuck::RPC_ApplyThrow_Implementation(FVector2D force)
{
	ThePuck->AddImpulse(FVector(force.X, force.Y, 0));
}

bool APuck::RPC_ApplyThrow_Validate(FVector2D force)
{
	return true;
}

inline UStaticMeshComponent* FindCap(EPuckColor color, AActor* parent)
{
	auto name = color == EPuckColor::Red ? "Puck_Cap_Red" : "Puck_Cap_Blue"; //TODO: better way of identify
	TInlineComponentArray<UStaticMeshComponent*> compList(parent);
	for (auto* c : compList) {
		if (c->GetName() == name) return c;
	}
	return nullptr;
}

void APuck::SetColor(EPuckColor newColor)
{
	auto prev = newColor == EPuckColor::Red ? EPuckColor::Blue : EPuckColor::Red;
	auto mesh = FindCap(prev, this);
	mesh->SetHiddenInGame(true);
//	Color = newColor;
}

void APuck::PreviewSpin(float spinAmount)
{
	auto mesh = FindCap(Color, this);
	mesh->SetRelativeRotation(FRotator(0, spinAmount, 0));
}

static float NormalArmLength;

void APuck::OnEnterSpin()
{
	NormalArmLength = TheSpringArm->TargetArmLength;
	TheSpringArm->TargetArmLength = SpringArmLenghtOnZoom;
	TheSpringArm->bEnableCameraLag = false;
}

void APuck::OnExitSpin()
{
	TheSpringArm->TargetArmLength = NormalArmLength;
	TheSpringArm->bEnableCameraLag = true;
}

void APuck::ApplySpin(float spinAmount)
{
	Velocity.Y = spinAmount; // X should have been set elsewhere
	State = EPuckState::Traveling;
}

void APuck::MoveTo(FVector location)
{
	RPC_MoveTo(location);
	State = EPuckState::Setup;
}

void APuck::RPC_MoveTo_Implementation(FVector location)
{
	ThePuck->SetWorldLocationAndRotation(location, FRotator::ZeroRotator,
		false/*sweep*/, nullptr/*hit result*/,
		ETeleportType::TeleportPhysics); //NOTE: ResetPhysics causes problems
}

bool APuck::RPC_MoveTo_Validate(FVector location)
{
	return true;
}

FBox APuck::GetBoundingBox()
{
	FVector _min, _max;
	ThePuck->GetLocalBounds(_min, _max);
	return FBox(_min, _max).MoveTo(ThePuck->GetComponentLocation());
}

void APuck::Tick(float deltaTime)
{
	if (TurnId > 0 && !Replicated) {
		SetColor(Color);

		auto* sys = UGameSubSys::Get(this);
		sys->PuckReplicated.Broadcast(TurnId);

		Replicated = true;
	}
	if (State != EPuckState::Traveling) return;

	Lifetime += deltaTime;
	FVector vel = ThePuck->GetPhysicsLinearVelocity();

	if (ThrowMode == EPuckThrowMode::WithSpin) {
		ThePuck->AddForce(FVector(0, Velocity.Y, 0));
		//TODO: find a way for the condition bellow to make sense 
	}

	if ((vel.SizeSquared() < .0001f) && Lifetime > ThresholdToResting) {
		State = EPuckState::Resting;
		RPC_OnPuckResting();
	}
}

void APuck::RPC_OnPuckResting_Implementation()
{
	auto iter = TActorIterator<ASceneProps>(GetWorld());
	make_sure(*iter);
	auto *SceneProps = Cast<ASceneProps>(*iter);

	FBox killVol = SceneProps->KillingVolume->GetBounds().GetBox();
	FBox puckVol = GetBoundingBox();
	if (killVol.Intersect(puckVol)) {
		Destroy();
	}

	// abort if next turn already happened
	auto* gameState = Cast<AShufflGameState>(GetWorld()->GetGameState());
	if (TurnId < gameState->GlobalTurnCounter) return;
	// otherwise force next turn/throw
	GetWorld()->GetAuthGameMode<AShufflCommonGameMode>()->NextTurn();
}

bool APuck::RPC_OnPuckResting_Validate()
{
	return true;
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