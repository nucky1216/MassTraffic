#pragma once
#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"
#include "TrafficTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTrafficLight, Log, All);


USTRUCT()
struct TRAFFICSIM_API FSide
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<int32> Lanes; // Lanes on this side of the intersection

	UPROPERTY()
	TArray<int32> StrLanes; // Straight lanes on this side of the intersection

	UPROPERTY()
	TArray<int32> LeftLanes; // Left turn lanes on this side of the intersection

	UPROPERTY()
	TArray<int32> RightLanes; // Right turn lanes on this side of the intersection
	
	UPROPERTY()
	TArray<uint8> EntryIndex; // IDs of lanes on this side of the intersection

	TArray<FVector> SlotPoitions; // Positions of slots for vehicles on this side

	TMap<int32, int32> LaneToSlotIndex; // Maps lane index to slot index

	FORCEINLINE void AddSlot(FZoneGraphStorage* ZoneGraphStorage, int32 LaneIndex);
};

USTRUCT()
struct TRAFFICSIM_API FIntersectionData
{
	GENERATED_BODY()
	
	TArray<uint8> EntryIndex; // IDs of lanes at the intersection
	TArray<FSide> Sides;
	//TODO::SideTypes
	FORCEINLINE void SideAddLane(FZoneGraphStorage* ZoneGraphStorage, int32 LaneIndex);
	FORCEINLINE void SideSortLanes(FZoneGraphStorage* ZoneGraphStorage);
};

