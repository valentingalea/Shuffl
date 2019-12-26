// Copyright (C) 2020 Valentin Galea


#include "PlayerCtrl.h"

#include "EngineMinimal.h"
#include "Components/InputComponent.h"
#include "Camera/CameraActor.h"

void APlayerCtrl::BeginPlay()
{
	Super::BeginPlay();

	// inviolable contracts
	ensure(GetPawn() != nullptr);
}

// Called to bind functionality to input
void APlayerCtrl::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindGesture(EKeys::Gesture_Flick, this, &APlayerCtrl::ConsumeGesture);

	constexpr auto DetailViewMapping = "DetailView";
	InputComponent->BindAction(DetailViewMapping, IE_Pressed, this, &APlayerCtrl::SwitchToDetailView);
	InputComponent->BindAction(DetailViewMapping, IE_Released, this, &APlayerCtrl::SwitchToPlayView);
}

void APlayerCtrl::ConsumeGesture(float value)
{
	static uint64 id = 0;
	GEngine->AddOnScreenDebugMessage(id++, 1/*sec*/, FColor::Green,
		FString::Printf(TEXT("%f"), value));

	GetPuck().ApplyForce(FVector2D(value, 0));
}

void APlayerCtrl::SwitchToDetailView()
{
	// some other variables to play with - but not needed here
	//bAutoManageActiveCameraTarget = false;
	//bFindCameraComponentWhenViewTarget = false;
	//GetPuck().FindComponentByClass<UCameraComponent>()->Deactivate();

	SetViewTargetWithBlend(GetPuck().DetailViewCamera, 0.5f);
}

void APlayerCtrl::SwitchToPlayView()
{
	SetViewTargetWithBlend(&GetPuck(), 0.25f);
}