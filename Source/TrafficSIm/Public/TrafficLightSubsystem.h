#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TrafficSimSubsystem.h"
#include "ZoneGraphTypes.h"
#include "TrafficTypes.h"
#include "TagFilter.h"
#include "ZoneGraphData.h"
#include "TrafficLightSubsystem.generated.h"



UCLASS()
class TRAFFICSIM_API UTrafficLightSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void GetZoneGraphaData();
	TMap<int32,FIntersectionData> IntersectionDatas;


	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim",meta=(ToolTip="Build Intersecion Data"))
	void BuildIntersectionsData(UTagFilter* DAFilter);

	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim", meta = (ToolTip = "Build Intersecion Data"))
	void LookUpIntersection(int32 ZoneIndex);

private:
	const FZoneGraphStorage* ZoneGraphStorage = nullptr;
	const UTrafficSimSubsystem* TrafficSimSubsystem = nullptr;


};
