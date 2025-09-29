#include "LaneCongestionAdjustProcessor.h"
#include "TrafficSimSubsystem.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassVehicleMovementFragment.h"
#include "MassEntitySubsystem.h"
#include "MassSpawnerSubsystem.h"
#include "MassSpawnLocationProcessor.h"
#include "ZoneGraphQuery.h"

ULaneCongestionAdjustProcessor::ULaneCongestionAdjustProcessor() : EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false; // 手动触发
}

void ULaneCongestionAdjustProcessor::Initialize(UObject& Owner)
{
	TrafficSimSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTrafficSimSubsystem>() : nullptr;
}

void ULaneCongestionAdjustProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
}

void ULaneCongestionAdjustProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!TrafficSimSubsystem || !TrafficSimSubsystem->ZoneGraphStorage)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: Missing TrafficSimSubsystem or ZoneGraphStorage"));
		return;
	}
	if (TargetLaneIndex < 0 || TargetLaneIndex >= TrafficSimSubsystem->ZoneGraphStorage->Lanes.Num())
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: Invalid TargetLaneIndex %d"), TargetLaneIndex);
		return;
	}

	// 收集当前车道车辆
	TArray<FLaneVehicleRuntime> Vehicles;
	CollectLaneVehicles(EntityManager, Context, Vehicles);

	// 计算车道长度
	float LaneLength = 0.f;
	UE::ZoneGraph::Query::GetLaneLength(*TrafficSimSubsystem->ZoneGraphStorage, TargetLaneIndex, LaneLength);
	if (LaneLength <= 0.f)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: Lane length invalid"));
		return;
	}

	// 计算目标数量
	int32 TargetCount = ComputeTargetCount(LaneLength, Vehicles);
	int32 CurrentCount = Vehicles.Num();
	UE_LOG(LogTrafficSim, Log, TEXT("LaneCongestionAdjust: Lane %d length=%.1f Current=%d Target=%d"), TargetLaneIndex, LaneLength, CurrentCount, TargetCount);

	if (TargetCount < 0) return;

	if (CurrentCount > TargetCount)
	{
		RemoveExcessVehicles(EntityManager, Vehicles, TargetCount);
	}
	else if (CurrentCount < TargetCount)
	{
		SpawnAdditionalVehicles(EntityManager, TargetCount - CurrentCount, LaneLength, Vehicles);
	}
	else
	{
		// 数量已满足，可根据需要重新均匀分布（调整位置信息）
	}
}

void ULaneCongestionAdjustProcessor::CollectLaneVehicles(FMassEntityManager& EntityManager, FMassExecutionContext& Context, TArray<FLaneVehicleRuntime>& OutVehicles)
{

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, &OutVehicles](FMassExecutionContext& Ctx)
	{
		TArrayView<FMassVehicleMovementFragment> MoveList = Ctx.GetMutableFragmentView<FMassVehicleMovementFragment>();
		TArrayView<FTransformFragment> Xforms = Ctx.GetMutableFragmentView<FTransformFragment>();
		for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
		{
			FMassVehicleMovementFragment& Move = MoveList[i];
			if (Move.LaneLocation.LaneHandle.Index == TargetLaneIndex)
			{
				FLaneVehicleRuntime V; V.Entity = Ctx.GetEntity(i); V.MoveFrag = &Move;
				OutVehicles.Add(V);
			}
		}
	});
	OutVehicles.Sort([](const FLaneVehicleRuntime& A, const FLaneVehicleRuntime& B){ return A.MoveFrag->LaneLocation.DistanceAlongLane < B.MoveFrag->LaneLocation.DistanceAlongLane; });
}

int32 ULaneCongestionAdjustProcessor::ComputeTargetCount(float LaneLength, const TArray<FLaneVehicleRuntime>& Vehicles) const
{
	if (MetricType == ELaneCongestionMetric::DensityPerKm)
	{
		// 转换: 输入密度 per 1000uu (假设 1uu ~ 1cm 或者游戏自定义, 仅保持一致性)。
		float DensityPerUU = TargetDensityPer1000 / 1000.f; // vehicles per uu
		int32 Count = FMath::FloorToInt(DensityPerUU * LaneLength + 0.5f);
		return FMath::Max(0, Count);
	}
	else // AverageGap
	{
		if (TargetAverageGap <= KINDA_SMALL_NUMBER)
			return Vehicles.Num();
		// 近似：车辆平均长度（如果已有车辆）
		float AvgLen = 400.f; // 默认
		if (Vehicles.Num() > 0)
		{
			float SumLen = 0.f; int32 N = 0;
			for (const auto& V : Vehicles) { SumLen += V.MoveFrag->VehicleLength; ++N; }
			if (N > 0) AvgLen = SumLen / N;
		}
		float Segment = TargetAverageGap + AvgLen;
		int32 Count = FMath::FloorToInt(LaneLength / Segment);
		return FMath::Max(0, Count);
	}
}

