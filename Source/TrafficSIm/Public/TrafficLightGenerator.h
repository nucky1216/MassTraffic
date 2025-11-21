// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "ZoneGraphData.h"
#include "TrafficLightGenerator.generated.h"

/**
 * 
 */
UCLASS()
class TRAFFICSIM_API UTrafficLightGenerator : public UMassEntitySpawnDataGeneratorBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	AZoneGraphData* ZoneGraphData;

	UPROPERTY(EditAnywhere,meta=(ToolTip="Intersection Tag Filter"))
	FZoneGraphTagFilter IntersectionTagFilter;

	UPROPERTY(EditAnywhere, meta = (ToolTip = "Phase Controlled Lanes"))
	UDataTable* DT_PhaseLanes;
	virtual void Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const override;

};
