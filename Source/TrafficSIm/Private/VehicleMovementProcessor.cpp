// Fill out your copyright notice in the Description page of Project Settings.


#include "VehicleMovementProcessor.h"
#include "MassVehicleMovementFragment.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "ZoneGraphQuery.h"

UVehicleMovementProcessor::UVehicleMovementProcessor():EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = FName(TEXT("TrafficSim"));
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
	UE_LOG(LogTrafficSim, VeryVerbose, TEXT("Executing VehicleMovementProcessor..."));
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
			
			float Speed = VehicleMovementFragment.Speed;
			// Update the transform based on the vehicle movement fragment

			float TargetDist = CurLaneLocation.DistanceAlongLane+ Speed *DeltaTime;
			int32 QueryLaneIndex = CurLaneLocation.LaneHandle.Index;
			float CurLaneDistance = 0.0f;

			if (QueryLaneIndex < 0)
			{
				continue;
			}

			UE::ZoneGraph::Query::GetLaneLength(*TrafficSimSubsystem->ZoneGraphStorage, CurLaneLocation.LaneHandle, CurLaneDistance);
			

			//如果超出当前车道长度，则需要切换到下一个车道
			if (TargetDist > CurLaneDistance && QueryLaneIndex>=0)
			{
				if (VehicleMovementFragment.NextLane < 0)
				{
					//如果没有下一个车道，则停止车辆
					DestroyedEntities.Add(Context.GetEntity(EntityIndex));
					//UE_LOG(LogTrafficSim, Log, TEXT("VehicleMovementProcessor: Entity %d at Lane:%d has no next lane to switch to, destroying entity."), Context.GetEntity(EntityIndex).SerialNumber, CurLaneLocation.LaneHandle.Index);
					continue;
				}

				TargetDist = TargetDist - CurLaneDistance;

				QueryLaneIndex = VehicleMovementFragment.NextLane;
				TArray<int32> NextLanes;
				VehicleMovementFragment.NextLane = TrafficSimSubsystem->ChooseNextLane(QueryLaneIndex, NextLanes);

				//bool Success=TrafficSimSubsystem->SwitchToNextLane(CurLaneLocation, TargetDist);

			}

			if (QueryLaneIndex < 0)
			{
				continue;
			}

			UE::ZoneGraph::Query::CalculateLocationAlongLane(*TrafficSimSubsystem->ZoneGraphStorage, QueryLaneIndex, TargetDist, CurLaneLocation);

			UE::ZoneGraph::Query::GetLaneLength(*TrafficSimSubsystem->ZoneGraphStorage, QueryLaneIndex, CurLaneDistance);

			VehicleMovementFragment.LeftDistance = CurLaneDistance - TargetDist;


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
