// Fill out your copyright notice in the Description page of Project Settings.


#include "LookAtPawn.h"
#include "Camera\CameraComponent.h"

ALookAtPawn::ALookAtPawn()
{
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera0"));
	Camera->AttachTo(RootComponent);
}