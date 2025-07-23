// Fill out your copyright notice in the Description page of Project Settings.


#include "TrafficSimSubsystem.h"
#include "EngineUtils.h"
#include "ZoneGraphQuery.h"

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
		UE_LOG(LogTrafficSim, Warning, TEXT("Current lane has no links, cannot choose next lane."));
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
