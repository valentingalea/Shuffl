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

#include "GameFramework/GameMode.h"
#include "GameFramework/GameState.h"

#include "Def.h"

#include "GameModes.generated.h"

//
// the GameMode classes are BP only for now...
//

UCLASS()
class SHUFFL_API ACommonGameState : public AGameState
{
	GENERATED_BODY()

public:
	bool FirstTurn = true;
	EGameTurn InPlayTurn;
	EPuckColor InPlayPuckColor;

	//TODO: this needs to be on GameMode & RPC'ed from client
	virtual void NextTurn() {}
};

UCLASS()
class SHUFFL_API APracticeGameState : public ACommonGameState
{
	GENERATED_BODY()

public:
	virtual void NextTurn() override;
};

UCLASS()
class SHUFFL_API A2PlayersGameState : public ACommonGameState
{
	GENERATED_BODY()

public:
	virtual void NextTurn() override;
};
