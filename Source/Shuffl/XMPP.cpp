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
#include "Engine/Engine.h"
#include "Online/XMPP/Public/XmppModule.h"
#include "Online/XMPP/Public/XmppMultiUserChat.h"
#include "Kismet/GameplayStatics.h"

#include "Shuffl.h"
#include "GameSubSys.h"
#include "Puck.h"

inline void TravelHost(const UObject* context, EPuckColor color)
{
	UGameplayStatics::OpenLevel(context, XMPPGameMode::Level, true/*absolute travel*/,
		FString::Printf(TEXT("game=%s?puck=%s?%s"),
			XMPPGameMode::Name_Host, PuckColorToString(color), XMPPGameMode::Option_Host));
}

inline void TravelInvitee(const UObject* context, EPuckColor color)
{
	UGameplayStatics::OpenLevel(context, XMPPGameMode::Level, true/*absolute travel*/,
		FString::Printf(TEXT("game=%s?puck=%s?%s"),
			XMPPGameMode::Name_Invitee, PuckColorToString(color), XMPPGameMode::Option_Invitee));
}

#define MUC /* multi user chat (i.e. via rooms)*/
#ifdef MUC
static TAutoConsoleVariable<FString> MUCRoom(TEXT("mucroom"), TEXT("theroom"), TEXT("XMPP tests"));
#endif

void FShufflXMPPService::Login(EPuckColor color)
{
	if (Connection.IsValid() &&
		(Connection->GetLoginStatus() == EXmppLoginStatus::ProcessingLogin ||
		Connection->GetLoginStatus() == EXmppLoginStatus::LoggedIn)) return;
	//TODO: the above doensn't protect against fast calls (button spam)

	Color = color;
	SelfId = color == EPuckColor::Red ? TEXT("p1") : TEXT("p2");
	OtherId = color == EPuckColor::Red ? TEXT("p2") : TEXT("p1");
	FString defaultPassword = TEXT("123");

	FXmppServer server;
	server.bUseSSL = true;
	server.AppId = TEXT("Shuffl");
#if 0
	server.ServerAddr = TEXT("127.0.0.1");
	server.Domain = TEXT("valentin-pc");
#else
	server.ServerAddr = TEXT("34.65.28.84");
	server.Domain = server.ServerAddr;
#endif
	server.ServerPort = 5222;

	Connection = FXmppModule::Get().CreateConnection(SelfId);
	Connection->SetServer(server);

	Connection->OnLoginComplete().AddRaw(this, &FShufflXMPPService::OnLogin);
	Connection->Login(SelfId, defaultPassword);
}

void FShufflXMPPService::OnLogin(const FXmppUserJid& userJid, bool bWasSuccess, const FString& /*unused*/)
{
	ShufflLog(TEXT("XMPP Login UserJid=%s Success=%s"),
		*userJid.GetFullPath(), bWasSuccess ? TEXT("true") : TEXT("false"));

	if (!bWasSuccess || !Connection.IsValid()) {
		ShufflErr(TEXT("XMPP invalid login"));
		return;
	}

	//NOTE: can only set, not query as Epic didn't implement
	FXmppUserPresence presence;
	presence.bIsAvailable = true;
	presence.Status = EXmppPresenceStatus::Online;
	Connection->Presence()->UpdatePresence(presence);

#ifdef MUC
	FXmppRoomConfig RoomConfig;
	RoomConfig.RoomName = MUCRoom.GetValueOnGameThread();
	RoomConfig.bIsPrivate = false;
	RoomConfig.bIsPersistent = false;

	if (Color == EPuckColor::Red) {
		Connection->MultiUserChat()->OnRoomCreated().AddLambda(
			[](const TSharedRef<IXmppConnection>& conn, bool success,
				const FXmppRoomId& roomId, const FString& err) {
				ShufflLog(TEXT("Room '%s' create Success=%s"),
					*roomId, success ? TEXT("true") : TEXT("false"));
		});
		Connection->MultiUserChat()->CreateRoom(RoomConfig.RoomName, SelfId, RoomConfig);
	} else {
		Connection->MultiUserChat()->OnJoinPublicRoom().AddLambda(
			[](const TSharedRef<IXmppConnection>& conn, bool success,
				const FXmppRoomId& roomId, const FString& err) {
				ShufflLog(TEXT("Room '%s' joined Success=%s"),
					*roomId, success ? TEXT("true") : TEXT("false"));
		});
		Connection->MultiUserChat()->JoinPublicRoom(RoomConfig.RoomName, SelfId);
	}

	Connection->MultiUserChat()->OnRoomChatReceived().AddLambda(
		[this](const TSharedRef<IXmppConnection>& conn, const FXmppRoomId&,
		const FXmppUserJid& from, const TSharedRef<FXmppChatMessage>& msg) {
			OnChat(conn, from, msg);
	});
#else
	Connection->PrivateChat()->OnReceiveChat().AddRaw(this, &FShufflXMPPService::OnChat);
#endif

	LoginTimestamp = FDateTime::UtcNow();
	if (auto sys = UGameSubSys::Get(UGameSubSys::GetWorldContext())) {
		sys->OnXMPPUserLoginChange.Broadcast(true);
	}
}

