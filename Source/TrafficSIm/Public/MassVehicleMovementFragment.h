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
    float DistanceAlongLane;

    UPROPERTY(EditAnywhere, Category = "Initialize")
    FVector QueryExtent;

	UPROPERTY(EditAnywhere, Category = "Initialize")
	FZoneGraphTagFilter LaneFilter;

	UPROPERTY(EditAnywhere, Category = "Initialize")
    float MaxGap;

	UPROPERTY(EditAnywhere, Category = "Initialize")
    float MinGap;

    UPROPERTY(EditAnywhere, Category = "Initialize")
	float MaxSpeed;

    UPROPERTY(EditAnywhere, Category = "Initialize")
    float MinSpeed;

    UPROPERTY(EditAnywhere, Category = "Initialize")
    float Speed;


    float TargetSpeed;


    UPROPERTY(EditAnywhere, Category = "Initialize")
    float Accelaration;
    UPROPERTY(EditAnywhere, Category = "Initialize")
    float Decelaration;

    UPROPERTY(EditAnywhere, Category = "Initialize")
    float VehicleLength;

    UPROPERTY(EditAnywhere, Category = "Initialize")
	float CrossStopDistance;


	FMassEntityHandle VehicleHandle;
	float LeftDistance;


    // 构造函数初始化所有成员
    FMassVehicleMovementFragment()
        : NextLane(-1)
        , DistanceAlongLane(0.0f)
        , QueryExtent(FVector::ZeroVector)
        , LaneFilter()
        , MaxGap(400.f)
        , MinGap(100.f)
        , MaxSpeed(400.f)
        , MinSpeed(50.f)
        , Speed(50.f)
        , TargetSpeed(100.f)
        , Accelaration(100.f)
        , Decelaration(100.f)
        , VehicleLength(400.f)
        , CrossStopDistance(800.f)
        , VehicleHandle()
        , LeftDistance(0.f)
    {}
};