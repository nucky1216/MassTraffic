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

	int32 ZoneNum= ZoneGraphStorage->Zones.Num();
	for(int32 i = 0; i < ZoneNum; ++i)
	{
		const FZoneData& ZoneData = ZoneGraphStorage->Zones[i];
		// Check if the zone is an intersection
		if (DAFilter->TagFilter.Pass(ZoneData.Tags))
		{
			FIntersectionData IntersectionData;
			IntersectionData.EntryIndex.Add(i); // Store the zone index as an entry
			
			UE_LOG(LogTrafficLight, Log, TEXT("Zone %d has lane num: %d "), ZoneData.GetLaneCount());

			for(int32 laneIndex=ZoneData.LanesBegin; laneIndex < ZoneData.LanesEnd; ++laneIndex)
			{
				const FZoneLaneData& LaneData = ZoneGraphStorage->Lanes[i];
				UE_LOG(LogTrafficLight, Log, TEXT("lane:%d has StartEntryID:%d"), 
					laneIndex, LaneData.StartEntryId);
				
				
			}
			
		}
	}

}
