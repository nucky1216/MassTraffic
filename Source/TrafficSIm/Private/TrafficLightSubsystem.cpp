#include "TrafficLightSubsystem.h"
#include "ZoneGraphQuery.h"



void UTrafficLightSubsystem::GetZoneGraphaData()
{
	TrafficSimSubsystem = UWorld::GetSubsystem<UTrafficSimSubsystem>(GetWorld());
	if(!TrafficSimSubsystem)
	{
		UE_LOG(LogTrafficLight, Error, TEXT("TrafficSimSubsystem is not initialized! Cannot get ZoneGraphData."));
		return;
	}
	ZoneGraphStorage = TrafficSimSubsystem->ZoneGraphStorage;
	if(!ZoneGraphStorage)
	{
		UE_LOG(LogTrafficLight, Error, TEXT("ZoneGraphStorage is not initialized! Cannot get ZoneGraphData."));
		return;
	}
}

void UTrafficLightSubsystem::BuildIntersectionsData(UTagFilter* DAFilter)
{
	GetZoneGraphaData();
	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTrafficLight, Error, TEXT("ZoneGraphStorage is not initialized! Cannot build intersections data."));
		return;
	}
	IntersectionDatas.Empty(); // Clear previous data

	int32 ZoneNum= ZoneGraphStorage->Zones.Num();
	for(int32 i = 0; i < ZoneNum; ++i)
	{
		const FZoneData& ZoneData = ZoneGraphStorage->Zones[i];
		// Check if the zone is an intersection
		if (DAFilter->TagFilter.Pass(ZoneData.Tags))
		{
			FIntersectionData IntersectionData;
						
			UE_LOG(LogTrafficLight, Log, TEXT("Zone %d has lane num: %d "), i,ZoneData.GetLaneCount());
			
			for(int32 laneIndex=ZoneData.LanesBegin; laneIndex < ZoneData.LanesEnd; ++laneIndex)
			{
				IntersectionData.SideAddLane(ZoneGraphStorage,laneIndex);
			}
			IntersectionData.SideSortLanes(ZoneGraphStorage);
			IntersectionDatas.Add(i,IntersectionData);
		}
			
	}
	

}

void UTrafficLightSubsystem::LookUpIntersection(int32 ZoneIndex)
{
	FIntersectionData* IntersectionData = IntersectionDatas.Find(ZoneIndex);
	if(!IntersectionData)
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("No intersection data found for ZoneIndex: %d"), ZoneIndex);
		return;
	}
	// Process the intersection data as needed
	for (int32 i=0;i<IntersectionData->Sides.Num();i++)
	{
		const FSide& side = IntersectionData->Sides[i];
		UE_LOG(LogTrafficLight, Log, TEXT("Side %d has %d lanes"), i, side.Lanes.Num());
		TArray<ETurnType> TurnTypes;
		side.TurnTypeToLanes.GetKeys(TurnTypes);;

		for(auto type:TurnTypes)
		{
			FString TurnTypeStr = UEnum::GetValueAsString(type);
			TArray<int32> Lanes;
			side.TurnTypeToLanes.MultiFind(type, Lanes);
			UE_LOG(LogTrafficLight, Log, TEXT("  Turn Type: %s"), *TurnTypeStr);
			for(auto lane:Lanes)
			{
				UE_LOG(LogTrafficLight, Log, TEXT("------LaneIndex: %d"), lane);
			}
		}
	}
}
