// Fill out your copyright notice in the Description page of Project Settings.


#include "VehicleParamsInitProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "EngineUtils.h"
#include "MassVehicleMovementFragment.h"
#include "ZoneGraphQuery.h"

UVehicleParamsInitProcessor::UVehicleParamsInitProcessor():EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = FName(TEXT("VehicleMovement"));
	//ExecutionOrder.ExecuteBefore.Add(FName(TEXT("VehicleCollision")));
	//ExecutionOrder.ExecuteAfter.Add(FName(TEXT("VehicleSpawn")));
	bAutoRegisterWithProcessingPhases = false;
}

void UVehicleParamsInitProcessor::Initialize(UObject& Owner)
{
	World = GetWorld();
	ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(World);
	
	for(TActorIterator<AZoneGraphData> It(World); It; ++It)
	{
		const AZoneGraphData* ZoneGraphData = *It;
		if (ZoneGraphData && ZoneGraphData->IsValidLowLevel())
		{
			ZoneGraphStorage = &ZoneGraphData->GetStorage();
			return;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("No valid ZoneGraphData found in the world!"));
}

void UVehicleParamsInitProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
}

void UVehicleParamsInitProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UE_LOG(LogTemp,Log,TEXT("VehicleInitProcessor.."));

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
	{
	    const int32 EntityCount = Context.GetNumEntities();

		const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FMassVehicleMovementFragment> VehicleMovementList = Context.GetMutableFragmentView<FMassVehicleMovementFragment>();

		if (!ZoneGraphStorage)
		{
			UE_LOG(LogTemp, Error, TEXT("ZoneGraphStorage is not initialized!"));
			return;
		}

		for (int32 EntityIndex = 0; EntityIndex < EntityCount; ++EntityIndex)
		{
			FMassVehicleMovementFragment& VehicleMovement = VehicleMovementList[EntityIndex];
			FVector SpawnLocation = TransformList[EntityIndex].GetTransform().GetLocation();

			FZoneGraphLaneLocation LaneLocation;
			float DistSq;

			FBox QueryBox = FBox::BuildAABB(SpawnLocation, VehicleMovement.QueryExtent);
			const bool bFound = UE::ZoneGraph::Query::FindNearestLane(
				*ZoneGraphStorage, QueryBox,VehicleMovement.LaneFilter,LaneLocation, DistSq);

			if (!bFound)
			{
				UE_LOG(LogTemp, Warning, TEXT("No suitable lane found for entity %d at location %s"), EntityIndex, *SpawnLocation.ToString());
				continue;
			}

			VehicleMovement.LaneLocation = LaneLocation;
			
			const FZoneLaneData& LaneData = ZoneGraphStorage->Lanes[LaneLocation.LaneHandle.Index];
			if(LaneData.GetLinkCount()!= 0)
			{
				int32 FirstLink=LaneData.LinksBegin;
				int32 LastLink = LaneData.LinksEnd;

				int32 LinkCount = LaneData.GetLinkCount();
				UE_LOG(LogTemp, Log, TEXT("Entity %d FirstLink:%d LastLink:%d LinkCount:%d"), Context.GetEntity(EntityIndex).SerialNumber,FirstLink,LastLink,LinkCount);
				for (int32 i = FirstLink; i <FirstLink+ LinkCount; i++)
				{
					FZoneLaneLinkData LinkData = ZoneGraphStorage->LaneLinks[i];

					UE_LOG(LogTemp, Log, TEXT("------- Lane %d has link to lane %d with type %d and flags %d"),
						 LaneLocation.LaneHandle.Index, LinkData.DestLaneIndex, (int32)LinkData.Type, (int32)LinkData.GetFlags());
				}
			}
			else
			{
				
			}

		}
	});
}
