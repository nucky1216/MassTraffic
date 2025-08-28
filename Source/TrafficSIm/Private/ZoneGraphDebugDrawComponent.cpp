// Fill out your copyright notice in the Description page of Project Settings.


#include "ZoneGraphDebugDrawComponent.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneGraphRenderingUtilities.h"
#include "ZoneGraphSettings.h"

UZoneGraphDebugDrawComponent::UZoneGraphDebugDrawComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	//bWantsOnUpdateTransform = false;
	//SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	//bSelectable = false;
	//bHiddenInGame = true;
	//bUseAttachParentBound = true;
	//SetGenerateOverlapEvents(false);
	// Only for editor
}

FPrimitiveSceneProxy* UZoneGraphDebugDrawComponent::CreateSceneProxy()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	FReZoneGraphSceneProxy* ZoneGraphSceneProxy = nullptr;
	//const AZoneGraphData* ZoneGraphDataTemp = Cast<UZoneGraphDebugDrawComponent>(GetOwner())->ZoneGraphData;

	//if (ZoneGraphData != nullptr && IsVisible() && IsNavigationShowFlagSet(GetWorld()))
	if (ZoneGraphData != nullptr && IsVisible() )
	{
		ZoneGraphSceneProxy = new FReZoneGraphSceneProxy(*this, *ZoneGraphData);
	}
	return ZoneGraphSceneProxy;
#else
	return nullptr;
#endif //!UE_BUILD_SHIPPING && !UE_BUILD_TEST
}

FBoxSphereBounds UZoneGraphDebugDrawComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(ForceInit);

	//const AZoneGraphData* ZoneGraphDataTemp = Cast<UZoneGraphDebugDrawComponent>(GetOwner())->ZoneGraphData;
	if (ZoneGraphData)
	{
		BoundingBox = ZoneGraphData->GetBounds();
	}
	return FBoxSphereBounds(BoundingBox);
}

void UZoneGraphDebugDrawComponent::UpdateRender()
{
	SetVisibility(true);
	MarkRenderStateDirty();
	
}

void FReZoneGraphSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	FDebugRenderSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);

	//UZoneGraphRenderingComponent* RenderingComp = WeakRenderingComponent.Get();
	//const AZoneGraphData* ZoneGraphData = RenderingComp ? Cast<AZoneGraphData>(RenderingComp->GetOwner()) : nullptr;
	if (!ZoneGraphData)
	{
		return;
	}

	FScopeLock RegistrationLock(&(ZoneGraphData->GetStorageLock()));
	const FZoneGraphStorage& ZoneStorage = ZoneGraphData->GetStorage();

	static const float DepthBias = 0.0001f;	// Little bias helps to make the lines visible when directly on top of geometry.
	static const float LaneLineThickness = 2.0f;
	static const float BoundaryLineThickness = 0.0f;

	const FDrawDistances Distances = GetDrawDistances(GetMinDrawDistance(), GetMaxDrawDistance());
	//UE_LOG(LogTemp, Warning, TEXT("ViewNum:%d"),Views.Num());
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			//if (!ShouldRenderZoneGraph(*View)) //Enable always
			//{
			//	continue;
			//}
			//UE_LOG(LogTemp, Warning, TEXT("FReZoneGraphSceneProxy::GetDynamicMeshElements"));
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			const FVector Origin = View->ViewMatrices.GetViewOrigin();

			const UZoneGraphSettings* ZoneGraphSettings = GetDefault<UZoneGraphSettings>();
			check(ZoneGraphSettings);
			FZoneGraphTagMask VisTagMask = ZoneGraphSettings->GetVisualizedTags();
			TConstArrayView<FZoneGraphTagInfo>TagInfo = ZoneGraphSettings->GetTagInfos();

			for (int i = 0; i < ZoneStorage.Zones.Num(); i++)
			{

				const FZoneData& Zone = ZoneStorage.Zones[i];
				const FZoneVisibility DrawInfo = CalculateZoneVisibility(Distances, Origin, Zone.Bounds.GetCenter());



				if (!DrawInfo.bVisible)
				{
					continue;
				}

				// Draw boundary
				//UE::ZoneGraph::RenderingUtilities::DrawZoneBoundary(ZoneStorage, i, PDI, FMatrix::Identity, BoundaryLineThickness, DepthBias, DrawInfo.Alpha);
				// Draw Lanes
				
				UE::ZoneGraph::RenderingUtilities::DrawZoneLanes(ZoneStorage, i, PDI, FMatrix::Identity, LaneLineThickness, DepthBias, DrawInfo.Alpha, DrawInfo.bDetailsVisible);
				for (int32 j = ZoneStorage.Zones[i].LanesBegin; j < ZoneStorage.Zones[i].LanesEnd; j++)
				{
					if (j <125 &&j>110)
						continue;

					FZoneGraphLaneHandle LaneHandle(j, ZoneStorage.DataHandle);

					const FZoneLaneData& Lane = ZoneStorage.Lanes[j];
					FColor Color = UE::ZoneGraph::RenderingUtilities::GetTagMaskColor(Lane.Tags & VisTagMask, TagInfo);

					UE::ZoneGraph::RenderingUtilities::DrawLane(ZoneStorage, PDI, LaneHandle, FColor::Green, 20.f);
					//j++;
				}
			}

		}
	}
}
