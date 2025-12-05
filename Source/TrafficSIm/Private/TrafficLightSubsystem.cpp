#include "TrafficLightSubsystem.h"
#include "ZoneGraphQuery.h"
#include "ZoneGraphRenderingUtilities.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "MassEntitySubsystem.h"
#include "CrossPhaseSetProcessor.h"
#include "MassExecutor.h"
#include "MassActorSubsystem.h"
#include "TrafficLightFragment.h"

void UTrafficLightSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTrafficLight, Log, TEXT("TrafficLightSubsystem Initialized"));

	if(GetWorld()->IsGameWorld())
	{
		//BeginPlay时获取ZoneGraphData
		FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UTrafficLightSubsystem::HandleRunWorldInitialized);
	}

	
	//if (GetWorld()->IsEditorWorld())
	//{
	//	//Editor only
	//	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UTrafficLightSubsystem::HandleEditorWorldInitialized);
	//}
}

void UTrafficLightSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.RemoveAll(this);
	Super::Deinitialize();
}

void UTrafficLightSubsystem::HandleRunWorldInitialized(const UWorld::FActorsInitializedParams& Params)
{
	GetZoneGraphaData();
	UE_LOG(LogTrafficLight, Log, TEXT("TrafficLightSubsystem World Initialized"));
}

void UTrafficLightSubsystem::HandleEditorWorldInitialized(UWorld* InWorld, const UWorld::InitializationValues IVS)
{
	GetZoneGraphaData();
	UE_LOG(LogTrafficLight, Log, TEXT("TrafficLightSubsystem Editor World Initialized"));
}

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
	CrossEntityHandleMap.Empty();


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

void UTrafficLightSubsystem::InitialCrossPhaseRow(UDataTable* DataTable, const FName& RowName, FCrossPhaseLaneData RowData)
{
	//CrossPhaseLaneInfor.Empty();

	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTrafficLight, Error, TEXT("ZoneGraphStorage is not initialized! Cannot initialize cross phase row."));
		return;
	}

	
	TArray<int32> LaneIndices;
	for (auto index : RowData.ControlledLaneIndice)
	{
		TArray<int32> NextLaneIndices ;
		GetNextLanesFromPhaseLane(index, NextLaneIndices);
		LaneIndices.Append(NextLaneIndices);

	}
	RowData.LaneIndices = LaneIndices;

	if (RowData.LaneIndices.Num() > 0 && RowData.LaneIndices[0] >= 0)
	{
		RowData.ZoneIndex = ZoneGraphStorage->Lanes[RowData.LaneIndices[0]].ZoneIndex;
		//UE_LOG(LogTrafficLight, Log, TEXT("CrossID:%s ZoneIndex: % d from Lane : % d"), *RowData.CrossID.ToString(), RowData.ZoneIndex, RowData.LaneIndices[0]);
	}
	else
		RowData.ZoneIndex = -1;

	if (RowData.LaneIndices.Contains(-1))
	{
		RowData.ZoneIndex = -1;
	}

	DataTable->AddRow(RowName,RowData);

}

void UTrafficLightSubsystem::DebugCrossPhase(TArray<int32> Lanes)
{
	
	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTrafficLight, Error, TEXT("ZoneGraphStorage is not initialized! Cannot debug cross phase."));
		return;
	}
	
	for(auto laneIndex: Lanes)
	{

		int32 PointBegin = ZoneGraphStorage->Lanes[laneIndex].PointsBegin;
		int32 PointEnd = ZoneGraphStorage->Lanes[laneIndex].PointsEnd;
		int32 PointMid = (PointBegin + PointEnd) / 2;
		PointEnd--;
		DrawDebugLine(GetWorld(), ZoneGraphStorage->LanePoints[PointBegin] + FVector(0, 0, 30), ZoneGraphStorage->LanePoints[PointMid] + FVector(0, 0, 30), FColor::Red, false, 10.0f);
		DrawDebugLine(GetWorld(), ZoneGraphStorage->LanePoints[PointMid] + FVector(0, 0, 30), ZoneGraphStorage->LanePoints[PointEnd] + FVector(0, 0, 30), FColor::Red, false, 10.0f);
	}
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

