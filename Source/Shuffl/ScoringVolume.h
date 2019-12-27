// Copyright (C) 2020 Valentin Galea

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerVolume.h"
#include "ScoringVolume.generated.h"

UCLASS()
class SHUFFL_API AScoringVolume : public ATriggerVolume
{
	GENERATED_BODY()
	
public:
	AScoringVolume();

	UFUNCTION()
	void OnOverlapBegin(class AActor* OverlappedActor, class AActor* OtherActor);
	UFUNCTION()
	void OnOverlapEnd(class AActor* OverlappedActor, class AActor* OtherActor);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Scoring)
	int PointsAwarded = 1;
};
