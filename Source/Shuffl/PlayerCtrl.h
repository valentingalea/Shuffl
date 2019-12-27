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

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UClass* PawnClass;
	APuck* GetPuck();

	void ConsumeTouchOn(const ETouchIndex::Type FingerIndex, const FVector Location);
	void ConsumeTouchOff(const ETouchIndex::Type FingerIndex, const FVector Location);
	void ConsumeGesture(float);

	void SwitchToDetailView();
	void SwitchToPlayView();
	UPROPERTY() class ACameraActor* DetailViewCamera;

	void Rethrow();
	FVector StartPoint, StartLine;
	enum struct EThrowSequence
	{
		LineUp,
		Shoot
	} ThrowSeq;
};

inline APuck* APlayerCtrl::GetPuck()
{
	ensure(GetPawn() && !GetPawn()->IsPendingKill());
	return GetPawn<APuck>();
}