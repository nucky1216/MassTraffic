// Fill out your copyright notice in the Description page of Project Settings.


#include "CollectLaneVehiclesProcessor.h"
#include "MassExecutionContext.h"

UCollectLaneVehiclesProcessor::UCollectLaneVehiclesProcessor():EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = FName(TEXT("TrafficSim"));
	ExecutionOrder.ExecuteAfter.Add(FName(TEXT("VehicleParamsInitProcessor")));
	ExecutionOrder.ExecuteBefore.Add(FName(TEXT("VehicleMovementProcessor")));
}

void UCollectLaneVehiclesProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadOnly);
}

void UCollectLaneVehiclesProcessor::Initialize(UObject& Owner)
{
	TrafficSimSubsystem = GetWorld()->GetSubsystem<UTrafficSimSubsystem>();
}

void UCollectLaneVehiclesProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UE_LOG(LogTrafficSim, Log, TEXT("CollectLaneVehiclesProcessor Executing..."));
	TrafficSimSubsystem->InitializeLaneToEntitiesMap();

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
	{
		const TConstArrayView<FMassVehicleMovementFragment> VehicleMovementFragments = Context.GetFragmentView<FMassVehicleMovementFragment>();
		int32 NumEntities = Context.GetNumEntities();
		for (int32 i=0;i< NumEntities;i++)
		{
			if (TrafficSimSubsystem)
			{
				TrafficSimSubsystem->CollectLaneVehicles(Context.GetEntity(i),VehicleMovementFragments[i]);
			}
		}
		});
}
