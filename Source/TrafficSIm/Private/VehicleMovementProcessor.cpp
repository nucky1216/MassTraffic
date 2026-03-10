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
	EntityQuery.AddRequirement<FMassRepresentationLODFragment >(EMassFragmentAccess::ReadOnly);
	// ��ѡ���� Chunk ���ˣ�������֡����¿��ӻ�ʱ��д�����������Բ��룩
	EntityQuery.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadOnly);
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
			const TArrayView<FMassRepresentationLODFragment> RepresentationLODList = Context.GetMutableFragmentView<FMassRepresentationLODFragment>();
			for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
			{
				FMassVehicleMovementFragment& VehicleMovementFragment = VehicleMovementFragments[EntityIndex];
				FTransformFragment& TransformFragment = TransformFragments[EntityIndex];
				FMassRepresentationFragment& Representation = RepresentationList[EntityIndex];

				FZoneGraphLaneLocation& CurLaneLocation = VehicleMovementFragment.LaneLocation;

				const float Speed = VehicleMovementFragment.Speed;
				// 1km/h = 27.777... cm/s
				float TargetDist = CurLaneLocation.DistanceAlongLane + Speed * DeltaTime * 27.77f;
				int32 QueryLaneIndex = CurLaneLocation.LaneHandle.Index;
				float CurLaneDistance = 0.0f;

				// д�� Per-Instance Custom Data������ʵ�� SerialNumber �ȣ�
				if (TrafficSim::MoveFrag::Debug::bEnbaleCustomData)
				{
					FMassEntityHandle Entity = Context.GetEntity(EntityIndex);
					const int32 EntitySN = Entity.SerialNumber;

					const int32 StaticMeshInstanceIndex = Representation.StaticMeshDescIndex;
					if (StaticMeshInstanceIndex == INDEX_NONE)
					{
						continue;
					}

					FMassInstancedStaticMeshInfo& MeshInfo = SMInfos[StaticMeshInstanceIndex];
					const float LODSignificance = RepresentationLODList[EntityIndex].LODSignificance;
					struct FVehicleISMCustomData
					{
						float SerialNumber;
						float EntityId;
						float Speed;
					};
					static_assert((sizeof(FVehicleISMCustomData) % sizeof(float)) == 0, "custom data must be float-packed");
					const FVehicleISMCustomData Custom{
						static_cast<float>(EntitySN),
						VehicleMovementFragment.TargetSpeed,
						VehicleMovementFragment.Speed
					};
					MeshInfo.AddBatchedCustomData(Custom, LODSignificance);
				}

				if (VehicleMovementFragment.FreezeTime >= VehicleMovementFragment.MaxFreezeTime)
				{
					DestroyedEntities.Add(Context.GetEntity(EntityIndex));
					continue;
				}

				if (QueryLaneIndex < 0)
				{
					continue;
				}

				UE::ZoneGraph::Query::GetLaneLength(*TrafficSimSubsystem->ZoneGraphStorage, CurLaneLocation.LaneHandle, CurLaneDistance);

				// ���������ǰ�������ȣ�����Ҫ�л�����һ������
				if (TargetDist > CurLaneDistance && QueryLaneIndex >= 0)
				{
					if (VehicleMovementFragment.NextLane < 0)
					{
						DestroyedEntities.Add(Context.GetEntity(EntityIndex));
						TrafficSimSubsystem->UnRegisterVehiPlateID(VehicleMovementFragment.VehID);
						continue;
					}

					TargetDist = TargetDist - CurLaneDistance;

					QueryLaneIndex = VehicleMovementFragment.NextLane;
					FZoneLaneData NewLane = TrafficSimSubsystem->ZoneGraphStorage->Lanes[VehicleMovementFragment.NextLane];
					TrafficSimSubsystem->GetLaneSpeedByTag(NewLane.Tags, VehicleMovementFragment.MaxSpeed, VehicleMovementFragment.MinSpeed, VehicleMovementFragment.LaneSpeedTag);
					VehicleMovementFragment.CruiseSpeed = FMath::RandRange(VehicleMovementFragment.MinSpeed, VehicleMovementFragment.MaxSpeed);

					TArray<int32> NextLanes;
					VehicleMovementFragment.NextLane = TrafficSimSubsystem->ChooseNextLane(QueryLaneIndex, NextLanes);
				}

				if (QueryLaneIndex < 0)
				{
					continue;
				}

				UE::ZoneGraph::Query::CalculateLocationAlongLane(*TrafficSimSubsystem->ZoneGraphStorage, QueryLaneIndex, TargetDist, CurLaneLocation);

				UE::ZoneGraph::Query::GetLaneLength(*TrafficSimSubsystem->ZoneGraphStorage, QueryLaneIndex, CurLaneDistance);

				VehicleMovementFragment.LeftDistance = CurLaneDistance - TargetDist;

				
				const FTransform CurrentTransform = TransformFragment.GetTransform();
				const FVector NewPos = CurLaneLocation.Position;
				const FQuat NewRot = FRotationMatrix::MakeFromX(CurLaneLocation.Direction).ToQuat();

				FTransform TargetTrans = FTransform(NewRot, NewPos, FVector(1, 1, 1));
				
				// �ȶ���д�룺�ٶȽ�����λ�� / �Ƕȱ仯������ֵʱ����д Transform�����ⶶ��
				if (VehicleMovementFragment.FreezeTime > UE_KINDA_SMALL_NUMBER * 100)
				{			
					continue;
				}
				TransformFragment.SetTransform(TargetTrans);
				
			}
		});

	// Destroy entities that have no lane to switch to
	for (const FMassEntityHandle& Entity : DestroyedEntities)
	{
		//EntityManager.Defer().DestroyEntity(Entity);
		EntityManager.Defer().AddTag<FVehicleDeleteTag>(Entity);
	}
}