#pragma once
#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"
#include "TrafficTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTrafficLight, Log, All);

USTRUCT()
struct TRAFFICSIM_API FIntersectionData
{
	GENERATED_BODY()
	
	TArray<uint8> EntryIndex; // IDs of lanes at the intersection
	TArray<TArray<FZoneGraphLaneHandle>> OutgoingLanes;
};