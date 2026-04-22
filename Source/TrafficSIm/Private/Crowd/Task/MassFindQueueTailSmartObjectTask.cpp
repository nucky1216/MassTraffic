#include "Crowd/Task/MassFindQueueTailSmartObjectTask.h"
#include "TrafficCommonFragments.h"

#include "MassAIBehaviorTypes.h"
#include "MassCommonFragments.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectFragments.h"
#include "MassSmartObjectHandler.h"
#include "MassStateTreeExecutionContext.h"
#include "MassZoneGraphNavigationFragments.h"
#include "StateTreeLinker.h"
#include "SmartObjectSubsystem.h"

FMassFindQueueTailSmartObjectTask::FMassFindQueueTailSmartObjectTask()
{
	bShouldStateChangeOnReselect = false;
}

bool FMassFindQueueTailSmartObjectTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	Linker.LinkExternalData(MassSignalSubsystemHandle);
	Linker.LinkExternalData(EntityTransformHandle);
	Linker.LinkExternalData(SmartObjectUserHandle);
	Linker.LinkExternalData(LocationHandle);
	Linker.LinkExternalData(QueuedAdvanceSlotHandle);

	return true;
}

EStateTreeRunStatus FMassFindQueueTailSmartObjectTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.FoundCandidateSlots.Reset();
	InstanceData.bHasCandidateSlots = false;
	InstanceData.SearchRequestID.Reset();
	InstanceData.NextUpdate = 0.0;
	InstanceData.LastLane = FZoneGraphLaneHandle();

	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	if (FMassQueuedAdvanceSlotFragment* QueuedAdvanceSlot = Context.GetExternalDataPtr(QueuedAdvanceSlotHandle))
	{
		if (QueuedAdvanceSlot->bHasPendingSlot && QueuedAdvanceSlot->PendingSlot.SmartObjectHandle.IsValid() && SmartObjectSubsystem.CanBeClaimed(QueuedAdvanceSlot->PendingSlot.SlotHandle))
		{
			InstanceData.FoundCandidateSlots.NumSlots = 1;
			InstanceData.FoundCandidateSlots.Slots[0] = FSmartObjectCandidateSlot(QueuedAdvanceSlot->PendingSlot, 0.0f);
			InstanceData.bHasCandidateSlots = true;

			const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
			UE_LOG(LogTemp, Log, TEXT("SN:%d Found pending slot from QueuedAdvanceSlotFragment with SlotIndex:%d"), MassContext.GetEntity().SerialNumber, QueuedAdvanceSlot->PendingSlot.SlotHandle.GetSlotIndex());
			return  EStateTreeRunStatus::Running;
		}

		QueuedAdvanceSlot->bHasPendingSlot = false;
		QueuedAdvanceSlot->PendingSlot = FSmartObjectRequestResult();
	}

	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	UE_LOG(LogTemp, Log, TEXT("SN:%d Enter Task:FindQueueTailSmartObject"), MassContext.GetEntity().SerialNumber);
	return EStateTreeRunStatus::Running;
}

void FMassFindQueueTailSmartObjectTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.SearchRequestID.IsSet())
	{
		const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
		const FMassSmartObjectHandler Handler(MassContext.GetEntityManager(), MassContext.GetEntitySubsystemExecutionContext(), SmartObjectSubsystem, SignalSubsystem);
		Handler.RemoveRequest(InstanceData.SearchRequestID);
		InstanceData.SearchRequestID.Reset();
	}
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	UE_LOG(LogTemp, Log, TEXT("SN:%d Exit Task:FindQueueTailSmartObject"), MassContext.GetEntity().SerialNumber);
}

