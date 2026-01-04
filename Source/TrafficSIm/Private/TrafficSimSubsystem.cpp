// Fill out your copyright notice in the Description page of Project Settings.


#include "TrafficSimSubsystem.h"
#include "EngineUtils.h"
#include "MassCommonFragments.h"
#include "ZoneGraphQuery.h"
#include "MassVehicleMovementFragment.h"
#include "MassSpawnerSubsystem.h"
#include "MassSpawnLocationProcessor.h"
#include "MassEntitySubsystem.h"
#include "TrafficLightSubsystem.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "VehicleDeletorProcessor.h"
#include "MassExecutor.h"
#include "LaneCongestionAdjustProcessor.h"
#include "ClearTagedEntitiesProcessor.h"
#include "Algo/MaxElement.h" 
#include "MassRepresentationSubsystem.h"
#include "TrafficCommonFragments.h"
#include "Engine/InstancedStaticMesh.h"
#include "MassActorSubsystem.h"
DEFINE_LOG_CATEGORY(LogTrafficSim);


void UTrafficSimSubsystem::InitializeLaneToEntitiesMap()
{
	//TODO:: 高效的结合Processor来管理车道到车辆映射

	LaneToEntitiesMap.Empty();
	if(!ZoneGraphStorage)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("ZoneGraphStorage is not initialized! Cannot initialize lane to entities map."));
		return;
	}
	for (int32 i = 0;i< ZoneGraphStorage->Lanes.Num();i++)
	{
		LaneToEntitiesMap.Add(i, TArray<FLaneVehicle>());
	}
	CollectAdjMergeLanes();
}

int32 UTrafficSimSubsystem::ChooseNextLane(int32 CurLaneIndex,TArray<int32>& NextLanes) const
{
	if (CurLaneIndex < 0)
	{
		UE_LOG(LogTrafficSim, VeryVerbose, TEXT("Invalid current lane index: %d"), CurLaneIndex);
		return -1;
	}

	const FZoneLaneData& CurLaneData = ZoneGraphStorage->Lanes[CurLaneIndex];

	int32 FirstLink = CurLaneData.LinksBegin;
	int32 EndLink = CurLaneData.LinksEnd;

	
	for (int32 LinkIndex = FirstLink; LinkIndex < EndLink; ++LinkIndex)
	{
		const FZoneLaneLinkData& LaneLink = ZoneGraphStorage->LaneLinks[LinkIndex];

		//TODO:: 根据lane类型找到左右转车道
		//..
		if(LaneLink.Type == EZoneLaneLinkType::Outgoing)
		{
			NextLanes.Add(LaneLink.DestLaneIndex);
		}

	}

	if (NextLanes.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Verbose, TEXT("Current lane:%d has no links, cannot choose next lane."), CurLaneIndex);
		return -1;
	}

	int32 RandomIndex = NextLanes[FMath::RandHelper(NextLanes.Num())];

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

	//FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UTrafficSimSubsystem::InitOnPostLoadMap);
	FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UTrafficSimSubsystem::InitOnPostLoadMap);

	//if(GetWorld()->IsEditorWorld())
	//{
	//	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UTrafficSimSubsystem::InitOnPostEditorWorld);
	//}
}

void UTrafficSimSubsystem::Deinitialize()
{
	LaneToEntitiesMap.Empty();
	AdjMergeLanes.Empty();
	ZoneGraphStorage = nullptr;
	mutableZoneGraphSotrage = nullptr;
	ZoneGraphData = nullptr;

	Super::Deinitialize();
}

void UTrafficSimSubsystem::GetZonesSeg(TArray<FVector> Points, FZoneGraphTag AnyTag,float Height, FDTRoadLanes& RoadLanes)
{
	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Get Invalid ZoneGraphStorage!"));
		return;
	}
	FZoneGraphTagFilter TagFilter;
	TagFilter.AnyTags.Add(AnyTag);

	TArray<int32> LaneIndiceSingle;
	TArray<TTuple<float, float>> StartEnd;
	UE::ZoneGraph::Query::FindNearestLanesBySeg(*ZoneGraphStorage, Points, Height, TagFilter, LaneIndiceSingle, StartEnd);

	//Debug Draw
	//for(int32 i=0;i< LaneIndiceSingle.Num();i++)
	//{
	//	FZoneGraphLaneLocation Start, End;
	//	UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, LaneIndiceSingle[i],StartEnd[i].Get<0>(), Start);
	//	UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, LaneIndiceSingle[i], StartEnd[i].Get<1>(), End);
	//	UE_LOG(LogTemp, Log, TEXT("LaneIndex:%d,Start:%f,End:%f"), LaneIndiceSingle[i], StartEnd[i].Get<0>(), StartEnd[i].Get<1>());
	//	DrawDebugDirectionalArrow(GetWorld(), Start.Position+FVector(0,0,300), End.Position + FVector(0, 0, 300), 3000, FColor::Blue, false, 30.f, 0, 35);
	//}
	for (int32 i=0;i<LaneIndiceSingle.Num() ;i++)
	{
		const FZoneLaneData& LaneData=ZoneGraphStorage->Lanes[LaneIndiceSingle[i]];
		const FZoneData& Zone = ZoneGraphStorage->Zones[LaneData.ZoneIndex];
	 
		FZoneSegLane SegLanes;
		SegLanes.StartDist=StartEnd[i].Get<0>();
		SegLanes.EndDist=StartEnd[i].Get<1>();

		TArray<int32>& VisitedLanes=SegLanes.Lanes;
		
		if(!Zone.Tags.Contains(ConnectorTag))
		{
			TQueue<int32> NeibourLanes;
			NeibourLanes.Enqueue(LaneIndiceSingle[i]);
			while(!NeibourLanes.IsEmpty())
			{
				int32 CurLaneIndex;
				NeibourLanes.Dequeue(CurLaneIndex);

				VisitedLanes.Add(CurLaneIndex);

				//if(1){
				//	FZoneGraphLaneLocation Start, End;
				//	UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, CurLaneIndex, SegLanes.StartDist, Start);
				//	UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, CurLaneIndex, SegLanes.EndDist, End);
				//	UE_LOG(LogTemp, Log, TEXT("		--**LaneIndex:%d,Start:%f,End:%f"), CurLaneIndex, StartEnd[i].Get<0>(), StartEnd[i].Get<1>());
				//	DrawDebugDirectionalArrow(GetWorld(), Start.Position + FVector(0, 0, 300), End.Position + FVector(0, 0, 300), 3000, FColor::Red, false, 15.f, 0, 35);
				//}
				
				const FZoneLaneData& CurLane = ZoneGraphStorage->Lanes[CurLaneIndex];

				for (int32 j = CurLane.LinksBegin; j < CurLane.LinksEnd; j++)
				{
					const FZoneLaneLinkData& LinkData = ZoneGraphStorage->LaneLinks[j];
					
					if (LinkData.Type == EZoneLaneLinkType::Adjacent && !LinkData.HasFlags(EZoneLaneLinkFlags::OppositeDirection))
					{
						if(!VisitedLanes.Contains(LinkData.DestLaneIndex))
						{
							NeibourLanes.Enqueue(LinkData.DestLaneIndex);
						}
					}
				}
			}
		}
		else {
			VisitedLanes.Add(LaneIndiceSingle[i]);
		}

		RoadLanes.ZoneSegLanes.Add(SegLanes);
		
	}
	
}

