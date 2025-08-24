#pragma once
#include "MassEntityTypes.h"	
#include "TrafficTypes.h"
#include "TrafficLightFragment.generated.h"


UENUM()
enum class ETrafficLightState : uint8
{
	Red =0,    // ���
	Green=1,  // �̵�
	Yellow=2  // �Ƶ�
};
/**
 * Fragment to represent traffic light state for vehicles.
 */
USTRUCT()
struct TRAFFICSIM_API FTrafficLightFragment : public FMassFragment
{
	GENERATED_BODY()
	/** ��ǰ��ͨ��״̬ */
	UPROPERTY(EditAnywhere, Category = "Traffic Light")
	ETrafficSignalType LightState = ETrafficSignalType::StraightAndRight; // 0: Red, 1: Green, 2: Yellow

	UPROPERTY(EditAnywhere, Category="Traffic Light",meta=(ToolTip="0�̵�ʱ�䣬1�Ƶ�ʱ��"))
	TMap<ETrafficSignalType, float> LightDurations = {
		{ETrafficSignalType::StraightAndRight,10.f},
		{ETrafficSignalType::Left,5.f}
	};

	int32 CurrentSide = 0;
	UPROPERTY(EditAnywhere, Category = "Traffic Light")
	ETrafficSignalType CurrentLightState = ETrafficSignalType::StraightAndRight;

	float TimeInDuration = 0.f;
	int32 ZoneIndex;
};