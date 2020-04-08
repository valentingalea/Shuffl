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

#define make_sure(cond) \
	if (!(ensure(cond))) return

DECLARE_LOG_CATEGORY_EXTERN(LogShuffl, Warning, All);

void ShufflScreenLog(const FString&);

template <typename FmtType, typename... Types>
inline void ShufflLog(FmtType && fmt, Types &&... args)
{
	UE_LOG(LogShuffl, Warning, fmt, args...);
	ShufflScreenLog(FString::Printf(fmt, Forward<Types>(args)...));
}

template <typename FmtType, typename... Types>
inline void ShufflErr(FmtType && fmt, Types &&... args)
{
	UE_LOG(LogShuffl, Error, fmt, args...);
	ShufflScreenLog(FString::Printf(fmt, Forward<Types>(args)...));
}
