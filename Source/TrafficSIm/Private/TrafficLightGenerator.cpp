// Fill out your copyright notice in the Description page of Project Settings.


#include "TrafficLightGenerator.h"
#include "MassSpawnLocationProcessor.h"
#include "MassGameplaySettings.h"
#include "TrafficSimSubsystem.h"
#include "TrafficTypes.h"
#include "TrafficlightInitProcessor.h"
#include "MassAssortedFragmentsTrait.h"



void UTrafficLightGenerator::Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const
{
	//��ȡZoneGraphData
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

		//�ǽ���·�ڵ�Tag
		if (IntersectionTagFilter.Pass(ZoneData.Tags))
		{
			UE_LOG(LogTrafficSim, Log, TEXT("Zone %d is an intersection"), i);
			IntersectionPoints.Add(ZoneData.Bounds.GetCenter());
		}
	}

	//����·����������λ�ü���
	TArray<FMassEntitySpawnDataGeneratorResult> Results;
	BuildResultsFromEntityTypes(IntersectionPoints.Num(), EntityTypes, Results);

	for (auto& Result : Results)
	{
		//ָ�����ɴ�����
		Result.SpawnDataProcessor = UTrafficLightInitProcessor::StaticClass();

		//��ʼ������λ������
		Result.SpawnData.InitializeAs<FTrafficLightInitData>();
		FTrafficLightInitData& InitSpawnData = Result.SpawnData.GetMutable<FTrafficLightInitData>();

		InitSpawnData.TrafficLightTransforms.Reserve(IntersectionPoints.Num());
		InitSpawnData.ZoneIndex.Reserve(IntersectionPoints.Num());
		InitSpawnData.StartSideIndex.Reserve(IntersectionPoints.Num());


		//auto* AssortedFragmentsTrait = EntityTypes[Result.EntityConfigIndex].EntityConfig->FindTrait<UMassAssortedFragmentsTrait>();
		//InitSpawnData.Periods = TrafficLightPeriod;

		int32 SpawnCount = 0;
		for (int32 LocationIndex = 0; LocationIndex < Result.NumEntities; LocationIndex++)
		{
			InitSpawnData.TrafficLightTransforms.Add(FTransform(IntersectionPoints[LocationIndex]));
			InitSpawnData.ZoneIndex.Add(LocationIndex);
			InitSpawnData.StartSideIndex.Add(0); //Ĭ�ϴ�·�ڵ�0�߿�ʼ
			
			SpawnCount++;
		}

		
	}

	FinishedGeneratingSpawnPointsDelegate.Execute(Results);

}
