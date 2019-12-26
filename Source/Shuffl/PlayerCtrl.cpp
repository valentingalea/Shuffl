// Copyright (C) 2020 Valentin Galea


#include "PlayerCtrl.h"

#include "EngineMinimal.h"
#include "EngineUtils.h"
#include "Components/InputComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerStart.h"

APlayerCtrl::APlayerCtrl()
{
	static ConstructorHelpers::FClassFinder<APawn> pawn(TEXT("/Game/BPC_Pawn"));
	PawnClass = pawn.Class;
}

void APlayerCtrl::BeginPlay()
{
	Super::BeginPlay();

	// inviolable contracts
	ensure(PawnClass);
	DetailViewCamera = [this]() -> ACameraActor* {
		for (TActorIterator<ACameraActor> i(GetWorld()); i; ++i) {
			if (i->ActorHasTag(TEXT("DetailViewCamera"))) {
				return *i;
			}
		}
		return nullptr;
	}();
	ensure(DetailViewCamera);

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
}

void APlayerCtrl::ConsumeGesture(float value)
{
	static uint64 id = 0;
	GEngine->AddOnScreenDebugMessage(id++, 1/*sec*/, FColor::Green,
		FString::Printf(TEXT("%f"), value));

	if (auto p = GetPuck()) {
		p->ApplyForce(FVector2D(value, 0));
	}
}

void APlayerCtrl::SwitchToDetailView()
{
	// some other variables to play with - but not needed here
	//bAutoManageActiveCameraTarget = false;
	//bFindCameraComponentWhenViewTarget = false;
	//GetPuck().FindComponentByClass<UCameraComponent>()->Deactivate();

	SetViewTargetWithBlend(DetailViewCamera, 0.5f);
}

void APlayerCtrl::SwitchToPlayView()
{
	SetViewTargetWithBlend(GetPuck(), 0.25f);
}

void APlayerCtrl::Rethrow()
{
	auto iter = TActorIterator<APlayerStart>(GetWorld());
	APlayerStart *start = *iter;
	if (!start) {
		ensure(0);
		return;
	}
	
	const FVector location = start->GetActorLocation();
	APuck *new_puck = static_cast<APuck *>(GetWorld()->SpawnActor(PawnClass, &location));
	if (!new_puck) {
		ensure(0);
		return;
	}

	Possess(new_puck);
}