void UTrafficSimSubsystem::FillVehsOnLane(TArray<int32> LaneIndice, TArray<float> StartDist, TArray<float> EndDist, float CruiseSpeed, UPARAM(ref) FTagLaneFillGap& TagLaneFillGap,
	UPARAM(ref)TArray<FName>& VehIDs, UPARAM(ref)TArray<int32>& VehTypeIndice, TArray<FName>& UsedVehIDs, TArray<int32>& UsedVehTypes){
	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("ZoneGraphData is not initialized! Cannot fill vehicles on lane."));
		return;
	}
	if (VehicleConfigTypes.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("VehicleConfigTypes is empty! Cannot fill vehicles on lane."));
		return;
	}
	if (VehTypeIndice.Num() == 0 || VehIDs.Num()==0)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("VehTypeIndice is empty! Cannot fill vehicles on lane."));
		return;
	}
	if (LaneIndice.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("LaneIndice is empty! Cannot fill vehicles on lane."));
		return;
	}
	if (VehTypeIndice.Num() != VehIDs.Num())
	{
		UE_LOG(LogTrafficSim, Error, TEXT("Type Num is not equal with IDs Num at LaneIndex:%d"));
		return;
	}


	TArray<float> VehLengths, PrefixSum;
	GetVehicleConfigs(VehLengths, PrefixSum);

	struct FVehInfo
	{
		FName VehID;
		FZoneGraphLaneLocation LaneLocation;
		int32 LaneIndex;
		FZoneGraphTagMask LaneTagMask;
		float DistAlongLane = 0.f;
	};
	TMultiMap<int32, FVehInfo> TypeToVehInfoMap;

	// TODO: 从配置中获取最小间距
	//float MinGap = 200.f, MaxGap = 350; 
	int32 LaneSearchIndex = LaneIndice.Num()-1;
	float DerivedDist = EndDist[LaneSearchIndex];
	
	FZoneGraphTagMask LaneTagMask = ZoneGraphStorage->Lanes[LaneIndice[LaneSearchIndex]].Tags;

	auto GetLaneGapByTag = [this, &TagLaneFillGap](FZoneGraphTagMask LaneTagMask, float& OutMinGap, float& OutMaxGap)
	{
		OutMinGap = 200.f;
		OutMaxGap = 350.f;
		for (const auto& Pair : TagLaneFillGap.TagedGaps)
		{
			if (LaneTagMask.Contains(Pair.Key))
			{
				OutMinGap = Pair.Value.MinValue;
				OutMaxGap = Pair.Value.MaxValue;
				return;
			}
		}
	};
	float MinGap, MaxGap;
	GetLaneGapByTag(LaneTagMask, MinGap, MaxGap);
	for (int32 i=0; VehTypeIndice.Num()>0 && LaneSearchIndex>=0;i++)
	{
		int32 TypeIndex = VehTypeIndice[0];
		if (!VehLengths.IsValidIndex(TypeIndex))
		{
			UE_LOG(LogTrafficSim, Warning, TEXT("Invalid vehicle type index %d.using as default index:0"), TypeIndex);
			TypeIndex = 0;
		}

		const float CurLength = VehLengths[TypeIndex];

		float RandomGap = 0;
		if (i == 0)
		{
			DerivedDist -= CurLength * 0.5f;
			RandomGap = FMath::FRandRange(0.0, 50.0);
		}
		else
		{
			const float PrevLength = VehLengths[UsedVehTypes.Last()];
			DerivedDist -= (PrevLength * 0.5f + CurLength * 0.5f);
			RandomGap = FMath::FRandRange(MinGap, MaxGap);
		}
		
		DerivedDist -= RandomGap;

		if (DerivedDist <= StartDist[LaneSearchIndex])
		{
			float offset = StartDist[LaneSearchIndex]- DerivedDist;
			if (LaneSearchIndex - 1 >= 0)
			{
				LaneSearchIndex--;
				DerivedDist = EndDist[LaneSearchIndex]- offset;

				LaneTagMask = ZoneGraphStorage->Lanes[LaneIndice[LaneSearchIndex]].Tags;
				GetLaneGapByTag(LaneTagMask, MinGap, MaxGap);

			}
			else
				break;

		}

		FZoneGraphLaneLocation LaneLocation;
		UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, LaneIndice[LaneSearchIndex], DerivedDist, LaneLocation);
		

		FVehInfo Info;
		Info.VehID = VehIDs[0] ;
		Info.LaneLocation = LaneLocation;
		Info.LaneIndex = LaneLocation.LaneHandle.Index;
		Info.DistAlongLane = DerivedDist;
		Info.LaneTagMask = ZoneGraphStorage->Lanes[LaneIndice[LaneSearchIndex]].Tags;
		

		UsedVehIDs.Add(Info.VehID);
		UsedVehTypes.Add(VehTypeIndice[0]);
		VehIDs.RemoveAt(0);
		VehTypeIndice.RemoveAt(0);

		TypeToVehInfoMap.Add(TypeIndex, Info);
	}

	

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!EntitySubsystem)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("FillVehsOnLane: Failed to get EntitySubsystem."));
		return;
	}
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	TSet<int32> UniqueTypeIndices;
	for (auto& Pair : TypeToVehInfoMap)
	{
		UniqueTypeIndices.Add(Pair.Key);
	}



	// 在一个自定义 Processor 或合适初始化阶段调用
	EntityManager.Defer().PushCommand<FMassDeferredCreateCommand>(
		[this,UniqueTypeIndices, CruiseSpeed, TypeToVehInfoMap](FMassEntityManager& System) mutable
		{
			for (int32 TypeIndex : UniqueTypeIndices)
			{
				if (!EntityTemplates.IsValidIndex(TypeIndex))
				{
					UE_LOG(LogTrafficSim, Warning, TEXT("Invalid type index %d."), TypeIndex);
					continue;
				}
				TArray<FVehInfo> VehInfos;
				TypeToVehInfoMap.MultiFind(TypeIndex, VehInfos);
				if (VehInfos.Num() == 0)
				{
					UE_LOG(LogTrafficSim, Warning, TEXT("No vehicle info found for type index %d. Skip to Spawn.."), TypeIndex);	
					continue;
				}
				const FMassEntityTemplate& Template = *EntityTemplates[TypeIndex];
				const FMassArchetypeSharedFragmentValues& SharedValues = Template.GetSharedFragmentValues();
				
				TArray<FMassEntityHandle> SpawnedEntities;
				TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = System.BatchCreateEntities(Template.GetArchetype(), SharedValues, VehInfos.Num(), SpawnedEntities);
				System.BatchSetEntityFragmentsValues(CreationContext->GetEntityCollection(), Template.GetInitialFragmentValues());

				for (int32 i = 0; i < SpawnedEntities.Num(); ++i)
				{
					const FMassEntityHandle Entity = SpawnedEntities[i];
					FTransformFragment& TransformFrag = System.GetFragmentDataChecked<FTransformFragment>(Entity);
					FMassVehicleMovementFragment& MovementFrag = System.GetFragmentDataChecked<FMassVehicleMovementFragment>(Entity);

					FTransform T(VehInfos[i].LaneLocation.Position);
					TransformFrag.SetTransform(T);

					RegisterVehPlateID(VehInfos[i].VehID, Entity);
					MovementFrag.LaneLocation = VehInfos[i].LaneLocation;
					MovementFrag.DistanceAlongLane = VehInfos[i].DistAlongLane;
					//MovementFrag.LeftDistance = LaneLength - VehInfos[i].DistAlongLane;
					MovementFrag.VehID = VehInfos[i].VehID;
					MovementFrag.Speed = 0.f;
					if (CruiseSpeed <= KINDA_SMALL_NUMBER)
					{
						float MinSpeed, MaxSpeed;
						FZoneGraphTag LaneTagType;
						GetLaneSpeedByTag(VehInfos[i].LaneTagMask,MaxSpeed,MinSpeed,LaneTagType);
						CruiseSpeed = FMath::RandRange(MinSpeed, MaxSpeed);
						UE_LOG(LogTrafficSim, Verbose, TEXT("CruiseSpeed%.2f is invalid, using Tagged RandomSpeed"), CruiseSpeed);
					}
					MovementFrag.CruiseSpeed = CruiseSpeed;
					MovementFrag.TargetSpeed = 0.0f;
					MovementFrag.VehicleHandle = SpawnedEntities[i];

					TArray<int32> NextLanes;
					MovementFrag.NextLane = ChooseNextLane(VehInfos[i].LaneIndex, NextLanes);

					//FVector Start = MovementFrag.LaneLocation.Position-MovementFrag.LaneLocation.Direction * (MovementFrag.VehicleLength / 2.0);
					//FVector End = MovementFrag.LaneLocation.Position + MovementFrag.LaneLocation.Direction * (MovementFrag.VehicleLength / 2.0);
					//DrawDebugDirectionalArrow(World, Start, End,100.f,
					//	FColor::Green,false,50.f,0,50.f);

					UE_LOG(LogTrafficSim, Verbose, TEXT("VehSN:%d,VehID:%s,CurIndex:%d,Next Lane Index:%d TypeIndex:%d TemplateName:%s Speed:%.2f TargetSpeed:%.2f"), MovementFrag.VehicleHandle.SerialNumber,
						*(VehInfos[i].VehID.ToString()), VehInfos[i].LaneIndex, MovementFrag.NextLane,TypeIndex, *Template.GetTemplateName(), MovementFrag.Speed, MovementFrag.TargetSpeed);
				}
			}
		}
	);
}

