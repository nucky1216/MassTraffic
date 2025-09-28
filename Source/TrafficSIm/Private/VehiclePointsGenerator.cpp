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

	TMultiMap<int32, FZoneGraphLaneLocation> TypeIdx2SpawnLoc;
	TypeIdx2SpawnLoc.Empty();

	//提取所有配置的车辆长度
	TArray<float> VehicleLenth;
	float TotalProportion = 0.0f;
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
		TotalProportion += SpawnedType.Proportion;
		// 获取实体模板
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

	//构建累积概率表
	TArray<float> PrefixSum;
	PrefixSum.Reserve(VehicleLenth.Num());
	float Accumulate = 0.0f;
	for (int32 i = 0; i < EntityTypes.Num(); i++)
	{
		Accumulate += EntityTypes[i].Proportion/TotalProportion;
		PrefixSum.Add(Accumulate);
	}

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
		float Distance = FMath::RandRange(MinGapBetweenSpaces, MaxGapBetweenSpaces);
		for (; Distance < LaneLength; )
		{
			int32 ConfigRandIndex = SelectRandomItem();

			float Gap = FMath::RandRange(MinGapBetweenSpaces, MaxGapBetweenSpaces);
			if (Distance+ VehicleLenth[ConfigRandIndex] / 2.0f < LaneLength)
			{
				// 在ZoneGraph上计算位置
				FZoneGraphLaneLocation LaneLocation;
				Distance = Distance + Gap + VehicleLenth[ConfigRandIndex] / 2.0f;
				UE::ZoneGraph::Query::CalculateLocationAlongLane(ZoneGraphStorage, i, Distance, LaneLocation);
				TypeIdx2SpawnLoc.Add(ConfigRandIndex, LaneLocation);
				Distance+=VehicleLenth[ConfigRandIndex] / 2.0f;
				//Debug
				//TArray<FColor> DebugColors = {FColor::Red,FColor::Green,FColor::Blue};
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
		TArray<FZoneGraphLaneLocation> SpawnLocations;
		TypeIdx2SpawnLoc.MultiFind(ResultsIdx[i], SpawnLocations);
		if (SpawnLocations.Num() > 0)
		{
			FMassEntitySpawnDataGeneratorResult& Res = Results.AddDefaulted_GetRef();
			Res.NumEntities = SpawnLocations.Num();
			Res.EntityConfigIndex = EntityTypeIdx;
			UE_LOG(LogTemp, Log, TEXT("EntityIndex:%d, NumEntities:%d"), Res.EntityConfigIndex, Res.NumEntities);
		}
	}



	int32 ResultIdx = 0;
	for(FMassEntitySpawnDataGeneratorResult& Result:Results)
	{
		TArray<FZoneGraphLaneLocation> SpawnLocations;
		TypeIdx2SpawnLoc.MultiFind(ResultIdx++, SpawnLocations);

		Result.SpawnDataProcessor = UMassSpawnLocationProcessor::StaticClass();
		Result.SpawnData.InitializeAs<FMassTransformsSpawnData>();
		FMassTransformsSpawnData& Transforms = Result.SpawnData.GetMutable<FMassTransformsSpawnData>();

		Transforms.Transforms.Reserve(Result.NumEntities);
		for(int32 LocationIdx = 0; LocationIdx < SpawnLocations.Num(); ++LocationIdx)
		{
			const FZoneGraphLaneLocation& LaneLocation = SpawnLocations[LocationIdx];
			FTransform& Transform = Transforms.Transforms.AddDefaulted_GetRef();
			Transform.SetLocation(LaneLocation.Position);
			Transform.SetRotation(LaneLocation.Direction.ToOrientationQuat());
			//TransformsSpawnData.Transforms.Add(Transform);
			
		}
		UE_LOG(LogTemp, Log, TEXT("EntityIndex:%d, CurNum:%d"), Result.EntityConfigIndex,Result.NumEntities);

	}
    
	FinishedGeneratingSpawnPointsDelegate.Execute(Results);

}

