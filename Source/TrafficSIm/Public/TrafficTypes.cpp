#include "TrafficTypes.h"
#include "ZoneGraphQuery.h"
DEFINE_LOG_CATEGORY(LogTrafficLight);

void FIntersectionData::SideAddLane(const FZoneGraphStorage* ZoneGraphStorage,int32 LaneIndex)
{
	const FZoneLaneData& Lane = ZoneGraphStorage->Lanes[LaneIndex];
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

void FIntersectionData::SideSortLanes(const FZoneGraphStorage* ZoneGraphStorage)
{
	if (!ZoneGraphStorage) return;

	int32 sideCount = Sides.Num();

	if (sideCount == 3)
	{
		FindAloneSide(ZoneGraphStorage);
	}

	for (FSide& side : Sides)
	{
		//Init Slot TurnType
		int32 SlotNum = side.SlotPoitions.Num();
		for (int32 i = 0; i < SlotNum; i++ )
		{
			ESlotTurnType SlotTurnType = ESlotTurnType::Straight;
			if (i == 0)
			{
				if (SlotNum >= 4)
					SlotTurnType = ESlotTurnType::RightTurn;
				else 
					SlotTurnType = ESlotTurnType::StraightRight	;
			}

			if(i==SlotNum-1)
			{
				if (SlotNum >= 4)
					SlotTurnType = ESlotTurnType::LeftTurn;
				else
					SlotTurnType = ESlotTurnType::StraightLeft;
			}

			side.SlotTurnTypes.Add(SlotTurnType);
		}


		for(int32 i = 0; i < side.Lanes.Num(); i++)
		{
			int32 laneIndex = side.Lanes[i];
			const FZoneLaneData& lane = ZoneGraphStorage->Lanes[laneIndex];

			//4-side intersection
			if (sideCount == 4)
			{
				if ((lane.StartEntryId + 3) % 4 == lane.EndEntryId)
				{
					side.TurnTypeToLanes.Add(ETurnType::RightTurn, laneIndex);
				}
				else if ((lane.StartEntryId + 1) % 4 == lane.EndEntryId)
				{
					side.TurnTypeToLanes.Add(ETurnType::LeftTurn, laneIndex);
				}
				else if ((lane.StartEntryId + 2) % 4 == lane.EndEntryId)
				{
					side.TurnTypeToLanes.Add(ETurnType::Straight, laneIndex);
				}
				side.OppositeSideIndex = (lane.StartEntryId + 2) % 4;
			}
			//TODO::3-side intersection
			if (sideCount == 3)
			{
				ETurnType TurnType = ETurnType::Straight;
				
				//Current Side is alone side(has no opposite side)
				if (side.bIsAloneSide)
				{
					if(lane.EndEntryId==(lane.StartEntryId+1)%3)
					{
						TurnType = ETurnType::LeftTurn;
					}
					else if (lane.EndEntryId == (lane.StartEntryId + 2) % 3)
					{
						TurnType = ETurnType::RightTurn;
					}
				}
				//Current Side is not alone side(has opposite side) and EndEntry is in alone side
				else if(Sides[lane.EndEntryId].bIsAloneSide)
				{
					if (lane.EndEntryId == (lane.StartEntryId + 1) % 3)
					{
						TurnType = ETurnType::LeftTurn;
					}
					else if (lane.EndEntryId == (lane.StartEntryId + 2) % 3)
					{
						TurnType = ETurnType::RightTurn;
					}
				}
				side.TurnTypeToLanes.Add(TurnType, laneIndex);
			}
			
			OpenLanes.Add(laneIndex, false);
		}
		
		
		side.Periods.Add(ETrafficSignalType::StraightAndRight, 10);
		side.Periods.Add(ETrafficSignalType::StraightYellow, 3.0);

		TArray<ETurnType> TurnTypes;
		side.TurnTypeToLanes.GetKeys(TurnTypes);

		if (TurnTypes.Contains(ETurnType::LeftTurn))
		{
			side.Periods.Add(ETrafficSignalType::Left, 10);
			side.Periods.Add(ETrafficSignalType::LeftYellow, 3.0);
		}

	}

}

void FIntersectionData::FindAloneSide(const FZoneGraphStorage* ZoneGraphStorage)
{
	for (int32 i = 0; i < Sides.Num(); i++)
	{
		FSide& side = Sides[i];
		const FZoneLaneData& lane = ZoneGraphStorage->Lanes[side.Lanes[0]];
		FZoneGraphLaneLocation StartLocation;
		UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, side.Lanes[0], 0.f, StartLocation);

		side.SideDirection = StartLocation.Direction;		
	}
	for (int32 i = 0; i < Sides.Num(); i++)
	{
		FSide& CurSide = Sides[i];
		CurSide.bIsAloneSide = true;
		for (int32 j = 0; j < Sides.Num(); j++)
		{
			if (i == j)
				continue;
			FSide& TargetSide = Sides[j];
			float Angle= FMath::RadiansToDegrees(FMath::Acos(TargetSide.SideDirection.Dot(CurSide.SideDirection)));
			if (Angle >= 130.f)
			{
				CurSide.bIsAloneSide = false;
				CurSide.OppositeSideIndex = j;
				break;
			}
		}
		if(CurSide.bIsAloneSide)
		{
			AloneSide = i;
		}
	}
}

void FIntersectionData::SetSideOpenLanes(int32 SideIndex, ETurnType TurnType, bool Reset)
{
	if (SideIndex >= Sides.Num()) return;

	if(Reset)
	{
		for (auto& Elem :OpenLanes)
		{
			Elem.Value = false;
		}
	}

	FSide& side = Sides[SideIndex];

	TArray<int32> lanes;
	side.TurnTypeToLanes.MultiFind(TurnType, lanes);

	for(int32 laneIndex:lanes)
	{
		OpenLanes.Add(laneIndex,true);
	}

}

TArray<int32> FIntersectionData::GetAllLaneIndex()
{
	TArray<int32> AllLaneIndex;
	OpenLanes.GetKeys(AllLaneIndex);
	return AllLaneIndex;
}

TArray<int32> FIntersectionData::GetOpenLaneIndex()
{
	TArray<int32> AllOpenLaneIndex;
	
	for (auto& Elem :OpenLanes)
	{
		if (Elem.Value)
		{
			AllOpenLaneIndex.Add(Elem.Key);
		}
	}
	return AllOpenLaneIndex;
}

void FSide::AddSlot(const FZoneGraphStorage* ZoneGraphStorage,int32 LaneIndex)
{
	int32 StartLocaitonIndex = ZoneGraphStorage->Lanes[LaneIndex].PointsBegin;
	FVector StartLocation = ZoneGraphStorage->LanePoints[StartLocaitonIndex];

	int32 NewIndex=SlotPoitions.AddUnique(StartLocation);
	LaneToSlotIndex.Add(LaneIndex, NewIndex);

}