void FMassFindQueueTailSmartObjectTask::StateCompleted(FStateTreeExecutionContext& Context, const EStateTreeRunStatus CompletionStatus, const FStateTreeActiveStates& CompletedActiveStates) const
{
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);

	bool bHasPendingAdvanceSlot = false;
	if (FMassQueuedAdvanceSlotFragment* QueuedAdvanceSlot = Context.GetExternalDataPtr(QueuedAdvanceSlotHandle))
	{
		if (QueuedAdvanceSlot->bHasPendingSlot
			&& QueuedAdvanceSlot->PendingSlot.SmartObjectHandle.IsValid()
			&& SmartObjectSubsystem.CanBeClaimed(QueuedAdvanceSlot->PendingSlot.SlotHandle))
		{
			InstanceData.FoundCandidateSlots.NumSlots = 1;
			InstanceData.FoundCandidateSlots.Slots[0] = FSmartObjectCandidateSlot(QueuedAdvanceSlot->PendingSlot, 0.0f);
			InstanceData.bHasCandidateSlots = true;
			bHasPendingAdvanceSlot = true;
			UE_LOG(LogTemp, Log, TEXT("SN:%d Found pending slot from QueuedAdvanceSlotFragment with SlotIndex:%d, Claimable:%d"),
				MassContext.GetEntity().SerialNumber, QueuedAdvanceSlot->PendingSlot.SlotHandle.GetSlotIndex(), SmartObjectSubsystem.CanBeClaimed(QueuedAdvanceSlot->PendingSlot.SlotHandle));
		}

		QueuedAdvanceSlot->bHasPendingSlot = false;
		QueuedAdvanceSlot->PendingSlot = FSmartObjectRequestResult();
	}

	if (SOUser.InteractionHandle.IsValid() && !bHasPendingAdvanceSlot)
	{
		InstanceData.FoundCandidateSlots.Reset();
		InstanceData.bHasCandidateSlots = false;
	}

	InstanceData.NextUpdate = 0.0;

	
	UE_LOG(LogTemp, Log, TEXT("SN:%d Complete Task:FindQueueTailSmartObject"), MassContext.GetEntity().SerialNumber);
}

