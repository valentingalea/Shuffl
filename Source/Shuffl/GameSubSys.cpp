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
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/ConfigCacheIni.h"
#include "CoreGlobals.h"

#include "Shuffl.h"
#include "GameModes.h"

UGameSubSys* UGameSubSys::Get(const UObject* ContextObject)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(ContextObject, 
		EGetWorldErrorMode::ReturnNull)) {
		return UGameInstance::GetSubsystem<UGameSubSys>(World->GetGameInstance());
	}

	return nullptr;
}

UObject* UGameSubSys::GetWorldContext()
{
	TArray<APlayerController*> arr;
	GEngine->GetAllLocalPlayerControllers(arr);

	if ((arr.Num() > 0) && arr[0]) {
		return arr[0]->GetWorld();
	} else {
		return nullptr;
	}
}

class APlayerController* UGameSubSys::ShufflGetActivePlayerCtrl(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, 
		EGetWorldErrorMode::LogAndReturnNull))
	{
		const auto &order = World->GetAuthGameMode<AShufflCommonGameMode>()->PlayOrder;
		const auto *game_state = World->GetGameState<AShufflGameState>();
		if (auto *pc = order[game_state->ActiveLocalPlayerCtrlIndex % 2]) {
			return pc;
		} else {
			// called too early (from BP probably) just get a safe value
			return World->GetPlayerControllerIterator()->Get();
		}
	}
	return nullptr;
}

int UGameSubSys::ShufflGetWinningScore()
{
	return ERound::WinningScore;
}

FString UGameSubSys::ShufflGetVersion()
{
	FString version;
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),
		TEXT("ProjectVersion"), version, GGameIni);
	return version;
}

void UGameSubSys::Initialize(FSubsystemCollectionBase& Collection)
{
}

void UGameSubSys::Deinitialize()
{
	XMPP.Logout();
}

void UGameSubSys::XMPPLogin(const UObject* context, EPuckColor color)
{
	Get(context)->XMPP.Login(color);
}

void UGameSubSys::XMPPLogout(const UObject* context)
{
	Get(context)->XMPP.Logout();
}

void UGameSubSys::XMPPStartGame(const UObject* context)
{
	Get(context)->XMPP.StartGame(context);
}