void UTrafficLightSubsystem::GetPhaseLanesByZoneIndex(int32 ZoneIndex, TMap<FName, TArray<int32>>& PhaseLanes, TMap<FName, TArray<int32>>& ControlledLanes, FName& CrossID)
{
	if (CrossPhaseLaneInfor.Num() == 0)
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("GetPhaseLanesByZoneIndex: Empty CrossPhaseLaneInfor!"));
		return;
	}
	UE_LOG(LogTrafficLight, Log, TEXT("GetPhaseLanesByZoneIndex: Searching for ZoneIndex:%d in CrossPhaseLaneInfor"), ZoneIndex);
	for (auto Pair : CrossPhaseLaneInfor)
	{
		if (Pair.Value.Get<0>().ZoneIndex == ZoneIndex)
		{
			PhaseLanes.Add(Pair.Value.Get<0>().PhaseName, Pair.Value.Get<0>().PhaseLanes);
			ControlledLanes.Add(Pair.Value.Get<0>().PhaseName, Pair.Value.Get<1>());
			CrossID = Pair.Value.Get<0>().CrossID;
			UE_LOG(LogTrafficLight, Log, TEXT("GetPhaseLanesByZoneIndex: Found PhaseName:%s with %d lanes for ZoneIndex:%d as CrossID:%s"), 
				*Pair.Value.Get<0>().PhaseName.ToString(), Pair.Value.Get<0>().PhaseLanes.Num(), ZoneIndex, *CrossID.ToString());
		}
	}
}

void UTrafficLightSubsystem::RegisterCrossEntity(FName CrossName, FMassEntityHandle EntityHandle)
{
	CrossEntityHandleMap.Add(CrossName, EntityHandle);
}

void UTrafficLightSubsystem::InitializeCrossPhaseLaneInfor(UDataTable* DataTable)
{
	CrossPhaseLaneInfor.Empty();
	//UE_LOG(LogTrafficLight, Log, TEXT("Initializing CrossPhaseLaneInfor from DataTable: %s RowStructName:%s"), *DataTable->GetName(), *DataTable->GetRowStructPathName().ToString());
	

	for(auto& Row : DataTable->GetRowMap())
	{
		FCrossPhaseLaneData* RowData = (FCrossPhaseLaneData*)(Row.Value);
		if(!RowData)
		{
			continue;
		}
		FPhaseLanes PhaseLanes;
		PhaseLanes.PhaseName = RowData->PhaseName;
		PhaseLanes.CrossID = RowData->CrossID;
		PhaseLanes.ZoneIndex = RowData->ZoneIndex;
		PhaseLanes.PhaseLanes = RowData->LaneIndices;
		FName Key = Row.Key;
		CrossPhaseLaneInfor.Add(Key,MakeTuple(PhaseLanes,RowData->ControlledLaneIndice));
		UE_LOG(LogTrafficLight, Log, TEXT("  Added CrossPhaseLaneInfor Key:%s CrossID:%s ZoneIndex:%d PhaseName:%s with %d lanes"), *Key.ToString(), *PhaseLanes.CrossID.ToString(), PhaseLanes.ZoneIndex, *PhaseLanes.PhaseName.ToString(), PhaseLanes.PhaseLanes.Num());
	}
}

