// Fill out your copyright notice in the Description page of Project Settings.


#include "TrafficSimSubsystem.h"
#include "EngineUtils.h"
#include "MassCommonFragments.h"
#include "ZoneGraphQuery.h"
#include "MassVehicleMovementFragment.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntitySubsystem.h"

DEFINE_LOG_CATEGORY(LogTrafficSim);


int32 UTrafficSimSubsystem::ChooseNextLane(FZoneGraphLaneLocation CurLane) const
{
	const FZoneLaneData& CurLaneData = ZoneGraphStorage->Lanes[CurLane.LaneHandle.Index];

	int32 FirstLink = CurLaneData.LinksBegin;
	int32 EndLink = CurLaneData.LinksEnd;

	TArray<int32> CandidateLinks;
	for (int32 LinkIndex = FirstLink; LinkIndex < EndLink; ++LinkIndex)
	{
		const FZoneLaneLinkData& LaneLink = ZoneGraphStorage->LaneLinks[LinkIndex];

		//TODO:: 根据lane类型找到左右转车道
		//..
		if(LaneLink.Type == EZoneLaneLinkType::Outgoing)
		{
			CandidateLinks.Add(LaneLink.DestLaneIndex);
		}
	}

	if (CandidateLinks.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("Current lane:%d has no links, cannot choose next lane."), CurLane.LaneHandle.Index);
		return -1;
	}

	int32 RandomIndex = CandidateLinks[FMath::RandHelper(CandidateLinks.Num())];

	return RandomIndex;
}

void UTrafficSimSubsystem::FindEntityLaneByQuery(FBox QueryBox, FZoneGraphTagFilter& TagFilter, FZoneGraphLaneLocation& OutLaneLocation)
{
	float DistSq;

	bool bFound = UE::ZoneGraph::Query::FindNearestLane(
		*ZoneGraphStorage, QueryBox, TagFilter, OutLaneLocation, DistSq);

	if (!bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("No suitable lane found for QueryBox %s"), *QueryBox.ToString());
	}

}


void UTrafficSimSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTrafficSim, Log,TEXT("Subsystem Initializing.."));

	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UTrafficSimSubsystem::InitOnPostLoadMap);

}

void UTrafficSimSubsystem::SpawnMassEntities(int32 NumEntities, int32 TargetLane, UMassEntityConfigAsset* EntityConfigAsset)
{
	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("ZoneGraphStorage is not initialized! Cannot spawn entities."));
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);


	FZoneGraphTagFilter TagFilter;
	//获取配置文件信息
	const FMassEntityTemplate& Template = EntityConfigAsset->GetOrCreateEntityTemplate(*World);

	float MaxGap = 0.0f, MinGap = 0.f;

	for (auto fragment : Template.GetInitialFragmentValues())
	{
		if (fragment.GetScriptStruct() == FMassVehicleMovementFragment::StaticStruct())
		{
			const FMassVehicleMovementFragment& VehicleMovementFragment = fragment.Get<FMassVehicleMovementFragment>();
			MaxGap = VehicleMovementFragment.MaxGap;
			MinGap = VehicleMovementFragment.MinGap;
			TagFilter = VehicleMovementFragment.LaneFilter;
			break;
		}
	}

	const FZoneLaneData& LaneData= ZoneGraphStorage->Lanes[TargetLane];
	const FZoneData& ZoneData=ZoneGraphStorage->Zones[LaneData.ZoneIndex];

	if (!TagFilter.Pass(LaneData.Tags))
	{
		UE_LOG(LogTrafficSim, Error, TEXT("Target lane %d does not match the tag filter! Cannot spawn entities."), TargetLane);
		return;
	}

	int32 BeginLane = ZoneData.LanesBegin;
	int32 EndLane = ZoneData.LanesEnd-1;


	int32 LaneMid = (BeginLane + EndLane) / 2;
	
	if (TargetLane > LaneMid)
	{
		BeginLane = LaneMid+1;
	}
	else {
		EndLane = LaneMid;
	}
	UE_LOG(LogTrafficSim, Log, TEXT("TargetLane:%d, BeginLane:%d, EndLane:%d, LaneMid:%d"),
		TargetLane, BeginLane, EndLane, LaneMid);
	//在道路一侧生成车辆实体

	TArray<float> LaneLenths;
	TArray<float> LaneOffsets;
	//计算每条车道的长度和偏移量
	for(int32 index=BeginLane; index<=EndLane; ++index)
	{
		FZoneGraphLaneLocation Location;
		const FZoneLaneData& CandLaneData = ZoneGraphStorage->Lanes[index];
		float laneLenth = 0.0f;
		UE::ZoneGraph::Query::GetLaneLength(*ZoneGraphStorage, index, laneLenth);
		LaneLenths.Add(laneLenth);
		LaneOffsets.Add(FMath::RandRange(MinGap,MaxGap));
	}
	//采样生成位置
	TArray<FVector> SpawnLocations;
	for (int32 i = 0,SpawnCount=0; SpawnCount < NumEntities; )
	{
		float offset = LaneOffsets[i];
		
		if(offset>0 && offset<LaneLenths[i])
		{
			FZoneGraphLaneLocation LaneLocation;
			UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, BeginLane+i, offset, LaneLocation);

			LaneOffsets[i] = offset + FMath::RandRange(MinGap, MaxGap);

			if(LaneOffsets[i]>=LaneLenths[i])
			{
				LaneOffsets[i] =-1.f;
			}
			SpawnLocations.Add(LaneLocation.Position);
			DrawDebugPoint(World, LaneLocation.Position, 10.0f, FColor::Red, false, 5.0f);
			SpawnCount++;
		}

		bool NoSpace = true;
		for(int32 j=0;j<LaneOffsets.Num();++j)
		{
			if(LaneOffsets[j]>0.f)
			{
				NoSpace = false;
				break;
			}
		}
		if(NoSpace)
		{
			UE_LOG(LogTrafficSim, Warning, TEXT("No space left to spawn entities in the selected lanes."));
			break;
		}

		i = (i + 1) % (LaneOffsets.Num());
	}

	//生成实体
	UMassSpawnerSubsystem* SpawnerSubsystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(World);
	if (!SpawnerSubsystem)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("Mass Spawner Subsystem is not available! Cannot spawn entities."));
		return;
	}
	TArray<FMassEntityHandle> SpawnedEntities;
	SpawnerSubsystem->SpawnEntities(Template, SpawnLocations.Num(), SpawnedEntities);

	for(int32 i=0;i<SpawnedEntities.Num(); ++i)
	{
		FMassEntityHandle Entity = SpawnedEntities[i];
		FVector SpawnPos = SpawnLocations[i];

		FTransformFragment LocationFragment;
		LocationFragment.SetTransform(FTransform(SpawnPos));

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(LocationFragment));

		EntitySubsystem->SetEntityFragmentsValues(Entity, Fragments);
	}

}

void UTrafficSimSubsystem::InitOnPostLoadMap(UWorld* LoadedWorld, const UWorld::InitializationValues IVS)
{
	UE_LOG(LogTrafficSim, Log, TEXT("Post Init.."));
	World = GetWorld();
	ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(World);

	for (TActorIterator<AZoneGraphData> It(World); It; ++It)
	{
		const AZoneGraphData* ZoneGraphData = *It;
		if (ZoneGraphData && ZoneGraphData->IsValidLowLevel())
		{
			ZoneGraphStorage = &ZoneGraphData->GetStorage();
			return;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("No valid ZoneGraphData found in the world!"));
}
