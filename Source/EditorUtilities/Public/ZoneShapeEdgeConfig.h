// ZoneShapeEdgeConfig.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ZoneShapeEdgeConfig.generated.h"

UENUM(BlueprintType)
enum class EEdgeTurnType : uint8
{
    Straight,
    RightTurn,
    LeftTurn,
    StraightRight
};

USTRUCT(BlueprintType)
struct FEdgeLaneConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Edge Config")
    int32 LaneIndex;

    UPROPERTY(EditAnywhere, Category = "Edge Config")
    EEdgeTurnType TurnType;

    FEdgeLaneConfig()
        : LaneIndex(-1)
        , TurnType(EEdgeTurnType::Straight)
    {}
};

USTRUCT(BlueprintType)
struct FEdgeConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Edge Config")
    int32 EdgeIndex;

    UPROPERTY(EditAnywhere, Category = "Edge Config")
    TArray<FEdgeLaneConfig> LaneConfigs;

    FEdgeConfig()
        : EdgeIndex(0)
    {}
};

UCLASS(BlueprintType)
class UZoneShapeEdgeConfig : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Polygon Edge Config")
    TArray<FEdgeConfig> EdgeConfigs;
};
