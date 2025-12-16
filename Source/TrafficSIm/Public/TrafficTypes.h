#pragma once
#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"
#include "MassEntityTypes.h"
#include "Engine/DataTable.h"
#include "TrafficTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTrafficLight, Log, All);


UENUM(BlueprintType)
enum class ETurnType:uint8
{
	Straight=0,
	RightTurn = 1,
	LeftTurn = 2,

};
UENUM(BlueprintType)
enum class ETrafficSignalType:uint8
{
	Straight= 0,
	StraightAndRight = 1,
	StraightYellow = 2,
	Left = 3,
	LeftYellow=4
};

USTRUCT()
struct TRAFFICSIM_API FSide
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<int32> Lanes; // Lanes on this side of the intersection
	
	UPROPERTY()
	TArray<uint8> EntryIndex; // IDs of lanes on this side of the intersection

	TArray<FVector> SlotPoitions; // Positions of slots for vehicles on this side

	TMap<int32, int32> LaneToSlotIndex; // Maps lane index to slot index

	TMultiMap<ETurnType, int32> TurnTypeToLanes; // Maps lane index to turn type

	bool bIsAloneSide = false; // If true, there is no opposite lane for this side
	FVector SideDirection; // Direction of the side

	int32 OppositeSideIndex = -1; // Index of the opposite side in the intersection

	UPROPERTY(EditAnywhere,Category="Period")
	TMap<ETrafficSignalType, float> Periods; // Periods for different traffic light phases

	FORCEINLINE void AddSlot(const FZoneGraphStorage* ZoneGraphStorage, int32 LaneIndex);


};

USTRUCT()
struct TRAFFICSIM_API FIntersectionData
{
	GENERATED_BODY()
	
	TArray<uint8> EntryIndex; // IDs of lanes at the intersection
	TArray<FSide> Sides;
	int32 AloneSide = -1; // Index of the alone side, -1 if none
	TMap<int32, bool> OpenLanes;

	//TODO::SideTypes
	FORCEINLINE void SideAddLane(const FZoneGraphStorage* ZoneGraphStorage, int32 LaneIndex);
	FORCEINLINE void SideSortLanes(const FZoneGraphStorage* ZoneGraphStorage);
	FORCEINLINE void FindAloneSide(const FZoneGraphStorage* ZoneGraphStorage);
	FORCEINLINE void SetSideOpenLanes(int32 SideIndex,ETurnType TurnType,bool Reset=false);
	FORCEINLINE void SetOpenLanes(TArray<int32> LaneIds);
	FORCEINLINE TArray<int32> GetAllLaneIndex();
	FORCEINLINE TArray<int32> GetOpenLaneIndex();
};

USTRUCT()
struct FTrafficLightInitData
{
	GENERATED_BODY()

	TArray<FTransform> TrafficLightTransforms;
	TMap<ETrafficSignalType, float> Periods;
	TArray<int32> ZoneIndex;
	TArray<int32> StartSideIndex;

	TArray<TMap<FName, TArray<int32>>> Arr_PhaseLanes;
	TArray<TMap<FName, FPhaseLanes>> Arr_PhaseControlledLanes;
	TArray<FName> Arr_CrossID;

};

USTRUCT()
struct FVehicleInitData
{
	GENERATED_BODY()
	TArray<FZoneGraphLaneLocation> LaneLocations;
	TArray<float> TargetSpeeds;
	TArray<float> MaxSpeeds;
	TArray<float> MinSpeeds;
	TArray<FZoneGraphTag> LaneSpeedTags;

};

USTRUCT(BlueprintType)
struct FZoneSegLane
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	TArray<int32> Lanes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	float StartDist;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	float EndDist;
};

USTRUCT(BlueprintType)
struct FDTRoadLanes :public FTableRowBase
{
	GENERATED_BODY()
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	//int64 RoadID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	TArray<FZoneSegLane> ZoneSegLanes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nav")
	int32 state=-1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nav")
	float speed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nav")
	float traveltime;

};

USTRUCT(BlueprintType)
struct FDTRoadGeoStatus : public FTableRowBase
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="Nav")
	FString name;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Nav")
	TArray<FVector> coords;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Nav")
	int32 roadclass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nav")
	int32 state;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nav")
	float speed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nav")
	float traveltime;

};

USTRUCT(BlueprintType)
struct FCrossPhaseLaneData :public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossPhase")
	FName CrossID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossPhase")
	FName PhaseName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossPhase")
	int32 ZoneIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossPhase")
	TArray<int32> ControlledLaneIndice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossPhase")
	TArray<int32> LaneIndices;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossPhase")
	TArray<FName> CtrLaneSectIDs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossPhase")
	TArray<FName> TurnType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossPhase")
	TArray<FName> RoadNames;

};

USTRUCT()
struct FMassGlobalDespawnTag : public FMassTag
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FTagLaneSpeed
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrafficSim")
	float MaxSpeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrafficSim")
	float MinSpeed; // in meters

};

USTRUCT(BlueprintType)
struct FTagLaneGap
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrafficSim")
	float MaxGap;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrafficSim")
	float MinGap; // in meters

};

USTRUCT(BlueprintType)
struct FPhaseLanes:public FTableRowBase
{
	GENERATED_BODY()


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrafficSim")
	int32 ZoneIndex;
	FName CrossID;
	FName PhaseName;
	TArray<int32> PhaseLanes;
	TArray<FName> LaneSectIDs;
	TArray<FName> LaneTurnTypes;
	TArray<int32> ControlledLaneIndice;
};


USTRUCT(BlueprintType)
struct FRoadToLaneIndexRow :public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaneID to Index")
	TArray<int32> LaneIndices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaneID to Index")
	TArray<float> StartDist;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaneID to Index")
	TArray<float> EndDist;
 };

USTRUCT(BlueprintType)
struct FVehTypeRow :public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaneID to Index")
	FString TypeDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaneID to Index")
	int32 TypeIndex;

};

USTRUCT(BlueprintType)
struct FOutterLaneVehicle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaneVehicle")
	FName LaneSectID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaneVehicle")
	TArray<FName> VehicleIDs;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaneVehicle")
	float FlowSpeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LaneVehicle")
	TArray<int32> VehicleTypeIndices;
};

USTRUCT(BlueprintType)
struct FArrayInt
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArrayInt")
	TArray<int32> IntArray;

	FArrayInt() {}

	FArrayInt(const TArray<int32> InArray) :IntArray(InArray){ }
};