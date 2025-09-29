// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoneGraphSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "MassVehicleMovementFragment.h"
#include "MassSpawnerTypes.h"
#include "MassEntityTemplate.h"

// Forward declare congestion enum (defined in LaneCongestionAdjustProcessor.h)
enum class ELaneCongestionMetric : uint8;

#include "TrafficSimSubsystem.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogTrafficSim, Log, All);

USTRUCT()
struct FLaneVehicle
{
	GENERATED_BODY()

	FMassEntityHandle EntityHandle;
	const FMassVehicleMovementFragment* VehicleMovementFragment;

	FLaneVehicle() : EntityHandle(), VehicleMovementFragment(nullptr) {}
	FLaneVehicle(FMassEntityHandle InEntity, const FMassVehicleMovementFragment* InFrag)
		: EntityHandle(InEntity), VehicleMovementFragment(InFrag) {}
};

UCLASS()
class TRAFFICSIM_API UTrafficSimSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	int32 ChooseNextLane(int32 CurLaneIndex, TArray<int32>& NextLanes) const;
	void FindEntityLaneByQuery(FBox QueryBox, FZoneGraphTagFilter& TagFilter, FZoneGraphLaneLocation& OutLaneLocation);
	void InitOnPostLoadMap(const UWorld::FActorsInitializedParams& Params);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="TrafficSim")
	void SpawnMassEntities(int32 NumEntities, int32 TargetLane, UMassEntityConfigAsset* EntityConfigAsset);
	UFUNCTION(BlueprintCallable, Category="TrafficSim")
	void DeleteMassEntities(int32 TargeLaneIndex);
	UFUNCTION(BlueprintCallable, Category="TrafficSim| Debug")
	void PrintLaneVehicles(int32 TargetLaneIndex);
	UFUNCTION(BlueprintCallable, Category="TrafficSim| Debug")
	void PrintLaneLinks(int32 TargetLaneIndex);
	UFUNCTION(BlueprintCallable, Category="TrafficSim| Debug")
	void InitializeManual();
	UFUNCTION(BlueprintCallable, Category="TrafficSim| Test")
	void DebugEntity(int32 TargetLane, int32 EntitySN);

	bool SwitchToNextLane(FZoneGraphLaneLocation& LaneLocation, float NewDist);
	bool FindFrontVehicle(int32 LaneIndex, int32 NextLaneIndex, FMassEntityHandle CurVehicle, const FMassVehicleMovementFragment*& FrontVehicle);

	void InitializeLaneToEntitiesMap();
	void CollectLaneVehicles(FMassEntityHandle EntityHandle, const FMassVehicleMovementFragment& VehicleFragment);
	void GetLaneVehicles(int32 LaneIndex, TConstArrayView<FLaneVehicle>& Vehilces);

	void InitializeTrafficTypes(TConstArrayView<FMassSpawnedEntityType> InTrafficTypes);
	void GetVehicleConfigs(TArray<float>& VehicleLenth, TArray<float>& PrefixSum);

	UFUNCTION(BlueprintCallable, Category="TrafficSim|Control")
	void AdjustLaneCongestion(int32 LaneIndex, ELaneCongestionMetric MetricType, float TargetValue, UMassEntityConfigAsset* OptionalConfig, float MinSafetyGap = 200.f);

	const UWorld* World = nullptr;
	const UZoneGraphSubsystem* ZoneGraphSubsystem = nullptr;
	const FZoneGraphStorage* ZoneGraphStorage = nullptr;
	FZoneGraphStorage* mutableZoneGraphSotrage = nullptr;
	AZoneGraphData* ZoneGraphData = nullptr;

	TMap<int32, TArray<FLaneVehicle>> LaneToEntitiesMap;
	TConstArrayView<FMassSpawnedEntityType> VehicleConfigTypes;
	TArray<const FMassEntityTemplate*> EntityTemplates;
};
