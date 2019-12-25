// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Puck.generated.h"

UCLASS()
class SHUFFL_API APuck : public APawn
{
	GENERATED_BODY()

public:
	APuck();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Setup)
	class UStaticMeshComponent* ThePuck;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup)
	class USpringArmComponent* TheSpringArm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup)
	class UCameraComponent* TheCamera;
};
