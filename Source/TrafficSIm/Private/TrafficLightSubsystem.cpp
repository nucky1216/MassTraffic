#include "TrafficLightSubsystem.h"
#include "ZoneGraphQuery.h"
#include "ZoneGraphRenderingUtilities.h"



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

void UTrafficLightSubsystem::BuildIntersectionsData(FZoneGraphTagFilter IntersectionTag)
{
	IntersectionTagFilter = IntersectionTag;

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
		if (IntersectionTag.Pass(ZoneData.Tags))
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

	//Debug
	for (auto Pair : IntersectionDatas)
	{
		UE_LOG(LogTrafficLight, Log, TEXT("Intersection %d has %d sides"), Pair.Key, Pair.Value.Sides.Num());
		for(auto side: Pair.Value.Sides)
		{
			UE_LOG(LogTrafficLight, Log, TEXT("  Side has %d lanes with aloneSide:%d"), side.Lanes.Num(), side.bIsAloneSide);
			TArray<ETurnType> TurnTypes;
			side.TurnTypeToLanes.GetKeys(TurnTypes);
			for(auto type:TurnTypes)
			{

				FColor Color = FColor::Red;

				if (side.bIsAloneSide)
						Color = FColor::Black;
				DrawDebugDirectionalArrow(GetWorld(), side.SlotPoitions[0], side.SlotPoitions[0] + side.SideDirection * 500.f,
						150, Color, false, 10.0f, 0, 5.0f);
				

				FString TurnTypeStr = UEnum::GetValueAsString(type);
				TArray<int32> Lanes;
				side.TurnTypeToLanes.MultiFind(type, Lanes);
				UE_LOG(LogTrafficLight, Log, TEXT("    Turn Type: %s"), *TurnTypeStr);
				for(auto lane:Lanes)
				{
					UE_LOG(LogTrafficLight, Log, TEXT("------LaneIndex: %d"), lane);
				}
			}
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
		UE_LOG(LogTrafficLight, Log, TEXT("Side %d has %d lanes with aloneSide:%d"), i, side.Lanes.Num(),side.bIsAloneSide);
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

void UTrafficLightSubsystem::DebugDrawState(int32 ZoneIndex,float DebugTime)
{
	FIntersectionData* IntersectionData = IntersectionDatas.Find(ZoneIndex);
	if(!IntersectionData)
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("No intersection data found for ZoneIndex: %d"), ZoneIndex);
		return;
	}
	TArray<int32> OpenLanes = IntersectionData->GetOpenLaneIndex();
	for(auto laneIndex:OpenLanes)
	{
		int32 PointBegin = ZoneGraphStorage->Lanes[laneIndex].PointsBegin;
		int32 PointEnd = ZoneGraphStorage->Lanes[laneIndex].PointsEnd;
		int32 PointMid = (PointBegin + PointEnd) / 2;
		PointEnd--;
		DrawDebugLine(GetWorld(), ZoneGraphStorage->LanePoints[PointBegin]+FVector(0,0,30), ZoneGraphStorage->LanePoints[PointMid] + FVector(0, 0, 30), FColor::Green, false, DebugTime);
		DrawDebugLine(GetWorld(), ZoneGraphStorage->LanePoints[PointMid] + FVector(0, 0, 30), ZoneGraphStorage->LanePoints[PointEnd] + FVector(0, 0, 30), FColor::Green, false, DebugTime);
	}
}

void UTrafficLightSubsystem::SetOpenLanes(int32 ZoneIndex, int32 SideIndex, ETurnType TurnType, bool Reset)
{
	FIntersectionData* IntersectionData = IntersectionDatas.Find(ZoneIndex);
	if(!IntersectionData)
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("No intersection data found for ZoneIndex: %d"), ZoneIndex);
		return;
	}
	
	IntersectionData->SetSideOpenLanes(SideIndex, TurnType, Reset);
}

