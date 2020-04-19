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
#include "GameFramework\HUD.h"

#include "UI.generated.h"

//
// the main menu HUD is too simple so it's just in Blueprint
// Content/MainMenu/BP_MainMenuHUD
//

UENUM(BlueprintType)
enum class ETutorialStep : uint8
{
	Start,
	ShowcaseFlick,
	ShowcaseSlingshot,
	End
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEvent_TutorialChange, ETutorialStep, Step);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEvent_TutorialHide);

UCLASS()
class ABoardPlayHUD : public AHUD
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Setup)
	TSubclassOf<class UUserWidget> HUDClass;

	ETutorialStep TutorialStep = ETutorialStep::Start;

	UPROPERTY(BlueprintAssignable)
	FEvent_TutorialChange OnTutorialChange;

	UPROPERTY(BlueprintAssignable)
	FEvent_TutorialHide OnTutorialHide;

	void HandleTutorial();
	void HideTutorial();
	
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;
};
