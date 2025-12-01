// Fill out your copyright notice in the Description page of Project Settings.


#include "TrafficLightInitProcessor.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "TrafficLightFragment.h"

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
