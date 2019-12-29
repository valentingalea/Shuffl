// Copyright (C) 2020 Valentin Galea

#pragma once

#include "CoreMinimal.h"
#include "GameFramework\Actor.h"
#include "GameFramework\PlayerController.h"

#include "Puck.h"

#include "PlayerCtrl.generated.h"

UCLASS(hidecategories = (Rendering, Replication, Collision, Input, Actor, LOD, Cooking))
class ASceneProps : public AActor
{
	GENERATED_BODY()

public:
	ASceneProps();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerCtrl)
	class APlayerStart* StartingPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerCtrl)
	class ACameraActor* DetailViewCamera;
};

UCLASS(hidecategories = (Actor, "Actor Tick", Input, Game, "Mouse Interface", "Cheat Manager", LOD, Cooking))
class SHUFFL_API APlayerCtrl : public APlayerController
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Setup)
	UClass* PawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Setup)
	TSubclassOf<class UUserWidget> HUDClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throwing)
	FVector StartingLine = FVector(0, 51.f, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throwing)
	float EscapeVelocity = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throwing)
	float ThrowForceScaling = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throwing)
	float ThrowForceMax = 150.f;

	UFUNCTION(BlueprintCallable, Category = Throwing)
	void MovePuckBasedOnScreenSpace(FVector2D Location);

	UFUNCTION(BlueprintCallable, Category = Throwing)
	void SetupNewThrow();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Cameras)
	float DetailViewCameraSwitchSpeed = .5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Cameras)
	float MainCameraSwitchSpeed = .25f;

	UFUNCTION(BlueprintCallable, Category = Cameras)
	void SwitchToDetailView();

	UFUNCTION(BlueprintCallable, Category = Cameras)
	void SwitchToPlayView();

private:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	TWeakObjectPtr<ASceneProps> SceneProps;

	APuck* GetPuck();

	void ConsumeTouchOn(const ETouchIndex::Type FingerIndex, const FVector Location);
	void ConsumeTouchOff(const ETouchIndex::Type FingerIndex, const FVector Location);

	FVector StartingPoint;
	float ThrowStartTime;
	FVector2D ThrowStartPoint;
};

inline APuck* APlayerCtrl::GetPuck()
{
	ensure(GetPawn() && !GetPawn()->IsPendingKill());
	return GetPawn<APuck>();
}