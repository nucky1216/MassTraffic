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
	UTrafficSimSubsystem* TrafficSimSubsystem = UWorld::GetSubsystem<UTrafficSimSubsystem>(World);


	ZoneGraphSubsystem->GetRegisteredZoneGraphData();

    
    const FZoneGraphStorage& ZoneGraphStorage = ZoneGraphData->GetStorage();

	struct FLaneConfigs {
		FZoneGraphLaneLocation LaneLocation;
		float TargetSpeed;
		float MaxSpeed;
		float MinSpeed;
		FZoneGraphTag ZoneLaneTag;

		FLaneConfigs(FZoneGraphLaneLocation LaneLocationIn, float TargetSpeedIn, float MaxSpeedIn, float MinSpeedIn, FZoneGraphTag ZoneLaneTagIn):
			LaneLocation(LaneLocationIn),TargetSpeed(TargetSpeedIn),MaxSpeed(MaxSpeedIn),MinSpeed(MinSpeedIn),ZoneLaneTag{ZoneLaneTagIn}{}
	};

	TMultiMap<int32, FLaneConfigs> TypeIdx2SpawnLoc;
	TypeIdx2SpawnLoc.Empty();

	//提取所有配置的车辆长度
	TArray<float> VehicleLenth;
	TArray<float> PrefixSum;
	TrafficSimSubsystem->InitializeTrafficTypes(EntityTypes, ConnectorTag);
	TrafficSimSubsystem->TagLaneSpeedGapSetup(TagLaneSpeed, TagLaneGap);
	TrafficSimSubsystem->GetVehicleConfigs(VehicleLenth, PrefixSum);

	auto SelectRandomItem = [&]()
		{
			float R = FMath::FRand(); // 生成 0~1 的随机数
			for (int32 i = 0; i < PrefixSum.Num(); i++)
			{
				if (R <= PrefixSum[i])
				{
					return i; // 返回选中的元素
				}
			}
			return PrefixSum.Num()-1; // 理论上不会走到这里
		};

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
		
		for (float Distance=0; Distance < LaneLength; )
		{
			int32 ConfigRandIndex = SelectRandomItem();
			
			float Gap = GetTagBasedGap(LaneTags);
			Distance = Gap + Distance + VehicleLenth[ConfigRandIndex] / 2.0f;
			if (Distance+ VehicleLenth[ConfigRandIndex]< LaneLength)
			{
				// 在ZoneGraph上计算位置
				FZoneGraphLaneLocation LaneLocation;
				UE::ZoneGraph::Query::CalculateLocationAlongLane(ZoneGraphStorage, i, Distance, LaneLocation);
				float MaxSpeed, MinSpeed;
				FZoneGraphTag LaneTag;
				float Speed = TrafficSimSubsystem->GetLaneSpeedByTag(LaneTags, MinSpeed, MaxSpeed,LaneTag);

				FLaneConfigs LaneConfigInfor(LaneLocation, Speed, MinSpeed, MaxSpeed, LaneTag);
				TypeIdx2SpawnLoc.Add(ConfigRandIndex, LaneConfigInfor);
				
				Distance = Distance + VehicleLenth[ConfigRandIndex] / 2.0f;
				//Debug
				//UE_LOG(LogTemp, Log, TEXT("Spawned:%d -> SpawnPoint at lane: %d with dist:%f on config:%d"), TypeIdx2SpawnLoc.Num()-1,i,Distance, ConfigRandIndex);
				//TArray<FColor> DebugColors = {FColor::Red,FColor::Green,FColor::Blue,FColor::Orange,FColor::Cyan,FColor::Emerald,FColor::Magenta,FColor::Purple};
				//DrawDebugBox(World, LaneLocation.Position, FVector(VehicleLenth[ConfigRandIndex] / 2.0f, 50.0f, 20.0f), LaneLocation.Direction.ToOrientationQuat(),
				//	DebugColors[ConfigRandIndex], true, 120.0f);
			}
			else{
				break;
			}
		}
	}
    
	TArray<FMassEntitySpawnDataGeneratorResult> Results;
	
	//构建生成数据
	//BuildResultsFromEntityTypes(Count, EntityTypes, Results);
	TArray<int32> ResultsIdx;
	TypeIdx2SpawnLoc.GetKeys(ResultsIdx);
	for (int32 i = 0; i < ResultsIdx.Num(); i++)
	{
		int32 EntityTypeIdx = ResultsIdx[i];
		TArray<FLaneConfigs> SpawnLocations;
		TypeIdx2SpawnLoc.MultiFind(ResultsIdx[i], SpawnLocations);
		if (SpawnLocations.Num() > 0)
		{
			FMassEntitySpawnDataGeneratorResult& Res = Results.AddDefaulted_GetRef();
			Res.NumEntities = SpawnLocations.Num();
			Res.EntityConfigIndex = EntityTypeIdx;
			//UE_LOG(LogTemp, Log, TEXT("EntityIndex:%d, NumEntities:%d"), Res.EntityConfigIndex, Res.NumEntities);
		}
	}



	//int32 ResultIdx = 0;
	for(FMassEntitySpawnDataGeneratorResult& Result:Results)
	{
		TArray<FLaneConfigs> SpawnLocations;
		TypeIdx2SpawnLoc.MultiFind(Result.EntityConfigIndex, SpawnLocations);

		//设置初始化处理器
		Result.SpawnDataProcessor = UVehicleParamsInitProcessor::StaticClass();
		//设置辅助数据类型
		Result.SpawnData.InitializeAs<FVehicleInitData>();
		FVehicleInitData& InitData = Result.SpawnData.GetMutable<FVehicleInitData>();

		InitData.LaneLocations.Reserve(Result.NumEntities);
		InitData.TargetSpeeds.Reserve(Result.NumEntities);
		InitData.MinSpeeds.Reserve(Result.NumEntities);
		InitData.MaxSpeeds.Reserve(Result.NumEntities);

		for(int32 LocationIdx = 0; LocationIdx < SpawnLocations.Num(); ++LocationIdx)
		{
			FZoneGraphLaneLocation LaneLocation = SpawnLocations[LocationIdx].LaneLocation;
			//FZoneGraphLaneLocation& LaneLocation = InitData.LaneLocations.AddDefaulted_GetRef();
			InitData.LaneLocations.Add(LaneLocation);
			InitData.TargetSpeeds.Add(SpawnLocations[LocationIdx].TargetSpeed);
			InitData.MinSpeeds.Add(SpawnLocations[LocationIdx].MinSpeed);
			InitData.MaxSpeeds.Add(SpawnLocations[LocationIdx].MaxSpeed);
			InitData.LaneSpeedTags.Add(SpawnLocations[LocationIdx].ZoneLaneTag);
		}
		UE_LOG(LogTemp, Log, TEXT("EntityIndex:%d, SpawnLocationNum:%d,NumEntities:%d"), Result.EntityConfigIndex, SpawnLocations.Num(),Result.NumEntities);

	}
    
	FinishedGeneratingSpawnPointsDelegate.Execute(Results);

}



float UVehiclePointsGenerator::GetTagBasedGap(FZoneGraphTagMask LaneTagMask) const
{
	for(auto& TagGapPair:TagLaneGap)
	{
		if(LaneTagMask.Contains(TagGapPair.Key))
		{
			return FMath::RandRange(TagGapPair.Value.MinGap, TagGapPair.Value.MaxGap);
		}
	}
	return FMath::RandRange(600.f, 1200.f);;
}

