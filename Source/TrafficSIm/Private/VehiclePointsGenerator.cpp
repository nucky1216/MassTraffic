// Fill out your copyright notice in the Description page of Project Settings.


#include "VehiclePointsGenerator.h"
#include "EngineUtils.h"
#include "ZoneGraphSubsystem.h"
#include "MassSpawnLocationProcessor.h"
#include "VehicleParamsInitProcessor.h"
#include "ZoneGraphQuery.h"
#include "MassEntityConfigAsset.h"
#include "MassVehicleMovementFragment.h"
#include "MassAssortedFragmentsTrait.h"

void UVehiclePointsGenerator::Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const
{
	UWorld* World = GetWorld();
	UZoneGraphSubsystem* ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(World);

	ZoneGraphSubsystem->GetRegisteredZoneGraphData();

    
    const FZoneGraphStorage& ZoneGraphStorage = ZoneGraphData->GetStorage();

	TArray<FZoneGraphLaneLocation> SpawnLocations;

	SpawnLocations.Reserve(Count);
	/*
    for (int32 LaneIdx = 0; LaneIdx < ZoneGraphStorage.Lanes.Num(); LaneIdx++)
    {
		float LaneLength = 0.0;
		UE::ZoneGraph::Query::GetLaneLength(ZoneGraphStorage, LaneIdx, LaneLength);


        //TO-DO:: tag的过滤
        const FZoneGraphTagMask& LaneTags = ZoneGraphStorage.Lanes[LaneIdx].Tags;
		if (!LaneFilter.Pass(LaneTags))
		{
			//UE_LOG(LogTemp, Warning, TEXT("Lane %d is filtered out by tag mask:%u"), LaneIdx,LaneTags.GetValue());
			continue;
		}


		for (float Distance = FMath::RandRange(MinGapBetweenSpaces, MaxGapBetweenSpaces); Distance < LaneLength; )
		{
			float Gap = FMath::RandRange(MinGapBetweenSpaces, MaxGapBetweenSpaces);
			if (Distance + Gap< LaneLength)
			{
				// 在ZoneGraph上计算位置
				FZoneGraphLaneLocation LaneLocation;
				UE::ZoneGraph::Query::CalculateLocationAlongLane(ZoneGraphStorage, LaneIdx, Distance + Gap, LaneLocation);
				
				// TODO:Filter location
				//...
				SpawnLocations.Add(LaneLocation);
				//UE_LOG(LogTemp, Log, TEXT("Lane %d, Distance:%.2f, Location:(%.2f,%.2f,%.2f)"), LaneIdx, Distance + Gap, LaneLocation.Position.X, LaneLocation.Position.Y, LaneLocation.Position.Z);
			}

			// Advance ahead past the space we just consumed, plus a random gap.
			Distance += Gap;
		}
    }*/

	TArray<float> VehicleLenth;
	for (int32 i = 0; i < EntityTypes.Num(); i++)
	{
		const FMassSpawnedEntityType& SpawnedType = EntityTypes[i];
		// 确保 EntityConfig 已加载
		const UMassEntityConfigAsset* EntityConfig = SpawnedType.EntityConfig.LoadSynchronous();
		if (!EntityConfig)
		{
			UE_LOG(LogTemp, Warning, TEXT("EntityConfig is null for EntityType %d"), i);
			continue;
		}

		const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World);

		TConstArrayView<FInstancedStruct> InitialFragmentValues = EntityTemplate.GetInitialFragmentValues();

		for (const FInstancedStruct& Fragment : InitialFragmentValues)
		{
			if (Fragment.GetScriptStruct() == TBaseStructure<FMassVehicleMovementFragment>::Get())
			{
				const FMassVehicleMovementFragment* InitFrag = Fragment.GetPtr<FMassVehicleMovementFragment>();
				if (InitFrag)
				{
					// 访问 fragment 的属性
					float VehicleLength = InitFrag->VehicleLength;
					VehicleLenth.Add(VehicleLength);
					UE_LOG(LogTemp, Log, TEXT("EntityIndex:%d, VehicleLength:%.2f"), i, VehicleLength);
				}
			}
		}
	}

	for (int32 i = 0; i < ZoneGraphStorage.Lanes.Num(); i++)
	{
		float LaneLength = 0.0;
		UE::ZoneGraph::Query::GetLaneLength(ZoneGraphStorage, i, LaneLength);
		//TO-DO:: tag的过滤
		const FZoneGraphTagMask& LaneTags = ZoneGraphStorage.Lanes[i].Tags;
		if (!LaneFilter.Pass(LaneTags))
		{
			//UE_LOG(LogTemp, Warning, TEXT("Lane %d is filtered out by tag mask:%u"), i, LaneTags.GetValue());
			continue;
		}
		for (float Distance = FMath::RandRange(MinGapBetweenSpaces, MaxGapBetweenSpaces); Distance < LaneLength; )
		{
			int32 ConfigRandIndex=FMath::RandRange(0,EntityTypes.Num());

			

			float Gap = FMath::RandRange(MinGapBetweenSpaces, MaxGapBetweenSpaces);
			if (Distance + Gap < LaneLength)
			{
				// 在ZoneGraph上计算位置
				FZoneGraphLaneLocation LaneLocation;
				UE::ZoneGraph::Query::CalculateLocationAlongLane(ZoneGraphStorage, i, Distance + Gap, LaneLocation);
				SpawnLocations.Add(LaneLocation);
			}
		}
	}
    
	TArray<FMassEntitySpawnDataGeneratorResult> Results;
	BuildResultsFromEntityTypes(Count, EntityTypes, Results);
	int32 SpawnedCount = 0;

	for(FMassEntitySpawnDataGeneratorResult& Result:Results)
	{
		Result.SpawnDataProcessor = UMassSpawnLocationProcessor::StaticClass();
		Result.SpawnData.InitializeAs<FMassTransformsSpawnData>();
		FMassTransformsSpawnData& Transforms = Result.SpawnData.GetMutable<FMassTransformsSpawnData>();

		Transforms.Transforms.Reserve(Result.NumEntities);
		for(int32 LocationIdx = 0; LocationIdx < Result.NumEntities; ++LocationIdx)
		{
			const FZoneGraphLaneLocation& LaneLocation = SpawnLocations[LocationIdx+ SpawnedCount];
			FTransform& Transform = Transforms.Transforms.AddDefaulted_GetRef();
			Transform.SetLocation(LaneLocation.Position);
			Transform.SetRotation(LaneLocation.Direction.ToOrientationQuat());
			//TransformsSpawnData.Transforms.Add(Transform);
			
		}
		SpawnedCount= Result.NumEntities;
		UE_LOG(LogTemp, Log, TEXT("EntityIndex:%d, CurNum:%d"), Result.EntityConfigIndex,Result.NumEntities);

	}
    
	FinishedGeneratingSpawnPointsDelegate.Execute(Results);

}