void UTrafficLightSubsystem::SetCrossPhaseQueue(FString JsonStr)
{
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if(!FJsonSerializer::Deserialize(Reader,JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTrafficLight, Error, TEXT("TraffiLightSubsystem::SetCrossPhaseQueue: Failed to deserialize JSON string"));
		return;
	}


	TSharedPtr<FJsonValue> CrossIDValue=JsonObject->TryGetField(FString(TEXT("cross_id")));
	if (!CrossIDValue)
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("TraffiLightSubsystem::SetCrossPhaseQueue: Failed to Get Value by Key:cross_id"));
		return;
	}
	FName CrossID = FName(*CrossIDValue->AsString());

	const TArray<TSharedPtr<FJsonValue>>* PhaseArrayValue = nullptr;
	if (!JsonObject->TryGetArrayField(TEXT("phase_queue"), PhaseArrayValue) || !PhaseArrayValue)
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("TraffiLightSubsystem::SetCrossPhaseQueue: Failed to Get Value by Key:phase_queue"));
		return;
	}
	
	TArray<TTuple<FName, double, double>> PhaseArray;
	for (auto& Elem : *PhaseArrayValue)
	{
		FString PhaseName = Elem->AsObject()->GetStringField("phase");
		FString StartTimeStr = Elem->AsObject()->GetStringField("phase_start_time");
		FString EndTimeStr = Elem->AsObject()->GetStringField("phase_end_time");

		FDateTime StartTime;
		FDateTime::Parse(StartTimeStr, StartTime);
		double StartTimeUnix = StartTime.ToUnixTimestampDecimal();

		FDateTime EndTime;
		FDateTime::Parse(EndTimeStr, EndTime);
		double EndTimeUnix = EndTime.ToUnixTimestampDecimal();	

		PhaseArray.Add(MakeTuple(FName(PhaseName), StartTimeUnix, EndTimeUnix));
	}

	UMassEntitySubsystem* EntitySubysem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if(!EntitySubysem)
	{
		UE_LOG(LogTrafficLight, Error, TEXT("TraffiLightSubsystem::SetCrossPhaseQueue: Failed to get MassEntitySubsystem"));
		return;
	}
	UCrossPhaseSetProcessor* Processor = NewObject<UCrossPhaseSetProcessor>(this);
	if(CrossEntityHandleMap.Find(CrossID)==nullptr)
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("TraffiLightSubsystem::SetCrossPhaseQueue: No EntityHandle found for CrossID:%s"), *CrossID.ToString());
		return;
	}
	Processor->EntityHandle=CrossEntityHandleMap[CrossID];
	Processor->PhaseArray = PhaseArray;

	FMassEntityManager& EntityManager = EntitySubysem->GetMutableEntityManager();
	FMassProcessingContext ProcessingContext(EntityManager, 0.f);
	UE::Mass::Executor::Run(*Processor, ProcessingContext);
}

void UTrafficLightSubsystem::GetCrossPhaseState(AActor* CrossActor, FName& Phase, float& LeftTime)
{
	Phase = TEXT("None");
	LeftTime = 0.f;
	if(IsValid(CrossActor)==false)
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("TraffiLightSubsystem::GetCrossPhaseState: Invalid CrossActor!"));
		return;
	}
	// 获取MassActorSubsystem
	UMassActorSubsystem* MassActorSubsystem = CrossActor->GetWorld()->GetSubsystem<UMassActorSubsystem>();
	if (!MassActorSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetVehIDFromActor: MassActorSubsystem not found"));
		return ;
	}
	// 获取MassEntitySubsystem
	UMassEntitySubsystem* MassEntitySubsystem = CrossActor->GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!MassEntitySubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetVehIDFromActor: MassEntitySubsystem not found"));
		return;
	}

	FMassEntityHandle EntityHandle = MassActorSubsystem->GetEntityHandleFromActor(CrossActor);
	if (!EntityHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("GetVehIDFromActor: No valid entity handle found for actor %s"),
			*CrossActor->GetName());
		return ;
	}
	const FTrafficLightFragment* PhaseFragment =MassEntitySubsystem->GetEntityManager().GetFragmentDataPtr<FTrafficLightFragment>(EntityHandle);
	if (!PhaseFragment)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetVehIDFromActor: No FTrafficLightFragment found for actor %s"),
			*CrossActor->GetName());
		return ;
	}
	LeftTime = PhaseFragment->TimeInDuration;
	if(PhaseFragment->PhaseControll==false)
	{
		//UE_LOG(LogTrafficLight, Warning, TEXT("TraffiLightSubsystem::GetCrossPhaseState: PhaseControll is false for actor %s"),
		//	*CrossActor->GetName());
		return;
	}
	Phase = PhaseFragment->CurrentPhase;
	
}

