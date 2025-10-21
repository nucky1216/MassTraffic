#include "LaneCongestionAdjustProcessor.h"
#include "TrafficSimSubsystem.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassVehicleMovementFragment.h"
#include "MassEntitySubsystem.h"
#include "MassSpawnerSubsystem.h"
#include "ZoneGraphTypes.h"
#include "MassSpawnLocationProcessor.h"
#include "ZoneGraphQuery.h"

ULaneCongestionAdjustProcessor::ULaneCongestionAdjustProcessor() : EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add("CollectLaneVehiclesProcessor");
	ExecutionOrder.ExecuteBefore.Add("VehicleMovementProcessor");
	bAutoRegisterWithProcessingPhases = false; // 手动触发
}

void ULaneCongestionAdjustProcessor::Initialize(UObject& Owner)
{
	TrafficSimSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTrafficSimSubsystem>() : nullptr;
	UE_LOG(LogTrafficSim,VeryVerbose,TEXT("LaneCongestionAdjustProcessor Initialized."));
}

void ULaneCongestionAdjustProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
}

void ULaneCongestionAdjustProcessor::CollectLaneVehicles(FMassEntityManager& EntityManager, FMassExecutionContext& Context, TArray<FLaneVehicleRuntime>& OutVehicles)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, &OutVehicles](FMassExecutionContext& Ctx)
	{
		TArrayView<FMassVehicleMovementFragment> MoveList = Ctx.GetMutableFragmentView<FMassVehicleMovementFragment>();
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
		float DensityPerUU = TargetDensityPer1000 / 1000.f;
		int32 Count = FMath::FloorToInt(DensityPerUU * LaneLength + 0.5f);
		return FMath::Max(0, Count);
	}
	else
	{
		if (TargetAverageGap <= KINDA_SMALL_NUMBER)
			return Vehicles.Num();
		float AvgLen = 400.f;
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
	int32 ToRemove = Vehicles.Num() - TargetCount;
	if (ToRemove <= 0) return;
	for (int32 i = 0; i < ToRemove; ++i)
	{
		FMassEntityHandle Entity = Vehicles[Vehicles.Num() - 1 - i].Entity;
		EntityManager.Defer().DestroyEntity(Entity);
	}
	UE_LOG(LogTrafficSim, Verbose, TEXT("LaneCongestionAdjust: Removed %d vehicles"), ToRemove);
}

