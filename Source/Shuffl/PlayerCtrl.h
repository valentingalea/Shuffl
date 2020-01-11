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
#include "GameFramework\HUD.h"

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
	class ALevelSequenceActor* Cinematic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerCtrl)
	class AKillingVolume* KillingVolume;
};

UCLASS()
class ACustomHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};

enum class EPlayMode
{
	Setup,
	Throw,
	Spin,
	Observe
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

	UFUNCTION(BlueprintCallable, Category = Throwing)
	void SetupNewThrow();

	UFUNCTION(BlueprintCallable, Category = Cameras)
	void SwitchToDetailView();

	UFUNCTION(BlueprintCallable, Category = Cameras)
	void SwitchToPlayView();

private:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	APuck* GetPuck();

	void ConsumeTouchOn(const ETouchIndex::Type FingerIndex, const FVector Location);
	void ConsumeTouchRepeat(const ETouchIndex::Type FingerIndex, const FVector Location);
	void ConsumeTouchOff(const ETouchIndex::Type FingerIndex, const FVector Location);
	
	void OnQuit();
	void OnAwardPoints(int);
	void OnPuckResting(APuck *);

	void MovePuckOnTouchPosition(FVector2D);
	void ThrowPuck(FVector2D, float);
	void EnterSpinMode();
	void ExitSpinMode();

	TWeakObjectPtr<ASceneProps> SceneProps;

	EPlayMode PlayMode;

	FVector StartingPoint;
	float ThrowStartTime;
	FVector2D ThrowStartPoint;
	FVector2D SpinStartPoint;
	float SpinAmount;
	FTimerHandle SpinTimer;
public: //TODO: needed for HUD access, encapsulate better
	TArray<FVector2D> TouchHistory;
};

inline APuck* APlayerCtrl::GetPuck()
{
	ensure(GetPawn() && !GetPawn()->IsPendingKill());
	return GetPawn<APuck>();
}