void UTrafficSimSubsystem::DeleteMassEntities(int32 TargeLaneIndex)
{
	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!EntitySubsystem) return;

	UVehicleDeletorProcessor* DeleteProcessor = NewObject<UVehicleDeletorProcessor>();
	DeleteProcessor->LaneToDelete = TargeLaneIndex;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FMassProcessingContext ProcessingContext(EntityManager, 0.f); // DeltaSeconds = 0
	UE::Mass::Executor::Run(*DeleteProcessor, ProcessingContext);

}

void UTrafficSimSubsystem::PrintLaneVehicles(int32 TargetLaneIndex)
{
	if(LaneToEntitiesMap.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("LaneToEntitiesMap is not initialized! Cannot print lane vehicles."));
		return;
	}
	TArray<FLaneVehicle>* LaneVehicles=LaneToEntitiesMap.Find(TargetLaneIndex);
	if(!LaneVehicles)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("No vehicles found in lane %d."), TargetLaneIndex);
		return;
	}
	UE_LOG(LogTrafficSim, Log, TEXT("Vehicles in lane %d:"), TargetLaneIndex);
	for(const FLaneVehicle& LaneVehicle : *LaneVehicles)
	{
		if(LaneVehicle.VehicleMovementFragment.VehicleLength>0)
		{
			FVector Position = LaneVehicle.VehicleMovementFragment.LaneLocation.Position;

			const FMassVehicleMovementFragment* FrontVehicle = nullptr;
			bool found=FindFrontVehicle(LaneVehicle.VehicleMovementFragment.LaneLocation.LaneHandle.Index,
				LaneVehicle.VehicleMovementFragment.NextLane,LaneVehicle.EntityHandle, FrontVehicle);

			int32 ForntVehicleSN = -1;
			if (found && FrontVehicle)
			{
				ForntVehicleSN = FrontVehicle->VehicleHandle.SerialNumber;
			}

			UE_LOG(LogTrafficSim, Log, TEXT("Entity: %d, DistanceAlongLane: %.2f Position:%s FrontVehicle:%d"),
				LaneVehicle.EntityHandle.SerialNumber, 
				LaneVehicle.VehicleMovementFragment.LaneLocation.DistanceAlongLane,
				*Position.ToString(),
				ForntVehicleSN);
		}
		else
		{
			UE_LOG(LogTrafficSim, Warning, TEXT("Entity: %d has no valid VehicleMovementFragment."),
				LaneVehicle.EntityHandle.SerialNumber);
		}
	}
}

void UTrafficSimSubsystem::PrintLaneLinks(int32 TargetLaneIndex)
{
	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("ZoneGraphStorage is not initialized! Cannot print lane links."));
		return;
	}
	if (TargetLaneIndex < 0 || TargetLaneIndex >= ZoneGraphStorage->Lanes.Num())
	{
		UE_LOG(LogTrafficSim, Error, TEXT("Invalid TargetLaneIndex %d! Cannot print lane links."), TargetLaneIndex);
		return;
	}
	const FZoneLaneData& CurLaneData = ZoneGraphStorage->Lanes[TargetLaneIndex];
	int32 FirstLink = CurLaneData.LinksBegin;
	int32 EndLink = CurLaneData.LinksEnd;
	UE_LOG(LogTrafficSim, Log, TEXT("Links for lane %d:"), TargetLaneIndex);
	for (int32 LinkIndex = FirstLink; LinkIndex < EndLink; ++LinkIndex)
	{
		const FZoneLaneLinkData& LaneLink = ZoneGraphStorage->LaneLinks[LinkIndex];
		FString LinkTypeStr;
		switch (LaneLink.Type)
		{
		case EZoneLaneLinkType::Outgoing:
			LinkTypeStr = "Outgoing";
			break;
		case EZoneLaneLinkType::Incoming:
			LinkTypeStr = "Incoming";
			break;
		case EZoneLaneLinkType::Adjacent:
			LinkTypeStr = "Adjacent";
			break;
		case EZoneLaneLinkType::All:
			LinkTypeStr = "All";
			break;
		case EZoneLaneLinkType::None:
			LinkTypeStr = "None";
			break;
		default:
			LinkTypeStr = "Unknown";
			break;
		}

		FString LinkFlagStr;
		
		if (LaneLink.HasFlags(EZoneLaneLinkFlags::None))
		{
			LinkFlagStr += "None ";
		}
		if(LaneLink.HasFlags(EZoneLaneLinkFlags::Left))
		{
			LinkFlagStr += "Left ";
		}
		if(LaneLink.HasFlags(EZoneLaneLinkFlags::Merging))
		{
			LinkFlagStr += " Merging ";
		}
		if(LaneLink.HasFlags(EZoneLaneLinkFlags::Right))
		{
			LinkFlagStr += " Right ";
		}
		if(LaneLink.HasFlags(EZoneLaneLinkFlags::OppositeDirection))
		{
			LinkFlagStr += " OppositeDirection ";
		}
		if (LaneLink.HasFlags(EZoneLaneLinkFlags::Splitting))
		{
			LinkFlagStr += " Splitting ";
		}



		UE_LOG(LogTrafficSim, Log, TEXT("  Link to lane %d of type %s with Flags:%s"), LaneLink.DestLaneIndex, *LinkTypeStr,*LinkFlagStr);
		TArray<int32> NextLanes;
		int32 index=ChooseNextLane(TargetLaneIndex, NextLanes);
		UE_LOG(LogTrafficSim, Log, TEXT(" Random Choose Next Lanes:%d"), index);

		
	}

	TArray<int32>* AdjMergeLaneArr = AdjMergeLanes.Find(TargetLaneIndex);
	if (!AdjMergeLaneArr)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("No Adjacent Mergine Lanes Found!"));
		return;
	}
	for (int32 i = 0; i < AdjMergeLaneArr->Num(); i++)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("===== Found a AdjMergeLane:%d"),(*AdjMergeLaneArr)[i]);
	}
}

