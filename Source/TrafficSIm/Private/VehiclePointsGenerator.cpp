// Fill out your copyright notice in the Description page of Project Settings.


#include "VehiclePointsGenerator.h"
#include "EngineUtils.h"
#include "ZoneGraphSubsystem.h"
#include "MassSpawnLocationProcessor.h"
#include "ZoneGraphQuery.h"

void UVehiclePointsGenerator::Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const
{
	UWorld* World = GetWorld();
	UZoneGraphSubsystem* ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(World);

	ZoneGraphSubsystem->GetRegisteredZoneGraphData();

    
    const FZoneGraphStorage& ZoneGraphStorage = ZoneGraphData->GetStorage();

	TArray<FZoneGraphLaneLocation> SpawnLocations;

    for (int LaneIdx = 0; LaneIdx < ZoneGraphStorage.Lanes.Num(); LaneIdx++)
    {
		float LaneLength = 0.0;
		UE::ZoneGraph::Query::GetLaneLength(ZoneGraphStorage, LaneIdx, LaneLength);

        //TO-DO:: tag的过滤
        const FZoneGraphTagMask& LaneTags = ZoneGraphStorage.Lanes[LaneIdx].Tags;
		if (!LaneFilter.Pass(LaneTags))
		{
			UE_LOG(LogTemp, Warning, TEXT("Lane %d is filtered out by tag mask:%u"), LaneIdx,LaneTags.GetValue());
			continue;
		}


		for (float Distance = FMath::RandRange(MinGapBetweenSpaces, MaxGapBetweenSpaces); Distance < LaneLength; /*see end of block*/)
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
			}

			// Advance ahead past the space we just consumed, plus a random gap.
			Distance += Gap;
		}
    }
    
	TArray<FMassEntitySpawnDataGeneratorResult> Results;
	BuildResultsFromEntityTypes(Count, EntityTypes, Results);

	for(FMassEntitySpawnDataGeneratorResult& Result:Results)
	{
		Result.SpawnDataProcessor = UMassSpawnLocationProcessor::StaticClass();
		Result.SpawnData.InitializeAs<FMassTransformsSpawnData>();
		FMassTransformsSpawnData& TransformsSpawnData = Result.SpawnData.GetMutable<FMassTransformsSpawnData>();

		TransformsSpawnData.Transforms.Reserve(SpawnLocations.Num());

		int32 SpawnedCount = 0;

		for(int32 LocationIdx = 0; LocationIdx < SpawnLocations.Num() && SpawnedCount < Count; ++LocationIdx)
		{
			const FZoneGraphLaneLocation& LaneLocation = SpawnLocations[LocationIdx];
			FTransform Transform;
			Transform.SetLocation(LaneLocation.Position);
			Transform.SetRotation(LaneLocation.Direction.ToOrientationQuat());
			TransformsSpawnData.Transforms.Add(Transform);
			++SpawnedCount;
		}

	}
    
	FinishedGeneratingSpawnPointsDelegate.Execute(Results);

}
