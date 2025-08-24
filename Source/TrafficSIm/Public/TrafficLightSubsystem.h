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
	void BuildIntersectionsData(UTagFilter* DAFilter) { BuildIntersectionsData(DAFilter->TagFilter); };
	
	void BuildIntersectionsData(FZoneGraphTagFilter IntersectionTagFilter);

	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim", meta = (ToolTip = "Build Intersecion Data"))
	void LookUpIntersection(int32 ZoneIndex);

	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim", meta = (ToolTip = "Build Intersecion Data"))
	void DebugDrawState(int32 ZoneIndex, float DebugTime=5.0f);

	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim", meta = (ToolTip = "Build Intersecion Data"))
	void SetOpenLanes(int32 ZoneIndex,int32 SideIndex, ETurnType TurnType,bool Reset=false);

private:
	const FZoneGraphStorage* ZoneGraphStorage = nullptr;
	const UTrafficSimSubsystem* TrafficSimSubsystem = nullptr;


};
