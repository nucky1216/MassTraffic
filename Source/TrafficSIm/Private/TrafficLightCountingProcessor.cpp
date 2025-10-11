// Fill out your copyright notice in the Description page of Project Settings.


#include "TrafficLightCountingProcessor.h"
#include "TrafficLightFragment.h"
#include "MassExecutionContext.h"
#include "TrafficSimSubsystem.h"
#include "TrafficTypes.h"

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
	TrafficLightSubsystem = Owner.GetWorld()->GetSubsystem<UTrafficLightSubsystem>();
	if(!TrafficLightSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("UTrafficLightCountingProcessor::Initialize: TrafficLightSubsystem is null"));
		return;
	}
}

void UTrafficLightCountingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	float DeltaTime = Context.GetDeltaTimeSeconds();
	
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, DeltaTime](FMassExecutionContext& Context)
	{
		const TArrayView<FTrafficLightFragment>& TrafficLightFragments = Context.GetMutableFragmentView<FTrafficLightFragment>();

		for (int32 i = 0; i < Context.GetNumEntities(); i++)
		{
			FTrafficLightFragment& TrafficLightFragment = TrafficLightFragments[i];
			int32& SideIndex=TrafficLightFragment.CurrentSide;
			float& TimeInDuration = TrafficLightFragment.TimeInDuration;

			TimeInDuration -= DeltaTime;
			/*UE_LOG(LogTemp, Log, TEXT("UTrafficLightCountingProcessor::Execute: LeftTime=%f in ZoneIndex:%d"), 
				TimeInDuration, TrafficLightFragment.ZoneIndex);*/
			if(TimeInDuration <=0.f)
			{
				ETrafficSignalType& CurrentSignal = TrafficLightFragment.CurrentLightState;
				TArray<ETrafficSignalType> SignalSequence;
				TrafficLightFragment.LightDurations.GetKeys(SignalSequence);
				const FIntersectionData* IntersectionData = TrafficLightSubsystem->IntersectionDatas.Find(TrafficLightFragment.ZoneIndex);
				if(!IntersectionData)
				{
					UE_LOG(LogTemp, Error, TEXT("UTrafficLightCountingProcessor::Execute: IntersectionData is null of ZoneIndex:%d"), TrafficLightFragment.ZoneIndex);
					continue;
				}

				int32 SequenceIndex=SignalSequence.IndexOfByKey(CurrentSignal);
				if(SequenceIndex==INDEX_NONE)
				{
					UE_LOG(LogTemp, Error, TEXT("UTrafficLightCountingProcessor::Execute: SignalSequence.IndexOfByKey(CurrentSignal) returned INDEX_NONE"));
					return;
				}
				else if(SequenceIndex+1<SignalSequence.Num())
				{
					SequenceIndex++;
					CurrentSignal = SignalSequence[SequenceIndex];
				}
				else
				{
					
					SideIndex = (SideIndex + 1) % (*IntersectionData).Sides.Num();
					CurrentSignal = SignalSequence[0];
				}
				TimeInDuration = TrafficLightFragment.LightDurations[CurrentSignal];
				
				TrafficLightSubsystem->SetCrossBySignalState(TrafficLightFragment.ZoneIndex, CurrentSignal, SideIndex);
				//TrafficLightSubsystem->DebugDrawState(TrafficLightFragment.ZoneIndex, TimeInDuration);

				
			}
			
			
		}
		});
}