void ULaneCongestionAdjustProcessor::SpawnAdditionalVehicles(FMassEntityManager& EntityManager, int32 AdditionalCount, float LaneLength, const TArray<FLaneVehicleRuntime>& ExistingVehicles) const
{
	if (AdditionalCount <= 0) return;
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
	struct FGap { float Start; float End; };
	TArray<FGap> Gaps;
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
			Gaps.Add({ Cursor, FMath::Max(Cursor, Range.Key) });
		Cursor = FMath::Max(Cursor, Range.Value);
	}
	if (Cursor < LaneLength) Gaps.Add({ Cursor, LaneLength });
	TArray<FVector> SpawnPositions;
	TArray<float> DistAlongLane;
	SpawnPositions.Reserve(AdditionalCount);
	DistAlongLane.Reserve(AdditionalCount);
	int32 Remaining = AdditionalCount;
	for (const FGap& Gap : Gaps)
	{
		if (Remaining <= 0) break;
		float GapLen = Gap.End - Gap.Start;
		if (GapLen <= MinSafetyGap) continue;
		int32 CanPlace = FMath::Max(1, FMath::FloorToInt(GapLen / (MinSafetyGap)) - 1);
		CanPlace = FMath::Min(CanPlace, Remaining);
		for (int32 i = 0; i < CanPlace; ++i)
		{
			float LocalT = (i + 1.f) / (CanPlace + 1.f);
			float Dist = Gap.Start + LocalT * GapLen;
			FZoneGraphLaneLocation LaneLoc;
			UE::ZoneGraph::Query::CalculateLocationAlongLane(*TrafficSimSubsystem->ZoneGraphStorage, TargetLaneIndex, Dist, LaneLoc);
			SpawnPositions.Add(LaneLoc.Position);
			DistAlongLane.Add(Dist);
		}
		Remaining -= CanPlace;
	}
	if (SpawnPositions.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Log, TEXT("LaneCongestionAdjust: No space to spawn additional vehicles"));
		return;
	}
	TArray<FMassEntityHandle> NewEntities;
	TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager.BatchCreateEntities(Template->GetArchetype(), Template->GetSharedFragmentValues(), SpawnPositions.Num(), NewEntities);
	EntityManager.BatchSetEntityFragmentsValues(CreationContext->GetEntityCollection(), Template->GetInitialFragmentValues());
	for (int32 i = 0; i < NewEntities.Num(); ++i)
	{
		const FMassEntityHandle Entity = NewEntities[i];
		FTransformFragment& TransformFrag = EntityManager.GetFragmentDataChecked<FTransformFragment>(Entity);
		FTransform& MutableTransform = TransformFrag.GetMutableTransform();
		MutableTransform.SetLocation(SpawnPositions[i]);
		FMassVehicleMovementFragment& MoveFrag = EntityManager.GetFragmentDataChecked<FMassVehicleMovementFragment>(Entity);
		FZoneGraphLaneLocation LaneLoc;
		UE::ZoneGraph::Query::CalculateLocationAlongLane(*TrafficSimSubsystem->ZoneGraphStorage, TargetLaneIndex, DistAlongLane[i], LaneLoc);
		MoveFrag.LaneLocation = LaneLoc;
	}
	UE_LOG(LogTrafficSim, Log, TEXT("LaneCongestionAdjust: Spawned %d vehicles (EntityManager)"), NewEntities.Num());
}
void ULaneCongestionAdjustProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!TrafficSimSubsystem || !TrafficSimSubsystem->ZoneGraphStorage)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: Missing TrafficSimSubsystem or ZoneGraphStorage"));
		if (!TrafficSimSubsystem)
		{
			UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: TrafficSimSubsystem is nullptr"));
			return;
		}
	}
	if (TargetLaneIndex < 0 || TargetLaneIndex >= TrafficSimSubsystem->ZoneGraphStorage->Lanes.Num())
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: Invalid TargetLaneIndex %d"), TargetLaneIndex);
		return;
	}

	float LaneLength = 0.f;
	UE::ZoneGraph::Query::GetLaneLength(*TrafficSimSubsystem->ZoneGraphStorage, TargetLaneIndex, LaneLength);
	if (LaneLength <= 0.f)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: Lane length invalid"));
		return;
	}

	// 仅在区间 [S, E] 内调整
	const float S = FMath::Clamp(FMath::Min(StartDist, EndDist), 0.f, LaneLength);
	const float E = FMath::Clamp(FMath::Max(StartDist, EndDist), 0.f, LaneLength);
	if (E - S <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: Invalid range [%.1f, %.1f] after clamp"), StartDist, EndDist);
		return;
	}

	TArray<float> VehicleLenth, PrefixSum;
	TrafficSimSubsystem->GetVehicleConfigs(VehicleLenth, PrefixSum);
	if (VehicleLenth.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: No vehicle configs available"));
		return;
	}
	TArray<float> SortedVehicleLenth = VehicleLenth;
	SortedVehicleLenth.Sort([](const float& A, const float& B) { return A > B; });
	const float MinVehicleLen = SortedVehicleLenth.Num() ? SortedVehicleLenth.Last() : 0.f;

	// Strict removal gap / insertion gap
	const float StrictRemovalGap = FMath::Max(TargetAverageGap, MinSafetyGap);
	const float GapRelaxPercent = 0.2f; // 20%
	float InsertionGap = TargetAverageGap > 0.f ? TargetAverageGap * (1.f - GapRelaxPercent) : MinSafetyGap;
	InsertionGap = FMath::Clamp(InsertionGap, MinSafetyGap, StrictRemovalGap);

	// 用 EntityQuery 遍历当前帧有效的实体，构建值类型快照
	struct FVehicleInfo
	{
		FMassEntityHandle Entity;
		float Center = 0.f;
		float Length = 0.f;
	};
	TArray<FVehicleInfo> All;
	All.Reserve(256);

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, &All](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FMassVehicleMovementFragment> MoveList = Ctx.GetFragmentView<FMassVehicleMovementFragment>();
			const int32 Num = Ctx.GetNumEntities();
			for (int32 i = 0; i < Num; ++i)
			{
				const FMassVehicleMovementFragment& Move = MoveList[i];
				if (Move.LaneLocation.LaneHandle.Index != TargetLaneIndex)
					continue;

				FVehicleInfo Info;
				Info.Entity = Ctx.GetEntity(i);
				Info.Center = Move.LaneLocation.DistanceAlongLane;
				Info.Length = Move.VehicleLength;
				All.Add(Info);
			}
		});

	if (All.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Verbose, TEXT("LaneCongestionAdjust: No vehicles to process on lane %d"), TargetLaneIndex);
		return;
	}

	// 预处理：区间内车辆 + 两侧边界车（基于快照）
	const FVehicleInfo* PrevOuter = nullptr;
	const FVehicleInfo* NextOuter = nullptr;
	TArray<const FVehicleInfo*> InRange;
	InRange.Reserve(All.Num());

	for (const FVehicleInfo& V : All)
	{
		const float Center = V.Center;
		if (Center < S)
		{
			if (!PrevOuter || Center > PrevOuter->Center)
				PrevOuter = &V;
		}
		else if (Center > E)
		{
			if (!NextOuter || Center < NextOuter->Center)
				NextOuter = &V;
		}
		else
		{
			InRange.Add(&V);
		}
	}

	ensureMsgf(!InRange.Contains(nullptr), TEXT("InRange contains nullptr"));
	// 指针元素排序：比较器使用“被指对象的引用”以配合 TDereferenceWrapper
	InRange.Sort([](const FVehicleInfo& A, const FVehicleInfo& B)
		{
			return A.Center < B.Center;
		});

	// 1) 删除过近车辆
	TArray<FMassEntityHandle> ToDelete;
	TArray<const FVehicleInfo*> KeptInRange;

	bool bHasPrevEnd = false;
	float PrevEnd = 0.f;
	if (PrevOuter)
	{
		PrevEnd = PrevOuter->Center + PrevOuter->Length * 0.5f;
		bHasPrevEnd = true;
	}

	for (int32 i = 0; i < InRange.Num(); ++i)
	{
		const FVehicleInfo* Cur = InRange[i];
		const float CurCenter = Cur->Center;
		const float CurLen = Cur->Length;
		const float CurStart = CurCenter - CurLen * 0.5f;

		if (!bHasPrevEnd && KeptInRange.Num() == 0)
		{
			KeptInRange.Add(Cur);
			PrevEnd = CurCenter + CurLen * 0.5f;
			bHasPrevEnd = true;
			continue;
		}

		const float Headway = CurStart - PrevEnd;
		if (Headway < StrictRemovalGap)
		{
			ToDelete.Add(Cur->Entity);
		}
		else
		{
			KeptInRange.Add(Cur);
			PrevEnd = CurCenter + CurLen * 0.5f;
		}
	}

	if (ToDelete.Num() > 0)
	{
		for (const FMassEntityHandle& EHandle : ToDelete)
		{
			EntityManager.Defer().DestroyEntity(EHandle);
		}
		UE_LOG(LogTrafficSim, Verbose, TEXT("LaneCongestionAdjust: Removed %d vehicles in [%.1f, %.1f] (strict gap %.1f)"),
			ToDelete.Num(), S, E, StrictRemovalGap);
	}

	// 2) 构造可用间隙
	struct FGap { float Start = 0.f; float End = 0.f; };
	TArray<FGap> Gaps;

	float BoundStart = S;
	if (PrevOuter)
	{
		const float PrevOuterEnd = PrevOuter->Center + PrevOuter->Length * 0.5f;
		BoundStart = FMath::Max(BoundStart, PrevOuterEnd + InsertionGap);
	}

	float BoundEnd = E;
	if (NextOuter)
	{
		const float NextOuterStart = NextOuter->Center - NextOuter->Length * 0.5f;
		BoundEnd = FMath::Min(BoundEnd, NextOuterStart - InsertionGap);
	}

	auto AddGapIfUsable = [&](float GS, float GE)
		{
			const float Start = FMath::Max(GS, BoundStart);
			const float End = FMath::Min(GE, BoundEnd);
			if (End - Start >= MinVehicleLen + KINDA_SMALL_NUMBER)
			{
				Gaps.Add({ Start, End });
			}
		};

	if (KeptInRange.Num() == 0)
	{
		AddGapIfUsable(S, E);
	}
	else
	{
		const FVehicleInfo* First = KeptInRange[0];
		const float FirstStart = First->Center - First->Length * 0.5f;
		AddGapIfUsable(S, FirstStart - InsertionGap);

		for (int32 i = 0; i < KeptInRange.Num() - 1; ++i)
		{
			const FVehicleInfo* A = KeptInRange[i];
			const FVehicleInfo* B = KeptInRange[i + 1];
			const float AEnd = A->Center + A->Length * 0.5f;
			const float BStart = B->Center - B->Length * 0.5f;
			AddGapIfUsable(AEnd + InsertionGap, BStart - InsertionGap);
		}

		const FVehicleInfo* Last = KeptInRange.Last();
		const float LastEnd = Last->Center + Last->Length * 0.5f;
		AddGapIfUsable(LastEnd + InsertionGap, E);
	}

	if (Gaps.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Verbose, TEXT("LaneCongestionAdjust: No usable gaps in [%.1f, %.1f] after cleanup"), S, E);
		return;
	}

	auto ChooseRandomVehicleIndex = [&PrefixSum]() -> int32
		{
			const float R = FMath::FRand();
			for (int32 i = 0; i < PrefixSum.Num(); ++i)
				if (R <= PrefixSum[i]) return i;
			return PrefixSum.Num() - 1;
		};

	// 3) 按 Gaps 迭代填充
	TMap<int32, TArray<float>> TemplateToDistances;
	const int32 MaxSpawnPerGapLoop = 512;

	for (const FGap& Gap : Gaps)
	{
		float Cursor = Gap.Start;
		int32 Iter = 0;
		while (Iter++ < MaxSpawnPerGapLoop)
		{
			const float Remaining = Gap.End - Cursor;
			if (Remaining < MinVehicleLen + KINDA_SMALL_NUMBER)
				break;

			float ChosenLen = 0.f;
			int32 VehicleIdx = INDEX_NONE;

			const int32 RandomAttempts = 6;
			for (int32 Attempt = 0; Attempt < RandomAttempts; ++Attempt)
			{
				const int32 CandIdx = ChooseRandomVehicleIndex();
				const float Len = VehicleLenth.IsValidIndex(CandIdx) ? VehicleLenth[CandIdx] : MinVehicleLen;
				if (Len <= Remaining) { ChosenLen = Len; VehicleIdx = CandIdx; break; }
			}

			if (VehicleIdx == INDEX_NONE)
			{
				for (float Len : SortedVehicleLenth) // descending
				{
					if (Len <= Remaining)
					{
						ChosenLen = Len;
						VehicleIdx = VehicleLenth.IndexOfByPredicate([Len](float V) { return FMath::IsNearlyEqual(V, Len); });
						break;
					}
				}
			}

			if (VehicleIdx == INDEX_NONE || ChosenLen <= 0.f)
				break;

			const float Start = Cursor;
			const float End = Start + ChosenLen;
			if (End > Gap.End + KINDA_SMALL_NUMBER)
				break;

			const float CenterDist = Start + ChosenLen * 0.5f;
			TemplateToDistances.FindOrAdd(VehicleIdx).Add(CenterDist);
			Cursor = End + InsertionGap; // advance
		}
	}

	if (TemplateToDistances.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Log, TEXT("LaneCongestionAdjust: No vehicles spawned in [%.1f, %.1f] (gaps too small)"), S, E);
		return;
	}
	if (TrafficSimSubsystem->EntityTemplates.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: No entity templates available for spawning"));
		return;
	}

	int32 TotalSpawned = 0;
	for (auto& Pair : TemplateToDistances)
	{
		const int32 TemplateIdx = Pair.Key;
		TArray<float>& DistList = Pair.Value;
		if (!TrafficSimSubsystem->EntityTemplates.IsValidIndex(TemplateIdx)) continue;

		const FMassEntityTemplate* Template = TrafficSimSubsystem->EntityTemplates[TemplateIdx];
		if (!Template || !Template->IsValid()) continue;

		TArray<FMassEntityHandle> NewEntities;
		TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext =
			EntityManager.BatchCreateEntities(Template->GetArchetype(), Template->GetSharedFragmentValues(), DistList.Num(), NewEntities);
		EntityManager.BatchSetEntityFragmentsValues(CreationContext->GetEntityCollection(), Template->GetInitialFragmentValues());

		for (int32 i = 0; i < NewEntities.Num(); ++i)
		{
			const float DistAlong = DistList[i];
			FZoneGraphLaneLocation LaneLoc;
			UE::ZoneGraph::Query::CalculateLocationAlongLane(*TrafficSimSubsystem->ZoneGraphStorage, TargetLaneIndex, DistAlong, LaneLoc);

			const FMassEntityHandle Entity = NewEntities[i];
			FTransformFragment& TransformFrag = EntityManager.GetFragmentDataChecked<FTransformFragment>(Entity);
			FTransform& MutableTransform = TransformFrag.GetMutableTransform();
			MutableTransform.SetLocation(LaneLoc.Position);

			FMassVehicleMovementFragment& MoveFrag = EntityManager.GetFragmentDataChecked<FMassVehicleMovementFragment>(Entity);
			MoveFrag.LaneLocation = LaneLoc;
			MoveFrag.DistanceAlongLane = LaneLoc.DistanceAlongLane;
			TArray<int32> NextLanes;
			MoveFrag.NextLane = TrafficSimSubsystem->ChooseNextLane(MoveFrag.LaneLocation.LaneHandle.Index, NextLanes);
			MoveFrag.TargetSpeed = FMath::RandRange(MoveFrag.MinSpeed, MoveFrag.MaxSpeed);
			MoveFrag.Speed = FMath::RandRange(MoveFrag.MinSpeed, MoveFrag.TargetSpeed);
		}

		TotalSpawned += NewEntities.Num();
		UE_LOG(LogTrafficSim, Verbose, TEXT("LaneCongestionAdjust: Spawned %d (template %d) in [%.1f, %.1f]"),
			NewEntities.Num(), TemplateIdx, S, E);
	}

	UE_LOG(LogTrafficSim, Verbose, TEXT("LaneCongestionAdjust: Spawn finished in [%.1f, %.1f]. Spawned=%d Removed=%d StrictGap=%.1f InsertGap=%.1f"),
		S, E, TotalSpawned, ToDelete.Num(), StrictRemovalGap, InsertionGap);
}