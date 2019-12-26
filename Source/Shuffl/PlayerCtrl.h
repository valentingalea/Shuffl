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

	APuck* GetPuck();

	void ConsumeGesture(float);
	void SwitchToDetailView();
	void SwitchToPlayView();
	void Rethrow();

	UPROPERTY()
	class ACameraActor* DetailViewCamera;

	UClass* PawnClass;
};

inline APuck* APlayerCtrl::GetPuck()
{
	ensure(GetPawn() && !GetPawn()->IsPendingKill());
	return GetPawn<APuck>();
}