void UTrafficSimSubsystem::InitializeManual()
{
	World = GetWorld();
	ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(World);

	for (TActorIterator<AZoneGraphData> It(World); It; ++It)
	{
		AZoneGraphData* ZoneGraphDataTemp = *It;
		if (ZoneGraphDataTemp && ZoneGraphDataTemp->IsValidLowLevel())
		{
			ZoneGraphStorage = &ZoneGraphDataTemp->GetStorage();
			mutableZoneGraphSotrage = &ZoneGraphDataTemp->GetStorageMutable();
			ZoneGraphData = ZoneGraphDataTemp;
			InitializeLaneToEntitiesMap();
			return;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("No valid ZoneGraphData found in the world!"));
}

void UTrafficSimSubsystem::RoadToLaneIndice(UPARAM(ref)UDataTable* RoadToLaneIndiceMap, FName RoadID, TArray<FVector> RoadPoints, FZoneGraphTagMask NotTag, float Height)
{
	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to get ZoneGraphStorage! Init the TrafficSubsystem Firstly!"));
		return;
	}
	TArray<TTuple<float, float>> StartEnd;
	TArray<int32> LaneIndice;
	
	FZoneGraphTagFilter Filter;
	Filter.NotTags.Add(NotTag);

	UE::ZoneGraph::Query::FindNearestLanesBySeg(*ZoneGraphStorage, RoadPoints, Height, Filter, LaneIndice, StartEnd);

	FRoadToLaneIndexRow RowData;
	RowData.LaneIndices = LaneIndice;

	for (auto tuple : StartEnd)
	{
		RowData.StartDist.Add(tuple.Get<0>());
		RowData.EndDist.Add(tuple.Get<1>());
	}
	if (!RoadToLaneIndiceMap || (RoadToLaneIndiceMap->GetRowStruct() != FRoadToLaneIndexRow::StaticStruct()))
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("RoadToLaneIndiceMap is invalid or has wrong RowStruct!"));
		return;
	}

	////Debug Draw
	for(int32 i=0;i< LaneIndice.Num();i++)
	{
		FZoneGraphLaneLocation Start, End;
		UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, LaneIndice[i],StartEnd[i].Get<0>(), Start);
		UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, LaneIndice[i], StartEnd[i].Get<1>(), End);
		UE_LOG(LogTemp, Log, TEXT("LaneIndex:%d,Start:%f,End:%f"), LaneIndice[i], StartEnd[i].Get<0>(), StartEnd[i].Get<1>());
		DrawDebugDirectionalArrow(GetWorld(), Start.Position+FVector(0,0,300), End.Position + FVector(0, 0, 300), 3000, FColor::Blue, false, 30.f, 0, 35);
	}

	//Merge LaneSegs
	TArray<int32> MergedLaneIndices;
	FRoadToLaneIndexRow MergedRowData;

	for (int i = 0; i < RowData.LaneIndices.Num(); i++)
	{
		if(MergedLaneIndices.Contains(i))
		{
			continue;
		}
		float StartDist = RowData.StartDist[i];
		float EndDist = RowData.EndDist[i];
		for(int j=i+1;j< RowData.LaneIndices.Num();j++)
		{
			if (MergedLaneIndices.Contains(j))
			{
				continue;
			}
			float ConnectedStartDist = RowData.StartDist[j];
			if(RowData.LaneIndices[i]== RowData.LaneIndices[j] && ConnectedStartDist == EndDist)
			{
				EndDist = RowData.EndDist[j];
				MergedLaneIndices.Add(j);
			}
		}
		MergedRowData.LaneIndices.Add(RowData.LaneIndices[i]);
		MergedRowData.StartDist.Add(StartDist);
		MergedRowData.EndDist.Add(EndDist);
	}
	//Debug
	for(int32 i=0;i< MergedRowData.LaneIndices.Num();i++)
	{
		FZoneGraphLaneLocation Start, End;
		UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, MergedRowData.LaneIndices[i],MergedRowData.StartDist[i], Start);
		UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, MergedRowData.LaneIndices[i], MergedRowData.EndDist[i], End);
		UE_LOG(LogTemp, Log, TEXT("Merged LaneIndex:%d,Start:%f,End:%f"), MergedRowData.LaneIndices[i], MergedRowData.StartDist[i], MergedRowData.EndDist[i]);
		DrawDebugDirectionalArrow(GetWorld(), Start.Position+FVector(0,0,600), End.Position + FVector(0, 0, 600), 3000, FColor::Red, false, 60.f, 0, 35);
	}

	RoadToLaneIndiceMap->AddRow(RoadID, MergedRowData);
}

void UTrafficSimSubsystem::DebugEntity(int32 TargetLane, int32 EntitySN)
{
	TArray<FLaneVehicle>* VehicleArray=LaneToEntitiesMap.Find(TargetLane);
	if(!VehicleArray)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("No vehicles found in lane %d."), TargetLane);
		return;
	}

	const FLaneVehicle& TargetVehicle = VehicleArray->Last();
	
	FMassVehicleMovementFragment Frag = TargetVehicle.VehicleMovementFragment;
	UTrafficLightSubsystem* TrafficLightSubsystem = UWorld::GetSubsystem<UTrafficLightSubsystem>(World);

	bool OpenLane = false,IntersectionLane = false;
	const FMassVehicleMovementFragment* FrontVehicleMovement = nullptr;

	bool First = FindFrontVehicle(Frag.LaneLocation.LaneHandle.Index,
		Frag.NextLane, Frag.VehicleHandle, FrontVehicleMovement);

	TrafficLightSubsystem->QueryLaneOpenState(Frag.NextLane, OpenLane, IntersectionLane);
	bool hasFrontVehicle = (FrontVehicleMovement != nullptr);
	UE_LOG(LogTemp, Log, TEXT("VehicleSN:%d,CurLane:%d, NextLane:%d,TargetSpeed:%f, Speed:%f,NextLaneInInsec:%d,IsOpen:%d,LeftDistance:%f,VehicleLength:%f,FirstAtCurLane:%d HasFrontVehilce:%d"),
		Frag.VehicleHandle.SerialNumber,
		Frag.LaneLocation.LaneHandle.Index, Frag.NextLane,
		Frag.TargetSpeed, Frag.Speed,
		IntersectionLane,OpenLane,
		Frag.LeftDistance, Frag.VehicleLength, First, hasFrontVehicle
		);
	if (hasFrontVehicle)
	{
		UE_LOG(LogTemp, Log, TEXT("FrontVehicleSN:%d Speed:%f TargetSpeed:%f at lane:%d HalfLenght:%f,Dist:%f"),
			FrontVehicleMovement->VehicleHandle.SerialNumber,
			FrontVehicleMovement->Speed,
			FrontVehicleMovement->TargetSpeed,
			FrontVehicleMovement->LaneLocation.LaneHandle.Index,
			(FrontVehicleMovement->VehicleLength+Frag.VehicleLength)/2,
			FVector::Distance(FrontVehicleMovement->LaneLocation.Position,Frag.LaneLocation.Position)
			);
		DrawDebugPoint(GetWorld(), FrontVehicleMovement->LaneLocation.Position+FVector(0,0,500.f), 50.0, FColor::Red, false, 10.0);
	}

	
}

void UTrafficSimSubsystem::RoadToLanes(UDataTable* UpdatedRoadStatus, UDataTable* StaticRoadGeos,UPARAM(ref)UDataTable*& LanesMap, ACesiumGeoreference* GeoRef,FZoneGraphTag AnyTag,float QueryHeight)
{

	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTemp, Log, TEXT("Failed to get ZoneGraphStorage! Init the TrafficSubsystem Firstly!"));
	}
	if (!GeoRef)
	{
		UE_LOG(LogTemp, Log, TEXT("Failed to get GeoReference! Assign a Valid GeoReference Firstly!"));
	}
	
	if (LanesMap->GetRowStruct() == nullptr)
	{
		// 运行时构造/未设置时，显式指定
		LanesMap->RowStruct = FDTRoadLanes::StaticStruct();
	}
	LanesMap->EmptyTable();

	for (auto& RoadRow : UpdatedRoadStatus->GetRowMap())
	{
		FName RoadID = RoadRow.Key;
		FDTRoadGeoStatus* UpdatedRoadStatusRow = (FDTRoadGeoStatus*)RoadRow.Value;

		FDTRoadGeoStatus* StaticGeoRow=StaticRoadGeos->FindRow<FDTRoadGeoStatus>(RoadID,TEXT("Look Up StaticRoads"),false);
		
		if (!StaticGeoRow)
		{
			continue;
		}
		TArray<FVector> UELocs;
		for (auto LongLatiHeight : StaticGeoRow->coords)
		{
			FVector UELoc= GeoRef->TransformLongitudeLatitudeHeightPositionToUnreal(LongLatiHeight);
			UELocs.Add(UELoc);
			
		}
		FDTRoadLanes RoadLanes;
		GetZonesSeg(UELocs,AnyTag,QueryHeight,RoadLanes);

		RoadLanes.speed = UpdatedRoadStatusRow->speed;
		RoadLanes.state = UpdatedRoadStatusRow->state;
		RoadLanes.traveltime = UpdatedRoadStatusRow->traveltime;

		LanesMap->AddRow(RoadID, RoadLanes);

	}
}

void UTrafficSimSubsystem::BathSetCongestionByDT(UPARAM(ref)UDataTable*& LanesMap,float TagetValue, TMap<int32, float> CongestionIndex)
{
	for (auto& Row : LanesMap->GetRowMap())
	{
		FDTRoadLanes* RoadLanes = (FDTRoadLanes*)Row.Value;
		if (!RoadLanes || RoadLanes->ZoneSegLanes.Num() == 0)
			continue;

		float TargetAverGap=CongestionIndex.FindRef(RoadLanes->state);
		for (auto& ZoneSegLane : RoadLanes->ZoneSegLanes)
		{
			
			for (auto Lane : ZoneSegLane.Lanes)
			{
				//UE_LOG(LogTemp, Log, TEXT("Setting Congetion at Lane:%d"),Lane);
				AdjustLaneCongestion(Lane,ELaneCongestionMetric::AverageGap, TargetAverGap,nullptr,ZoneSegLane.StartDist,ZoneSegLane.EndDist);
			}
		}
	}
}