void ULaneCongestionAdjustProcessor::RemoveExcessVehicles(FMassEntityManager& EntityManager, TArray<FLaneVehicleRuntime>& Vehicles, int32 TargetCount) const
{
	// 策略：删除最靠后的(或随机)。这里删除多余的尾部车辆，使前部稳定。
	int32 ToRemove = Vehicles.Num() - TargetCount;
	if (ToRemove <= 0) return;
	for (int32 i = 0; i < ToRemove; ++i)
	{
		FMassEntityHandle Entity = Vehicles[Vehicles.Num() - 1 - i].Entity;
		EntityManager.DestroyEntity(Entity);
	}
	UE_LOG(LogTrafficSim, Log, TEXT("LaneCongestionAdjust: Removed %d vehicles"), ToRemove);
}

void ULaneCongestionAdjustProcessor::SpawnAdditionalVehicles(FMassEntityManager& EntityManager, int32 AdditionalCount, float LaneLength, const TArray<FLaneVehicleRuntime>& ExistingVehicles) const
{
	if (AdditionalCount <= 0) return;
	UMassSpawnerSubsystem* Spawner = GetWorld()->GetSubsystem<UMassSpawnerSubsystem>();
	if (!Spawner)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: Missing SpawnerSubsystem"));
		return;
	}

	// 选择模板
	const FMassEntityTemplate* Template = nullptr;
	if (SpawnEntityConfig)
	{
		Template = &SpawnEntityConfig->GetOrCreateEntityTemplate(*GetWorld());
	}
	else if (TrafficSimSubsystem && TrafficSimSubsystem->EntityTemplates.Num() > 0)
	{
		Template = TrafficSimSubsystem->EntityTemplates[0];
	}
	if (!Template || !Template->IsValid())
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: No valid entity template"));
		return;
	}

	// 构建可用插入区间 (间隙)
	struct FGap { float Start; float End; };
	TArray<FGap> Gaps;

	// 提取已存在车辆的占用范围 (中心 - Len/2, 中心 + Len/2)
	TArray<TPair<float,float>> Occupied;
	for (const auto& V : ExistingVehicles)
	{
		float Center = V.MoveFrag->LaneLocation.DistanceAlongLane;
		float HalfL = V.MoveFrag->VehicleLength * 0.5f;
		Occupied.Add({ Center - HalfL - MinSafetyGap*0.5f, Center + HalfL + MinSafetyGap*0.5f });
	}
	Occupied.Sort([](const TPair<float,float>& A, const TPair<float,float>& B){ return A.Key < B.Key; });

	float Cursor = 0.f;
	for (const auto& Range : Occupied)
	{
		if (Range.Key > Cursor)
		{
			Gaps.Add({ Cursor, FMath::Max(Cursor, Range.Key) });
		}
		Cursor = FMath::Max(Cursor, Range.Value);
	}
	if (Cursor < LaneLength)
	{
		Gaps.Add({ Cursor, LaneLength });
	}

	// 在 gaps 中放置 AdditionalCount 个位置（简单均匀/随机混合）
	TArray<FVector> SpawnPositions;
	SpawnPositions.Reserve(AdditionalCount);

	int32 Remaining = AdditionalCount;
	for (const FGap& Gap : Gaps)
	{
		if (Remaining <= 0) break;
		float GapLen = Gap.End - Gap.Start;
		if (GapLen <= MinSafetyGap) continue;

		int32 CanPlace = FMath::Max(1, FMath::FloorToInt(GapLen / (MinSafetyGap)) - 1); // 预估
		CanPlace = FMath::Min(CanPlace, Remaining);
		for (int32 i = 0; i < CanPlace; ++i)
		{
			float LocalT = (i + 1.f) / (CanPlace + 1.f); // 均匀分布
			float Dist = Gap.Start + LocalT * GapLen;
			// 查询 lane 上位置
			FZoneGraphLaneLocation LaneLoc;
			UE::ZoneGraph::Query::CalculateLocationAlongLane(*TrafficSimSubsystem->ZoneGraphStorage, TargetLaneIndex, Dist, LaneLoc);
			SpawnPositions.Add(LaneLoc.Position);
		}
		Remaining -= CanPlace;
	}

	if (SpawnPositions.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Log, TEXT("LaneCongestionAdjust: No space to spawn additional vehicles"));
		return;
	}

	FInstancedStruct SpawnData;
	SpawnData.InitializeAs<FMassTransformsSpawnData>();
	FMassTransformsSpawnData& Transforms = SpawnData.GetMutable<FMassTransformsSpawnData>();
	for (const FVector& Pos : SpawnPositions)
	{
		FTransform& T = Transforms.Transforms.AddDefaulted_GetRef();
		T.SetLocation(Pos);
	}

	TArray<FMassEntityHandle> OutEntities;
	Spawner->SpawnEntities(Template->GetTemplateID(), SpawnPositions.Num(), SpawnData, UMassSpawnLocationProcessor::StaticClass(), OutEntities);
	UE_LOG(LogTrafficSim, Log, TEXT("LaneCongestionAdjust: Spawned %d vehicles"), SpawnPositions.Num());
}
