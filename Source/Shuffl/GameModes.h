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

#include "GameFramework/PlayerState.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameState.h"

#include "Def.h"

#include "GameModes.generated.h"

UCLASS()
class SHUFFL_API AShufflPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	//`float Score` is inherited

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = Shuffl)
	EPuckColor Color = EPuckColor::Red;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = Shuffl)
	uint8 PucksToPlay = ERound::PucksPerPlayer;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &) const override;
};

UCLASS()
class SHUFFL_API AShufflGameState : public AGameState
{
	GENERATED_BODY()

public:
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = Shuffl)
	int GlobalTurnCounter = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Shuffl)
	int ActiveLocalPlayerCtrlIndex = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &) const override;
};

UCLASS()
class SHUFFL_API AShufflCommonGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shuffl)
	bool AutoTurnStart = true;

	void SetupRound();
	void CalculateRoundScore(EPuckColor &, int &);

	virtual void NextTurn() { /*interface*/ }
};

UCLASS()
class SHUFFL_API AShufflPracticeGameMode : public AShufflCommonGameMode
{
	GENERATED_BODY()

public:
	virtual void HandleMatchHasStarted() override;

	virtual void NextTurn() override;
};

UCLASS()
class SHUFFL_API AShuffl2PlayersGameMode : public AShufflCommonGameMode
{
	GENERATED_BODY()

public:
	virtual void HandleMatchIsWaitingToStart() override;
	virtual void StartMatch() override;

	virtual void NextTurn() override;
};
