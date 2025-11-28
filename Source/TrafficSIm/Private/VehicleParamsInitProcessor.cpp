// Fill out your copyright notice in the Description page of Project Settings.


#include "VehicleParamsInitProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "EngineUtils.h"
#include "TrafficTypes.h"
#include "MassVehicleMovementFragment.h"
#include "ZoneGraphQuery.h"
#include "MassRepresentationFragments.h"
#include "Engine/InstancedStaticMesh.h"
#include "MassRepresentationSubsystem.h"
//#include "MassRepresentationFragments.h"
UVehicleParamsInitProcessor::UVehicleParamsInitProcessor() :EntityQuery(*this)
{
	//ObservedType = FMassVehicleMovementFragment::StaticStruct();
	//Operation = EMassObservedOperation::Add;
	bAutoRegisterWithProcessingPhases = false;
}

void UVehicleParamsInitProcessor::Initialize(UObject& Owner)
{

}

void UVehicleParamsInitProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationLODFragment >(EMassFragmentAccess::ReadOnly);
	// 可选：按 Chunk 过滤，仅当本帧会更新可视化时才写（避免数量对不齐）
	EntityQuery.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadOnly);
}

void UVehicleParamsInitProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UE_LOG(LogTrafficSim, VeryVerbose, TEXT("VehicleInitProcessor.."));

	if (!ensure(Context.ValidateAuxDataType<FVehicleInitData>()))
	{
		UE_LOG(LogTrafficLight, Error, TEXT("TrafficLightInitProcessor requires FVehicleInitData to be set in the context."));
	}
	FVehicleInitData& InitData = Context.GetMutableAuxData().GetMutable<FVehicleInitData>();


	UMassRepresentationSubsystem* RepSubsystem = UWorld::GetSubsystem<UMassRepresentationSubsystem>(GetWorld());
	if (!RepSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("No RepresentationSubsystem found"));
		return;
	}
	auto SMInfos = RepSubsystem->GetMutableInstancedStaticMeshInfos();

	UTrafficSimSubsystem* TrafficSimSubsystem = UWorld::GetSubsystem<UTrafficSimSubsystem>(Context.GetWorld());
	if (!TrafficSimSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find TrafficSimSubsystem!"));
	}

	int32 CountInTypes = 0;

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& Context)
		{
			const int32 EntityCount = Context.GetNumEntities();

			const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
			const TArrayView<FMassVehicleMovementFragment> VehicleMovementList = Context.GetMutableFragmentView<FMassVehicleMovementFragment>();
			const TArrayView<FMassRepresentationFragment> RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();
			const TArrayView<FMassRepresentationLODFragment> RepresentationLODList = Context.GetMutableFragmentView<FMassRepresentationLODFragment>();

			UE_LOG(LogTrafficSim, VeryVerbose, TEXT("Cur Init Data with LaneLocationNum:%d, CurEntityCount:%d, CountInTypes:%d"),
				InitData.LaneLocations.Num(),
				EntityCount,
				CountInTypes);

			for (int32 EntityIndex = 0; EntityIndex < EntityCount; ++EntityIndex)
			{
				FMassVehicleMovementFragment& VehicleMovement = VehicleMovementList[EntityIndex];
				FTransformFragment& Transform = TransformList[EntityIndex];
				FMassRepresentationFragment& Representation = RepresentationList[EntityIndex];

				//设置相关属性			
				FZoneGraphLaneLocation LaneLocation = InitData.LaneLocations[CountInTypes];

				VehicleMovement.LaneLocation = LaneLocation;
				//Transform.SetTransform(FTransform(LaneLocation.Direction.ToOrientationQuat(), LaneLocation.Position));

				//UE_LOG(LogTemp, Log, TEXT("Found Lane: %s"), *LaneLocation.LaneHandle.ToString());
				TArray<int32> NextLanes;
				VehicleMovement.NextLane = TrafficSimSubsystem->ChooseNextLane(VehicleMovement.LaneLocation.LaneHandle.Index, NextLanes);

				VehicleMovement.VehicleHandle = Context.GetEntity(EntityIndex);

				VehicleMovement.TargetSpeed = InitData.TargetSpeeds[CountInTypes];
				VehicleMovement.MinSpeed = InitData.MinSpeeds[CountInTypes];
				VehicleMovement.MaxSpeed = InitData.MaxSpeeds[CountInTypes];
				VehicleMovement.LaneSpeedTag = InitData.LaneSpeedTags[CountInTypes];
				VehicleMovement.CruiseSpeed = FMath::RandRange(VehicleMovement.MinSpeed, VehicleMovement.MaxSpeed);
				CountInTypes++;

				VehicleMovement.Speed = FMath::RandRange(VehicleMovement.MinSpeed, VehicleMovement.TargetSpeed);

				UE_LOG(LogTrafficSim, VeryVerbose, TEXT("Entity %s Init: Lane %s NextLane %d Speed %.2f,TargetSpeed:%.2f"), *VehicleMovement.VehicleHandle.DebugGetDescription(),
					*VehicleMovement.LaneLocation.LaneHandle.ToString(), VehicleMovement.NextLane, VehicleMovement.Speed, VehicleMovement.TargetSpeed);

				// 写入 Per-Instance Custom Data（包含实体 SerialNumber 等）
				if(0)
				{
					// 快速跳过不会更新的 Chunk
					if (!FMassVisualizationChunkFragment::ShouldUpdateVisualizationForChunk(Context))
					{
						return;
					}


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
					const float LODSignificance = RepresentationLODList[EntityIndex].LODSignificance;

					// 1) 为该实例追加一次 Transform 更新（InstanceId 使用实体唯一值即可）
					//const FTransform CurrentXf = Transform.GetTransform();
					//MeshInfo.AddBatchedTransform(
					//	EntitySN,             // InstanceId（与自定义数据同一实例要一致）
					//	CurrentXf,            // Transform
					//	CurrentXf,            // PrevTransform（初始化用相同值即可）
					//	LODSignificance
					//);

					// 2) 逐实例写入自定义数据（数量与上面的 AddBatchedTransform 调用次数严格一致）
					struct FVehicleISMCustomData
					{
						float SerialNumber;
						float EntityId;
						float Speed;
					};
					static_assert((sizeof(FVehicleISMCustomData) % sizeof(float)) == 0, "custom data must be float-packed");

					const FVehicleISMCustomData Custom{
						static_cast<float>(EntitySN),
						static_cast<float>(VehicleMovement.NextLane),
						VehicleMovement.Speed
					};
					
					//UE_LOG(LogTemp, Log, TEXT("MeshIndex:%d"), StaticMeshInstanceIndex);
					//MeshInfo.AddBatchedCustomData(Custom, LODSignificance);

					
				}
			}
		});
}