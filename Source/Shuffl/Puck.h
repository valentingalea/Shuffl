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
#include "GameFramework/Pawn.h"
#include "Puck.generated.h"

enum class EPuckState
{
	Setup,
	Traveling,
	Resting
};

enum class EThrowMode
{
	SimpleThrow,
	ThrowAndSpin
};

UENUM(BlueprintType)
enum class EPuckColor : uint8
{
	Red,
	Blue
};

UCLASS()
class SHUFFL_API APuck : public APawn
{
	GENERATED_BODY()

public:
	APuck();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup)
	EPuckColor Color = EPuckColor::Red;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Setup)
	UClass* PuckMeshClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Setup)
	class UStaticMeshComponent* ThePuck;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup)
	class USpringArmComponent* TheSpringArm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup)
	class UCameraComponent* MainCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	float SpringArmLenghtOnZoom = 15.f;

	/** Time in sec to allow for until puck is checked for rest (no velocity) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Debug)
	float ThresholdToResting = 3.f;

	EThrowMode ThrowMode;

	FVector2D Velocity;

	void ApplyThrow(FVector2D);
	void MoveTo(FVector);

	void ApplySpin(float);
	void PreviewSpin(float);
	void OnEnterSpin();
	void OnExitSpin();
	
	FBox GetBoundingBox(); // will return just the puck component not the auxiliary elements

private:
	virtual void Tick(float DeltaTime) override;

	EPuckState State = EPuckState::Setup;
	float Lifetime = 0.f; // since it started Traveling (in sec)
};