void UTrafficLightSubsystem::QueryLaneOpenState(int32 LaneIndex, bool& OpenState, bool& IntersectionLane)
{
	IntersectionLane = false;
	OpenState = true;

	if(!ZoneGraphStorage)
	{
		UE_LOG(LogTrafficLight, Error, TEXT("ZoneGraphStorage is not initialized! Cannot query lane open state."));
		return ;
	}
	if (LaneIndex < 0)
	{
		UE_LOG(LogTrafficLight, VeryVerbose, TEXT("Invalid LaneIndex %d! Cannot query lane open state."), LaneIndex);
		return;
	}
	const FZoneLaneData& LaneData = ZoneGraphStorage->Lanes[LaneIndex];

	if (!IntersectionTagFilter.Pass(LaneData.Tags))
	{	
		return;
	}
	IntersectionLane = true;

	int32 ZoneIndex = ZoneGraphStorage->Lanes[LaneIndex].ZoneIndex;
	FIntersectionData* IntersectionData = IntersectionDatas.Find(ZoneIndex);
	if (!IntersectionData)
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("No intersection data found for ZoneIndex: %d"), ZoneIndex);
		return;
	}
	OpenState = IntersectionData->OpenLanes.FindRef(LaneIndex);

	return;
}

void UTrafficLightSubsystem::SetCrossBySignalState(int32 ZoneIndex, ETrafficSignalType SignalType, int32 SideIndex)
{
	FIntersectionData* IntersectionData = IntersectionDatas.Find(ZoneIndex);
	if(!IntersectionData)
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("No intersection data found for ZoneIndex: %d"), ZoneIndex);
		return;
	}
	
	int32 SideNum=IntersectionData->Sides.Num();
	int32 OppositeSideIndex = IntersectionData->Sides[SideIndex].OppositeSideIndex;
	if (SideNum == 4)
	{
		switch (SignalType) {

		case ETrafficSignalType::Straight:
			SetOpenLanes(ZoneIndex, SideIndex, ETurnType::Straight, true);
			SetOpenLanes(ZoneIndex, OppositeSideIndex, ETurnType::Straight, false);
			break;

		case ETrafficSignalType::StraightAndRight:
			SetOpenLanes(ZoneIndex,SideIndex, ETurnType::Straight,true);
			SetOpenLanes(ZoneIndex, SideIndex, ETurnType::RightTurn, false);

			SetOpenLanes(ZoneIndex, (SideIndex + 2) % 4, ETurnType::Straight, false);
			SetOpenLanes(ZoneIndex, OppositeSideIndex, ETurnType::RightTurn, false);
			break;
		case ETrafficSignalType::Left:
			SetOpenLanes(ZoneIndex, SideIndex, ETurnType::LeftTurn, true);
			SetOpenLanes(ZoneIndex, OppositeSideIndex, ETurnType::LeftTurn, false);
			break;

		default:
			SetOpenLanes(ZoneIndex, -1, ETurnType::LeftTurn, true);
			break;
		}
	}
	else if (SideNum == 3)
	{
		FSide& side = IntersectionData->Sides[SideIndex];
		int32 AloneSide = IntersectionData->AloneSide;
		if (!side.bIsAloneSide)
		{
			switch (SignalType) {
			case ETrafficSignalType::Straight:
				SetOpenLanes(ZoneIndex, SideIndex, ETurnType::Straight, true);
				SetOpenLanes(ZoneIndex, OppositeSideIndex, ETurnType::Straight, false);

				//SetOpenLanes(ZoneIndex, AloneSide, ETurnType::RightTurn, false);
				break;
			case ETrafficSignalType::StraightAndRight:
				SetOpenLanes(ZoneIndex, SideIndex, ETurnType::Straight, true);
				SetOpenLanes(ZoneIndex, SideIndex, ETurnType::RightTurn, false);

				SetOpenLanes(ZoneIndex, OppositeSideIndex, ETurnType::Straight, false);
				SetOpenLanes(ZoneIndex, OppositeSideIndex, ETurnType::RightTurn, false);

				//SetOpenLanes(ZoneIndex, AloneSide, ETurnType::RightTurn, false);
				break;
			case ETrafficSignalType::Left:
				SetOpenLanes(ZoneIndex, SideIndex, ETurnType::LeftTurn, true);

				SetOpenLanes(ZoneIndex, OppositeSideIndex, ETurnType::LeftTurn, false);

				SetOpenLanes(ZoneIndex, AloneSide, ETurnType::RightTurn, false);
				break;
			}
		}
		else {
			SetOpenLanes(ZoneIndex, SideIndex, ETurnType::LeftTurn, true);
			SetOpenLanes(ZoneIndex, SideIndex, ETurnType::RightTurn, false);
		}
	}
}
