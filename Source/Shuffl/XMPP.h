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
#include "Online/XMPP/Public/XmppConnection.h"
#include "Online/XMPP/Public/XmppChat.h"

#include "Def.h"

#include "XMPP.generated.h"

USTRUCT()
struct FShufflXMPPService
{
	GENERATED_BODY()

	void Login(EPuckColor Color);
	void OnLogin(const FXmppUserJid&, bool, const FString&);

	void Logout();

	void StartGame(const UObject*);

	void OnChat(const TSharedRef<IXmppConnection>&,
		const FXmppUserJid&,
		const TSharedRef<FXmppChatMessage>&);
	void SendChat(const FString&);

	TSharedPtr<class IXmppConnection> Connection;
	EPuckColor Color;
	FString SelfId;
	FString OtherId;
	FDateTime LoginTimestamp;
	int32 HandshakeSyn;
	int32 HandshakeAck;
};

namespace XMPPGameMode //TODO: move these to .ini config
{
	constexpr static auto Level = TEXT("L_Main");
	constexpr static auto Name_Host = TEXT("/Game/Play/GM_XMPP_Host.GM_XMPP_Host_C");
	constexpr static auto Name_Invitee = TEXT("/Game/Play/GM_XMPP_Invitee.GM_XMPP_Invitee_C");
	constexpr static auto Option_PuckColor = TEXT("puck");
	constexpr static auto Option_Host = TEXT("xmpphost");
	constexpr static auto Option_Invitee = TEXT("xmppinvitee");
};