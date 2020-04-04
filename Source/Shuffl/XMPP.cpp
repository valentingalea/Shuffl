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

#include "XMPP.h"
#include "Online/XMPP/Public/XmppModule.h"
#include "Kismet/GameplayStatics.h"

#include "Shuffl.h"
#include "GameSubSys.h"
#include "PlayerCtrl.h" //TODO: remove this dependency

void FShufflXMPPService::Login(EPuckColor color)
{
	Color = color;
	SelfId = color == EPuckColor::Red ? TEXT("p1") : TEXT("p2");
	OtherId = color == EPuckColor::Red ? TEXT("p2") : TEXT("p1");
	FString defaultPassword = TEXT("123");

	FXmppServer server;
	server.bUseSSL = true;
	server.AppId = TEXT("Shuffl");
	server.ServerAddr = TEXT("34.65.28.84");
	server.Domain = TEXT("34.65.28.84");
	server.ServerPort = 5222;

	Connection = FXmppModule::Get().CreateConnection(SelfId);
	Connection->SetServer(server);

	Connection->OnLoginComplete().AddRaw(this, &FShufflXMPPService::OnLogin);
	Connection->Login(SelfId, defaultPassword);
}

void FShufflXMPPService::OnLogin(const FXmppUserJid& userJid, bool bWasSuccess, const FString& /*unused*/)
{
	UE_LOG(LogShuffl, Warning, TEXT("XMPP Login UserJid=%s Success=%s"),
		*userJid.GetFullPath(), bWasSuccess ? TEXT("true") : TEXT("false"));

	if (!bWasSuccess ||
		!Connection.IsValid() ||
		Connection->GetLoginStatus() != EXmppLoginStatus::LoggedIn)
	{
		UE_LOG(LogShuffl, Error, TEXT("XMPP invalid login"));
		return;
	}

	//NOTE: can only set, not query as Epic didn't implement
	FXmppUserPresence presence;
	presence.bIsAvailable = true;
	presence.Status = EXmppPresenceStatus::Online;
	Connection->Presence()->UpdatePresence(presence);

	Connection->PrivateChat()->OnReceiveChat().AddRaw(this, &FShufflXMPPService::OnChat);
}

void FShufflXMPPService::Logout()
{
	make_sure(Connection.IsValid());
	make_sure(Connection->GetLoginStatus() == EXmppLoginStatus::LoggedIn);

	Connection->OnLogoutComplete().AddLambda(
		[](const FXmppUserJid& userJid, bool bWasSuccess, const FString& /*unused*/) {
		UE_LOG(LogShuffl, Warning, TEXT("Logout UserJid=%s Success=%s"),
			*userJid.GetFullPath(), bWasSuccess ? TEXT("true") : TEXT("false"));
	}
	);

	Connection->Logout();
	FXmppModule::Get().RemoveConnection(Connection.ToSharedRef());
}

void FShufflXMPPService::StartGame(const UObject* WorldContextObject)
{
	make_sure(Connection.IsValid());

	//TODO: expose this out to BP via event -- OnChat() as well
	UGameplayStatics::OpenLevel(WorldContextObject,
		TEXT("L_Main"),
		true,
		FString::Printf(TEXT("game=%s?%s"), XMPPGameMode::Name, XMPPGameMode::Host));

	SendChat(TEXT("/travel"));
	//TODO: gather these hardcoded things in vars
}

void FShufflXMPPService::OnChat(const TSharedRef<IXmppConnection>& connection,
	const FXmppUserJid& fromJid,
	const TSharedRef<FXmppChatMessage>& chatMsg)
{
	const auto& msg = chatMsg->Body;
	UE_LOG(LogShuffl, Warning, TEXT("From: `%s` Msg: `%s`"), *fromJid.Id, *msg);

	auto sys = UGameSubSys::Get(UGameSubSys::GetWorldContext());

	if (msg == TEXT("/travel")) {
		UGameplayStatics::OpenLevel(sys->GetWorldContext(),
			TEXT("L_Main"),
			true,
			FString::Printf(TEXT("game=%s?%s"), XMPPGameMode::Name, XMPPGameMode::Invited));
		return;
	}

	if (auto* pc = Cast<AXMPPPlayerCtrl>(sys->ShufflGetActivePlayerCtrl(sys->GetWorldContext()))) {
		pc->OnReceiveChat(msg);
	}
}

void FShufflXMPPService::SendChat(const FString& msg)
{
	make_sure(Connection.IsValid());

	FXmppUserJid to(OtherId, Connection->GetServer().Domain);
	Connection->PrivateChat()->SendChat(to, msg);
}
