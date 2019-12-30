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

#include "ScoringVolume.h"
#include "GameSubSys.h"

AScoringVolume::AScoringVolume()
{
	OnActorBeginOverlap.AddDynamic(this, &AScoringVolume::OnOverlapBegin);
	OnActorEndOverlap.AddDynamic(this, &AScoringVolume::OnOverlapEnd);
}

void AScoringVolume::OnOverlapBegin(AActor* OverlappedActor, AActor* OtherActor)
{
	if (auto sys = UGameSubSys::Get(this)) {
		sys->AwardPoints.ExecuteIfBound(PointsAwarded);
	}
}

void AScoringVolume::OnOverlapEnd(AActor* OverlappedActor, AActor* OtherActor)
{
	if (auto sys = UGameSubSys::Get(this)) {
		sys->AwardPoints.ExecuteIfBound(-PointsAwarded);
	}
}