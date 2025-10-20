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
#include "CesiumGeoreference.h"
#include "Engine/DataTable.h"
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

	UFUNCTION(BlueprintCallable, Category = "TrafficSim")
	void GetZonesSeg(TArray<FVector> Points, FZoneGraphTag AnyTag, float Height, FDTRoadLanes& ZoneSegLanes);

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

	UFUNCTION(BlueprintCallable,Category="TrafficSim| Convert")
	void RoadToLanes(UDataTable* RoadGeos, UPARAM(ref)UDataTable*& LanesMap, ACesiumGeoreference* GeoRef, FZoneGraphTag AnyTag,float QueryHeight);

	UFUNCTION(BlueprintCallable, Category = "TrafficSim| Congestion")
	void BathSetCongestionByDT(UPARAM(ref)UDataTable*& LanesMap, float TagetValue, TMap<int32,float> CongestionIndex);

	bool SwitchToNextLane(FZoneGraphLaneLocation& LaneLocation, float NewDist);
	bool FindFrontVehicle(int32 LaneIndex, int32 NextLaneIndex, FMassEntityHandle CurVehicle, const FMassVehicleMovementFragment*& FrontVehicle);
	bool WaitForMergeVehilce(FMassVehicleMovementFragment* CurVehicle, const FMassVehicleMovementFragment*& AheadVehicle);

	void InitializeLaneToEntitiesMap();
	void CollectLaneVehicles(FMassEntityHandle EntityHandle, const FMassVehicleMovementFragment& VehicleFragment);
	void GetLaneVehicles(int32 LaneIndex, TConstArrayView<FLaneVehicle>& Vehilces);

	void InitializeTrafficTypes(TConstArrayView<FMassSpawnedEntityType> InTrafficTypes, FZoneGraphTag IntersectionTagIn);
	void GetVehicleConfigs(TArray<float>& VehicleLenth, TArray<float>& PrefixSum);

	UFUNCTION(BlueprintCallable, Category="TrafficSim|Control")
	void AdjustLaneCongestion(int32 LaneIndex, ELaneCongestionMetric MetricType, float TargetValue, UMassEntityConfigAsset* OptionalConfig, float StartDist,float EndDist,float MinSafetyGap = 200.f);

	const UWorld* World = nullptr;
	const UZoneGraphSubsystem* ZoneGraphSubsystem = nullptr;
	const FZoneGraphStorage* ZoneGraphStorage = nullptr;
	FZoneGraphStorage* mutableZoneGraphSotrage = nullptr;
	AZoneGraphData* ZoneGraphData = nullptr;

	TMap<int32, TArray<FLaneVehicle>> LaneToEntitiesMap;
	TConstArrayView<FMassSpawnedEntityType> VehicleConfigTypes;
	TArray<const FMassEntityTemplate*> EntityTemplates;
	FZoneGraphTag ConnectorTag;
};
