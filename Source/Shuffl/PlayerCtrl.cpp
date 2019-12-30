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

#include "GameSubSys.h"

ASceneProps::ASceneProps()
{
	auto root = CreateDefaultSubobject<USceneComponent>(TEXT("Dummy"));
	RootComponent = root;
}

void APlayerCtrl::BeginPlay()
{
	Super::BeginPlay();

	// inviolable contracts
	//TODO: do something more radical if these contract are violated
	{
		ensure(PawnClass);

		ensure(HUDClass);
		auto widget = CreateWidget<UUserWidget>(this, HUDClass);
		widget->AddToViewport();
	}

	{
		auto iter = TActorIterator<ASceneProps>(GetWorld());
		ensure(*iter);
		SceneProps = *iter;
		ensure(SceneProps->DetailViewCamera);
		ensure(SceneProps->StartingPoint);

		StartingPoint = (SceneProps->StartingPoint)->GetActorLocation() - StartingLine / 2.f;
	}

	if (auto sys = UGameSubSys::Get(this)) {
		sys->AwardPoints.BindLambda([ps = PlayerState, sys](int points) {
			if (!ps) return;
			ps->Score += points;

			sys->ScoreChanged.Broadcast(ps->Score);
		});
	}

	SetupNewThrow();
}

void APlayerCtrl::SetupInputComponent()
{
	Super::SetupInputComponent();

	constexpr auto dv = "DetailView";
	InputComponent->BindAction(dv, IE_Pressed, this, &APlayerCtrl::SwitchToDetailView);
	InputComponent->BindAction(dv, IE_Released, this, &APlayerCtrl::SwitchToPlayView);

	InputComponent->BindAction("Rethrow", IE_Released, this, &APlayerCtrl::SetupNewThrow);

	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &APlayerCtrl::ConsumeTouchOn);
	InputComponent->BindTouch(EInputEvent::IE_Released, this, &APlayerCtrl::ConsumeTouchOff);
}

void APlayerCtrl::ConsumeTouchOn(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	ThrowStartTime = GetWorld()->GetRealTimeSeconds();
	ThrowStartPoint = FVector2D(Location);
}

void APlayerCtrl::ConsumeTouchOff(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	float deltaTime = GetWorld()->GetRealTimeSeconds() - ThrowStartTime;
	FVector2D gestureEndPoint = FVector2D(Location);
	FVector2D gestureVector = gestureEndPoint - ThrowStartPoint;
	float distance = gestureVector.Size();
	float velocity = distance / deltaTime;
	
	static uint64 id = 0;
	GEngine->AddOnScreenDebugMessage(id++, 1/*sec*/, FColor::Green,
		FString::Printf(TEXT("(%f %f) vel %f"), gestureVector.X, gestureVector.Y, velocity));

	if (velocity < EscapeVelocity) {
		MovePuckBasedOnScreenSpace(gestureEndPoint);
	} else { 
		if (auto p = GetPuck()) {
			gestureVector.Normalize();
			gestureVector *= velocity / ThrowForceScaling;

			auto X = FMath::Clamp(FMath::Abs(gestureVector.Y), 0.f, ThrowForceMax);
			auto Y = FMath::Clamp(gestureVector.X, -ThrowForceMax, ThrowForceMax);
			p->ApplyForce(FVector2D(X, Y));
		}
	}
}

void APlayerCtrl::MovePuckBasedOnScreenSpace(FVector2D ScreenSpaceLocation)
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

void APlayerCtrl::SetupNewThrow()
{
	const FVector location = StartingPoint + StartingLine / 2.f;
	APuck *new_puck = static_cast<APuck *>(GetWorld()->SpawnActor(PawnClass, &location));
	if (!new_puck) {
		ensure(0);
		return;
	}
	
	Possess(new_puck);
}

void APlayerCtrl::SwitchToDetailView()
{
	if (!SceneProps.IsValid()) return;
	SetViewTargetWithBlend(SceneProps->DetailViewCamera, DetailViewCameraSwitchSpeed);
}

void APlayerCtrl::SwitchToPlayView()
{
	SetViewTargetWithBlend(GetPuck(), MainCameraSwitchSpeed);
}
