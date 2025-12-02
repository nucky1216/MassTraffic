// Fill out your copyright notice in the Description page of Project Settings.


#include "TrafficLightInitProcessor.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "TrafficLightFragment.h"
#include "ZoneGraphQuery.h"

UTrafficLightInitProcessor::UTrafficLightInitProcessor():EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;
}

void UTrafficLightInitProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTrafficLightFragment>(EMassFragmentAccess::ReadWrite);

}

void UTrafficLightInitProcessor::Initialize(UObject& Owner)
{
	TrafficLightSubsystem =GetWorld()->GetSubsystem<UTrafficLightSubsystem>();
	if (!TrafficLightSubsystem)
	{
		UE_LOG(LogTrafficLight, Error, TEXT("TrafficLightInitProcessor could not find UTrafficLightSubsystem."));
		return;
	}
	TrafficSimSubsystem = GetWorld()->GetSubsystem<UTrafficSimSubsystem>();
	if (!TrafficSimSubsystem)
	{
		UE_LOG(LogTrafficLight, Error, TEXT("TrafficLightInitProcessor could not findUTrafficSimSubsystem."));
		return;
	}
}

void UTrafficLightInitProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!ensure(Context.ValidateAuxDataType<FTrafficLightInitData>()))
	{
		UE_LOG(LogTrafficLight, Error, TEXT("TrafficLightInitProcessor requires FTrafiicLightInitData to be set in the context."));
	}
	FTrafficLightInitData& InitData = Context.GetMutableAuxData().GetMutable<FTrafficLightInitData>();

	
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		TArrayView<FTransformFragment> LocationList = Context.GetMutableFragmentView<FTransformFragment>();
		TArrayView<FTrafficLightFragment> TrafficLightFragments = Context.GetMutableFragmentView<FTrafficLightFragment>();

		
		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{	
			FTrafficLightFragment& TrafficLightFragment = TrafficLightFragments[EntityIndex];
			LocationList[EntityIndex].GetMutableTransform() = InitData.TrafficLightTransforms[EntityIndex];

			//DrawDebugPoint(GetWorld(), LocationList[EntityIndex].GetTransform().GetLocation(), 20.0f, FColor::Green, true, 60.0f);

			int32& SideIndex = TrafficLightFragment.CurrentSide;
			int32& ZoneIndex = TrafficLightFragment.ZoneIndex;
			double& TimeInDuration = TrafficLightFragment.TimeInDuration;

			TrafficLightFragment.CrossID = InitData.Arr_CrossID[EntityIndex];

			TrafficLightFragment.PhaseControlledLanes = InitData.Arr_PhaseControlledLanes[EntityIndex];

			for(auto & Pair: TrafficLightFragment.PhaseControlledLanes)
			{
				for(auto & LaneID : Pair.Value)
				{
					if(LaneID<0)
					{
						continue;
					}
					const FZoneLaneData& LaneData = TrafficSimSubsystem->ZoneGraphStorage->Lanes[LaneID];
					FZoneGraphLaneLocation LaneLocation;
					float LaneLenth;
					UE::ZoneGraph::Query::GetLaneLength(*TrafficSimSubsystem->ZoneGraphStorage, LaneID, LaneLenth);
					UE::ZoneGraph::Query::CalculateLocationAlongLane(*TrafficSimSubsystem->ZoneGraphStorage, LaneID, LaneLenth , LaneLocation);

					TrafficLightFragment.CtlLaneTransforms.Add(LaneID, FTransform(LaneLocation.Direction.Rotation(),LaneLocation.Position));
				}
			}


			TrafficLightSubsystem->RegisterCrossEntity(TrafficLightFragment.CrossID,Context.GetEntity(EntityIndex));

			ZoneIndex = InitData.ZoneIndex[EntityIndex];
			SideIndex= InitData.StartSideIndex[EntityIndex];

			ETrafficSignalType TrafficSignalType = TrafficLightFragment.CurrentLightState;
			TimeInDuration = TrafficLightFragment.LightDurations[TrafficSignalType];

			//Init the PhaseLanes
			TrafficLightFragment.CrossPhaseLanes = InitData.Arr_PhaseLanes[EntityIndex];

			//Init Light State
			ETrafficSignalType& CurrentSignal = TrafficLightFragment.CurrentLightState;
			
			TrafficLightSubsystem->SetCrossBySignalState(TrafficLightFragment.ZoneIndex, CurrentSignal, SideIndex);


			//TrafficLightSubsystem->DebugDrawState(TrafficLightFragment.ZoneIndex, TimeInDuration);
		}
		});
}
