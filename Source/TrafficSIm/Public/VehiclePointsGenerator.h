// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "ZoneGraphTypes.h"
#include "ZoneGraphData.h"
#include "VehiclePointsGenerator.generated.h"

/**
 * 
 */
UCLASS()
class TRAFFICSIM_API UVehiclePointsGenerator : public UMassEntitySpawnDataGeneratorBase
{
	GENERATED_BODY()
	

	UPROPERTY(EditAnywhere)
	FZoneGraphTagFilter LaneFilter;

	UPROPERTY(EditAnywhere)
	AZoneGraphData* ZoneGraphData;

	UPROPERTY(EditAnywhere)
	float MinGapBetweenSpaces;

	UPROPERTY(EditAnywhere)
	float MaxGapBetweenSpaces;

	UPROPERTY(EditAnywhere)
	FZoneGraphTag ConnectorTag;

	virtual void Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const override;
	//void BuildResultsFromRandomEntityTypes(TConstArrayView<FMassSpawnedEntityType> EntityTypes, TArray<FMassEntitySpawnDataGeneratorResult>& OutResults) const;
};
