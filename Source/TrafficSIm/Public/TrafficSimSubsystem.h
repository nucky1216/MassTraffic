// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoneGraphSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "MassVehicleMovementFragment.h"
#include "MassSpawnerTypes.h"
#include "MassEntityTemplate.h"
#include "CesiumGeoreference.h"
#include "Engine/DataTable.h"
#include "TrafficTypes.h"
// Forward declare congestion enum (defined in LaneCongestionAdjustProcessor.h)
enum class ELaneCongestionMetric : uint8;

#include "TrafficSimSubsystem.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogTrafficSim, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEntitySpawned, 
	const TArray<FName>&,VehIDs,
	const TArray<int32>&,VehTypes,
	const TArray<FVector>&,VehPositions);


namespace TrafficSim::MoveFrag::Debug {

	bool bEnbaleCustomData = false;
	//bool bRemoveSameDestination = true;
	//bool bFillEmptyDestination = true;

	FAutoConsoleVariableRef VarsGeneration[] = {
		FAutoConsoleVariableRef(TEXT("trafficsim.debug.customdata"), bEnbaleCustomData, TEXT("show movefrag by custom data writing.")),
		//FAutoConsoleVariableRef(TEXT("ai.debug.zonegraph.generation.RemoveSameDestination"), bRemoveSameDestination, TEXT("Remove merging lanes leading to same destination.")),
		//FAutoConsoleVariableRef(TEXT("ai.debug.zonegraph.generation.FillEmptyDestination"), bFillEmptyDestination, TEXT("Fill stray empty destination lanes.")),
	};

} // UE::ZoneGraph::Debug
USTRUCT()
struct FLaneVehicle
{
	GENERATED_BODY()

	FMassEntityHandle EntityHandle;
	FMassVehicleMovementFragment VehicleMovementFragment;

	FLaneVehicle() = default;

