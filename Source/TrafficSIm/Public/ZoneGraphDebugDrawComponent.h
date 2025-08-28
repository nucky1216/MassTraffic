// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "ZoneGraphTypes.h"
#include "ZoneGraphData.h"
#include "ZoneGraphRenderingComponent.h"
#include "ZoneGraphDebugDrawComponent.generated.h"

/**
 * 
 */
class FReZoneGraphSceneProxy : public FZoneGraphSceneProxy
{
public:
	FReZoneGraphSceneProxy(const UPrimitiveComponent& InComponent, const AZoneGraphData& ZoneGraph)
		: FZoneGraphSceneProxy(InComponent, ZoneGraph)
	{
		ZoneGraphData = &ZoneGraph;
	}
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
private :
	const AZoneGraphData* ZoneGraphData = nullptr;
};


UCLASS(Blueprintable, ClassGroup = (Draw), meta = (BlueprintSpawnableComponent))
class TRAFFICSIM_API UZoneGraphDebugDrawComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	UZoneGraphDebugDrawComponent();
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	UPROPERTY(EditAnywhere, Category = "Debug")
	AZoneGraphData* ZoneGraphData = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Debug")
	void UpdateRender();
private:
	const FZoneGraphStorage* ZoneGraphStorage = nullptr;
};
