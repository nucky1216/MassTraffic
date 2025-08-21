// Fill out your copyright notice in the Description page of Project Settings.


#include "TrafficLightGenerator.h"
#include "MassSpawnLocationProcessor.h"
#include "MassGameplaySettings.h"
#include "TrafficSimSubsystem.h"
#include "TrafficTypes.h"
#include "TrafficlightInitProcessor.h"



void UTrafficLightGenerator::Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const
{
	//获取ZoneGraphData
	if (!ZoneGraphData)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("ZoneGraphData is not set in TrafficLightGenerator"));
		return;
	}

	const FZoneGraphStorage& ZoneGraphStorage = ZoneGraphData->GetStorage();

	TArray<FVector> IntersectionPoints;

	int32 ZoneNum = ZoneGraphStorage.Zones.Num();
	for (int32 i = 0; i < ZoneNum; ++i)
	{
		const FZoneData& ZoneData = ZoneGraphStorage.Zones[i];

		//是交叉路口的Tag
		if (IntersectionTagFilter.Pass(ZoneData.Tags))
		{
			UE_LOG(LogTrafficSim, Log, TEXT("Zone %d is an intersection"), i);
			IntersectionPoints.Add(ZoneData.Bounds.GetCenter());
		}
	}

	//根据路口类型生成位置集合
	TArray<FMassEntitySpawnDataGeneratorResult> Results;
	BuildResultsFromEntityTypes(IntersectionPoints.Num(), EntityTypes, Results);

	for (auto& Result : Results)
	{
		//指定生成处理器
		Result.SpawnDataProcessor = UTrafficLightInitProcessor::StaticClass();

		//初始化生成位置数据
		Result.SpawnData.InitializeAs<FTrafficLightInitData>();
		FTrafficLightInitData& InitSpawnData = Result.SpawnData.GetMutable<FTrafficLightInitData>();

		InitSpawnData.TrafficLightTransforms.Reserve(IntersectionPoints.Num());
		InitSpawnData.ZoneIndex.Reserve(IntersectionPoints.Num());
		InitSpawnData.StartSideIndex.Reserve(IntersectionPoints.Num());

		int32 SpawnCount = 0;
		for (int32 LocationIndex = 0; LocationIndex < Result.NumEntities; LocationIndex++)
		{
			InitSpawnData.TrafficLightTransforms.Add(FTransform(IntersectionPoints[LocationIndex]));
			InitSpawnData.ZoneIndex.Add(LocationIndex);
			InitSpawnData.StartSideIndex.Add(0); //默认从第0个路口开始
			SpawnCount++;
		}

		
	}

	FinishedGeneratingSpawnPointsDelegate.Execute(Results);

}