int32 UTrafficSimSubsystem::GetZoneLaneIndexByPoints(TArray<FVector> Points, FZoneGraphTag NotTag, float Height)
{
	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Get Invalid ZoneGraphStorage!"));
		return -1;
	}
	TArray<int32> LaneIndiceSingle;
	TArray<TTuple<float, float>> StartEnd;
	FZoneGraphTagFilter Filter;
	Filter.NotTags.Add(NotTag);
	UE::ZoneGraph::Query::FindNearestLanesBySeg(*ZoneGraphStorage, Points, Height, Filter, LaneIndiceSingle, StartEnd);

	if(LaneIndiceSingle.Num()==0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No Lane Found by Given Points!"));
		for (auto P : Points)
		{
			DrawDebugPoint(GetWorld(), P, 30.f, FColor::Red, false, 20.f);
		}
		return -1;
	}
	return LaneIndiceSingle[0];
}

void UTrafficSimSubsystem::ClearAllEntities()
{
	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if(!EntitySubsystem)
	{
		UE_LOG(LogTrafficSim,Warning,TEXT("Failed to Get EntitySubsystem!"));
		return;
	}

	UClearTagedEntitiesProcessor* ClearProcessor = NewObject<UClearTagedEntitiesProcessor>();
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FMassProcessingContext ProcessingContext(EntityManager,0.f);
	UE::Mass::Executor::Run(*ClearProcessor,ProcessingContext);
}

void UTrafficSimSubsystem::AddSpawnPointAtLane(int32 LaneIndex, float DistanceAlongLane, float CruiseSpeed, UMassEntityConfigAsset* EntityConfigAsset, TArray<FName> VehIDs, TArray<int32> VehTypes,UPARAM(ref)FTagLaneFillGap TagLaneFillGap)
{
	if (!World || !ZoneGraphStorage)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("AddSpawnPointAtLane: World or ZoneGraphStorage invalid."));
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!EntitySubsystem)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("AddSpawnPointAtLane: Failed to get EntitySubsystem."));
		return;
	}

	if (!EntityConfigAsset)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("AddSpawnPointAtLane: EntityConfigAsset is null."));
		return;
	}
	if (VehIDs.Num() != VehTypes.Num() || VehIDs.Num() == 0 || VehTypes.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("AddSpawnPointAtLane: VehIDs and VehTypes count mismatch.VehIDs:%d,VehTypes Num:%d"),VehIDs.Num(),VehTypes.Num());
		return;
	}

	// 计算车道上的位置
	FZoneGraphLaneLocation LaneLocation;
	UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, LaneIndex, DistanceAlongLane, LaneLocation);

	const FMassEntityTemplate& Template = EntityConfigAsset->GetOrCreateEntityTemplate(*World);
	if (!Template.IsValid())
	{
		UE_LOG(LogTrafficSim, Error, TEXT("AddSpawnPointAtLane: Invalid entity template."));
		return;
	}

	if (CruiseSpeed <= KINDA_SMALL_NUMBER)
	{
		const FZoneGraphTagMask& CurLaneTagMask=ZoneGraphStorage->Lanes[LaneIndex].Tags;
		FZoneGraphTag TypedLaneTag;
		float MaxSpeed, MinSpeed;
		GetLaneSpeedByTag(CurLaneTagMask, MaxSpeed, MinSpeed, TypedLaneTag);
		CruiseSpeed = FMath::RandRange(MinSpeed, MaxSpeed);
	}

	const FZoneGraphTagMask& LaneTagMask = ZoneGraphStorage->Lanes[LaneIndex].Tags;


	float MinGap, MaxGap;
	for (const auto& Pair : TagLaneFillGap.TagedGaps)
	{
		if (LaneTagMask.Contains(Pair.Key))
		{
			MinGap = Pair.Value.MinValue;
			MaxGap = Pair.Value.MaxValue;
			return;
		}
	}
	

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	const FMassArchetypeSharedFragmentValues& SharedValues = Template.GetSharedFragmentValues();

	// 使用 deferred 命令在安全时机创建并初始化实体（避免同步 API）
	EntityManager.Defer().PushCommand<FMassDeferredCreateCommand>(
		[this, LaneLocation, CruiseSpeed, VehIDs, VehTypes, &Template, SharedValues,MinGap,MaxGap](FMassEntityManager& System)
		{
			TArray<FMassEntityHandle> Spawned;
			TSharedRef<FMassEntityManager::FEntityCreationContext> Ctx =
				System.BatchCreateEntities(Template.GetArchetype(), SharedValues, 1, Spawned);

			// 应用模板初始片段值
			System.BatchSetEntityFragmentsValues(Ctx->GetEntityCollection(), Template.GetInitialFragmentValues());

			if (Spawned.Num() == 1)
			{
				const FMassEntityHandle SpawnPointEntity = Spawned[0];

				// 设置 Transform
				FTransformFragment& TransformFrag = System.GetFragmentDataChecked<FTransformFragment>(SpawnPointEntity);
				TransformFrag.SetTransform(FTransform(LaneLocation.Position));

				// 设置 SpawnPointFragment
				FMassSpawnPointFragment& SpawnPointFrag = System.GetFragmentDataChecked<FMassSpawnPointFragment>(SpawnPointEntity);
				SpawnPointFrag.LaneLocation = LaneLocation;
				SpawnPointFrag.Controlled = true;
				SpawnPointFrag.Duration = 1.f;
				SpawnPointFrag.RandOffset = 2.f;
				SpawnPointFrag.Clock = 1.0;
				SpawnPointFrag.SpawnVehicleIDIndex = 0;
				SpawnPointFrag.VehicleIDs = VehIDs;
				SpawnPointFrag.VehicleTypes = VehTypes;
				SpawnPointFrag.CruiseSpeed = CruiseSpeed;
				SpawnPointFrag.MinGap = MinGap;
				SpawnPointFrag.MaxGap = MaxGap;

				UE_LOG(LogTrafficSim, Log, TEXT("SpawnPoint entity (deferred) created at Lane %d Dist %.2f EntitySN:%d VehIDs:%d"),
					LaneLocation.LaneHandle.Index, LaneLocation.DistanceAlongLane, SpawnPointEntity.SerialNumber, VehIDs.Num());
			}
			else
			{
				UE_LOG(LogTrafficSim, Warning, TEXT("AddSpawnPointAtLane: deferred BatchCreateEntities failed."));
			}
		}
	);

	// 如需本帧立即可见（慎用，可能影响性能），在此处 flush
	// EntityManager.FlushCommands();
}

FName UTrafficSimSubsystem::GetVehIDFromActor(AActor* ClickedActor)
{

		if (!ClickedActor)
		{
			UE_LOG(LogTemp, Warning, TEXT("GetVehIDFromActor: ClickedActor is null"));
			return NAME_None;
		}

		//UWorld* World = ClickedActor->GetWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Warning, TEXT("GetVehIDFromActor: World is null"));
			return NAME_None;
		}

		// 获取MassActorSubsystem
		UMassActorSubsystem* MassActorSubsystem = World->GetSubsystem<UMassActorSubsystem>();
		if (!MassActorSubsystem)
		{
			UE_LOG(LogTemp, Warning, TEXT("GetVehIDFromActor: MassActorSubsystem not found"));
			return NAME_None;
		}

		// 获取MassEntitySubsystem
		UMassEntitySubsystem* MassEntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (!MassEntitySubsystem)
		{
			UE_LOG(LogTemp, Warning, TEXT("GetVehIDFromActor: MassEntitySubsystem not found"));
			return NAME_None;
		}

		// 从Actor获取实体句柄
		FMassEntityHandle EntityHandle = MassActorSubsystem->GetEntityHandleFromActor(ClickedActor);
		if (!EntityHandle.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("GetVehIDFromActor: No valid entity handle found for actor %s"),
				*ClickedActor->GetName());
			return NAME_None;
		}
		UE_LOG(LogTrafficSim, Log, TEXT("Get Valid Handle:%d"), EntityHandle.SerialNumber);
		// 获取实体的VehicleMovementFragment来获取VehID
		const FMassVehicleMovementFragment* VehicleMovementFragment =
			MassEntitySubsystem->GetEntityManager().GetFragmentDataPtr<FMassVehicleMovementFragment>(EntityHandle);

		//Debug Info
