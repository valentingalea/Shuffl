// Copyright (C) 2020 Valentin Galea


#include "PlayerCtrl.h"

#include "EngineMinimal.h"
#include "EngineUtils.h"
#include "Components/InputComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "DrawDebugHelpers.h"
#include "Math/UnrealMathUtility.h"
#include "Blueprint/UserWidget.h"

#include "GameSubSys.h"

APlayerCtrl::APlayerCtrl()
{
}

void APlayerCtrl::BeginPlay()
{
	Super::BeginPlay();

	// inviolable contracts
	//TODO: do something more radical if these contract are violated
	{
		ensure(PawnClass);
	}
	{
		//TODO: find a better way to reference this
		DetailViewCamera = [this]() -> ACameraActor* {
			for (TActorIterator<ACameraActor> i(GetWorld()); i; ++i) {
				if (i->ActorHasTag(TEXT("DetailViewCamera"))) {
					return *i;
				}
			}
			return nullptr;
		}();
		ensure(DetailViewCamera);
	}
	{
		auto iter = TActorIterator<APlayerStart>(GetWorld());
		ensure(*iter);
		StartingLine = FVector(0, 51.f, 0); //TODO: find a way to data drive this
		StartingPoint = (*iter)->GetActorLocation() - StartingLine / 2.f;
	}
	{
		ensure(HUDClass);
		auto widget = CreateWidget<UUserWidget>(this, HUDClass);
		widget->AddToViewport();
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

// Called to bind functionality to input
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
	
	//static uint64 id = 0;
	//GEngine->AddOnScreenDebugMessage(id++, 1/*sec*/, FColor::Green,
	//	FString::Printf(TEXT("(%f %f) vel %f"), gestureVector.X, gestureVector.Y, velocity));

	if (velocity < 100.f) {
		MovePuckBasedOnScreenSpace(gestureEndPoint);
	} else { 
		if (auto p = GetPuck()) {
#if 1
			gestureVector.Normalize();
			gestureVector *= velocity / 25.f;
#endif
			constexpr auto maxForce = 150.f;
			auto X = FMath::Clamp(FMath::Abs(gestureVector.Y), 0.f, maxForce);
			auto Y = FMath::Clamp(gestureVector.X, -maxForce, maxForce);
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
	SetViewTargetWithBlend(DetailViewCamera, 0.5f); //TODO: collect these and data-drive
}

void APlayerCtrl::SwitchToPlayView()
{
	SetViewTargetWithBlend(GetPuck(), 0.25f);
}
