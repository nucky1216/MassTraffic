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

DEFINE_LOG_CATEGORY(LogTrafficSim);


void UTrafficSimSubsystem::InitializeLaneToEntitiesMap()
{
	//TODO:: 高效的结合Processor来管理车道车辆映射

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
	GetWorld()->OnActorsInitialized.AddUObject(this, &UTrafficSimSubsystem::InitOnPostLoadMap);
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
		if(LaneVehicle.VehicleMovementFragment)
		{
			FVector Position = LaneVehicle.VehicleMovementFragment->LaneLocation.Position;

			const FMassVehicleMovementFragment* FrontVehicle = nullptr;
			bool found=FindFrontVehicle(LaneVehicle.VehicleMovementFragment->LaneLocation.LaneHandle.Index,
				LaneVehicle.VehicleMovementFragment->NextLane,LaneVehicle.EntityHandle, FrontVehicle);

			int32 ForntVehicleSN = -1;
			if (found && FrontVehicle)
			{
				ForntVehicleSN = FrontVehicle->VehicleHandle.SerialNumber;
			}

			UE_LOG(LogTrafficSim, Log, TEXT("Entity: %d, DistanceAlongLane: %.2f Position:%s FrontVehicle:%d"),
				LaneVehicle.EntityHandle.SerialNumber, 
				LaneVehicle.VehicleMovementFragment->LaneLocation.DistanceAlongLane,
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
	FLaneVehicle TargetVehicle = (*VehicleArray)[EntitySN];

	const FMassVehicleMovementFragment* Frag = TargetVehicle.VehicleMovementFragment;
	UTrafficLightSubsystem* TrafficLightSubsystem = UWorld::GetSubsystem<UTrafficLightSubsystem>(World);

	bool OpenLane = false,IntersectionLane = false;
	const FMassVehicleMovementFragment* FrontVehicleMovement = nullptr;

	bool HasFrontCar = FindFrontVehicle(Frag->LaneLocation.LaneHandle.Index,
		Frag->NextLane, Frag->VehicleHandle, FrontVehicleMovement);

	TrafficLightSubsystem->QueryLaneOpenState(Frag->NextLane, OpenLane, IntersectionLane);
	UE_LOG(LogTemp, Log, TEXT("CurLane:%d, NextLane:%d,TargetSpeed:%f, Speed:%f,NextLaneInInsec:%d,IsOpen:%d,LeftDistance:%f,VehicleLength:%f,HasForntCar:%d"),
		Frag->LaneLocation.LaneHandle.Index, Frag->NextLane,
		Frag->TargetSpeed, Frag->Speed,
		IntersectionLane,OpenLane,
		Frag->LeftDistance, Frag->VehicleLength, HasFrontCar
		);
	
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

bool UTrafficSimSubsystem::FindFrontVehicle(int32 LaneIndex, int32 NextLaneIndex,FMassEntityHandle CurVehicle, const FMassVehicleMovementFragment* & FrontVehicle)
{

	TArray<FLaneVehicle>* LaneVehicles=LaneToEntitiesMap.Find(LaneIndex);
	if(!LaneVehicles)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("No vehicles found in lane %d."), LaneIndex);
		return false;
	}

	int32 VehicleNumInCurLane = LaneVehicles->Num();

	int32 CurVehicleIndex = LaneVehicles->IndexOfByPredicate([CurVehicle](const FLaneVehicle& Vehicle) {
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
		FrontVehicle = (*LaneVehicles)[CurVehicleIndex + 1].VehicleMovementFragment;
		return true;
	}
	if (NextLaneIndex < 0)
	{
		//UE_LOG(LogTrafficSim, VeryVerbose, TEXT("NextLaneIndex is invalid: %d."), NextLaneIndex);
		return false;
	}
	//当前车辆是车道第一辆车，即将进入下一车道且下条车道有车辆
	if(CurVehicleIndex == VehicleNumInCurLane - 1 )
	{
		//TArray<int32> NextLanes;
		//ChooseNextLane((*LaneVehicles)[CurVehicleIndex].VehicleMovementFragment->LaneLocation.LaneHandle.Index, NextLanes);

		TArray<FLaneVehicle>* NextLaneVehicles = LaneToEntitiesMap.Find(NextLaneIndex);
		if (!LaneVehicles || NextLaneVehicles->Num()==0)
		{
			return false;
		}
		FrontVehicle = (*NextLaneVehicles)[0].VehicleMovementFragment;
		return false;

	}
	//TODO:: 当前车辆处于合并车道，如何避免周围的车的距离过近？

	return false;
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

	FLaneVehicle NewVehicle(EntityHandle, &VehicleFragment);
	LaneVehicles->Add(NewVehicle);
	LaneVehicles->Sort([](const FLaneVehicle& A, const FLaneVehicle& B) {
		return A.VehicleMovementFragment->LaneLocation.DistanceAlongLane < B.VehicleMovementFragment->LaneLocation.DistanceAlongLane;
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

void UTrafficSimSubsystem::InitializeTrafficTypes(TConstArrayView<FMassSpawnedEntityType> InTrafficTypes)
{
	VehicleConfigTypes = InTrafficTypes;
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

void UTrafficSimSubsystem::AdjustLaneCongestion(int32 LaneIndex, ELaneCongestionMetric MetricType, float TargetValue, UMassEntityConfigAsset* OptionalConfig, float MinSafetyGap)
{
	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!EntitySubsystem)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("AdjustLaneCongestion: Missing EntitySubsystem"));
		return;
	}
	ULaneCongestionAdjustProcessor* Processor = NewObject<ULaneCongestionAdjustProcessor>();
	Processor->TargetLaneIndex = LaneIndex;
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