#if WITH_EDITOR

		//FMassVehicleMovementFragment Frag = TargetVehicle.VehicleMovementFragment;
		UTrafficLightSubsystem* TrafficLightSubsystem = UWorld::GetSubsystem<UTrafficLightSubsystem>(World);

		bool OpenLane = false, IntersectionLane = false;
		const FMassVehicleMovementFragment* FrontVehicleMovement = nullptr;

		bool First = FindFrontVehicle(VehicleMovementFragment->LaneLocation.LaneHandle.Index,
			VehicleMovementFragment->NextLane, VehicleMovementFragment->VehicleHandle, FrontVehicleMovement);

		TrafficLightSubsystem->QueryLaneOpenState(VehicleMovementFragment->NextLane, OpenLane, IntersectionLane);
		bool hasFrontVehicle = (FrontVehicleMovement != nullptr);
		UE_LOG(LogTemp, Log, TEXT("CurVehSN:%d,CruiseSpeed:%f"), EntityHandle.SerialNumber, VehicleMovementFragment->CruiseSpeed);
		UE_LOG(LogTemp, Log, TEXT("VehicleSN:%d,CurLane:%d, NextLane:%d,TargetSpeed:%f, Speed:%f,NextLaneInInsec:%d,IsOpen:%d,LeftDistance:%f,VehicleLength:%f,FirstAtCurLane:%d HasFrontVehilce:%d"),
			VehicleMovementFragment->VehicleHandle.SerialNumber,
			VehicleMovementFragment->LaneLocation.LaneHandle.Index, VehicleMovementFragment->NextLane,
			VehicleMovementFragment->TargetSpeed, VehicleMovementFragment->Speed,
			IntersectionLane, OpenLane,
			VehicleMovementFragment->LeftDistance, VehicleMovementFragment->VehicleLength, First, hasFrontVehicle
		);
		if (hasFrontVehicle)
		{
			UE_LOG(LogTemp, Log, TEXT("FrontVehicleSN:%d Speed:%f TargetSpeed:%f at lane:%d HalfLenght:%f,Dist:%f"),
				FrontVehicleMovement->VehicleHandle.SerialNumber,
				FrontVehicleMovement->Speed,
				FrontVehicleMovement->TargetSpeed,
				FrontVehicleMovement->LaneLocation.LaneHandle.Index,
				(FrontVehicleMovement->VehicleLength + VehicleMovementFragment->VehicleLength) / 2,
				FVector::Distance(FrontVehicleMovement->LaneLocation.Position, VehicleMovementFragment->LaneLocation.Position)
			);
			DrawDebugPoint(GetWorld(), FrontVehicleMovement->LaneLocation.Position + FVector(0, 0, 500.f), 50.0, FColor::Red, false, 10.0);
		}

#endif


		if (VehicleMovementFragment)
		{
			UE_LOG(LogTemp, Log, TEXT("GetVehIDFromActor: Found VehID %s for actor %s"),
				*VehicleMovementFragment->VehID.ToString(), *ClickedActor->GetName());
			UE_LOG(LogTrafficSim, Log, TEXT("VehicleMovementFragment SN:%d,TargetSpeed:%f,CurIndex:%d,NextLane:%d"), 
				VehicleMovementFragment->VehicleHandle.SerialNumber, VehicleMovementFragment->TargetSpeed, VehicleMovementFragment->LaneLocation.LaneHandle.Index,
				VehicleMovementFragment->NextLane);

			return VehicleMovementFragment->VehID;
		}

		UE_LOG(LogTemp, Warning, TEXT("GetVehIDFromActor: No VehicleMovementFragment found for entity"));
		return NAME_None;
	
}

void UTrafficSimSubsystem::SetManualSimMode(bool bInManualSim)
{
	bManualSim = bInManualSim;
}

void UTrafficSimSubsystem::LineTraceEntity(FVector Start, FVector End)
{

	UMassRepresentationSubsystem* RepSub = UWorld::GetSubsystem<UMassRepresentationSubsystem>(World);

	
}

bool UTrafficSimSubsystem::SwitchToNextLane(FZoneGraphLaneLocation& LaneLocation, float NewDist)
{
	TArray<int32> NextLanes;
	int NextIndex = ChooseNextLane(LaneLocation.LaneHandle.Index, NextLanes);

	if(NextIndex < 0)
	{
		UE_LOG(LogTrafficSim, Verbose, TEXT("No next lane found for current lane %d."), LaneLocation.LaneHandle.Index);
		return false;
	}
	
	UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, NextIndex, NewDist, LaneLocation);

	return true;
}
//返回该车是否是当前车道的第一辆车
bool UTrafficSimSubsystem::FindFrontVehicle(int32 LaneIndex, int32 NextLaneIndex, FMassEntityHandle CurVehicle, const FMassVehicleMovementFragment*& FrontVehicle)
{
	FrontVehicle = nullptr;

	// 收集当前车道与其合并相邻车道的车辆指针
	TArray<const FLaneVehicle*> VehiclesPtr;

	const TArray<FLaneVehicle>* CurLaneVehicles = LaneToEntitiesMap.Find(LaneIndex);
	if (CurLaneVehicles && CurLaneVehicles->Num() > 0)
	{
		VehiclesPtr.Reserve(VehiclesPtr.Num() + CurLaneVehicles->Num());
		for (const FLaneVehicle& V : *CurLaneVehicles)
		{
			VehiclesPtr.Add(&V);
		}
	}

	const TArray<int32>* AdjLanes = AdjMergeLanes.Find(LaneIndex);
	if (AdjLanes)
	{
		for (int32 AdjLane : *AdjLanes)
		{
			const TArray<FLaneVehicle>* AdjLaneVehicles = LaneToEntitiesMap.Find(AdjLane);
			if (!AdjLaneVehicles || AdjLaneVehicles->Num() == 0)
			{
				continue;
			}
			VehiclesPtr.Reserve(VehiclesPtr.Num() + AdjLaneVehicles->Num());
			for (const FLaneVehicle& V : *AdjLaneVehicles)
			{
				VehiclesPtr.Add(&V);
			}
		}
	}

	if (VehiclesPtr.Num() == 0)
	{
		UE_LOG(LogTrafficSim, VeryVerbose, TEXT("FindFrontVehicle: No vehicles found for lane %d and adjacents."), LaneIndex);
		return false;
	}

	// 合并场景下按剩余距离 LeftDistance 降序排序（剩余距离越大越靠前）
	VehiclesPtr.Sort([](const FLaneVehicle& A, const FLaneVehicle& B) {
		return A.VehicleMovementFragment.LeftDistance > B.VehicleMovementFragment.LeftDistance;
		});

    
	// 找到当前车辆在降序序列中的位置（对于指针数组，参数类型必须是 const FLaneVehicle* const&）
	const int32 CurIndex = VehiclesPtr.IndexOfByPredicate([CurVehicle](const FLaneVehicle* const& V) {
		return V->EntityHandle == CurVehicle;
		});

	if (CurIndex == INDEX_NONE)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("FindFrontVehicle: Current vehicle not found in merged list for lane %d."), LaneIndex);
		return false;
	}

	// 当前车道集合中是否存在前车（降序下标更大表示更靠前）
	if (CurIndex < VehiclesPtr.Num() - 1)
	{
		FrontVehicle = &VehiclesPtr[CurIndex + 1]->VehicleMovementFragment; // 指向原数据
		return false; // 不是第一辆
	}

	// 当前是集合中的最靠前车辆，检查下一车道及其合并相邻
	if (NextLaneIndex < 0)
	{
		return false;
	}

	const FLaneVehicle* NextHead = nullptr;

	auto ConsiderLaneHeadByLeftDist = [&NextHead](const TArray<FLaneVehicle>* LaneVehicles)
		{
			if (!LaneVehicles || LaneVehicles->Num() == 0) return;

			// 在该车道中选择 LeftDistance 最大的作为“队头”
			const FLaneVehicle* Candidate = &(*LaneVehicles)[0];
			for (const FLaneVehicle& V : *LaneVehicles)
			{
				if (V.VehicleMovementFragment.LeftDistance > Candidate->VehicleMovementFragment.LeftDistance)
				{
					Candidate = &V;
				}
			}

			if (!NextHead || Candidate->VehicleMovementFragment.LeftDistance > NextHead->VehicleMovementFragment.LeftDistance)
			{
				NextHead = Candidate;
			}
		};

	ConsiderLaneHeadByLeftDist(LaneToEntitiesMap.Find(NextLaneIndex));

	if (const TArray<int32>* NextAdjLanes = AdjMergeLanes.Find(NextLaneIndex))
	{
		for (int32 AdjLane : *NextAdjLanes)
		{
			ConsiderLaneHeadByLeftDist(LaneToEntitiesMap.Find(AdjLane));
		}
	}

	if (!NextHead)
	{
		// 下一段没有车辆，当前为第一辆
		return true;
	}

	FrontVehicle = &NextHead->VehicleMovementFragment; // 指向原数据
	return true; // 当前为第一辆（下一段存在前车信息）
}

