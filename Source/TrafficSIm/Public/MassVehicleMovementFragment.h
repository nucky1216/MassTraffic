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


    // ���캯����ʼ�����г�Ա
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