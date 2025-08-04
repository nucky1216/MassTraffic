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
	TArray<FMassEntityHandle> DestroyedEntities;

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		TArrayView<FMassVehicleMovementFragment> VehicleMovementFragments = Context.GetMutableFragmentView<FMassVehicleMovementFragment>();
		const TArrayView<FTransformFragment> TransformFragments = Context.GetMutableFragmentView<FTransformFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FMassVehicleMovementFragment& VehicleMovementFragment = VehicleMovementFragments[EntityIndex];
			FTransformFragment& TransformFragment = TransformFragments[EntityIndex];

			FZoneGraphLaneLocation& CurLaneLocation = VehicleMovementFragment.LaneLocation;
			
			float TargetSpeed = VehicleMovementFragment.TargetSpeed;
			// Update the transform based on the vehicle movement fragment

			float TargetDist = CurLaneLocation.DistanceAlongLane+ TargetSpeed*DeltaTime;

			float CurLaneDistance = 0.0f;
			UE::ZoneGraph::Query::GetLaneLength(*TrafficSimSubsystem->ZoneGraphStorage, CurLaneLocation.LaneHandle, CurLaneDistance);
			
			//如果超出当前车道长度，则需要切换到下一个车道
			if (CurLaneDistance < TargetDist)
			{
				TargetDist = TargetDist - CurLaneDistance;
				bool Success=TrafficSimSubsystem->SwitchToNextLane(CurLaneLocation, TargetDist);
				if(!Success)
				{
					//如果没有下一个车道，则停止车辆
					DestroyedEntities.Add(Context.GetEntity(EntityIndex));
					continue;
				}
			}

			UE::ZoneGraph::Query::CalculateLocationAlongLane(*TrafficSimSubsystem->ZoneGraphStorage, CurLaneLocation.LaneHandle, TargetDist, CurLaneLocation);

			// Update the transform based on the new lane location
			TransformFragment.SetTransform(
				FTransform(FRotationMatrix::MakeFromX(CurLaneLocation.Direction).ToQuat(),
										CurLaneLocation.Position,
										FVector(1, 1, 1)));

		}
		});

	// Destroy entities that have no lane to switch to
	for (const FMassEntityHandle& Entity : DestroyedEntities)
	{
		EntityManager.Defer().DestroyEntity(Entity);
	}
}
