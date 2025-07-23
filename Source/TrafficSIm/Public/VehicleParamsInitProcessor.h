// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneGraphData.h"
#include "VehicleParamsInitProcessor.generated.h"

/**
 * 
 */
UCLASS()
class TRAFFICSIM_API UVehicleParamsInitProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UVehicleParamsInitProcessor();
	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
	const UWorld* World ;
	const UZoneGraphSubsystem* ZoneGraphSubsystem ;
	const FZoneGraphStorage* ZoneGraphStorage;
	
};
