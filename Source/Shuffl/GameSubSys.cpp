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
#include "Online/XMPP/Public/XmppModule.h"
#include "Kismet/GameplayStatics.h"

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
	return 5; //ERound::WinningScore;
}

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
	FXmppUserPresence presence;
	presence.bIsAvailable = true;
	presence.Status = EXmppPresenceStatus::Online;
	XmppConnection->Presence()->UpdatePresence(presence);

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

#include "PlayerCtrl.h"

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
	if (msg == TEXT("/travel")) {
		UGameplayStatics::OpenLevel(this,
			TEXT("L_Main"), //TODO: expose this out
			true,
			TEXT("game=/Game/GM_XMPP_Invited.GM_XMPP_Invited_C"));
		return;
	}

	//cast to AXMMPPlayer and just call directly - TODO: later do event
	if (auto* pc = Cast<AXMPPPlayerCtrl>(ShufflGetActivePlayerCtrl(this))) {
		pc->OnReceiveChat(msg);
	}
}

void UGameSubSys::XmppSend(const FString& msg)
{
	make_sure(XmppConnection.IsValid());

	FXmppUserJid to(XmppOtherId, XmppConnection->GetServer().Domain);
	XmppConnection->PrivateChat()->SendChat(to, msg);
}

void UGameSubSys::XmppHandshake() //TODO: get rid of this and do the invite/accept idea
{
	make_sure(XmppConnection.IsValid());

	FXmppUserJid to(XmppOtherId, XmppConnection->GetServer().Domain);
	XmppConnection->PrivateChat()->SendChat(to, HandshakeMsg);
}

void UGameSubSys::XmppStartGame()
{
	make_sure(XmppConnection.IsValid());
//	if (!XmppHandshaken) return;

	UGameplayStatics::OpenLevel(this,
		TEXT("L_Main"), //TODO: expose this out
		true,
		TEXT("game=/Game/GM_XMPP_Sender.GM_XMPP_Sender_C"));

	XmppSend(TEXT("/travel"));
}

void UGameSubSys::Initialize(FSubsystemCollectionBase& Collection)
{
}

void UGameSubSys::Deinitialize()
{
	if (XmppConnection.IsValid() &&
		XmppConnection->GetLoginStatus() == EXmppLoginStatus::LoggedIn)
	{
		XmppLogout();
	}
}