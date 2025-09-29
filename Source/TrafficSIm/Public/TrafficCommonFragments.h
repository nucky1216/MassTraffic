#pragma once
#include "MassEntityTypes.h"
#include "ZoneGraphTypes.h"
#include "MassSpawnerTypes.h"
#include "TrafficCommonFragments.generated.h"


USTRUCT()
struct TRAFFICSIM_API FMassSpawnPointFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FZoneGraphLaneLocation LaneLocation;

	UPROPERTY(EditAnywhere)
	float Duration = 5.f;

	UPROPERTY()
	int32 NextVehicleType = -1;
};