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

#include "UI.h"

#include "Blueprint/UserWidget.h"

#include "Shuffl.h"
#include "PlayerCtrl.h"

void ABoardPlayHUD::BeginPlay()
{
	Super::BeginPlay();

	make_sure(HUDClass);
	auto widget = CreateWidget<UUserWidget>(GetWorld(), HUDClass);
	widget->AddToViewport();
}

void ABoardPlayHUD::DrawHUD()
{
	Super::DrawHUD();

#ifdef DEBUG_DRAW_TOUCH
	const auto& pc = *static_cast<APlayerCtrl*>(this->GetOwningPlayerController());
	for (int i = 0; i < pc.TouchHistory.Num() - 1; ++i) {
		const FVector2D& a = pc.TouchHistory[i];
		const FVector2D& b = pc.TouchHistory[i + 1];
		constexpr float d = 3.f;
		DrawRect(FColor::Yellow, a.X - d / 2, a.Y - d / 2, d, d);
		DrawLine(a.X, a.Y, b.X, b.Y, FColor::Red);
	}
#endif
}

void ABoardPlayHUD::HandleTutorial()
{
	switch (TutorialStep)
	{
	case ETutorialStep::Start:
		TutorialStep = ETutorialStep::ShowcaseFlick;
		break;
	case ETutorialStep::ShowcaseFlick:
		TutorialStep = ETutorialStep::ShowcaseSlingshot;
		break;
	case ETutorialStep::ShowcaseSlingshot:
		TutorialStep = ETutorialStep::End;
		break;
	case ETutorialStep::End:
		return;
	}

	OnTutorialChange.Broadcast(TutorialStep);
}

void ABoardPlayHUD::HideTutorial()
{
	OnTutorialHide.Broadcast();
}