// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoneGraphSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "MassVehicleMovementFragment.h"
#include "TrafficSimSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTrafficSim, Log, All);

/**
 * 
 */
USTRUCT()
struct FLaneVehicle
{
	GENERATED_BODY()

	FMassEntityHandle EntityHandle;
	const FMassVehicleMovementFragment *VehicleMovementFragment;

};

UCLASS()
class TRAFFICSIM_API UTrafficSimSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

private:
	


public:
	int32 ChooseNextLane(FZoneGraphLaneLocation CurLane, TArray<int32>& NextLanes) const;

	void FindEntityLaneByQuery(FBox QueryBox,FZoneGraphTagFilter& TagFilter,FZoneGraphLaneLocation& OutLaneLocation);
	void InitOnPostLoadMap(UWorld* LoadedWorld,const UWorld::InitializationValues IVS);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "TrafficSim")
	void SpawnMassEntities(int32 NumEntities,int32 TargetLane, UMassEntityConfigAsset* EntityConfigAsset);

	UFUNCTION(BlueprintCallable, Category = "TrafficSim")
	void DeleteMassEntities(int32 TargeLaneIndex);

	UFUNCTION(BlueprintCallable, Category = "TrafficSim")
	void PrintLaneVehicles(int32 TargetLaneIndex);


	bool SwitchToNextLane(FZoneGraphLaneLocation& LaneLocation, float NewDist);


	//UFUNCTION(BlueprintCallable, Category = "TrafficSim| Avoidance")
	bool UTrafficSimSubsystem::FindFrontVehicle(int32 LaneIndex, FMassEntityHandle CurVehicle, const FMassVehicleMovementFragment* FrontVehicle);


	//laneVehicles
	void InitializeLaneToEntitiesMap();
	void CollectLaneVehicles(FMassEntityHandle EntityHandle,const FMassVehicleMovementFragment& VehicleFragment );

	const UWorld* World = nullptr;
	const UZoneGraphSubsystem* ZoneGraphSubsystem = nullptr;
	const FZoneGraphStorage* ZoneGraphStorage = nullptr;

	TMap<int32, TArray<FLaneVehicle>> LaneToEntitiesMap;
};
