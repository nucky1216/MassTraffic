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
	void DebugCrossPhase(TArray<int32> Lanes);

	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim| CrossPhase")
	void GetNextLanesFromPhaseLane(int32 CurLaneIndex, TArray<int32>& NextLanes);

	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim| CrossPhase")
	void SetPhaseLanesOpened(int32 ZoneIndex, TArray<int32> PhaseLanes);

	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim| CrossPhase")
	void InitializeCrossPhaseLaneInfor(UDataTable* DataTable);

	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim| CrossPhase")
	void SetCrossPhaseQueue(FString JsonStr);

	UFUNCTION(BlueprintCallable, Category = "TrafficLightSim| CrossPhase")
	void GetCrossPhaseState(AActor* CrossActor, FName& Phase, float& LeftTime);

	void SetCrossBySignalState(int32 ZoneIndex,ETrafficSignalType SignalType,int32 SideIndex);

	void GetPhaseLanesByZoneIndex(int32 ZoneIndex, TMap<FName, TArray<int32>>& PhaseLanes, FName& CrossID);

	void RegisterCrossEntity(FName CrossName,FMassEntityHandle EntityHandle);


	FZoneGraphTagFilter IntersectionTagFilter;
	TMap<FName, FPhaseLanes> CrossPhaseLaneInfor;
	TMap<int32, FIntersectionData> IntersectionDatas;
	TMap<FName, FMassEntityHandle> CrossEntityHandleMap;


private:
	const FZoneGraphStorage* ZoneGraphStorage = nullptr;
	const UTrafficSimSubsystem* TrafficSimSubsystem = nullptr;
	


};
