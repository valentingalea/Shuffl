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

#pragma optimize("", off)

auto HandshakeMsg = TEXT("Hello World!");

void UGameSubSys::XmppLogin(EPuckColor color)
{
	//TODO: handle edge cases

	XmppColor = color;
	XmppSelfId = color == EPuckColor::Red ? TEXT("p1") : TEXT("p2");
	XmppOtherId = color == EPuckColor::Red ? TEXT("p2") : TEXT("p1");
	FString Password = TEXT("123");

	FXmppServer XmppServer;
	XmppServer.bUseSSL = true;
	XmppServer.AppId = TEXT("Shuffl");
	XmppServer.ServerAddr = TEXT("34.65.28.84");
	XmppServer.Domain = TEXT("34.65.28.84");
	XmppServer.ServerPort = 5222;

	XmppConnection = FXmppModule::Get().CreateConnection(XmppSelfId);
	XmppConnection->SetServer(XmppServer);

	XmppConnection->OnLoginComplete().AddUObject(this, &UGameSubSys::XmppOnLogin);
	XmppConnection->Login(XmppSelfId, Password);
}

void UGameSubSys::XmppOnLogin(const FXmppUserJid& userJid, bool bWasSuccess, const FString& /*unused*/)
{
	UE_LOG(LogShuffl, Warning, TEXT("Login UserJid=%s Success=%s"),
		*userJid.GetFullPath(), bWasSuccess ? TEXT("true") : TEXT("false"));

	if (!bWasSuccess ||
		!XmppConnection.IsValid() ||
		XmppConnection->GetLoginStatus() != EXmppLoginStatus::LoggedIn)
	{
		UE_LOG(LogShuffl, Error, TEXT("Invalid login"));
		return;
	}

	//NOTE: can only set, not query as Epic didn't implement
	FXmppUserPresence Presence;
	Presence.bIsAvailable = true;
	Presence.Status = EXmppPresenceStatus::Online;
	XmppConnection->Presence()->UpdatePresence(Presence);

	XmppConnection->PrivateChat()->OnReceiveChat().AddUObject(this, &UGameSubSys::XmppOnChat);
}

void UGameSubSys::XmppLogout()
{
	make_sure(XmppConnection.IsValid());
	make_sure(XmppConnection->GetLoginStatus() == EXmppLoginStatus::LoggedIn);

	XmppConnection->OnLogoutComplete().AddLambda(
		[](const FXmppUserJid& userJid, bool bWasSuccess, const FString& /*unused*/) {
			UE_LOG(LogShuffl, Warning, TEXT("Logout UserJid=%s Success=%s"),
				*userJid.GetFullPath(), bWasSuccess ? TEXT("true") : TEXT("false"));
		}
	);

	XmppConnection->Logout();
	FXmppModule::Get().RemoveConnection(XmppConnection.ToSharedRef());
}

void UGameSubSys::XmppOnChat(const TSharedRef<IXmppConnection>& connection,
	const FXmppUserJid& fromJid,
	const TSharedRef<FXmppChatMessage>& chatMsg)
{
	const auto& msg = chatMsg->Body;
	UE_LOG(LogShuffl, Warning, TEXT("From: `%s` Msg: `%s`"), *fromJid.Id, *msg);
	//TODO: reject older messages than login
	if (msg == HandshakeMsg) {
		XmppHandshaken = true;
		EventHandshaken.Broadcast();
		return;
	}
}

void UGameSubSys::XmppHandshake()
{
	make_sure(XmppConnection.IsValid());
	//TODO: do 3 way TCP style to verify connection both ways

	FXmppUserJid RecipientId(XmppOtherId, XmppConnection->GetServer().Domain);
	XmppConnection->PrivateChat()->SendChat(RecipientId, HandshakeMsg);
}

void UGameSubSys::XmppStartGame()
{
	make_sure(XmppConnection.IsValid());
	if (!XmppHandshaken) return;
}

void UGameSubSys::Initialize(FSubsystemCollectionBase& Collection)
{
}