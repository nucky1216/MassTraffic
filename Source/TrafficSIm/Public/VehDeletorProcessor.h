// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "TrafficSimSubsystem.h"
#include "VehDeletorProcessor.generated.h"

/**
 * 
 */
UCLASS()
class TRAFFICSIM_API UVehDeletorProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UVehDeletorProcessor();
	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
	UTrafficSimSubsystem* TrafficSimSubsystem = nullptr;
};
