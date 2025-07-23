#pragma once
#include "MassEntityTypes.h"
#include "ZoneGraphTypes.h"
#include "MassVehicleMovementFragment.generated.h"


USTRUCT()
struct TRAFFICSIM_API FMassVehicleMovementFragment : public FMassFragment
{
    GENERATED_BODY()

    /** ��ǰ���ڵ� lane */
    UPROPERTY()
    FZoneGraphLaneLocation LaneLocation;

    /** ��һ�� lane������·���滮�� */
    UPROPERTY()
    int32 NextLane;

    /** �ڵ�ǰ lane �ϵ�λ�� [0,1] */
    UPROPERTY()
    float DistanceAlongLane = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Initialize")
    FVector QueryExtent;

	UPROPERTY(EditAnywhere, Category = "Initialize")
	FZoneGraphTagFilter LaneFilter;
};