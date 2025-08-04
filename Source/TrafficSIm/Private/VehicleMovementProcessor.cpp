// Fill out your copyright notice in the Description page of Project Settings.


#include "VehicleMovementProcessor.h"
#include "MassVehicleMovementFragment.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "ZoneGraphQuery.h"

UVehicleMovementProcessor::UVehicleMovementProcessor():EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add("VehicleParamsInitProcessor");
	bAutoRegisterWithProcessingPhases = true;
}

void UVehicleMovementProcessor::Initialize(UObject& Owner)
{
	TrafficSimSubsystem = UWorld::GetSubsystem<UTrafficSimSubsystem>(GetWorld());
	if(!TrafficSimSubsystem)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("TrafficSimSubsystem is not initialized! Cannot execute VehicleMovementProcessor."));
		return;
	}
}

void UVehicleMovementProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);;
}

void UVehicleMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		TConstArrayView<FMassVehicleMovementFragment> VehicleMovementFragments = Context.GetFragmentView<FMassVehicleMovementFragment>();
		const TArrayView<FTransformFragment> TransformFragments = Context.GetMutableFragmentView<FTransformFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			const FMassVehicleMovementFragment& VehicleMovementFragment = VehicleMovementFragments[EntityIndex];
			FTransformFragment& TransformFragment = TransformFragments[EntityIndex];

			FZoneGraphLaneLocation CurLaneLocation = VehicleMovementFragment.LaneLocation;
			float TargetSpeed = VehicleMovementFragment.TargetSpeed;
			// Update the transform based on the vehicle movement fragment

			float TargetDist = CurLaneLocation.DistanceAlongLane+;

			
			FZoneGraphLaneLocation LaneLocation;
			UE::ZoneGraph::Query::CalculateLocationAlongLane(TrafficSimSubsystem->ZoneGraphStorage,LaneHandle,)
		}
		});
}
