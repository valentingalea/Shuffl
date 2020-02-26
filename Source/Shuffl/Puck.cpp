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
#include "PlayerCtrl.h"

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
	ThePuck->AddImpulse(FVector(force.X, force.Y, 0));
	State = EPuckState::Traveling;
}

inline UStaticMeshComponent* FindCap(EPuckColor color, AActor* parent) //TODO: move to blueprint OnBeginPlay
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
	ThePuck->SetWorldLocationAndRotation(location, FRotator::ZeroRotator,
		false/*sweep*/, nullptr/*hit result*/,
		ETeleportType::TeleportPhysics); //NOTE: ResetPhysics causes problems
	State = EPuckState::Setup;
}

FBox APuck::GetBoundingBox()
{
	FVector _min, _max;
	ThePuck->GetLocalBounds(_min, _max);
	return FBox(_min, _max).MoveTo(ThePuck->GetComponentLocation());
}

void APuck::Tick(float deltaTime)
{
	//TODO: decide where to tick: locally or server, currently both
	if (TurnId > 0 && Role == ROLE_SimulatedProxy && !Setup) {
		SetColor(Color);
		Cast<APlayerCtrl>(UGameplayStatics::GetPlayerController(GetWorld(), 0))->Client_ObserveThrow(TurnId);
		Setup = true;
		return;
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
		if (auto sys = UGameSubSys::Get(this)) {
			sys->PuckResting.Broadcast(this);
		}
	}
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