// Copyright (C) 2020 Valentin Galea


#include "PlayerCtrl.h"

#include "EngineMinimal.h"
#include "EngineUtils.h"
#include "Components/InputComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "DrawDebugHelpers.h"

#include "GameSubSys.h"

APlayerCtrl::APlayerCtrl()
{
	//TODO: extract this outside (.ini for ex)
	static ConstructorHelpers::FClassFinder<APawn> pawn(TEXT("/Game/BPC_Pawn"));
	PawnClass = pawn.Class;
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
		StartLine = FVector(0, 51.f, 0); //TODO: find a way to data drive this
		StartPoint = (*iter)->GetActorLocation() - StartLine / 2.f;
	}
	
	if (auto sys = UGameSubSys::Get(this)) {
		sys->AwardPoints.BindLambda([ps = PlayerState](int points) {
			if (!ps) return;
			ps->Score += points;

			GEngine->AddOnScreenDebugMessage(-1, 1/*sec*/, FColor::Green,
				FString::Printf(TEXT("Points: %i"), int(ps->Score)));
		});
	}

	Rethrow();
}

// Called to bind functionality to input
void APlayerCtrl::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindGesture(EKeys::Gesture_Flick, this, &APlayerCtrl::ConsumeGesture);

	constexpr auto dv = "DetailView";
	InputComponent->BindAction(dv, IE_Pressed, this, &APlayerCtrl::SwitchToDetailView);
	InputComponent->BindAction(dv, IE_Released, this, &APlayerCtrl::SwitchToPlayView);

	InputComponent->BindAction("Rethrow", IE_Released, this, &APlayerCtrl::Rethrow);

	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &APlayerCtrl::ConsumeTouchOn);
	InputComponent->BindTouch(EInputEvent::IE_Repeat, this, &APlayerCtrl::ConsumeTouchOn);
	InputComponent->BindTouch(EInputEvent::IE_Released, this, &APlayerCtrl::ConsumeTouchOff);
}

void APlayerCtrl::ConsumeGesture(float value)
{
	static uint64 id = 0;
	GEngine->AddOnScreenDebugMessage(id++, 1/*sec*/, FColor::Green,
		FString::Printf(TEXT("%f"), value));

	if ((value < 10.f) && ThrowSeq != EThrowSequence::Shoot) { //TODO: extract value
		return;
	}

	if (auto p = GetPuck()) {
		p->ApplyForce(FVector2D(value, 0));
	}
}

void APlayerCtrl::ConsumeTouchOn(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (ThrowSeq != EThrowSequence::LineUp) {
		return;
	}

	FVector2D ScreenSpaceLocation(Location);
	FHitResult HitResult;
	GetHitResultAtScreenPosition(ScreenSpaceLocation, CurrentClickTraceChannel, true, HitResult);
	
	if (HitResult.bBlockingHit)
	{
		// project the touched point onto the start line
		const auto &P = HitResult.ImpactPoint;
		const FVector AP = P - StartPoint;
		FVector AB = StartLine;
		AB.Normalize();
		const float d = FVector::DotProduct(AP, AB);
		const FVector location = StartPoint + FVector(0, d, 0);

		//TODO: wire this via the Pawn
		GetPuck()->FindComponentByClass<UStaticMeshComponent>()->SetWorldLocation(
			location, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void APlayerCtrl::ConsumeTouchOff(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	ThrowSeq = EThrowSequence::Shoot;
}

void APlayerCtrl::SwitchToDetailView()
{
	//INFO: some other variables to play with - but not needed here
	//bAutoManageActiveCameraTarget = false;
	//bFindCameraComponentWhenViewTarget = false;
	//GetPuck().FindComponentByClass<UCameraComponent>()->Deactivate();

	SetViewTargetWithBlend(DetailViewCamera, 0.5f); //TODO: collect these and data-drive
}

void APlayerCtrl::SwitchToPlayView()
{
	SetViewTargetWithBlend(GetPuck(), 0.25f);
}

void APlayerCtrl::Rethrow()
{
	const FVector location = StartPoint + StartLine / 2.f;
	APuck *new_puck = static_cast<APuck *>(GetWorld()->SpawnActor(PawnClass, &location));
	if (!new_puck) {
		ensure(0);
		return;
	}

	Possess(new_puck);

	ThrowSeq = EThrowSequence::LineUp;
}