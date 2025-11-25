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

	if(GetWorld()->IsEditorWorld())
	{
		FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UTrafficSimSubsystem::InitOnPostEditorWorld);
	}
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

	//准备生成数据

	// 1. 获取模板 ID
	const FMassEntityTemplate& EntityTemplate = EntityConfigAsset->GetOrCreateEntityTemplate(*World);
	if (!EntityTemplate.IsValid())
	{
		UE_LOG(LogTrafficSim, Error, TEXT("Invalid EntityTemplate"));
		return;
	}

	// 2. 构建批量 SpawnData
	FInstancedStruct SpawnData;
	SpawnData.InitializeAs<FMassTransformsSpawnData>();
	FMassTransformsSpawnData& Transforms = SpawnData.GetMutable<FMassTransformsSpawnData>();

	Transforms.Transforms.Reserve(SpawnLocations.Num());
	for (const FVector& Loc : SpawnLocations)
	{
		FTransform& Transform = Transforms.Transforms.AddDefaulted_GetRef();
		Transform.SetLocation(Loc);
	}

	// 3. 调用批量 Spawn
	UMassSpawnerSubsystem* SpawnerSubsystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(World);
	TArray<FMassEntityHandle> OutEntities;
	SpawnerSubsystem->SpawnEntities(
		EntityTemplate.GetTemplateID(),
		SpawnLocations.Num(),
		SpawnData,
		UMassSpawnLocationProcessor::StaticClass(), // 和 MassSpawner 一致
		OutEntities);

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	
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
	ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(World);
	World = GetWorld();
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

