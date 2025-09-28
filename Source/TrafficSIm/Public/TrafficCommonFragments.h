#pragma once
#include "MassEntityTypes.h"
#include "ZoneGraphTypes.h"
#include "TrafficCommonFragments.generated.h"


USTRUCT()
struct TRAFFICSIM_API FMassSpawnPointFragment : public FMassSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FZoneLaneLocation LaneLocation;
};