// Fill out your copyright notice in the Description page of Project Settings.


#include "TrafficLightCountingProcessor.h"
#include "TrafficLightFragment.h"
#include "MassExecutionContext.h"
#include "TrafficSimSubsystem.h"
#include "MassCommonFragments.h"
#include "TrafficTypes.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationSubsystem.h"

UTrafficLightCountingProcessor::UTrafficLightCountingProcessor():EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = FName(TEXT("TrafficSim"));

	bAutoRegisterWithProcessingPhases = true;
}

void UTrafficLightCountingProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTrafficLightFragment>(EMassFragmentAccess::ReadWrite);
//	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
//	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
//	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
//	EntityQuery.AddRequirement<FMassRepresentationLODFragment >(EMassFragmentAccess::ReadOnly);
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
		//const TArrayView<FMassActorFragment>& ActorFragments = Context.GetMutableFragmentView<FMassActorFragment>();
		//const TArrayView<FMassRepresentationFragment> RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();
		//const TArrayView<FMassRepresentationLODFragment> RepresentationLODList = Context.GetMutableFragmentView<FMassRepresentationLODFragment>();
	//	TArrayView<FTransformFragment> TransformFragments = Context.GetMutableFragmentView<FTransformFragment>();
		for (int32 i = 0; i < Context.GetNumEntities(); i++)
		{
			FTrafficLightFragment& TrafficLightFragment = TrafficLightFragments[i];
			//FMassActorFragment& ActorFragment = ActorFragments[i];
			
			double& TimeInDuration = TrafficLightFragment.TimeInDuration;
			TimeInDuration -= DeltaTime;

			//RepresentationLODList[i].
			//UE_LOG(LogTrafficLight, Log, TEXT("LODSignifcance:%f ZoneIndex:%d"), RepresentationLODList[i].LODSignificance, TrafficLightFragment.ZoneIndex);

			//Default to Auto Set Pass Lanes 
			if(!TrafficLightFragment.PhaseControll)
			{
				int32& SideIndex = TrafficLightFragment.CurrentSide;			
				/*UE_LOG(LogTemp, Log, TEXT("UTrafficLightCountingProcessor::Execute: LeftTime=%f in ZoneIndex:%d"),
					TimeInDuration, TrafficLightFragment.ZoneIndex);*/
				if (TimeInDuration <= 0.f)
				{
					ETrafficSignalType& CurrentSignal = TrafficLightFragment.CurrentLightState;
					TArray<ETrafficSignalType> SignalSequence;
					TrafficLightFragment.LightDurations.GetKeys(SignalSequence);
					const FIntersectionData* IntersectionData = TrafficLightSubsystem->IntersectionDatas.Find(TrafficLightFragment.ZoneIndex);
					if (!IntersectionData)
					{
						UE_LOG(LogTemp, Error, TEXT("UTrafficLightCountingProcessor::Execute: IntersectionData is null of ZoneIndex:%d"), TrafficLightFragment.ZoneIndex);
						continue;
					}

					int32 SequenceIndex = SignalSequence.IndexOfByKey(CurrentSignal);
					if (SequenceIndex == INDEX_NONE)
					{
						UE_LOG(LogTemp, Error, TEXT("UTrafficLightCountingProcessor::Execute: SignalSequence.IndexOfByKey(CurrentSignal) returned INDEX_NONE"));
						return;
					}
					else if (SequenceIndex + 1 < SignalSequence.Num())
					{
						SequenceIndex= (SequenceIndex+1)% SignalSequence.Num();
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
			//Set Pass Lanes By PhaseList
			else
			{
				if (TimeInDuration <= 5.f && TrafficLightFragment.bRemainToYellow)
				{
					TrafficLightFragment.bRemainToYellow = false;
					TArray<int32> EmptyOpenLanes;
					TrafficLightSubsystem->SetPhaseLanesOpened(TrafficLightFragment.ZoneIndex, EmptyOpenLanes);
				}
				if(TimeInDuration <= 0.f)
				{
					if (TrafficLightFragment.PhaseList.Num()==0)
					{
						TrafficLightFragment.PhaseControll = false;
						continue;
					}
					//ActorFragment.Get();
					//set the next phase
					TTuple<FName, double, double> CurPhase= TrafficLightFragment.PhaseList[0];
					TrafficLightFragment.CurrentPhase = CurPhase.Get<0>();
					TrafficLightFragment.PhaseList.RemoveAt(0);

					TimeInDuration = CurPhase.Get<2>()-CurPhase.Get<1>();
					FName PhaseName = CurPhase.Get<0>();
					//Set Open Lanes
					if(!TrafficLightFragment.CrossPhaseLanes.Contains(PhaseName))
					{
						UE_LOG(LogTemp, Error, TEXT("UTrafficLightCountingProcessor::Execute: CrossPhaseLanes not contains PhaseName:%s"), *PhaseName.ToString());
						continue;
					}

					TrafficLightSubsystem->SetPhaseLanesOpened(TrafficLightFragment.ZoneIndex, TrafficLightFragment.CrossPhaseLanes[PhaseName]);
					TrafficLightFragment.bRemainToYellow = true;
					//Debug
					//TrafficLightSubsystem->DebugCrossPhase(TrafficLightFragment.CrossPhaseLanes[PhaseName]);
				}
				

			}
			
		}
		});
}
