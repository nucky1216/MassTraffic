// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "MassSpawner.h"
#include "SpawnPointGenerator.generated.h"

/**
 * 
 */
UCLASS()
class TRAFFICSIM_API USpawnPointGenerator : public UMassEntitySpawnDataGeneratorBase
{
	GENERATED_BODY()
	
	virtual void Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const override;
	
	UPROPERTY(EditAnywhere)
	AZoneGraphData* ZoneGraphData;

	UPROPERTY(EditAnywhere)
	AMassSpawner* VehicleSpawner;

};