	FLaneVehicle(FMassEntityHandle InEntity, const FMassVehicleMovementFragment& InFrag)
		: EntityHandle(InEntity)
	{

		VehicleMovementFragment.Accelaration = InFrag.Accelaration;
		VehicleMovementFragment.CrossStopDistance = InFrag.CrossStopDistance;
		VehicleMovementFragment.Decelaration = InFrag.Decelaration;
		VehicleMovementFragment.DistanceAlongLane = InFrag.DistanceAlongLane;
		VehicleMovementFragment.LaneFilter = InFrag.LaneFilter;
		VehicleMovementFragment.LaneLocation = InFrag.LaneLocation;
		VehicleMovementFragment.LeftDistance = InFrag.LeftDistance;
		VehicleMovementFragment.MaxGap = InFrag.MaxGap;
		VehicleMovementFragment.MaxSpeed = InFrag.MaxSpeed;
		VehicleMovementFragment.MinGap = InFrag.MinGap;
		VehicleMovementFragment.MinSpeed = InFrag.MinSpeed;
		VehicleMovementFragment.NextLane = InFrag.NextLane;
		VehicleMovementFragment.QueryExtent = InFrag.QueryExtent;
		VehicleMovementFragment.Speed = InFrag.Speed;
		VehicleMovementFragment.TargetSpeed = InFrag.TargetSpeed;
		VehicleMovementFragment.VehicleHandle = InFrag.VehicleHandle;
		VehicleMovementFragment.VehicleLength = InFrag.VehicleLength;
	}
};
UCLASS()
class TRAFFICSIM_API UTrafficSimSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	int32 ChooseNextLane(int32 CurLaneIndex, TArray<int32>& NextLanes) const;
	void FindEntityLaneByQuery(FBox QueryBox, FZoneGraphTagFilter& TagFilter, FZoneGraphLaneLocation& OutLaneLocation);
	void InitOnPostLoadMap(const UWorld::FActorsInitializedParams& Params);
	void InitOnPostEditorWorld(UWorld* InWorld, UWorld::InitializationValues IVS);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "TrafficSim|Event")
	FOnEntitySpawned OnEntitySpawned;


	UFUNCTION(BlueprintCallable, Category = "TrafficSim| RoadToLane| Utilities")
	void RoadToLaneIndice(UPARAM(ref)UDataTable* RoadToLaneIndiceMap,FName RoadID,TArray<FVector> RoadPoints , FZoneGraphTagMask NotTag, float Height);


	UFUNCTION(BlueprintCallable, Category = "TrafficSim")
	void GetZonesSeg(TArray<FVector> Points, FZoneGraphTag AnyTag, float Height, FDTRoadLanes& ZoneSegLanes);

	UFUNCTION(BlueprintCallable, Category="TrafficSim")
	void FillVehsOnLane(TArray<int32> LaneIndice, TArray<float> StartDist, TArray<float> EndDist,float CruiseSpeed,float MinGap,float MaxGap,
		UPARAM(ref)TArray<FName>& VehIDs, UPARAM(ref)TArray<int32>& VehTypeIndice, TArray<FName>& UsedVehIDs, TArray<int32>& UsedVehTypes);

	UFUNCTION(BlueprintCallable, Category="TrafficSim")
	void DeleteMassEntities(int32 TargeLaneIndex);
	UFUNCTION(BlueprintCallable, Category="TrafficSim| Debug")
	void PrintLaneVehicles(int32 TargetLaneIndex);
	UFUNCTION(BlueprintCallable, Category="TrafficSim| Debug")
	void PrintLaneLinks(int32 TargetLaneIndex);
	UFUNCTION(BlueprintCallable, Category="TrafficSim| Debug")
	void InitializeManual();

	UFUNCTION(BlueprintCallable, Category="TrafficSim| Test")
	void DebugEntity(int32 TargetLane, int32 EntitySN);

	UFUNCTION(BlueprintCallable,Category="TrafficSim| Convert")
	void RoadToLanes(UDataTable* UpdatedRoadStatus, UDataTable* StaticRoadGeos, UPARAM(ref)UDataTable*& LanesMap, ACesiumGeoreference* GeoRef, FZoneGraphTag AnyTag, float QueryHeight);

	UFUNCTION(BlueprintCallable, Category = "TrafficSim| Congestion")
	void BathSetCongestionByDT(UPARAM(ref)UDataTable*& LanesMap, float TagetValue, TMap<int32,float> CongestionIndex);

	UFUNCTION(BlueprintCallable, Category = "TrafficSim| LaneMapping")
	int32 GetZoneLaneIndexByPoints(TArray<FVector> Points, FZoneGraphTag NotTag, float Height);


	UFUNCTION(BlueprintCallable, Category = "TrafficSim| Clear")
	void ClearAllEntities();

	UFUNCTION(BlueprintCallable, Category = "TrafficSim| SpawnPoint")
	void AddSpawnPointAtLane(int32 LaneIndex, float DistanceAlongLane, float CruiseSpeed,UMassEntityConfigAsset* EntityConfigAsset,TArray<FName> VehIDs,TArray<int32> VehTypes);

	UFUNCTION(BlueprintCallable, Category = "Traffic|Mass")
	FName GetVehIDFromActor(AActor* ClickedActor);


	UFUNCTION(BlueprintCallable, Category = "TrafficSim| ManualSim")
	void SetManualSimMode(bool bInManualSim);

	void LineTraceEntity(FVector Start, FVector End);

	bool SwitchToNextLane(FZoneGraphLaneLocation& LaneLocation, float NewDist);
	bool FindFrontVehicle(int32 LaneIndex, int32 NextLaneIndex, FMassEntityHandle CurVehicle, const FMassVehicleMovementFragment*& FrontVehicle);
	bool WaitForMergeVehilce(FMassVehicleMovementFragment* CurVehicle, const FMassVehicleMovementFragment*& AheadVehicle);
	void CollectAdjMergeLanes();

	void InitializeLaneToEntitiesMap();
	void CollectLaneVehicles(FMassEntityHandle EntityHandle, const FMassVehicleMovementFragment& VehicleFragment);
	void GetLaneVehicles(int32 LaneIndex, TConstArrayView<FLaneVehicle>& Vehilces);

	void InitializeTrafficTypes(TConstArrayView<FMassSpawnedEntityType> InTrafficTypes, FZoneGraphTag IntersectionTagIn);
	void GetVehicleConfigs(TArray<float>& VehicleLenth, TArray<float>& PrefixSum);
	void BroadcastEntitySpawnedEvent(const TArray<FName>& VehIDs,
		const TArray<int32>& VehTypes,
		const TArray<FVector>& VehPositions);

	UFUNCTION(BlueprintCallable, Category="TrafficSim|Control")
	void AdjustLaneCongestion(int32 LaneIndex, ELaneCongestionMetric MetricType, float TargetValue, UMassEntityConfigAsset* OptionalConfig, float StartDist,float EndDist,float MinSafetyGap = 200.f);




	void TagLaneSpeedGapSetup(const TMap<FZoneGraphTag, FTagLaneSpeed>& InTagLaneSpeed,
		const TMap<FZoneGraphTag, FTagLaneGap>& InTagLaneGap);
	float GetLaneSpeedByTag(FZoneGraphTagMask LaneTagMask, float& OutMaxSpeed, float& OutMinSpeed, FZoneGraphTag& ZoneLaneTag);

	void RegisterVehPlateID(FName VehID,FMassEntityHandle EntityHandle);
	void UnRegisterVehiPlateID(FName VehID);
	void GetEntityHandleByVehID(FName VehID, FMassEntityHandle& OutEntityHandle) const;
	
	UFUNCTION(BlueprintCallable, Category = "TrafficSim|GetFocus")
	FTransform GetEntityTransformByVehID(FName VehID,AActor*& VehActor,bool& bSuccess);


	UFUNCTION(BlueprintCallable, Category = "TrafficSim|ZoneGraphQuery")
	void GetZonesByViewBounds(TArray<FVector> BoundPoints, FZoneGraphTag AnyTag, float Height, TArray<int32>& OutZoneIndices);

	const UWorld* World = nullptr;
	const UZoneGraphSubsystem* ZoneGraphSubsystem = nullptr;
	const FZoneGraphStorage* ZoneGraphStorage = nullptr;
	FZoneGraphStorage* mutableZoneGraphSotrage = nullptr;
	AZoneGraphData* ZoneGraphData = nullptr;

	TMap<int32, TArray<int32>> AdjMergeLanes;
	TMap<int32, TArray<FLaneVehicle>> LaneToEntitiesMap;
	TConstArrayView<FMassSpawnedEntityType> VehicleConfigTypes;
	TArray<const FMassEntityTemplate*> EntityTemplates;
	FZoneGraphTag ConnectorTag;

	TMap<FZoneGraphTag, FTagLaneSpeed> TagLaneSpeed;
	TMap<FZoneGraphTag, FTagLaneGap> TagLaneGap;
private:
	bool bManualSim = false;
	TMap<FName, FMassEntityHandle> VehIDToEntityMap;


};