EStateTreeRunStatus FMassFindQueueTailSmartObjectTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const UWorld* World = Context.GetWorld();
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FMassSmartObjectHandler Handler(MassContext.GetEntityManager(), MassContext.GetEntitySubsystemExecutionContext(), SmartObjectSubsystem, SignalSubsystem);
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	//UE_LOG(LogTemp, Log, TEXT("SN:%d Tick Task:FindQueueTailSmartObject"), MassContext.GetEntity().SerialNumber);

	if (!InstanceData.SearchRequestID.IsSet() && !InstanceData.bHasCandidateSlots)
	{
		if (FMassQueuedAdvanceSlotFragment* QueuedAdvanceSlot = Context.GetExternalDataPtr(QueuedAdvanceSlotHandle))
		{
			if (QueuedAdvanceSlot->bHasPendingSlot && QueuedAdvanceSlot->PendingSlot.SmartObjectHandle.IsValid() && SmartObjectSubsystem.CanBeClaimed(QueuedAdvanceSlot->PendingSlot.SlotHandle))
			{
				InstanceData.FoundCandidateSlots.NumSlots = 1;
				InstanceData.FoundCandidateSlots.Slots[0] = FSmartObjectCandidateSlot(QueuedAdvanceSlot->PendingSlot, 0.0f);
				InstanceData.bHasCandidateSlots = true;
				UE_LOG(LogTemp, Log, TEXT("SN:%d Found pending slot from QueuedAdvanceSlotFragment with SlotIndex:%d"), MassContext.GetEntity().SerialNumber, QueuedAdvanceSlot->PendingSlot.SlotHandle.GetSlotIndex());

				QueuedAdvanceSlot->bHasPendingSlot = false;
				QueuedAdvanceSlot->PendingSlot = FSmartObjectRequestResult();

				return EStateTreeRunStatus::Running;
			}

			QueuedAdvanceSlot->bHasPendingSlot = false;
			QueuedAdvanceSlot->PendingSlot = FSmartObjectRequestResult();
		}
	}

	if (!InstanceData.SearchRequestID.IsSet())
	{
		if (SOUser.InteractionHandle.IsValid())
		{
			InstanceData.FoundCandidateSlots.Reset();
			InstanceData.bHasCandidateSlots = false;
			return EStateTreeRunStatus::Running;
		}

		if (InstanceData.bHasCandidateSlots)
		{
			return EStateTreeRunStatus::Running;
		}

		const FMassZoneGraphLaneLocationFragment* LaneLocation = Context.GetExternalDataPtr(LocationHandle);
		const bool bLaneHasChanged = (LaneLocation && InstanceData.LastLane != LaneLocation->LaneHandle);
		const bool bTimeForNextUpdate = World->GetTimeSeconds() > InstanceData.NextUpdate;

		if (bTimeForNextUpdate || bLaneHasChanged)
		{
			const FMassEntityHandle RequestingEntity = MassContext.GetEntity();
			if (LaneLocation != nullptr && LaneLocation->LaneHandle.IsValid())
			{
				InstanceData.LastLane = LaneLocation->LaneHandle;
				InstanceData.SearchRequestID = Handler.FindCandidatesAsync(RequestingEntity, SOUser.UserTags, ActivityRequirements, { LaneLocation->LaneHandle, LaneLocation->DistanceAlongLane });
			}
			else
			{
				const FTransformFragment& TransformFragment = Context.GetExternalData(EntityTransformHandle);
				InstanceData.SearchRequestID = Handler.FindCandidatesAsync(RequestingEntity, SOUser.UserTags, ActivityRequirements, TransformFragment.GetTransform().GetLocation());
			}

			//UE_LOG(LogTemp, Warning, TEXT("[QueueFind][SubmitRequest] EntitySN:%d RequestSet:%d LaneValid:%d NextUpdate:%.3f Now:%.3f"),
			//	RequestingEntity.SerialNumber,
			//	InstanceData.SearchRequestID.IsSet() ? 1 : 0,
			//	(LaneLocation && LaneLocation->LaneHandle.IsValid()) ? 1 : 0,
			//	InstanceData.NextUpdate,
			//	World->GetTimeSeconds());
		}
	}
	else
	{
		if (const FMassSmartObjectCandidateSlots* NewCandidates = Handler.GetRequestCandidates(InstanceData.SearchRequestID))
		{
			InstanceData.FoundCandidateSlots.Reset();
			InstanceData.bHasCandidateSlots = false;

			for (uint8 CandidateIndex = 0; CandidateIndex < NewCandidates->NumSlots; ++CandidateIndex)
			{
				const FSmartObjectRequestResult& CandidateResult = NewCandidates->Slots[CandidateIndex].Result;
				if (!CandidateResult.SmartObjectHandle.IsValid())
				{
					continue;
				}

				TArray<FSmartObjectSlotHandle> AllSlots;
				SmartObjectSubsystem.GetAllSlots(CandidateResult.SmartObjectHandle, AllSlots);

				for (int32 SlotIndex = 0; SlotIndex <AllSlots.Num(); ++SlotIndex)
				{
					const FSmartObjectSlotHandle SlotHandle = AllSlots[SlotIndex];
					if (!SmartObjectSubsystem.CanBeClaimed(SlotHandle))
					{
						continue;
					}

					InstanceData.FoundCandidateSlots.NumSlots = 1;
					InstanceData.FoundCandidateSlots.Slots[0] = FSmartObjectCandidateSlot(
						FSmartObjectRequestResult(CandidateResult.SmartObjectHandle, SlotHandle),
						0.0f);
					InstanceData.bHasCandidateSlots = true;
					break;
				}

				if (InstanceData.bHasCandidateSlots)
				{
					break;
				}
			}

			//UE_LOG(LogTemp, Warning, TEXT("[QueueFind][CandidateReady] EntitySN:%d NewCandidatesNum:%d bHasCandidateSlots:%d"),
			//	MassContext.GetEntity().SerialNumber,
			//	NewCandidates->NumSlots,
			//	InstanceData.bHasCandidateSlots ? 1 : 0);

			Handler.RemoveRequest(InstanceData.SearchRequestID);
			InstanceData.SearchRequestID.Reset();

			const FMassEntityHandle Entity = MassContext.GetEntity();
			constexpr float SearchIntervalDeviation = 0.1f;
			const float DelayInSeconds = SearchInterval * FMath::FRandRange(1.0f - SearchIntervalDeviation, 1.0f + SearchIntervalDeviation);
			InstanceData.NextUpdate = World->GetTimeSeconds() + DelayInSeconds;
			UMassSignalSubsystem& MassSignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
			MassSignalSubsystem.DelaySignalEntity(UE::Mass::Signals::SmartObjectRequestCandidates, Entity, DelayInSeconds);
		}
	}

	return EStateTreeRunStatus::Running;
}
