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



			int32& SideIndex = TrafficLightFragment.CurrentSide;
			int32& ZoneIndex = TrafficLightFragment.ZoneIndex;
			float& TimeInDuration = TrafficLightFragment.TimeInDuration;

			ZoneIndex = InitData.ZoneIndex[EntityIndex];
			SideIndex= InitData.StartSideIndex[EntityIndex];

			ETrafficSignalType TrafficSignalType = TrafficLightFragment.CurrentLightState;
			TimeInDuration = TrafficLightFragment.LightDurations[TrafficSignalType];

			//Init Light State
			ETrafficSignalType& CurrentSignal = TrafficLightFragment.CurrentLightState;
			
			FIntersectionData* IntersectionData=TrafficLightSubsystem->IntersectionDatas.Find(TrafficLightFragment.ZoneIndex);
			if (CurrentSignal == ETrafficSignalType::StraightAndRight && (*IntersectionData).Sides.Num() == 4)
			{
				// Reset the vehicle count when the light turns green
				TrafficLightSubsystem->SetOpenLanes(TrafficLightFragment.ZoneIndex, SideIndex, ETurnType::Straight, true);
				TrafficLightSubsystem->SetOpenLanes(TrafficLightFragment.ZoneIndex, SideIndex, ETurnType::RightTurn);

				TrafficLightSubsystem->SetOpenLanes(TrafficLightFragment.ZoneIndex, (SideIndex + 2) % 4, ETurnType::Straight);
				TrafficLightSubsystem->SetOpenLanes(TrafficLightFragment.ZoneIndex, (SideIndex + 2) % 4, ETurnType::RightTurn);
			}

			if (CurrentSignal == ETrafficSignalType::Left && (*IntersectionData).Sides.Num() == 4)
			{
				// Reset the vehicle count when the light turns green
				TrafficLightSubsystem->SetOpenLanes(TrafficLightFragment.ZoneIndex, SideIndex, ETurnType::LeftTurn, true);

				TrafficLightSubsystem->SetOpenLanes(TrafficLightFragment.ZoneIndex, (SideIndex + 2) % 4, ETurnType::LeftTurn);
			}
			TrafficLightSubsystem->DebugDrawState(TrafficLightFragment.ZoneIndex, TimeInDuration);
		}
		});
}
