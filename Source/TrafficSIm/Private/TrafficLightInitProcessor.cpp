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

void UTrafficLightInitProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!ensure(Context.ValidateAuxDataType<FTrafficLightInitData>()))
	{
		UE_LOG(LogTrafficLight, Error, TEXT("TrafficLightInitProcessor requires FTrafiicLightInitData to be set in the context."));
	}
	FTrafficLightInitData& InitData = Context.GetMutableAuxData().GetMutable<FTrafficLightInitData>();
	
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [InitData](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		TArrayView<FTransformFragment> LocationList = Context.GetMutableFragmentView<FTransformFragment>();
		TArrayView<FTrafficLightFragment> TrafficLightFragment = Context.GetMutableFragmentView<FTrafficLightFragment>();


		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{		
			LocationList[EntityIndex].GetMutableTransform() = InitData.TrafficLightTransforms[EntityIndex];
			TrafficLightFragment[EntityIndex].ZoneIndex = InitData.ZoneIndex[EntityIndex];
			TrafficLightFragment[EntityIndex].CurrentSide = InitData.StartSideIndex[EntityIndex];
			//TrafficLightFragment[EntityIndex].LightDurations = InitData.Periods;
		}
		});
}
