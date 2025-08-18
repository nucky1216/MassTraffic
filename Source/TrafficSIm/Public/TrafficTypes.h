#pragma once
#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"
#include "TrafficTypes.generated.h"

USTRUCT()
struct TRAFFICSIM_API FIntersectionData
{
	GENERATED_BODY()
	
	TArray<uint8> EntryIndex; // IDs of lanes at the intersection
	TArray<TArray<FZoneGraphLaneHandle>> OutgoingLanes;
};