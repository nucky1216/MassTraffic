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
	TMap<ETrafficSignalType,float> LightDurations ; 

	int32 CurrentSide = 0;

	int32 ZoneIndex;
};