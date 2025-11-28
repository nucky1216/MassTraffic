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

	UPROPERTY(EditAnywhere)
	float RandOffset = 2.f;

	float Clock = 5.f;
	UPROPERTY()
	int32 NextVehicleType = -1;


	TArray<FName> VehicleIDs;
	int32 SpawnVehicleIDIndex = -1;
	TArray<int32> VehicleTypes;
	bool Controlled = false;
};