// Copyright (C) 2020 Valentin Galea

#pragma once

#include "CoreMinimal.h"
#include "Puck.h"
#include "PlayerCtrl.generated.h"

UCLASS()
class SHUFFL_API APlayerCtrl : public APlayerController
{
	GENERATED_BODY()

public:
	APlayerCtrl();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Setup)
	UClass* PawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Setup)
	TSubclassOf<class UUserWidget> HUDClass;

private:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	APuck* GetPuck();

	void ConsumeTouchOn(const ETouchIndex::Type FingerIndex, const FVector Location);
	void ConsumeTouchOff(const ETouchIndex::Type FingerIndex, const FVector Location);
	void ConsumeGesture(float);

	void SwitchToDetailView();
	void SwitchToPlayView();
	UPROPERTY() class ACameraActor* DetailViewCamera;

	void MovePuckBasedOnScreenSpace(FVector2D);
	void SetupNewThrow();
	FVector StartingPoint, StartingLine;
	float ThrowStartTime;
	FVector2D ThrowStartPoint;
};

inline APuck* APlayerCtrl::GetPuck()
{
	ensure(GetPawn() && !GetPawn()->IsPendingKill());
	return GetPawn<APuck>();
}