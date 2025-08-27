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

	UPROPERTY(EditAnywhere, Category = "Initialize")
    float MaxGap = 400.f;

	UPROPERTY(EditAnywhere, Category = "Initialize")
    float MinGap = 100.f;

    UPROPERTY(EditAnywhere, Category = "Initialize")
	float MaxSpeed = 400.f;

    UPROPERTY(EditAnywhere, Category = "Initialize")
    float MinSpeed = 50.f;

    UPROPERTY(EditAnywhere, Category = "Initialize")
    float Speed = 50.f;


    float TargetSpeed = 100.f;

    UPROPERTY(EditAnywhere, Category = "Initialize")
    float Accelaration = 100.f;
    UPROPERTY(EditAnywhere, Category = "Initialize")
    float Decelaration = 100.f;

    UPROPERTY(EditAnywhere, Category = "Initialize")
    float VehicleLength = 400.f;


    UPROPERTY(EditAnywhere, Category = "Initialize")
	float CrossStopDistance = 800.f;


	FMassEntityHandle VehicleHandle;
	float LeftDistance = 0.f;
};