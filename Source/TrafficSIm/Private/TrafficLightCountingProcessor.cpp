// Fill out your copyright notice in the Description page of Project Settings.


#include "TrafficLightCountingProcessor.h"
#include "TrafficLightFragment.h"
#include "MassExecutionContext.h"

UTrafficLightCountingProcessor::UTrafficLightCountingProcessor():EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = FName(TEXT("TrafficLight"));

	bAutoRegisterWithProcessingPhases = true;
}

void UTrafficLightCountingProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTrafficLightFragment>(EMassFragmentAccess::ReadOnly);
}

void UTrafficLightCountingProcessor::Initialize(UObject& Owner)
{
	TrafficSimSubsystem = Owner.GetWorld()->GetSubsystem<UTrafficSimSubsystem>();
	if(!TrafficSimSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("UTrafficLightCountingProcessor::Initialize: TrafficSimSubsystem is null"));
		return;
	}
}

void UTrafficLightCountingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, DeltaTime](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTrafficLightFragment> TrafficLightFragments = Context.GetFragmentView<FTrafficLightFragment>();

		for (int32 i = 0; i < Context.GetNumEntities(); i++)
		{

		}
		});
}