//返回true则需要执行避让等操作
bool UTrafficSimSubsystem::WaitForMergeVehilce(FMassVehicleMovementFragment* CurVehicle, const FMassVehicleMovementFragment*& AheadVehicle)
{
	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("ZoneGraphStorage is not initialized! Cannot print lane links."));
		return false;
	}
	int32 LaneIndex = CurVehicle->LaneLocation.LaneHandle.Index;
	if (LaneIndex < 0 || LaneIndex >= ZoneGraphStorage->Lanes.Num())
	{
		UE_LOG(LogTrafficSim, Error, TEXT("Invalid TargetLaneIndex %d! Cannot print lane links."), LaneIndex);
		return false;
	}

	//当前Lane是否位于Connector区域
	int32 ZoneIndex = ZoneGraphStorage->Lanes[LaneIndex].ZoneIndex;
	if(!ZoneGraphStorage->Zones[ZoneIndex].Tags.Contains(ConnectorTag))
		return false;

	//是否是当前车道的第一辆车
	TArray<FLaneVehicle>* Vehicles=LaneToEntitiesMap.Find(LaneIndex);
	if (!Vehicles )
		return false;
	if (Vehicles->Num()>0 && (*Vehicles)[Vehicles->Num() - 1].EntityHandle != CurVehicle->VehicleHandle)
		return false;

	const FZoneLaneData& CurLaneData = ZoneGraphStorage->Lanes[LaneIndex];
	int32 FirstLink = CurLaneData.LinksBegin;
	int32 EndLink = CurLaneData.LinksEnd;

	TArray<FLaneVehicle> AdjVehicles;
	for (int32 LinkIndex = FirstLink; LinkIndex < EndLink; ++LinkIndex)
	{
		const FZoneLaneLinkData& LaneLink = ZoneGraphStorage->LaneLinks[LinkIndex];
		if (LaneLink.Type == EZoneLaneLinkType::Adjacent && LaneLink.HasFlags(EZoneLaneLinkFlags::Merging))
		{
			//找到相邻合并车道的第一辆车
			int32 AdjLaneIndex = LaneLink.DestLaneIndex;
			TArray<FLaneVehicle>* AdjLaneVehicles = LaneToEntitiesMap.Find(AdjLaneIndex);

			if (!AdjLaneVehicles || AdjLaneVehicles->Num() == 0)
				continue;

			AdjVehicles.Add((*AdjLaneVehicles)[AdjLaneVehicles->Num()-1]);
		}
	}

	if (AdjVehicles.Num() == 0)
		return false;

	//找到相邻车道的第一辆车们的最短剩余距离
	float MinLeftDist=FLT_MAX;
	for (auto Vehicle : AdjVehicles)
	{
		float LeftDist=Vehicle.VehicleMovementFragment.LeftDistance - Vehicle.VehicleMovementFragment.VehicleLength / 2;
		if (LeftDist < MinLeftDist)
		{
			MinLeftDist = LeftDist;
			AheadVehicle = &Vehicle.VehicleMovementFragment;
		}
	}

	if (CurVehicle->LeftDistance - CurVehicle->VehicleLength / 2 < MinLeftDist)
		return false;
	else
		return true;
}

void UTrafficSimSubsystem::CollectAdjMergeLanes()
{
	AdjMergeLanes.Empty();
	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("Failed to Get ZoneGraphStorage when CollectAdjMergeLanes!"));
		return;
	}
	int32 LaneCount = ZoneGraphStorage->Lanes.Num();
	for (int32 i = 0; i < LaneCount; i++)
	{
		const FZoneLaneData& Lane = ZoneGraphStorage->Lanes[i];
		TArray<int32> AdjMergeLaneArr;
		for (int32 j = Lane.LinksBegin; j < Lane.LinksEnd; j++)
		{
			const FZoneLaneLinkData& Link = ZoneGraphStorage->LaneLinks[j];
			if (Link.HasFlags(EZoneLaneLinkFlags::Merging)|| Link.HasFlags(EZoneLaneLinkFlags::Splitting))
			{
				AdjMergeLaneArr.Add(Link.DestLaneIndex);
			}
		}
		if (AdjMergeLaneArr.Num() != 0)
		{
			AdjMergeLanes.Add(i, AdjMergeLaneArr);
		}
	}
}

void UTrafficSimSubsystem::CollectLaneVehicles(FMassEntityHandle EntityHandle, const FMassVehicleMovementFragment& VehicleFragment)
{
	if(LaneToEntitiesMap.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("LaneToEntitiesMap is not initialized! Cannot collect lane vehicles."));
		return;
	}

	int32 LaneIndex = VehicleFragment.LaneLocation.LaneHandle.Index;
	
	TArray<FLaneVehicle>* LaneVehicles=LaneToEntitiesMap.Find(LaneIndex);

	if(!LaneVehicles)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("No vehicles found in lane %d."), LaneIndex);
		return;
	}

	FLaneVehicle NewVehicle(EntityHandle, VehicleFragment);
	LaneVehicles->Add(NewVehicle);
	LaneVehicles->Sort([](const FLaneVehicle& A, const FLaneVehicle& B) {
		return A.VehicleMovementFragment.LaneLocation.DistanceAlongLane < B.VehicleMovementFragment.LaneLocation.DistanceAlongLane;
		});
}

void UTrafficSimSubsystem::GetLaneVehicles(int32 LaneIndex, TConstArrayView<FLaneVehicle>& Vehilces)
{
	if (!LaneToEntitiesMap.Contains(LaneIndex))
	{
		UE_LOG(LogTrafficSim, Error, TEXT("Failed to find the lane by index:%d in the LaneToEntityMap!"),LaneIndex);
	}

	Vehilces=LaneToEntitiesMap.FindRef(LaneIndex);
}

void UTrafficSimSubsystem::InitializeTrafficTypes(TConstArrayView<FMassSpawnedEntityType> InTrafficTypes,FZoneGraphTag ConnectorTagIn)
{
	VehicleConfigTypes = InTrafficTypes;
	ConnectorTag = ConnectorTagIn;
	EntityTemplates.Empty();
	for (const FMassSpawnedEntityType& Type : VehicleConfigTypes)
	{
		// 获取实体模板
		const UMassEntityConfigAsset* EntityConfig = Type.EntityConfig.LoadSynchronous();
		if (!IsValid(EntityConfig))
		{
			UE_LOG(LogTrafficSim, Error, TEXT("TrafficSimSubsystem::InitializeTrafficTypes: Failed to load EntityConfigAsset for vehicle type."));
			continue;
		}
		if (!World)
		{
			UE_LOG(LogTrafficSim, Error, TEXT("TrafficSimSubsystem::InitializeTrafficTypes: World is not initialized."));
		}
		EntityTemplates.Add( &EntityConfig->GetOrCreateEntityTemplate(*GetWorld()));
		UE_LOG(LogTrafficSim, Log, TEXT("Add a EntityConfig.."));
	}
}

