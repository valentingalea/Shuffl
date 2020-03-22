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

#include "GameSubSys.h"
#include "Engine.h"
#include "Engine\GameInstance.h"

#include "GameModes.h"

UGameSubSys* UGameSubSys::Get(const UObject* ContextObject)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(ContextObject, 
		EGetWorldErrorMode::ReturnNull)) {
		return UGameInstance::GetSubsystem<UGameSubSys>(World->GetGameInstance());
	}

	return nullptr;
}

class APlayerController* UGameSubSys::ShufflGetActivePlayerCtrl(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, 
		EGetWorldErrorMode::LogAndReturnNull))
	{
		auto it = World->GetPlayerControllerIterator();
		auto *game_state = World->GetGameState<AShufflGameState>();

		if (game_state->ActiveLocalPlayerCtrlIndex == 0) {
			return it->Get();
		} else {
			it++;
			return it->Get();
		}
	}
	return nullptr;
}

int UGameSubSys::ShufflGetWinningScore()
{
	return ERound::WinningScore;
}