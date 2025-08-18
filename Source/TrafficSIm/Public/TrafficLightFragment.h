#pragma once
#include "MassEntityTypes.h"	
#include "TrafficLightFragment.generated.h"


UENUM()
enum class ETrafficLightState : uint8
{
	Red =0,    // ºìµÆ
	Green=1,  // ÂÌµÆ
	Yellow=2  // »ÆµÆ
};
/**
 * Fragment to represent traffic light state for vehicles.
 */
USTRUCT()
struct TRAFFICSIM_API FTrafficLightFragment : public FMassFragment
{
	GENERATED_BODY()
	/** µ±Ç°½»Í¨µÆ×´Ì¬ */
	UPROPERTY(EditAnywhere, Category = "Traffic Light")
	ETrafficLightState LightState = ETrafficLightState::Red; // 0: Red, 1: Green, 2: Yellow

	UPROPERTY(EditAnywhere, Category="Traffic Light",meta=(ToolTip="0ÂÌµÆÊ±¼ä£¬1»ÆµÆÊ±¼ä"))
	TArray<float> LightDurations = { 30.f, 5.f }; // Ä¬ÈÏ30ÃëÂÌµÆ£¬5Ãë»ÆµÆ

	uint8 CurrentSide = 0;

	TArray<TArray<int32>> SideToLanes;
};