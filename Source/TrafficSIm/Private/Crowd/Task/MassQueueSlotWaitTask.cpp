#include "Crowd/Task/MassQueueSlotWaitTask.h"

#include "Crowd/CrowdSimSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectFragments.h"
#include "MassSmartObjectHandler.h"
#include "MassStateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"

bool FMassQueueSlotWaitTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	Linker.LinkExternalData(MassSignalSubsystemHandle);
	Linker.LinkExternalData(SmartObjectUserHandle);
	Linker.LinkExternalData(QueuedAdvanceSlotHandle);
	return true;
}

EStateTreeRunStatus FMassQueueSlotWaitTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	InstanceData.FinishReason = EQueueSlotWaitFinishReason::NotSet;

	if (FMassQueuedAdvanceSlotFragment* QueuedAdvanceSlot = Context.GetExternalDataPtr(QueuedAdvanceSlotHandle))
	{
		QueuedAdvanceSlot->bHasPendingSlot = false;
		QueuedAdvanceSlot->PendingSlot = FSmartObjectRequestResult();
	}

	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	if (UCrowdSimSubsystem* CrowdSimSubsystem = Context.GetWorld()->GetSubsystem<UCrowdSimSubsystem>())
	{
		CrowdSimSubsystem->RegisterClaimedSlot(InstanceData.ClaimedSlot, MassContext.GetEntity());
	}

	if (InstanceData.WaitDuration > 0.f)
	{
		UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
		SignalSubsystem.DelaySignalEntity(UE::Mass::Signals::StandTaskFinished, MassContext.GetEntity(), InstanceData.WaitDuration);
	}

	return EStateTreeRunStatus::Running;
}

void FMassQueueSlotWaitTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.ClaimedSlot.IsValid())
	{
		FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
		const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
		const FMassSmartObjectHandler Handler(MassContext.GetEntityManager(), MassContext.GetEntitySubsystemExecutionContext(), SmartObjectSubsystem, SignalSubsystem);
		Handler.ReleaseSmartObject(MassContext.GetEntity(), SOUser, InstanceData.ClaimedSlot);
	}

	if (UCrowdSimSubsystem* CrowdSimSubsystem = Context.GetWorld()->GetSubsystem<UCrowdSimSubsystem>())
	{
		CrowdSimSubsystem->UnregisterClaimedSlot(InstanceData.ClaimedSlot);
	}
}

EStateTreeRunStatus FMassQueueSlotWaitTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);

	InstanceData.ElapsedTime += DeltaTime;
	const bool bTimedOut = InstanceData.WaitDuration > 0.f && InstanceData.ElapsedTime >= InstanceData.WaitDuration;

	bool bAdvanceRequested = false;
	if (UCrowdSimSubsystem* CrowdSimSubsystem = Context.GetWorld()->GetSubsystem<UCrowdSimSubsystem>())
	{
		bAdvanceRequested = CrowdSimSubsystem->ConsumeQueueAdvance(MassContext.GetEntity());
	}

	const bool bClaimStillValid = InstanceData.ClaimedSlot.IsValid() && SmartObjectSubsystem.IsClaimedSmartObjectValid(InstanceData.ClaimedSlot);
	if (!bTimedOut && !bAdvanceRequested && bClaimStillValid)
	{
		return EStateTreeRunStatus::Running;
	}

	if (bAdvanceRequested)
	{
		InstanceData.FinishReason = EQueueSlotWaitFinishReason::AdvanceRequested;
	}
	else if (bTimedOut)
	{
		InstanceData.FinishReason = EQueueSlotWaitFinishReason::TimedOut;
	}
	else
	{
		InstanceData.FinishReason = EQueueSlotWaitFinishReason::ClaimInvalid;
	}

	if (FMassQueuedAdvanceSlotFragment* QueuedAdvanceSlot = Context.GetExternalDataPtr(QueuedAdvanceSlotHandle))
	{
		QueuedAdvanceSlot->bHasPendingSlot = false;
		QueuedAdvanceSlot->PendingSlot = FSmartObjectRequestResult();

		if (InstanceData.FinishReason == EQueueSlotWaitFinishReason::AdvanceRequested && InstanceData.ClaimedSlot.IsValid())
		{
			TArray<FSmartObjectSlotHandle> AllSlots;
			SmartObjectSubsystem.GetAllSlots(InstanceData.ClaimedSlot.SmartObjectHandle, AllSlots);
			const int32 CurrentSlotIndex = InstanceData.ClaimedSlot.SlotHandle.GetSlotIndex();

			for (int32 SlotIndex = CurrentSlotIndex - 1; SlotIndex >= 0; --SlotIndex)
			{
				if (!AllSlots.IsValidIndex(SlotIndex))
				{
					continue;
				}

				const FSmartObjectSlotHandle CandidateSlotHandle = AllSlots[SlotIndex];
				if (!SmartObjectSubsystem.CanBeClaimed(CandidateSlotHandle))
				{
					continue;
				}

				QueuedAdvanceSlot->PendingSlot = FSmartObjectRequestResult(InstanceData.ClaimedSlot.SmartObjectHandle, CandidateSlotHandle);
				QueuedAdvanceSlot->bHasPendingSlot = true;
				break;
			}
		}
	}

	FMassEntityHandle NextEntity;
	if (const UCrowdSimSubsystem* CrowdSimSubsystem = Context.GetWorld()->GetSubsystem<UCrowdSimSubsystem>())
	{
		NextEntity = CrowdSimSubsystem->FindEntityOnNextSlot(InstanceData.ClaimedSlot);
	}

	if (UCrowdSimSubsystem* CrowdSimSubsystem = Context.GetWorld()->GetSubsystem<UCrowdSimSubsystem>())
	{
		CrowdSimSubsystem->UnregisterClaimedSlot(InstanceData.ClaimedSlot);
	}
	UE_LOG(LogTemp, Log, TEXT("Entity %d leaving queue slot %d. Reason=%d TimedOut: %d, AdvanceRequested: %d, ClaimValid: %d. NextEntity in queue is %d"),
		MassContext.GetEntity().SerialNumber, InstanceData.ClaimedSlot.SlotHandle.GetSlotIndex(), static_cast<int32>(InstanceData.FinishReason), bTimedOut, bAdvanceRequested, bClaimStillValid, NextEntity.SerialNumber);

	if (LeaveSignal != NAME_None)
	{
		SignalSubsystem.SignalEntity(LeaveSignal, MassContext.GetEntity());
	}
	SignalSubsystem.SignalEntity(UE::Mass::Signals::NewStateTreeTaskRequired, MassContext.GetEntity());

	const bool bShouldNotifyNextToAdvance =
		(InstanceData.FinishReason == EQueueSlotWaitFinishReason::TimedOut)
		|| (InstanceData.FinishReason == EQueueSlotWaitFinishReason::AdvanceRequested);
	if (NextEntity.IsSet() && bShouldNotifyNextToAdvance)
	{
		if (UCrowdSimSubsystem* CrowdSimSubsystem = Context.GetWorld()->GetSubsystem<UCrowdSimSubsystem>())
		{
			CrowdSimSubsystem->RequestQueueAdvance(NextEntity);
		}

		if (AdvanceSignal != NAME_None)
		{
			SignalSubsystem.SignalEntity(AdvanceSignal, NextEntity);
		}
		SignalSubsystem.SignalEntity(UE::Mass::Signals::NewStateTreeTaskRequired, NextEntity);
	}

	return EStateTreeRunStatus::Succeeded;
}
