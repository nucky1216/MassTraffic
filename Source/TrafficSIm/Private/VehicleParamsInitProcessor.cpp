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
UVehicleParamsInitProcessor::UVehicleParamsInitProcessor():EntityQuery(*this)
{
	//ObservedType = FMassVehicleMovementFragment::StaticStruct();
	//Operation = EMassObservedOperation::Add;
}

void UVehicleParamsInitProcessor::Initialize(UObject& Owner)
{

}

void UVehicleParamsInitProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
}

void UVehicleParamsInitProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UE_LOG(LogTrafficSim,VeryVerbose,TEXT("VehicleInitProcessor.."));

	if (!ensure(Context.ValidateAuxDataType<FVehicleInitData>()))
	{
		UE_LOG(LogTrafficLight, Error, TEXT("TrafficLightInitProcessor requires FVehicleInitData to be set in the context."));
	}
	FVehicleInitData& InitData = Context.GetMutableAuxData().GetMutable<FVehicleInitData>();
	UE_LOG(LogTemp,Log,TEXT("Process EntityNum:%d"),Context.GetEntities().Num());

	UMassRepresentationSubsystem* RepSubsystem = UWorld::GetSubsystem<UMassRepresentationSubsystem>(GetWorld());
	const auto SMInfors=RepSubsystem->GetMutableInstancedStaticMeshInfos();
	if (!RepSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("No RepresentationSubsystem found"));
		return;
	}

	UTrafficSimSubsystem* TrafficSimSubsystem = UWorld::GetSubsystem<UTrafficSimSubsystem>(Context.GetWorld());
	if (!TrafficSimSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find TrafficSimSubsystem!"));
	}


	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& Context)
	{
	    const int32 EntityCount = Context.GetNumEntities();

		const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FMassVehicleMovementFragment> VehicleMovementList = Context.GetMutableFragmentView<FMassVehicleMovementFragment>();
		auto RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();
		bool Debug = false;

		for (int32 EntityIndex = 0; EntityIndex < EntityCount; ++EntityIndex)
		{
			FMassVehicleMovementFragment& VehicleMovement = VehicleMovementList[EntityIndex];
			FTransformFragment& Transform = TransformList[EntityIndex];
			FMassRepresentationFragment& Representation = RepresentationList[EntityIndex];

			//�����������			
			//FBox QueryBox = FBox::BuildAABB(SpawnLocation, VehicleMovement.QueryExtent);
			//FZoneGraphLaneLocation LaneLocation;
			//TrafficSimSubsystem->FindEntityLaneByQuery(QueryBox, VehicleMovement.LaneFilter, LaneLocation);
			FZoneGraphLaneLocation LaneLocation = InitData.LaneLocations[EntityIndex];
			VehicleMovement.LaneLocation = LaneLocation;
			Transform.SetTransform(FTransform(LaneLocation.Direction.ToOrientationQuat(),LaneLocation.Position));
			
			//UE_LOG(LogTemp, Log, TEXT("Found Lane: %s"), *LaneLocation.LaneHandle.ToString());
			TArray<int32> NextLanes;
			VehicleMovement.NextLane = TrafficSimSubsystem->ChooseNextLane(VehicleMovement.LaneLocation.LaneHandle.Index, NextLanes);

			VehicleMovement.VehicleHandle = Context.GetEntity(EntityIndex);

			VehicleMovement.TargetSpeed = FMath::RandRange(VehicleMovement.MinSpeed,VehicleMovement.MaxSpeed);

			VehicleMovement.Speed = FMath::RandRange(VehicleMovement.MinSpeed, VehicleMovement.TargetSpeed);

			UE_LOG(LogTrafficSim, VeryVerbose, TEXT("Entity %s Init: Lane %s NextLane %d Speed %.2f,TargetSpeed:%.2f"), *VehicleMovement.VehicleHandle.DebugGetDescription(),
				*VehicleMovement.LaneLocation.LaneHandle.ToString(), VehicleMovement.NextLane, VehicleMovement.Speed, VehicleMovement.TargetSpeed);
			//����debug��ʾ
			if(Debug)
			{
				FMassEntityHandle Entity = Context.GetEntity(EntityIndex);
				const int32 EntitySN = Entity.SerialNumber;

				// ��ȡ InstancedStaticMesh ������
				const int32 StaticMeshInstanceIndex = Representation.StaticMeshDescIndex;
				if (StaticMeshInstanceIndex == INDEX_NONE)
				{
					continue; // ��Ч����������
				}

				// ��ȡ InstancedStaticMesh ��Ϣ

				FMassInstancedStaticMeshInfo& MeshInfo = SMInfors[Representation.StaticMeshDescIndex];
				float Singnificance = 0.0f; // ������Ը�����Ҫ���� LOD ������ֵ


				TArray<float> CustomData = { (float)EntitySN,(float)Entity.AsNumber(),VehicleMovement.Speed };

				MeshInfo.AddBatchedCustomDataFloats(CustomData, Singnificance);
			}
		}
	});
}
