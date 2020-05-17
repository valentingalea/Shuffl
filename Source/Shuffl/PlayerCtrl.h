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
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

#include "Puck.h"

#include "PlayerCtrl.generated.h"

UCLASS(Config = Game, hidecategories = (Actor, "Actor Tick", Input, Game, "Mouse Interface", "Cheat Manager", LOD, Cooking))
class SHUFFL_API APlayerCtrl : public APlayerController
{
	GENERATED_BODY()

public:
	APlayerCtrl();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup)
	bool ARSetup = false;

	UPROPERTY(Config, VisibleDefaultsOnly, BlueprintReadOnly, Category = Setup)
	UClass* PawnClass;

	/** axis of allowed puck placement */
	UPROPERTY(Config, VisibleAnywhere, BlueprintReadOnly, Category = Throwing)
	FVector StartingLine;

	/**
	 * pixels per sec
	 * Galaxy S9 screen: average ~2000 for short(normal) thumb flick
	 * Logitech G305 mouse: about same but can easily double
	 */
	UPROPERTY(Config, VisibleAnywhere, BlueprintReadOnly, Category = Throwing)
	float EscapeVelocity;

	/** divider for the flick velocity */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = Throwing)
	float ThrowForceScaling;

	/** upper limit for the puck throw velocity after scaling has been applied */
	UPROPERTY(Config, VisibleAnywhere, BlueprintReadOnly, Category = Throwing)
	float ThrowForceMax;

	/** seconds */
	UPROPERTY(Config, VisibleAnywhere, BlueprintReadOnly, Category = Spin)
	float SpinTime;

	UPROPERTY(Config, VisibleAnywhere, BlueprintReadOnly, Category = Spin)
	float SpinSlowMoFactor;

	/** multiplier for the slingshot effect */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = Slingshot)
	float SlingshotForceScaling;

	UFUNCTION(BlueprintCallable)
	virtual void RequestNewThrow();

	UFUNCTION(BlueprintCallable)
	virtual void SwitchToDetailView();

	UFUNCTION(BlueprintCallable)
	virtual void SwitchToPlayView();

	UFUNCTION(BlueprintCallable)
	virtual void SetupBowling();

//
// GameMode interface (in true net play these would be RPC's)
//
	virtual void HandleNewThrow();
	virtual void HandleScoreCounting(EPuckColor winnerColor, int winnerTotalScore,
		int winnerRoundScore);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	void OnQuit();

	APuck* GetPuck();
	virtual FVector MovePuckOnTouchPosition(FVector2D);

	FVector StartingPoint = FVector::ZeroVector;
	TWeakObjectPtr<class ASceneProps> SceneProps;
	EPlayerCtrlMode PlayMode = EPlayerCtrlMode::Setup;

	void ConsumeTouchOn(const ETouchIndex::Type, const FVector);
	void ConsumeTouchRepeat(const ETouchIndex::Type, const FVector);
	void ConsumeTouchOff(const ETouchIndex::Type, const FVector);
#ifdef TOUCH_HISTORY
	TArray<FVector2D> TouchHistory;
#endif

//
// Flick mode
//
	virtual FVector2D ThrowPuck(FVector2D, float);
	float ThrowStartTime = 0.f;
	FVector2D ThrowStartPoint = FVector2D::ZeroVector;
	FVector2D SpinStartPoint = FVector2D::ZeroVector;

//
// Spin mode
//
	virtual void EnterSpinMode();
	virtual void ExitSpinMode(float);
	float CalculateSpin(FVector);
	float SpinAmount = 0.f;
	FTimerHandle SpinTimer;

//
// Slingshot mode
//
	FVector SlingshotDir = FVector::ZeroVector;
	void PreviewSlingshot(FVector);
	virtual FVector2D DoSlingshot();

	virtual void HandleTutorial(bool show = true);
};

inline APuck* APlayerCtrl::GetPuck()
{
	ensure(GetPawn() && !GetPawn()->IsPendingKill());
	return GetPawn<APuck>();
}

UCLASS()
class AAIPlayerCtrl : public APlayerCtrl
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;
	virtual void HandleNewThrow() override;
};

UCLASS()
class AXMPPPlayerCtrl : public APlayerCtrl
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	virtual void RequestNewThrow() override;
	virtual FVector MovePuckOnTouchPosition(FVector2D) override;
	virtual FVector2D ThrowPuck(FVector2D, float) override;
//TODO:	virtual void ExitSpinMode(float) override; 
	virtual FVector2D DoSlingshot() override;
	virtual void SetupBowling() override;

	virtual void SwitchToDetailView() override; // to add debug

	void SendSync(int);

private:
	struct FShufflXMPPService* XMPP;
};

UCLASS()
class AXMPPPlayerSpectator : public APlayerCtrl
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	virtual void RequestNewThrow() override;
	virtual void SwitchToDetailView() override; // to add debug
	virtual void HandleTutorial(bool /*show*/) override;

	void OnReceiveChat(FString);

private:
	struct FShufflXMPPService* XMPP;
};
