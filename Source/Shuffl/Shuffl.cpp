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

#include "Shuffl.h"
#include "Engine/Engine.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, Shuffl, "Shuffl" );

DEFINE_LOG_CATEGORY(LogShuffl);

void ShufflScreenLog(const FString& msg)
{
	static uint64 id = 0;
	GEngine->AddOnScreenDebugMessage(id++, 5/*sec*/, FColor::Green,
		msg, false/*newer on top*/, FVector2D(1.5f, 1.5f)/*scale*/);
}