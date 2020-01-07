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

UCLASS()
class SHUFFL_API APuck : public APawn
{
	GENERATED_BODY()

public:
	APuck();

	virtual void Tick(float DeltaTime) override;

	void ApplyForce(FVector2D);
	void MoveTo(FVector);
	FBox GetBoundingBox(); // will return just the puck component not the auxiliary elements

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Setup)
	class UStaticMeshComponent* ThePuck;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup)
	class USpringArmComponent* TheSpringArm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup)
	class UCameraComponent* MainCamera;

	/** Time in sec to allow for until puck is checked for rest (no velocity) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Setup)
	float ThresholdToResting = 3.f;

private:
	EPuckState State = EPuckState::Setup;
	float Lifetime = 0.f; // since it started Traveling (in sec)
};
