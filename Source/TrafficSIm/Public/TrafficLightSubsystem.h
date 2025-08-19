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
	TArray<FIntersectionData> InteectionsDatas;


	UFUNCTION(BlueprintCallable, Category = "TrafficSim",meta=(ToolTip="构建路口数据"))
	void BuildIntersectionsData(UTagFilter* DAFilter);

private:
	const FZoneGraphStorage* ZoneGraphStorage = nullptr;
	const UTrafficSimSubsystem* TrafficSimSubsystem = nullptr;


};