int32 UTrafficSimSubsystem::GetZoneLaneIndexByPoints(TArray<FVector> Points, FZoneGraphTag AnyTag, float Height)
{
	if (!ZoneGraphStorage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Get Invalid ZoneGraphStorage!"));
		return -1;
	}
	TArray<int32> LaneIndiceSingle;
	TArray<TTuple<float, float>> StartEnd;
	FZoneGraphTagFilter Filter;
	Filter.AnyTags.Add(AnyTag);
	UE::ZoneGraph::Query::FindNearestLanesBySeg(*ZoneGraphStorage, Points, Height, Filter, LaneIndiceSingle, StartEnd);

	if(LaneIndiceSingle.Num()==0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No Lane Found by Given Points!"));
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



void UTrafficSimSubsystem::AddSpawnPointAtLane(int32 LaneIndex, float DistanceAlongLane, UMassEntityConfigAsset * EntityConfigAsset, TArray<FName> VehIDs)
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

		// 计算车道上的位置
		FZoneGraphLaneLocation LaneLocation;
		UE::ZoneGraph::Query::CalculateLocationAlongLane(*ZoneGraphStorage, LaneIndex, DistanceAlongLane, LaneLocation);

		const FMassEntityTemplate& Template = EntityConfigAsset->GetOrCreateEntityTemplate(*World);
		if (!Template.IsValid())
		{
			UE_LOG(LogTrafficSim, Error, TEXT("AddSpawnPointAtLane: Invalid entity template."));
			return;
		}

		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

		// 创建实体
		FMassEntityHandle SpawnPointEntity = EntityManager.CreateEntity(Template.GetArchetype());


		FTransformFragment& TransformFrag = EntityManager.GetFragmentDataChecked<FTransformFragment>(SpawnPointEntity);
		FMassSpawnPointFragment& SpawnPointFrag = EntityManager.GetFragmentDataChecked<FMassSpawnPointFragment>(SpawnPointEntity);

		TransformFrag.SetTransform(FTransform(LaneLocation.Position));
		

		// 初始化 Fragment
		SpawnPointFrag.LaneLocation = LaneLocation;
		SpawnPointFrag.Duration = 5.f;
		SpawnPointFrag.RandOffset = 2.f;
		SpawnPointFrag.Clock = 0.0;
		SpawnPointFrag.NextVehicleType = 0; // 示例：默认指向第一个
		SpawnPointFrag.VehicleIDs = VehIDs;

		// 调试可视化
		DrawDebugPoint(World, LaneLocation.Position, 30.f, FColor::Green, false, 6.f);

		UE_LOG(LogTrafficSim, Log, TEXT("SpawnPoint entity created at Lane %d Dist %.2f EntitySN:%d VehIDs:%d"),
			LaneIndex, DistanceAlongLane, SpawnPointEntity.SerialNumber, VehIDs.Num());
	
}

void UTrafficSimSubsystem::LineTraceEntity(FVector Start, FVector End)
{
	UMassRepresentationSubsystem* Rep = UWorld::GetSubsystem<UMassRepresentationSubsystem>(World);

	//Rep->GetEnt
	
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
bool UTrafficSimSubsystem::FindFrontVehicle(int32 LaneIndex, int32 NextLaneIndex,FMassEntityHandle CurVehicle, const FMassVehicleMovementFragment* & FrontVehicle)
{

	TArray<FLaneVehicle>* LaneVehicles = LaneToEntitiesMap.Find(LaneIndex);
	TArray<FLaneVehicle> LaneVehicleCopy;

	LaneVehicleCopy.Append(*LaneVehicles);

	TArray<int32>* AdjLanes = AdjMergeLanes.Find(LaneIndex);
	if (AdjLanes)
	{
		//找到合并车道上的所有车辆
		for (int32 i=0; i < AdjLanes->Num(); ++i)
		{
			LaneVehicleCopy.Append(*LaneToEntitiesMap.Find((*AdjLanes)[i]));
		}

		LaneVehicleCopy.Sort([](const FLaneVehicle& A, const FLaneVehicle& B) {
			return A.VehicleMovementFragment.LeftDistance > B.VehicleMovementFragment.LeftDistance;
			});
		//UE_LOG(LogTemp, Log, TEXT("Total Vehicles Num:%d"), LaneVehicleCopy.Num());
		
	}

		
	if(LaneVehicleCopy.Num()==0)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("No vehicles found in lane %d."), LaneIndex);
		return false;
	}

	int32 VehicleNumInCurLane = LaneVehicleCopy.Num();

	int32 CurVehicleIndex = LaneVehicleCopy.IndexOfByPredicate([CurVehicle](const FLaneVehicle& Vehicle) {
		return Vehicle.EntityHandle == CurVehicle;
		});

	if(CurVehicleIndex==INDEX_NONE)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("Current vehicle not found in lane %d."), LaneIndex);
		return false;
	}
	
	//当前车辆不是车道第一辆车 (越靠前的车辆下标越大)
	if (CurVehicleIndex < VehicleNumInCurLane - 1)
	{
		FrontVehicle = &LaneVehicleCopy[CurVehicleIndex + 1].VehicleMovementFragment;
		return false;
	}
	if (NextLaneIndex < 0)
	{
		//UE_LOG(LogTrafficSim, VeryVerbose, TEXT("NextLaneIndex is invalid: %d."), NextLaneIndex);
		return false;
	}
	//当前车辆是车道第一辆车，即将进入下一车道且下条车道有车辆
	if(CurVehicleIndex == VehicleNumInCurLane - 1 )
	{
		TArray<int32>* NextAdjLanes = AdjMergeLanes.Find(NextLaneIndex);
		if (NextAdjLanes)
		{
			LaneVehicleCopy.Empty();
			LaneVehicleCopy.Append(*LaneToEntitiesMap.Find(NextLaneIndex));
			for (int32 i = 0; i < NextAdjLanes->Num(); ++i)
			{
				LaneVehicleCopy.Append(*LaneToEntitiesMap.Find((*NextAdjLanes)[i]));
			}
			const FLaneVehicle* MaxVeh = Algo::MaxElementBy(
				LaneVehicleCopy,
				[](const FLaneVehicle& V) { return V.VehicleMovementFragment.LeftDistance; }
			);
			
			if(LaneVehicleCopy.Num() == 0)
				return true;

			FrontVehicle = &MaxVeh->VehicleMovementFragment;
			return true;
		}
		else 
		{
			TArray<FLaneVehicle>* NextLaneVehicles = LaneToEntitiesMap.Find(NextLaneIndex);
			if (LaneVehicleCopy.Num()==0 || NextLaneVehicles->Num() == 0)
			{
				return true;
			}
			FrontVehicle = &(*NextLaneVehicles)[0].VehicleMovementFragment;
			return true;
		}
	}

	

	return false;
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
	}
	for (int32 i = 0; i < ZoneGraphStorage->Lanes.Num(); i++)
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
		const UMassEntityConfigAsset* EntityConfig = Type.EntityConfig.LoadSynchronous();;
		EntityTemplates.Add( &EntityConfig->GetOrCreateEntityTemplate(*World));
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
		const AZoneGraphData* ZoneGraphDataTemp = *It;
		if (ZoneGraphDataTemp && ZoneGraphDataTemp->IsValidLowLevel())
		{
			ZoneGraphStorage = &ZoneGraphDataTemp->GetStorage();
			InitializeLaneToEntitiesMap();
			return;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("No valid ZoneGraphData found in the Editor world!"));
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
