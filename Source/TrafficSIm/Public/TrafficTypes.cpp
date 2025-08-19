#include "TrafficTypes.h"


DEFINE_LOG_CATEGORY(LogTrafficLight);

void FIntersectionData::SideAddLane(FZoneGraphStorage* ZoneGraphStorage,int32 LaneIndex)
{
	FZoneLaneData& Lane = ZoneGraphStorage->Lanes[LaneIndex];
	int32 index=Lane.StartEntryId;
	if(Sides.Num() <= index)
	{
		for (int32 i = Sides.Num(); i <= index; i++)
		{
			FSide newSide;
			Sides.Add(newSide);
		}
	}
	FSide& side = Sides[index];
	side.Lanes.Add(LaneIndex);
	side.AddSlot(ZoneGraphStorage, LaneIndex);

}

void FIntersectionData::SideSortLanes(FZoneGraphStorage* ZoneGraphStorage)
{
	if (!ZoneGraphStorage) return;

	int32 sideCount = Sides.Num();

	for (FSide& side : Sides)
	{
		for(int32 i = 0; i < side.Lanes.Num(); i++)
		{
			int32 laneIndex = side.Lanes[i];
			FZoneLaneData& lane = ZoneGraphStorage->Lanes[laneIndex];

			//4-side intersection
			if (sideCount == 4)
			{
				if ((lane.StartEntryId + 1) % 4 == lane.EndEntryId)
				{
					side.RightLanes.Add(laneIndex);
				}
			}
		}
	
	}

}

void FSide::AddSlot(FZoneGraphStorage* ZoneGraphStorage,int32 LaneIndex)
{
	int32 StartLocaitonIndex = ZoneGraphStorage->Lanes[LaneIndex].PointsBegin;
	FVector StartLocation = ZoneGraphStorage->LanePoints[StartLocaitonIndex];

	int32 NewIndex=SlotPoitions.AddUnique(StartLocation);
	LaneToSlotIndex.Add(LaneIndex, NewIndex);
}