void UTrafficLightSubsystem::GetCrossPhaseCtlLanes(AActor* CrossActor, TMap<int32, FTransform>& CtlLanes, TMap<FName, FArrayInt>& PhaseToCtlLanes)
{
 
	if (IsValid(CrossActor) == false)
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("GetCrossPhaseCtlLanes::GetCrossPhaseState: Invalid CrossActor!"));
		return;
	}
	// 获取MassActorSubsystem
	UMassActorSubsystem* MassActorSubsystem = CrossActor->GetWorld()->GetSubsystem<UMassActorSubsystem>();
	if (!MassActorSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetCrossPhaseCtlLanes: MassActorSubsystem not found"));
		return;
	}
	// 获取MassEntitySubsystem
	UMassEntitySubsystem* MassEntitySubsystem = CrossActor->GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!MassEntitySubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetCrossPhaseCtlLanes: MassEntitySubsystem not found"));
		return;
	}

	FMassEntityHandle EntityHandle = MassActorSubsystem->GetEntityHandleFromActor(CrossActor);
	if (!EntityHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("GetCrossPhaseCtlLanes: No valid entity handle found for actor %s"),
			*CrossActor->GetName());
		return;
	}
	const FTrafficLightFragment* PhaseFragment = MassEntitySubsystem->GetEntityManager().GetFragmentDataPtr<FTrafficLightFragment>(EntityHandle);
	if (!PhaseFragment)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetCrossPhaseCtlLanes: No FTrafficLightFragment found for actor %s"),
			*CrossActor->GetName());
		return;
	}
	CtlLanes = PhaseFragment->CtlLaneTransforms;
	for (auto Pair : PhaseFragment->PhaseControlledLanes)
	{
		PhaseToCtlLanes.Add(Pair.Key, FArrayInt(Pair.Value));
	}

}

void UTrafficLightSubsystem::GetNextLanesFromPhaseLane(int32 CurLaneIndex, TArray<int32>& NextLanes)
{
	if(ZoneGraphStorage==nullptr)
	{
		UE_LOG(LogTrafficLight, Error, TEXT("ZoneGraphStorage is not initialized! Cannot get next lanes from phase lane."));
		return;
	}
	if(CurLaneIndex<0 || CurLaneIndex>=ZoneGraphStorage->Lanes.Num())
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("Invalid CurLaneIndex %d! Cannot get next lanes from phase lane."), CurLaneIndex);
		return;
	}

	FZoneLaneData LaneData = ZoneGraphStorage->Lanes[CurLaneIndex];
	for (int32 i = LaneData.LinksBegin;i<LaneData.LinksEnd; ++i)
	{
		FZoneLaneLinkData LinkData = ZoneGraphStorage->LaneLinks[i];
		if(LinkData.Type==EZoneLaneLinkType::Outgoing)
		{
			NextLanes.Add(LinkData.DestLaneIndex);

		}
	}
}

void UTrafficLightSubsystem::SetPhaseLanesOpened(int32 ZoneIndex, TArray<int32> PhaseLanes)
{
	FIntersectionData* IntersectData=IntersectionDatas.Find(ZoneIndex);

	if (IntersectData)
	{
		IntersectData->SetOpenLanes(PhaseLanes);
	}
	else
	{
		UE_LOG(LogTrafficLight, Warning, TEXT("SetPhaseLaneOpened: No intersection data found for ZoneIndex: %d"), ZoneIndex);
	}
}

