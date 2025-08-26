// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "TrafficLightSubsystem.h"
#include "TrafficLightInitProcessor.generated.h"

/**
 * 
 */

UCLASS()
class TRAFFICSIM_API UTrafficLightInitProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UTrafficLightInitProcessor();
protected:
	virtual void ConfigureQueries() override;
		virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery EntityQuery;
	UTrafficLightSubsystem* TrafficLightSubsystem;
	
};
