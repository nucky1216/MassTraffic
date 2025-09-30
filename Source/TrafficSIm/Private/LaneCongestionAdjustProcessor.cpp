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
	bAutoRegisterWithProcessingPhases = false; // ÊÖ¶¯´¥·¢
}

void ULaneCongestionAdjustProcessor::Initialize(UObject& Owner)
{
	TrafficSimSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTrafficSimSubsystem>() : nullptr;
	UE_LOG(LogTrafficSim,Log,TEXT("LaneCongestionAdjustProcessor Initialized."));
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
		if(!TrafficSimSubsystem)
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

	TConstArrayView<FLaneVehicle> Vehicles;
	TrafficSimSubsystem->GetLaneVehicles(TargetLaneIndex, Vehicles);

	float LaneLength = 0.f;
	UE::ZoneGraph::Query::GetLaneLength(*TrafficSimSubsystem->ZoneGraphStorage, TargetLaneIndex, LaneLength);
	if (LaneLength <= 0.f)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("LaneCongestionAdjust: Lane length invalid"));
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
	SortedVehicleLenth.Sort([](const float& A, const float& B) {return A > B; });
	const float MinVehicleLen = SortedVehicleLenth.Num() ? SortedVehicleLenth.Last() : 0.f;

	// Strict removal gap (keep front car, remove followers too close)
	const float StrictRemovalGap = FMath::Max(TargetAverageGap, MinSafetyGap);
	// Relaxed insertion gap to improve packing
	const float GapRelaxPercent = 0.2f; // 20%
	float InsertionGap = TargetAverageGap > 0.f ? TargetAverageGap * (1.f - GapRelaxPercent) : MinSafetyGap;
	InsertionGap = FMath::Clamp(InsertionGap, MinSafetyGap, StrictRemovalGap);

	// 1) Remove too-close vehicles (tail-head gap < StrictRemovalGap)
	TArray<FMassEntityHandle> ToDelete;
	TArray<const FLaneVehicle*> KeptVehicles;
	for (int32 i = 0; i < Vehicles.Num(); ++i)
	{
		const FLaneVehicle& Cur = Vehicles[i];
		if (KeptVehicles.Num() == 0)
		{
			KeptVehicles.Add(&Cur);
			continue;
		}
		const FLaneVehicle* PrevKept = KeptVehicles.Last();
		float PrevCenter = PrevKept->VehicleMovementFragment->LaneLocation.DistanceAlongLane;
		float PrevLen = PrevKept->VehicleMovementFragment->VehicleLength;
		float PrevEnd = PrevCenter + PrevLen * 0.5f;
		float CurCenter = Cur.VehicleMovementFragment->LaneLocation.DistanceAlongLane;
		float CurLen = Cur.VehicleMovementFragment->VehicleLength;
		float CurStart = CurCenter - CurLen * 0.5f;
		float Headway = CurStart - PrevEnd;
		if (Headway < StrictRemovalGap)
		{
			ToDelete.Add(Cur.EntityHandle); // keep previous, drop current
		}
		else
		{
			KeptVehicles.Add(&Cur);
		}
	}
	if (ToDelete.Num() > 0)
	{
		for (const FMassEntityHandle& E : ToDelete)
			EntityManager.Defer().DestroyEntity(E);
		UE_LOG(LogTrafficSim, Log, TEXT("LaneCongestionAdjust: Removed %d vehicles (strict gap %.1f)"), ToDelete.Num(), StrictRemovalGap);
	}

	// 2) Build relaxed gaps using InsertionGap
	struct FGap { float Start=0.f; float End=0.f; };
	TArray<FGap> Gaps;
	if (KeptVehicles.Num() == 0)
	{
		Gaps.Add({0.f, LaneLength});
	}
	else
	{
		float FirstCenter = KeptVehicles[0]->VehicleMovementFragment->LaneLocation.DistanceAlongLane;
		float FirstLen = KeptVehicles[0]->VehicleMovementFragment->VehicleLength;
		float FirstStart = FirstCenter - FirstLen * 0.5f;
		float HeadGapEnd = FirstStart - InsertionGap;
		if (HeadGapEnd > 0.f) Gaps.Add({0.f, HeadGapEnd});
		for (int32 i = 0; i < KeptVehicles.Num() - 1; ++i)
		{
			const FLaneVehicle* A = KeptVehicles[i];
			const FLaneVehicle* B = KeptVehicles[i+1];
			float ACenter = A->VehicleMovementFragment->LaneLocation.DistanceAlongLane;
			float ALen = A->VehicleMovementFragment->VehicleLength;
			float AEnd = ACenter + ALen * 0.5f;
			float BCenter = B->VehicleMovementFragment->LaneLocation.DistanceAlongLane;
			float BLen = B->VehicleMovementFragment->VehicleLength;
			float BStart = BCenter - BLen * 0.5f;
			float GapStart = AEnd + InsertionGap;
			float GapEnd = BStart - InsertionGap;
			if (GapEnd - GapStart >= MinVehicleLen + KINDA_SMALL_NUMBER)
				Gaps.Add({GapStart, GapEnd});
		}
		float LastCenter = KeptVehicles.Last()->VehicleMovementFragment->LaneLocation.DistanceAlongLane;
		float LastLen = KeptVehicles.Last()->VehicleMovementFragment->VehicleLength;
		float LastEnd = LastCenter + LastLen * 0.5f;
		float TailGapStart = LastEnd + InsertionGap;
		if (LaneLength - TailGapStart >= MinVehicleLen + KINDA_SMALL_NUMBER)
			Gaps.Add({TailGapStart, LaneLength});
	}
	if (Gaps.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Log, TEXT("LaneCongestionAdjust: No usable gaps after cleanup"));
		return;
	}

	auto ChooseRandomVehicleIndex = [&PrefixSum]() -> int32
	{
		float R = FMath::FRand();
		for (int32 i = 0; i < PrefixSum.Num(); ++i)
			if (R <= PrefixSum[i]) return i;
		return PrefixSum.Num() - 1;
	};

	// 3) Fill each gap iteratively
	TMap<int32, TArray<float>> TemplateToDistances;
	const int32 MaxSpawnPerGapLoop = 512;

	for (const FGap& Gap : Gaps)
	{
		float Cursor = Gap.Start;
		int32 Iter = 0;
		while (Iter++ < MaxSpawnPerGapLoop)
		{
			float Remaining = Gap.End - Cursor;
			if (Remaining < MinVehicleLen + KINDA_SMALL_NUMBER)
				break;
			float ChosenLen = 0.f; int32 VehicleIdx = INDEX_NONE;
			const int32 RandomAttempts = 6;
			for (int32 Attempt = 0; Attempt < RandomAttempts; ++Attempt)
			{
				int32 CandIdx = ChooseRandomVehicleIndex();
				float Len = VehicleLenth.IsValidIndex(CandIdx) ? VehicleLenth[CandIdx] : MinVehicleLen;
				if (Len <= Remaining) { ChosenLen = Len; VehicleIdx = CandIdx; break; }
			}
			if (VehicleIdx == INDEX_NONE)
			{
				for (float Len : SortedVehicleLenth) // descending
				{
					if (Len <= Remaining)
					{ ChosenLen = Len; VehicleIdx = VehicleLenth.IndexOfByPredicate([Len](float V){ return FMath::IsNearlyEqual(V, Len); }); break; }
				}
			}
			if (VehicleIdx == INDEX_NONE || ChosenLen <= 0.f)
				break;
			float Start = Cursor;
			float End = Start + ChosenLen;
			if (End > Gap.End + KINDA_SMALL_NUMBER)
				break;
			float CenterDist = Start + ChosenLen * 0.5f;
			TemplateToDistances.FindOrAdd(VehicleIdx).Add(CenterDist);
			Cursor = End + InsertionGap; // advance
		}
	}

	if (TemplateToDistances.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Log, TEXT("LaneCongestionAdjust: No vehicles spawned (all gaps too small after relax)"));
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
		int32 TemplateIdx = Pair.Key;
		TArray<float>& DistList = Pair.Value;
		if (!TrafficSimSubsystem->EntityTemplates.IsValidIndex(TemplateIdx)) continue;
		const FMassEntityTemplate* Template = TrafficSimSubsystem->EntityTemplates[TemplateIdx];
		if (!Template || !Template->IsValid()) continue;
		TArray<FMassEntityHandle> NewEntities;
		TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager.BatchCreateEntities(Template->GetArchetype(), Template->GetSharedFragmentValues(), DistList.Num(), NewEntities);
		EntityManager.BatchSetEntityFragmentsValues(CreationContext->GetEntityCollection(), Template->GetInitialFragmentValues());
		for (int32 i = 0; i < NewEntities.Num(); ++i)
		{
			float DistAlong = DistList[i];
			FZoneGraphLaneLocation LaneLoc;
			UE::ZoneGraph::Query::CalculateLocationAlongLane(*TrafficSimSubsystem->ZoneGraphStorage, TargetLaneIndex, DistAlong, LaneLoc);
			const FMassEntityHandle Entity = NewEntities[i];
			FTransformFragment& TransformFrag = EntityManager.GetFragmentDataChecked<FTransformFragment>(Entity);
			FTransform& MutableTransform = TransformFrag.GetMutableTransform();
			MutableTransform.SetLocation(LaneLoc.Position);
			FMassVehicleMovementFragment& MoveFrag = EntityManager.GetFragmentDataChecked<FMassVehicleMovementFragment>(Entity);
			MoveFrag.LaneLocation = LaneLoc;
			MoveFrag.DistanceAlongLane = LaneLoc.DistanceAlongLane;
		}
		TotalSpawned += NewEntities.Num();
		UE_LOG(LogTrafficSim, Verbose, TEXT("LaneCongestionAdjust: Spawned %d (template %d)"), NewEntities.Num(), TemplateIdx);
	}
	UE_LOG(LogTrafficSim, Log, TEXT("LaneCongestionAdjust: Spawn finished. Spawned=%d Removed=%d StrictGap=%.1f InsertGap=%.1f"), TotalSpawned, ToDelete.Num(), StrictRemovalGap, InsertionGap);
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
	UE_LOG(LogTrafficSim, Log, TEXT("LaneCongestionAdjust: Removed %d vehicles"), ToRemove);
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
