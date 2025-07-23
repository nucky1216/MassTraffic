// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoneGraphSubsystem.h"
#include "TrafficSimSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTrafficSim, Log, All);

/**
 * 
 */
UCLASS()
class TRAFFICSIM_API UTrafficSimSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	int32 ChooseNextLane(FZoneGraphLaneLocation CurLane) const;

	void FindEntityLaneByQuery(FBox QueryBox,FZoneGraphTagFilter& TagFilter,FZoneGraphLaneLocation& OutLaneLocation);
	void InitOnPostLoadMap(UWorld* LoadedWorld,const UWorld::InitializationValues IVS);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const UWorld* World = nullptr;
	const UZoneGraphSubsystem* ZoneGraphSubsystem = nullptr;
	const FZoneGraphStorage* ZoneGraphStorage = nullptr;
};
