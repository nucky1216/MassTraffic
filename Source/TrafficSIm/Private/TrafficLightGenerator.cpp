// Fill out your copyright notice in the Description page of Project Settings.


#include "TrafficLightGenerator.h"
#include "MassSpawnLocationProcessor.h"
#include "MassGameplaySettings.h"
#include "TrafficSimSubsystem.h"
#include "TrafficTypes.h"
#include "TrafficLightInitProcessor.h"
#include "MassAssortedFragmentsTrait.h"
#include "TrafficLightSubsystem.h"



void UTrafficLightGenerator::Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const
{
	//��ȡZoneGraphData
	if (!ZoneGraphData)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("ZoneGraphData is not set in TrafficLightGenerator"));
		return;
	}
	UTrafficLightSubsystem* TrafficLightSubsystem = GetWorld()->GetSubsystem<UTrafficLightSubsystem>();
	if(!TrafficLightSubsystem)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("TrafficLightSubsystem is not found"));
		return;
	}

	TrafficLightSubsystem->BuildIntersectionsData(IntersectionTagFilter);

	const FZoneGraphStorage& ZoneGraphStorage = ZoneGraphData->GetStorage();

	TArray<FVector> IntersectionPoints;
	TArray<int32> IntersectionZoneIndices;

	int32 ZoneNum = ZoneGraphStorage.Zones.Num();
	for (int32 i = 0; i < ZoneNum; ++i)
	{
		const FZoneData& ZoneData = ZoneGraphStorage.Zones[i];

		//�ǽ���·�ڵ�Tag
		if (IntersectionTagFilter.Pass(ZoneData.Tags))
		{
			UE_LOG(LogTrafficSim, Log, TEXT("Zone %d is an intersection"), i);
			IntersectionPoints.Add(ZoneData.Bounds.GetCenter());
			IntersectionZoneIndices.Add(i);
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
		InitSpawnData.Arr_PhaseLanes.Reserve(IntersectionPoints.Num());
		InitSpawnData.Arr_CrossID.Reserve(IntersectionPoints.Num());
		InitSpawnData.Arr_PhaseControlledLanes.Reserve(IntersectionPoints.Num());

		if (DT_PhaseLanes)
		{
			TrafficLightSubsystem->InitializeCrossPhaseLaneInfor(DT_PhaseLanes);
		}


		//auto* AssortedFragmentsTrait = EntityTypes[Result.EntityConfigIndex].EntityConfig->FindTrait<UMassAssortedFragmentsTrait>();
		//InitSpawnData.Periods = TrafficLightPeriod;

		int32 SpawnCount = 0;

		UE_LOG(LogTrafficLight, Log, TEXT("IntersectionPointsNum: %d SpawnNumEntities:%d"), IntersectionPoints.Num(), Result.NumEntities);
		for (int32 LocationIndex = 0; LocationIndex < Result.NumEntities; LocationIndex++)
		{
			InitSpawnData.TrafficLightTransforms.Add(FTransform(IntersectionPoints[LocationIndex]));
			InitSpawnData.ZoneIndex.Add(IntersectionZoneIndices[LocationIndex]);
			InitSpawnData.StartSideIndex.Add(0); //Ĭ�ϴ�·�ڵ�0�߿�ʼ
			
			TMap<FName, TArray<int32>> PhaseLanes;
			TMap<FName, FPhaseLanes> PhaseControlledLanes;
			FName CrossID;
			if(DT_PhaseLanes)
			{
				TrafficLightSubsystem->GetPhaseLanesByZoneIndex(IntersectionZoneIndices[LocationIndex], PhaseLanes, PhaseControlledLanes, CrossID);
			}
			InitSpawnData.Arr_PhaseLanes.Add(PhaseLanes);
			InitSpawnData.Arr_CrossID.Add(CrossID);
			InitSpawnData.Arr_PhaseControlledLanes.Add(PhaseControlledLanes);

			SpawnCount++;
		}

		
	}

	FinishedGeneratingSpawnPointsDelegate.Execute(Results);

}
