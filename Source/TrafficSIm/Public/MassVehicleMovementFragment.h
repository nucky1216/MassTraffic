#pragma once
#include "MassEntityTypes.h"
#include "ZoneGraphTypes.h"
#include "MassVehicleMovementFragment.generated.h"


USTRUCT()
struct TRAFFICSIM_API FMassVehicleMovementFragment : public FMassFragment
{
    GENERATED_BODY()

    /** 当前所在的 lane */
    UPROPERTY()
    FZoneGraphLaneLocation LaneLocation;

    /** 下一条 lane（用于路径规划） */
    UPROPERTY()
    int32 NextLane;

    /** 在当前 lane 上的位置 [0,1] */
    UPROPERTY()
    float DistanceAlongLane = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Initialize")
    FVector QueryExtent;

	UPROPERTY(EditAnywhere, Category = "Initialize")
	FZoneGraphTagFilter LaneFilter;
};