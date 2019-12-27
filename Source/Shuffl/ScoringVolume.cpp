// Copyright (C) 2020 Valentin Galea

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