#include "CrossPhaseSetProcessor.h"
#include "TrafficLightFragment.h"

UCrossPhaseSetProcessor::UCrossPhaseSetProcessor():EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = FName(TEXT("TrafficLight"));
	ExecutionOrder.ExecuteBefore.Add(FName(TEXT("TrafficLightCountingProcessor")));

	bAutoRegisterWithProcessingPhases = true;
}

void UCrossPhaseSetProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTrafficLightFragment>(EMassFragmentAccess::ReadWrite);
}

void UCrossPhaseSetProcessor::Initialize(UObject& Owner)
{
	TrafficSimSubsystem = Owner.GetWorld()->GetSubsystem<UTrafficSimSubsystem>();
	if (!TrafficSimSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("UCrossPhaseSetProcessor::Initialize: TrafficSimSubsystem is null"));
		return;
	}
}

void UCrossPhaseSetProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (EntityManager.IsEntityValid(EntityHandle) && EntityManager.IsEntityActive(EntityHandle))
	{
		FTrafficLightFragment& TLFrag = EntityManager.GetFragmentDataChecked<FTrafficLightFragment>(EntityHandle);

		TLFrag.PhaseControll = true;
		TLFrag.TimeInDuration = 0.1;
		TLFrag.PhaseList = PhaseArray;

	}
}
