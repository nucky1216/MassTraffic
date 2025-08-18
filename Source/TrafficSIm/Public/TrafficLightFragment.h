#pragma once
#include "MassEntityTypes.h"	
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
	ETrafficLightState LightState = ETrafficLightState::Red; // 0: Red, 1: Green, 2: Yellow

	UPROPERTY(EditAnywhere, Category="Traffic Light",meta=(ToolTip="0�̵�ʱ�䣬1�Ƶ�ʱ��"))
	TArray<float> LightDurations = { 30.f, 5.f }; // Ĭ��30���̵ƣ�5��Ƶ�

	uint8 CurrentSide = 0;

	TArray<TArray<int32>> SideToLanes;
};