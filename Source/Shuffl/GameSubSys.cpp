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

#include "Online/XMPP/Public/XmppModule.h"
#include "Online/XMPP/Public/XmppConnection.h"
#include "Online/XMPP/Public/XmppPresence.h"

TSharedPtr<IXmppConnection> XmppConnection;

void UGameSubSys::XmppTest()
{
	FString UserId = TEXT("p2");
	FString Password = TEXT("123");
	FXmppServer XmppServer;
	XmppServer.bUseSSL = true;
	XmppServer.AppId = TEXT("Shuffl");
	XmppServer.ServerAddr = TEXT("34.65.28.84");
	XmppServer.Domain = TEXT("34.65.28.84");
	XmppServer.ServerPort = 5222;

	XmppConnection = FXmppModule::Get().CreateConnection(UserId);
	XmppConnection->SetServer(XmppServer);

	XmppConnection->OnLoginComplete().AddLambda(
		[&](const FXmppUserJid& userJid, bool bWasSuccess, const FString& error) {
			UE_LOG(LogShuffl, Warning, TEXT("Login UserJid=%s Success=%s Error=%s"),
				*userJid.GetFullPath(), bWasSuccess ? TEXT("true") : TEXT("false"), *error);
			if (!bWasSuccess) return;
		//	if (!XmppConnection.IsValid() ||
		//		XmppConnection->GetLoginStatus() != EXmppLoginStatus::LoggedIn) return;

			FXmppUserPresence Presence;
			Presence.bIsAvailable = true;
			Presence.Status = EXmppPresenceStatus::Online;
			Presence.StatusStr = (TEXT("Test rich presence status"));
			XmppConnection->Presence()->UpdatePresence(Presence);

			FXmppUserJid RecipientId(TEXT("p1"), XmppConnection->GetServer().Domain);
			XmppConnection->PrivateChat()->SendChat(RecipientId, TEXT("Hello World!"));

		//	XmppConnection->Logout();
		//	FXmppModule::Get().RemoveConnection(XmppConnection.ToSharedRef());
		}
	);

	XmppConnection->Login(UserId, Password);
}