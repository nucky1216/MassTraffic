// Fill out your copyright notice in the Description page of Project Settings.


#include "VehicleMovementProcessor.h"
#include "MassVehicleMovementFragment.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "ZoneGraphQuery.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationSubsystem.h"

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
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
}

void UVehicleMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UE_LOG(LogTrafficSim, VeryVerbose, TEXT("Executing VehicleMovementProcessor..."));
	float DeltaTime = Context.GetDeltaTimeSeconds();
	TArray<FMassEntityHandle> DestroyedEntities;

	UMassRepresentationSubsystem* RepSubsystem = UWorld::GetSubsystem<UMassRepresentationSubsystem>(GetWorld());
	if (!RepSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("No RepresentationSubsystem found"));
		return;
	}
	auto SMInfos = RepSubsystem->GetMutableInstancedStaticMeshInfos();

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		TArrayView<FMassVehicleMovementFragment> VehicleMovementFragments = Context.GetMutableFragmentView<FMassVehicleMovementFragment>();
		const TArrayView<FTransformFragment> TransformFragments = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FMassRepresentationFragment> RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FMassVehicleMovementFragment& VehicleMovementFragment = VehicleMovementFragments[EntityIndex];
			FTransformFragment& TransformFragment = TransformFragments[EntityIndex];
			FMassRepresentationFragment& Representation = RepresentationList[EntityIndex];

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
			// 写入 Per-Instance Custom Data（包含实体 SerialNumber 等）
			{
				FMassEntityHandle Entity = Context.GetEntity(EntityIndex);
				const int32 EntitySN = Entity.SerialNumber;

				// 获取 InstancedStaticMesh 的索引
				const int32 StaticMeshInstanceIndex = Representation.StaticMeshDescIndex;
				if (StaticMeshInstanceIndex == INDEX_NONE)
				{
					continue; // 无效索引，跳过
				}

				// 获取 InstancedStaticMesh 信息
				FMassInstancedStaticMeshInfo& MeshInfo = SMInfos[StaticMeshInstanceIndex];

				// 本帧用于选择 LOD 的值；如果你能拿到真实 LODSignificance，建议替换
				const float LODSignificance = 1.0f;

				struct FVehicleISMCustomData
				{
					float SerialNumber;
					float EntityId;
					float Speed;
				};
				static_assert((sizeof(FVehicleISMCustomData) % sizeof(float)) == 0, "custom data must be float-packed");

				const FVehicleISMCustomData Custom{
					static_cast<float>(EntitySN),
					static_cast<float>(Entity.AsNumber()),
					100.f
				};

				//UE_LOG(LogTemp, Log, TEXT("MeshIndex:%d"), StaticMeshInstanceIndex);
				MeshInfo.AddBatchedCustomData(Custom, LODSignificance);

			}
		}
		});

	// Destroy entities that have no lane to switch to
	for (const FMassEntityHandle& Entity : DestroyedEntities)
	{
		EntityManager.Defer().DestroyEntity(Entity);
	}
}
