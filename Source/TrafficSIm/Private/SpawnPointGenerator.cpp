#include "SpawnPointGenerator.h"
#include "ZoneGraphSubsystem.h"
#include "MassSpawnLocationProcessor.h"
#include "SpawnPointInitProcessor.h"
#include "TrafficTypes.h"
#include "ZoneGraphQuery.h"
#include "TrafficSimSubsystem.h"

void USpawnPointGenerator::Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const
{
	UWorld* World = GetWorld();

	TArray<FMassEntitySpawnDataGeneratorResult> Results; // 确保任何情况下都能回调

	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("USpawnPointGenerator::Generate - World is null"));
		FinishedGeneratingSpawnPointsDelegate.Execute(Results);
		return;
	}

	UZoneGraphSubsystem* ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(World);
	if (!ZoneGraphSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("No ZoneGraphSubsystem found"));
		FinishedGeneratingSpawnPointsDelegate.Execute(Results);
		return;
	}

	if (!ZoneGraphData)
	{
		UE_LOG(LogTemp, Warning, TEXT("No ZoneGraphData found"));
		FinishedGeneratingSpawnPointsDelegate.Execute(Results);
		return;
	}

	const FZoneGraphStorage& ZoneGraphStorage = ZoneGraphData->GetStorage();

	UTrafficSimSubsystem* TrafficSimSubsystem = World->GetSubsystem<UTrafficSimSubsystem>();
	if (!TrafficSimSubsystem)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("Failed to Get TrafficSimSubsystem"));
	}

	// 找到所有没有 Incoming 链接且不包含 ConnectorTag 的车道
	TArray<int32> StartLanes;
	StartLanes.Reserve(ZoneGraphStorage.Lanes.Num());

	for (int32 i = 0; i < ZoneGraphStorage.Lanes.Num(); i++)
	{
		const FZoneLaneData& Lane = ZoneGraphStorage.Lanes[i];

		bool bIsStartLane = true;

		// 仅当 TrafficSimSubsystem 存在时检查 ConnectorTag
		if (TrafficSimSubsystem && Lane.Tags.Contains(TrafficSimSubsystem->ConnectorTag))
		{
			bIsStartLane = false;
		}

		// 若仍可能是起始车道，则检查是否有 Incoming
		for (int32 j = Lane.LinksBegin; bIsStartLane && j < Lane.LinksEnd; j++)
		{
			const FZoneLaneLinkData& Link = ZoneGraphStorage.LaneLinks[j];
			if (Link.Type == EZoneLaneLinkType::Incoming)
			{
				bIsStartLane = false;
				break;
			}
		}

		if (bIsStartLane)
		{
			StartLanes.Add(i);
		}
	}

	// 若请求数量有限制，遵循 Count
	const int32 NumToSpawn = (Count >= 0) ? FMath::Min(Count, StartLanes.Num()) : StartLanes.Num();

	if (NumToSpawn <= 0)
	{
		// 仍需回调以通知 MassSpawner：生成完成（但无实体）
		FinishedGeneratingSpawnPointsDelegate.Execute(Results);
		return;
	}

	// 构建生成数据
	FMassEntitySpawnDataGeneratorResult& Res = Results.AddDefaulted_GetRef();
	Res.EntityConfigIndex = 0; // 默认使用第一个配置（如需按类型分配，可在此根据 EntityTypes 决策）
	Res.NumEntities = NumToSpawn;

	Res.SpawnDataProcessor = USpawnPointInitProcessor::StaticClass();
	Res.SpawnData.InitializeAs<FVehicleInitData>();
	FVehicleInitData& InitData = Res.SpawnData.GetMutable<FVehicleInitData>();
	InitData.LaneLocations.Reserve(NumToSpawn);

	for (int32 k = 0; k < NumToSpawn; ++k)
	{
		const int32 LaneIndex = StartLanes[k];
		FZoneGraphLaneLocation LaneLocation;
		UE::ZoneGraph::Query::CalculateLocationAlongLane(ZoneGraphStorage, LaneIndex, 0.0f, LaneLocation);

		FZoneGraphLaneLocation& InitLaneLocation = InitData.LaneLocations.AddDefaulted_GetRef();
		InitLaneLocation = LaneLocation;
	}

	FinishedGeneratingSpawnPointsDelegate.Execute(Results);
}