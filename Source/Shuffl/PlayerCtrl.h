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
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// guarantees to return a valid one, thus enforcing a contract
	// TODO: will have to see about cases where we don't posses - like in Main Menu
	APuck& GetPuck();

	void ConsumeGesture(float);
	void SwitchToDetailView();
	void SwitchToPlayView();
};

inline APuck& APlayerCtrl::GetPuck()
{
	ensure(GetPawn() != nullptr && !GetPawn()->IsPendingKill());
	return *GetPawn<APuck>();
}