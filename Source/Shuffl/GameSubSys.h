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
#include "Engine/Public/Subsystems/GameInstanceSubsystem.h"

#include "Def.h"
#include "XMPP.h"

#include "GameSubSys.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEvent_ScoreChanged,
							EPuckColor, WinnerColor,
							int, WinnerTotalScore,
							int, WinnerRoundScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEvent_PlayersChangeTurn,
							EPuckColor, NewColor);

DECLARE_MULTICAST_DELEGATE_OneParam(FEvent_XMPPChatReceived,
							FString);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEvent_XMPPUserLoginChange,
							bool, LoggedIn);


UCLASS()
class UGameSubSys : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
//
// Game Instance
//
	virtual void Initialize(FSubsystemCollectionBase&) override;
	virtual void Deinitialize() override;

	static UGameSubSys* Get(const UObject* ContextObject);
	static UObject* GetWorldContext();

//
// Utility general stuff
//
	UPROPERTY(BlueprintAssignable)
	FEvent_ScoreChanged ScoreChanged;

	UPROPERTY(BlueprintAssignable)
	FEvent_PlayersChangeTurn PlayersChangeTurn;

	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static class APlayerController* ShufflGetActivePlayerCtrl(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure)
	static int ShufflGetWinningScore();

	UFUNCTION(BlueprintPure)
	static FString ShufflGetVersion();

//
// XMPP multiplayer chat
//
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static bool XMPPGetLoggedIn(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void XMPPLogin(const UObject* WorldContextObject, EPuckColor Color);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void XMPPLogout(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void XMPPStartGame(const UObject* WorldContextObject);

	UPROPERTY()
	FShufflXMPPService XMPP;

	FEvent_XMPPChatReceived OnXMPPChatReceived;

	UPROPERTY(BlueprintAssignable)
	FEvent_XMPPUserLoginChange OnXMPPUserLoginChange;
};
