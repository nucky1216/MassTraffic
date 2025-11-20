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


	virtual void  Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void  Deinitialize() override;


	void HandleRunWorldInitialized(const UWorld::FActorsInitializedParams& Params);
	void HandleEditorWorldInitialized(UWorld* InWorld,const UWorld::InitializationValues IVS);
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


	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim", meta = (ToolTip = "Build Intersecion Data"))
	void QueryLaneOpenState(int32 LaneIndex,bool& OpenState,bool & IntersectionLane);

	UFUNCTION(BlueprintCallable,Category="TrafficLightSim| CrossPhase")
	void InitialCrossPhaseRow(UDataTable* DataTable, const FName& RowName, FCrossPhaseLaneData RowData);

	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim| CrossPhase")
	void DebugCrossPhase(FName CrossPhaseName);

	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim| CrossPhase")
	void GetNextLanesFromPhaseLane(int32 CurLaneIndex, TArray<int32>& NextLanes);

	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim| CrossPhase")
	void InitializeCrossPhaseLaneInfor(UDataTable* DataTable);

	void SetCrossBySignalState(int32 ZoneIndex,ETrafficSignalType SignalType,int32 SideIndex);




	FZoneGraphTagFilter IntersectionTagFilter;
	TMap<FName, FPhaseLanes> CrossPhaseLaneInfor;


private:
	const FZoneGraphStorage* ZoneGraphStorage = nullptr;
	const UTrafficSimSubsystem* TrafficSimSubsystem = nullptr;
	


};
