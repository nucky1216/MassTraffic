#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassEntityConfigAsset.h"
#include "LaneCongestionAdjustProcessor.generated.h"

class UTrafficSimSubsystem;

/** 拥堵指标类型 */
UENUM(BlueprintType)
enum class ELaneCongestionMetric : uint8
{
	DensityPerKm UMETA(DisplayName="Density (vehicles / 1000uu)"),
	AverageGap   UMETA(DisplayName="Average Gap (uu)")
};

/** 手动触发：根据目标拥堵程度（密度或平均车间距）对指定车道车辆重新分布 */
UCLASS()
class TRAFFICSIM_API ULaneCongestionAdjustProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULaneCongestionAdjustProcessor();

	// 目标车道
	int32 TargetLaneIndex = -1;
	// 拥堵指标类型
	ELaneCongestionMetric MetricType = ELaneCongestionMetric::AverageGap;
	// 目标密度 (车辆 / 1000uu) 当 MetricType=DensityPerKm 时使用
	float TargetDensityPer1000 = 20.f; // 默认
	// 目标平均车间距 (中心到中心距离) 当 MetricType=AverageGap 时使用
	float TargetAverageGap = 800.f;
	// 最小安全间距 (用于生成/过滤) 可根据车辆长度再附加
	float MinSafetyGap = 200.f;
	// 用于新生成车辆的配置（可为空，若为空则使用 TrafficSimSubsystem 第一个模板）
	TObjectPtr<UMassEntityConfigAsset> SpawnEntityConfig = nullptr;

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	virtual void Initialize(UObject& Owner) override;

private:
	mutable FMassEntityQuery EntityQuery; // mutable so it can be used in const contexts if needed
	UTrafficSimSubsystem* TrafficSimSubsystem = nullptr;

	struct FLaneVehicleRuntime
	{
		FMassEntityHandle Entity;
		FMassVehicleMovementFragment* MoveFrag = nullptr;
	};

	void CollectLaneVehicles(FMassEntityManager& EntityManager, FMassExecutionContext& Context, TArray<FLaneVehicleRuntime>& OutVehicles); // removed const
	void RemoveExcessVehicles(FMassEntityManager& EntityManager, TArray<FLaneVehicleRuntime>& Vehicles, int32 TargetCount) const;
	void SpawnAdditionalVehicles(FMassEntityManager& EntityManager, int32 AdditionalCount, float LaneLength, const TArray<FLaneVehicleRuntime>& ExistingVehicles) const;
	int32 ComputeTargetCount(float LaneLength, const TArray<FLaneVehicleRuntime>& Vehicles) const;
};
