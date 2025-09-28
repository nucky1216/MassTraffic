// Fill out your copyright notice in the Description page of Project Settings.


#include "SpawnPointGenerator.h"
#include "ZoneGraphSubsystem.h"
#include "MassSpawnLocationProcessor.h"
#include "ZoneGraphQuery.h"

void USpawnPointGenerator::Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const
{
	UWorld* World = GetWorld();
	UZoneGraphSubsystem* ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(World);
	if (!ZoneGraphSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("No ZoneGraphSubsystem found"));
		return;
	}
	if (!ZoneGraphData)
	{
		UE_LOG(LogTemp, Warning, TEXT("No ZoneGraphData found"));
		return;
	}
	ZoneGraphSubsystem->GetRegisteredZoneGraphData();
	const FZoneGraphStorage& ZoneGraphStorage = ZoneGraphData->GetStorage();
	
	//找到所有没有Incoming链接的车道
	TArray<int32> StartLanes;
	for (int32 i = 0; i < ZoneGraphStorage.Lanes.Num(); i++)
	{
		const FZoneLaneData& Lane = ZoneGraphStorage.Lanes[i];

		StartLanes.Add(i); // 先假设是起始车道
		for (int32 j = Lane.LinksBegin; j < Lane.LinksEnd; j++)
		{
			const FZoneLaneLinkData& Link = ZoneGraphStorage.LaneLinks[j];
			if (Link.Type == EZoneLaneLinkType::Incoming)
			{
				StartLanes.RemoveAt(StartLanes.Num()-1);
			}
		}
	}

	TArray<FMassEntitySpawnDataGeneratorResult> Results;
	//构建生成数据
	FMassEntitySpawnDataGeneratorResult& Res = Results.AddDefaulted_GetRef();
	Res.EntityConfigIndex = 0; //默认使用第一个配置
	Res.NumEntities = StartLanes.Num();
	
	Res.SpawnDataProcessor = UMassSpawnLocationProcessor::StaticClass();
	Res.SpawnData.InitializeAs<FMassTransformsSpawnData>();
	FMassTransformsSpawnData& Transforms = Res.SpawnData.GetMutable<FMassTransformsSpawnData>();

	for(int32 LaneIndex : StartLanes)
	{
		FZoneGraphLaneLocation LaneLocation;
		UE::ZoneGraph::Query::CalculateLocationAlongLane(ZoneGraphStorage, LaneIndex, 0.0f, LaneLocation);
		FTransform& Transform = Transforms.Transforms.AddDefaulted_GetRef();
		Transform.SetLocation(LaneLocation.Position);
	}

	FinishedGeneratingSpawnPointsDelegate.Execute(Results);
}
