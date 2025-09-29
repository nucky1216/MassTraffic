#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityTypes.h"
#include "MassEntityConfigAsset.h"
#include "LaneCongestionAdjustProcessor.generated.h"

class UTrafficSimSubsystem;

/** ӵ��ָ������ */
UENUM(BlueprintType)
enum class ELaneCongestionMetric : uint8
{
	DensityPerKm UMETA(DisplayName="Density (vehicles / 1000uu)"),
	AverageGap   UMETA(DisplayName="Average Gap (uu)")
};

/** �ֶ�����������Ŀ��ӵ�³̶ȣ��ܶȻ�ƽ������ࣩ��ָ�������������·ֲ� */
UCLASS()
class TRAFFICSIM_API ULaneCongestionAdjustProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	ULaneCongestionAdjustProcessor();

	// Ŀ�공��
	int32 TargetLaneIndex = -1;
	// ӵ��ָ������
	ELaneCongestionMetric MetricType = ELaneCongestionMetric::AverageGap;
	// Ŀ���ܶ� (���� / 1000uu) �� MetricType=DensityPerKm ʱʹ��
	float TargetDensityPer1000 = 20.f; // Ĭ��
	// Ŀ��ƽ������� (���ĵ����ľ���) �� MetricType=AverageGap ʱʹ��
	float TargetAverageGap = 800.f;
	// ��С��ȫ��� (��������/����) �ɸ��ݳ��������ٸ���
	float MinSafetyGap = 200.f;
	// ���������ɳ��������ã���Ϊ�գ���Ϊ����ʹ�� TrafficSimSubsystem ��һ��ģ�壩
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
