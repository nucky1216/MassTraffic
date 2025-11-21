#include "CrossPhaseSetProcessor.h"

UCrossPhaseSetProcessor::UCrossPhaseSetProcessor()
{
	ExecuationOrder.ExecuteInGroup = FName(TEXT("TrafficLight"));
	ExecuationOrder.ExecuteBefore.Add(FName(TEXT("TrafficLightCountingProcessor")));

	bAutoRegisterWithProcessoringPhase = true;
}

void UCrossPhaseSetProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTrafficLightFragment>(EMassFragmentAccess::ReadWirte);
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
	EntityManager;
}
