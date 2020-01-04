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

// Sets default values
APuck::APuck()
{
	// this should be taken from a .ini config
	// official documentation warns against this practice https://docs.unrealengine.com/en-US/Resources/SampleGames/ARPG/BalancingBlueprintAndCPP/index.html
	// here it's used to make sure the the "look at" class is fixed and save recreating it
	static ConstructorHelpers::FClassFinder<UStaticMeshComponent> PuckParentClass(TEXT("/Game/BPC_Puck"));
	ensure(PuckParentClass.Class);

	// this is the best way to create a component from a custom UClass (Blueprint one in this case)
	// calling NewObject() forces to declare an 'outer' and that's a rabbit hole
	ThePuck = static_cast<UStaticMeshComponent *>(CreateDefaultSubobject("Puck", PuckParentClass.Class, PuckParentClass.Class,
		/*bIsRequired =*/ true, /*bTransient*/ false));
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

	PrimaryActorTick.bCanEverTick = false;
}

void APuck::ApplyForce(FVector2D force)
{
	ThePuck->AddImpulse(FVector(force.X, force.Y, 0));
}

void APuck::MoveTo(FVector location)
{
	ThePuck->SetWorldLocationAndRotation(location, FRotator::ZeroRotator,
		false/*sweep*/, nullptr/*hit result*/, ETeleportType::TeleportPhysics);
	//NOTE: ResetPhysics causes problems
}