void UTrafficSimSubsystem::GetVehicleConfigs(TArray<float>& VehicleLenth, TArray<float>& PrefixSum)
{
	if(VehicleConfigTypes.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("VehicleConfigTypes is not initialized! Cannot get vehicle configs."));
		return;
	}
	VehicleLenth.Empty();
	PrefixSum.Empty();
	float TotalProportion = 0.0f;
	int32 ConfigIndex = 0;
	for(const FMassSpawnedEntityType& Type : VehicleConfigTypes)
	{
		TotalProportion += Type.Proportion;
		// 获取实体模板
		const FMassEntityTemplate& EntityTemplate =*EntityTemplates[ConfigIndex++];

		TConstArrayView<FInstancedStruct> InitialFragmentValues = EntityTemplate.GetInitialFragmentValues();

		for (const FInstancedStruct& Fragment : InitialFragmentValues)
		{
			if (Fragment.GetScriptStruct() == TBaseStructure<FMassVehicleMovementFragment>::Get())
			{
				const FMassVehicleMovementFragment* InitFrag = Fragment.GetPtr<FMassVehicleMovementFragment>();
				if (InitFrag)
				{
					// 访问 fragment 的属性
					float VehicleLength = InitFrag->VehicleLength;
					VehicleLenth.Add(VehicleLength);
				}
			}
		}

	}
	//构建累积概率表
	PrefixSum.Reserve(VehicleLenth.Num());
	float Accumulate = 0.0f;
	for (int32 i = 0; i < VehicleConfigTypes.Num(); i++)
	{
		Accumulate += VehicleConfigTypes[i].Proportion / TotalProportion;
		PrefixSum.Add(Accumulate);
	}
}

void UTrafficSimSubsystem::BroadcastEntitySpawnedEvent(const TArray<FName>& VehIDs, const TArray<int32>& VehTypes, const TArray<FVector>& VehPositions)
{
	if(bManualSim)
	{
		OnEntitySpawned.Broadcast(VehIDs, VehTypes, VehPositions);
	}
}


void UTrafficSimSubsystem::InitOnPostLoadMap(const UWorld::FActorsInitializedParams& Params)
{
	UE_LOG(LogTrafficSim, Log, TEXT("Post Init.."));
	World = Params.World;
	ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(World);

	for (TActorIterator<AZoneGraphData> It(World); It; ++It)
	{
		const AZoneGraphData* ZoneGraphDataTemp = *It;
		if (ZoneGraphDataTemp && ZoneGraphDataTemp->IsValidLowLevel())
		{
			ZoneGraphStorage = &ZoneGraphDataTemp->GetStorage();
			InitializeLaneToEntitiesMap();
			return;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("No valid ZoneGraphData found in the world!"));
}

void UTrafficSimSubsystem::InitOnPostEditorWorld(UWorld* InWorld, UWorld::InitializationValues IVS)
{
	UE_LOG(LogTrafficSim, Log, TEXT("Editor Post Init.."));
	World = InWorld;
	ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(World);

	for (TActorIterator<AZoneGraphData> It(World); It; ++It)
	{
		AZoneGraphData* ZoneGraphDataTemp = *It;
		if (IsValid(ZoneGraphDataTemp))
		{
			ZoneGraphStorage = &ZoneGraphDataTemp->GetStorage();
			mutableZoneGraphSotrage = &ZoneGraphDataTemp->GetStorageMutable();
			ZoneGraphData = ZoneGraphDataTemp;

			InitializeLaneToEntitiesMap();
			return;
		}
	}

	UE_LOG(LogTrafficSim, Error, TEXT("TrafficSimSubsystem::InitOnPostEditorWorld: No valid ZoneGraphData found in the Editor world!"));
}

void UTrafficSimSubsystem::AdjustLaneCongestion(int32 LaneIndex, ELaneCongestionMetric MetricType, float TargetValue, UMassEntityConfigAsset* OptionalConfig, float StartDist, float EndDist, float MinSafetyGap)
{
	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!EntitySubsystem)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("AdjustLaneCongestion: Missing EntitySubsystem"));
		return;
	}
	ULaneCongestionAdjustProcessor* Processor = NewObject<ULaneCongestionAdjustProcessor>(this);
	Processor->ManualInit(*this); // 调用包装的初始化
	Processor->TargetLaneIndex = LaneIndex;
	Processor->StartDist = StartDist;
	Processor->EndDist = EndDist;
	Processor->MetricType = MetricType;
	if (MetricType == ELaneCongestionMetric::DensityPerKm)
	{
		Processor->TargetDensityPer1000 = TargetValue; // interpret as vehicles per 1000uu
	}
	else
	{
		Processor->TargetAverageGap = TargetValue; // uu
	}
	Processor->SpawnEntityConfig = OptionalConfig;
	Processor->MinSafetyGap = MinSafetyGap;

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FMassProcessingContext ProcessingContext(EntityManager, 0.f);
	UE::Mass::Executor::Run(*Processor, ProcessingContext);
}

void UTrafficSimSubsystem::TagLaneSpeedGapSetup(const TMap<FZoneGraphTag, FTagLaneSpeed>& InTagLaneSpeed, const TMap<FZoneGraphTag, FTagLaneGap>& InTagLaneGap)
{
	TagLaneSpeed = InTagLaneSpeed;
	TagLaneGap = InTagLaneGap;
}

float UTrafficSimSubsystem::GetLaneSpeedByTag(FZoneGraphTagMask LaneTagMask, float& OutMaxSpeed, float& OutMinSpeed,FZoneGraphTag& ZoneLaneTag)
{
	for (auto& TagSpeedPair : TagLaneSpeed)
	{
		if (LaneTagMask.Contains(TagSpeedPair.Key))
		{
			OutMaxSpeed = TagSpeedPair.Value.MaxSpeed;
			OutMinSpeed = TagSpeedPair.Value.MinSpeed;
			ZoneLaneTag = TagSpeedPair.Key;
			return FMath::RandRange(OutMinSpeed, OutMaxSpeed);
		}
	}
	OutMaxSpeed = 60.f;
	OutMinSpeed = 40.f;
	ZoneLaneTag = FZoneGraphTag::None;
	return  FMath::RandRange(40.f, 60.f);;
}

void UTrafficSimSubsystem::RegisterVehPlateID(FName VehID, FMassEntityHandle EntityHandle)
{
	VehIDToEntityMap.Add(VehID, EntityHandle);
}

void UTrafficSimSubsystem::UnRegisterVehiPlateID(FName VehID)
{
	if(VehID!=NAME_None)
		VehIDToEntityMap.Remove(VehID);
}

void UTrafficSimSubsystem::GetEntityHandleByVehID(FName VehID, FMassEntityHandle& OutEntityHandle) const
{
	OutEntityHandle = VehIDToEntityMap.FindRef(VehID);
	//if(!OutEntityHandle.IsValid())
	//{		
	//	UE_LOG(LogTrafficSim, Warning, TEXT("GetEntityHandleByVehID: No valid entity handle found for VehID %s."), *VehID.ToString());
	//}
}

FTransform UTrafficSimSubsystem::GetEntityTransformByVehID(FName VehID,AActor*& VehActor, bool& bSuccess)
{
	bSuccess = false;
	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	UMassActorSubsystem* ActorSubsystem = UWorld::GetSubsystem<UMassActorSubsystem>(World);
	if (!EntitySubsystem)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("FillVehsOnLane: Failed to get EntitySubsystem."));
		return FTransform();
	}
	const FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	FMassEntityHandle EntityHandle;
	GetEntityHandleByVehID(VehID, EntityHandle);
	
	if(EntityHandle.IsValid()&& EntityManager.IsEntityValid(EntityHandle))
	{
		
		const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(EntityHandle);
		VehActor=ActorSubsystem->GetActorFromHandle(EntityHandle);
		if (TransformFrag && VehActor)
		{
			bSuccess = true;
			return TransformFrag->GetTransform();
		}
		else
		{
			UE_LOG(LogTrafficSim, Warning, TEXT("GetEntityTransformByVehID: No TransformFragment found for VehID %s."), *VehID.ToString());
		}
	}
	else
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("GetEntityTransformByVehID: No valid entity handle found for VehID %s."), *VehID.ToString());
	}

	return FTransform();
}

void UTrafficSimSubsystem::GetZonesByViewBounds(TArray<FVector> BoundPoints, FZoneGraphTag AnyTag, float Height, TArray<int32>& OutZoneIndices)
{
	FZoneGraphTagFilter TagFilter;
	TagFilter.AnyTags.Add(AnyTag);
	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Get Invalid ZoneGraphStorage!"));
		return;
	}
	if(BoundPoints.Num()==0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetZonesByViewBounds: BoundPoints is empty!"));
		return;
	}
	UE::ZoneGraph::Query::GetZonesByBoundPoints(*ZoneGraphStorage, BoundPoints, Height, TagFilter, OutZoneIndices);
}


