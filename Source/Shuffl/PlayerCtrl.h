// Copyright (C) 2020 Valentin Galea
//
// This program is free software : you can redistribute it and /or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerCtrl)
	class AKillingVolume* KillingVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AR)
	AActor* ARTable;
};

UCLASS(hidecategories = (Actor, "Actor Tick", Input, Game, "Mouse Interface", "Cheat Manager", LOD, Cooking))
class SHUFFL_API APlayerCtrl : public APlayerController
{
	GENERATED_BODY()

public:
	APlayerCtrl();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup)
	bool ARSetup = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Setup)
	UClass* PawnClass;

	/** axis of allowed puck placement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throwing)
	FVector StartingLine = FVector(0, 51.f/*cm*/, 0);

	/**
	 * pixels per sec
	 * Galaxy S9 screen: average ~2000 for short(normal) thumb flick
	 * Logitech G305 mouse: about same but can easily double
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throwing)
	float EscapeVelocity = 100.f;

	/** divider for the flick velocity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throwing)
	float ThrowForceScaling = 25.f;

	/** upper limit for the puck throw velocity after scaling has been applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throwing)
	float ThrowForceMax = 150.f;

	/** seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Spin)
	float SpinTime = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Spin)
	float SpinSlowMoFactor = .1f;

	/** multiplier for the slingshot effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Slingshot)
	float SlingshotForceScaling = 5.f;

	UFUNCTION(BlueprintCallable)
	void RequestNewThrow();

	UFUNCTION(Unreliable, Server, WithValidation)
	void Server_NewThrow();

	UFUNCTION(Client, Reliable)
	void Client_NewThrow(); //TODO: possibly move more of this on server

	UFUNCTION(BlueprintCallable)
	void SwitchToDetailView();

	UFUNCTION(BlueprintCallable)
	void SwitchToPlayView();

	UFUNCTION(Client, Reliable)
	void Client_EnterScoreCounting(EPuckColor winnerColor, int winnerTotalScore, 
		int winnerRoundScore, int matchScore);

private:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	void OnQuit();

	APuck* GetPuck();
	void OnPuckResting(APuck*);
	void MovePuckOnTouchPosition(FVector2D);

	void ConsumeTouchOn(const ETouchIndex::Type, const FVector);
	void ConsumeTouchRepeat(const ETouchIndex::Type, const FVector);
	void ConsumeTouchOff(const ETouchIndex::Type, const FVector);
	TArray<FVector2D> TouchHistory;

	TWeakObjectPtr<ASceneProps> SceneProps;
	EPlayerCtrlMode PlayMode = EPlayerCtrlMode::Setup;

//
// Flick mode
//
	void ThrowPuck(FVector2D, float);
	FVector StartingPoint = FVector::ZeroVector;
	float ThrowStartTime = 0.f;
	FVector2D ThrowStartPoint = FVector2D::ZeroVector;
	FVector2D SpinStartPoint = FVector2D::ZeroVector;

//
// Spin mode
//
	void EnterSpinMode();
	void ExitSpinMode(float);
	float CalculateSpin(FVector);
	float SpinAmount = 0.f;
	FTimerHandle SpinTimer;

//
// Slingshot mode
//
	FVector SlingshotDir = FVector::ZeroVector;
	void PreviewSlingshot(FVector);
	void DoSlingshot();
};

inline APuck* APlayerCtrl::GetPuck()
{
	ensure(GetPawn() && !GetPawn()->IsPendingKill());
	return GetPawn<APuck>();
}