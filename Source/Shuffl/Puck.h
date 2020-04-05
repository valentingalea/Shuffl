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

#include "Def.h"

#include "Puck.generated.h"

UCLASS()
class SHUFFL_API APuck : public APawn
{
	GENERATED_BODY()

public:
	APuck();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Puck)
	EPuckColor Color = EPuckColor::Red;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Puck)
	float Radius = 2.5f; //cm

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

	EPuckThrowMode ThrowMode = EPuckThrowMode::Simple;
	int TurnId = 0;

	FVector Impulse = FVector::ZeroVector; // X: flick Y: spin-angle Z: spin-velocity
	void ApplyThrow(FVector2D);
	void MoveTo(FVector);
	void SetColor(EPuckColor);

	void ApplySpin(float, float);
	void PreviewSpin(float);
	void OnEnterSpin();
	void OnExitSpin();

	void ShowSlingshotPreview(FVector, FColor);
	void HideSlingshotPreview();
	
	FBox GetBoundingBox(); // will return just the puck component not the auxiliary elements

private:
	virtual void Tick(float) override;
	void OnResting();

	EPuckState State = EPuckState::Setup;
	float Lifetime = 0.f; // since it started Traveling (in sec)
	float SpinAccumulator = 0.f;
};

inline const TCHAR* PuckColorToString(EPuckColor color)
{
	return color == EPuckColor::Red ? TEXT("Red") : TEXT("Blue");
}

inline EPuckColor StringToPuckColor(const TCHAR* str)
{
	return (str && str[0] == 'R') ? EPuckColor::Red : EPuckColor::Blue;
}

inline EPuckColor OppositePuckColor(EPuckColor color)
{
	return color == EPuckColor::Red ? EPuckColor::Blue : EPuckColor::Red;
}