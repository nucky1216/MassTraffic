// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassSpawnerSubsystem.h"
#include "DynamicSpawnProcessor.generated.h"

/**
 * 
 */
UCLASS()
class TRAFFICSIM_API UDynamicSpawnProcessor : public UMassProcessor
{
	GENERATED_BODY()
	UDynamicSpawnProcessor();
	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	UTrafficSimSubsystem* TrafficSimSubsystem;
	UMassSpawnerSubsystem* MassSpawnerSubsystem;
	FMassEntityQuery EntityQuery;
	TArray<float> VehicleLenth;
	TArray<float> PrefixSum;
	
};