void FShufflXMPPService::Logout()
{
	if (!(Connection.IsValid() && 
		(Connection->GetLoginStatus() == EXmppLoginStatus::LoggedIn))) return;

	Connection->OnLogoutComplete().AddLambda(
		[&time = LoginTimestamp](const FXmppUserJid& userJid, bool bWasSuccess,
			const FString& /*unused*/) {
		ShufflLog(TEXT("Logout UserJid=%s Success=%s"),
			*userJid.GetFullPath(), bWasSuccess ? TEXT("true") : TEXT("false"));

		time = FDateTime(0);
		if (auto sys = UGameSubSys::Get(UGameSubSys::GetWorldContext())) {
			sys->OnXMPPUserLoginChange.Broadcast(false);
		}
	});

	Connection->Logout();
	FXmppModule::Get().RemoveConnection(Connection.ToSharedRef());
}

void FShufflXMPPService::StartGame(const UObject* context)
{
	make_sure(Connection.IsValid());

	HandshakeSyn = FMath::Rand();
	SendChat(FString::Printf(TEXT("/travel-syn %i"), HandshakeSyn));
}

void FShufflXMPPService::OnChat(const TSharedRef<IXmppConnection>& connection,
	const FXmppUserJid& fromJid,
	const TSharedRef<FXmppChatMessage>& chatMsg)
{
	make_sure(Connection.IsValid() && Connection->GetLoginStatus() == EXmppLoginStatus::LoggedIn);
	if (fromJid.Id == SelfId || fromJid.Resource == SelfId) return;

	const auto& msg = chatMsg->Body;
//	ShufflLog(TEXT("From: `%s` Msg: `%s`"), *fromJid.Id, *msg);
	if (chatMsg->Timestamp < LoginTimestamp) {
		ShufflErr(TEXT("got offline chat message!"));
		return; //TODO: lift this to support offline invites?
	}

	auto sys = UGameSubSys::Get(UGameSubSys::GetWorldContext());
	make_sure(sys);

	//TODO: find better way as this allocates too much => security vulnerability
	TArray<FString> args;
	msg.ParseIntoArrayWS(args);
	make_sure(args.Num() >= 1);
	const FString& cmd = args[0];

//
// TCP style handshake
// https://en.wikipedia.org/wiki/Transmission_Control_Protocol#Connection_establishment
//
	if (cmd == TEXT("/travel-syn") && args.Num() == 2) {
		HandshakeSyn = FMath::Rand();
		HandshakeAck = FCString::Atoi(*args[1]);
		SendChat(FString::Printf(TEXT("/travel-syn-ack %i %i"), HandshakeSyn, HandshakeAck + 1));
		return;
	}

	if (cmd == TEXT("/travel-syn-ack") && args.Num() == 3) {
		int32 otherSyn = FCString::Atoi(*args[1]);
		HandshakeAck = FCString::Atoi(*args[2]);
		if (HandshakeSyn != HandshakeAck - 1) {
			ShufflErr(TEXT("got faulty handshake syn!"));
			return;
		}
		SendChat(FString::Printf(TEXT("/travel-ack %i %i"), otherSyn + 1, HandshakeAck));
		return;
	}

	if (cmd == TEXT("/travel-ack") && args.Num() == 3) {
		int32 otherSyn = FCString::Atoi(*args[1]);
		int32 otherAck = FCString::Atoi(*args[2]);
		if (HandshakeSyn != otherSyn - 1) {
			ShufflErr(TEXT("got faulty handshake ack!"));
			return;
		}

		TravelInvitee(sys->GetWorldContext(), Color);
		SendChat(TEXT("/travel"));
		return;
	}

	if (cmd == TEXT("/travel")) {
		TravelHost(sys->GetWorldContext(), Color);
		return;
	}

//
// pass everything else to the Controllers
//
	sys->OnXMPPChatReceived.Broadcast(msg);
}

void FShufflXMPPService::SendChat(const FString& msg)
{
	make_sure(Connection.IsValid());

#ifdef MUC
	Connection->MultiUserChat()->SendChat(MUCRoom.GetValueOnGameThread(), msg, FString());
#else
	FXmppUserJid to(OtherId, Connection->GetServer().Domain);
	Connection->PrivateChat()->SendChat(to, msg);
#endif
}
