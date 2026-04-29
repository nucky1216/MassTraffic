#include "Crowd/Task/MassQueueSlotWaitTask.h"

#include "Crowd/CrowdSimSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassAIBehaviorTypes.h"
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
	return true;
}

EStateTreeRunStatus FMassQueueSlotWaitTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	InstanceData.FinishReason = EQueueSlotWaitFinishReason::NotSet;

	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
	const FMassSmartObjectHandler MassSmartObjectHandler(MassContext.GetEntityManager(), MassContext.GetEntitySubsystemExecutionContext(), SmartObjectSubsystem, SignalSubsystem);

	// Ensure we are using the slot and valid InteractionHandle to support proper queue advancing.
	if (InstanceData.ClaimedSlot.IsValid() && !SOUser.InteractionHandle.IsValid())
	{
		if (!MassSmartObjectHandler.StartUsingSmartObject(MassContext.GetEntity(), SOUser, InstanceData.ClaimedSlot))
		{
			UE_LOG(LogTemp, Warning, TEXT("QueueSlotWait: Entity %d failed to start using slot %d."), MassContext.GetEntity().SerialNumber, InstanceData.ClaimedSlot.SlotHandle.GetSlotIndex());
			// If already failed, we might want to drop the Claim handle.
			return EStateTreeRunStatus::Failed;
		}
	}

	if (InstanceData.WaitDuration > 0.f)
	{
		SignalSubsystem.DelaySignalEntity(UE::Mass::Signals::StandTaskFinished, MassContext.GetEntity(), InstanceData.WaitDuration);
	}

	return EStateTreeRunStatus::Running;
}

void FMassQueueSlotWaitTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);

	if (InstanceData.ClaimedSlot.IsValid())
	{
		const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
		const FMassSmartObjectHandler Handler(MassContext.GetEntityManager(), MassContext.GetEntitySubsystemExecutionContext(), SmartObjectSubsystem, SignalSubsystem);

		if (SmartObjectSubsystem.IsClaimedSmartObjectValid(InstanceData.ClaimedSlot))
		{
			// Add a check before releasing to see if it's the exact interaction handle.
			if (SOUser.InteractionHandle == InstanceData.ClaimedSlot)
			{
				if (SOUser.InteractionStatus == EMassSmartObjectInteractionStatus::InProgress)
				{
					const EMassSmartObjectInteractionStatus NewStatus = Transition.TargetState.IsValid()
						? EMassSmartObjectInteractionStatus::TaskCompleted
						: EMassSmartObjectInteractionStatus::Aborted;

					Handler.StopUsingSmartObject(MassContext.GetEntity(), SOUser, NewStatus);
				}
				else if (SOUser.InteractionStatus != EMassSmartObjectInteractionStatus::Unset)
				{
					SOUser.InteractionHandle.Invalidate();
					SOUser.InteractionStatus = EMassSmartObjectInteractionStatus::Unset;
				}
			}

			if(SmartObjectSubsystem.GetSlotState(InstanceData.ClaimedSlot.SlotHandle) != ESmartObjectSlotState::Free)
			{
				Handler.ReleaseSmartObject(MassContext.GetEntity(), SOUser, InstanceData.ClaimedSlot);
			}
		}
	}
}

EStateTreeRunStatus FMassQueueSlotWaitTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);

	EStateTreeRunStatus Status = EStateTreeRunStatus::Failed;

	// Keep checking the interaction status similar to UseSmartObjectTask
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	switch (SOUser.InteractionStatus)
	{
	case EMassSmartObjectInteractionStatus::InProgress:
		Status = EStateTreeRunStatus::Running;
		break;
	case EMassSmartObjectInteractionStatus::BehaviorCompleted:
		Status = EStateTreeRunStatus::Succeeded;
		break;
	case EMassSmartObjectInteractionStatus::TaskCompleted:
		Status = EStateTreeRunStatus::Failed;
		break;
	case EMassSmartObjectInteractionStatus::Aborted:
		Status = EStateTreeRunStatus::Failed;
		break;
	case EMassSmartObjectInteractionStatus::Unset:
		Status = EStateTreeRunStatus::Failed;
		break;
	default:
		Status = EStateTreeRunStatus::Failed;
	}

	if (Status == EStateTreeRunStatus::Running)
	{
		InstanceData.ElapsedTime += DeltaTime;
		const bool bTimedOut = InstanceData.WaitDuration > 0.f && InstanceData.ElapsedTime >= InstanceData.WaitDuration;

		const bool bClaimStillValid = InstanceData.ClaimedSlot.IsValid() && SmartObjectSubsystem.IsClaimedSmartObjectValid(InstanceData.ClaimedSlot);

		if (!bTimedOut && bClaimStillValid)
		{
			return EStateTreeRunStatus::Running;
		}

		if (bTimedOut)
		{
			InstanceData.FinishReason = EQueueSlotWaitFinishReason::TimedOut;
		}
		else
		{
			InstanceData.FinishReason = EQueueSlotWaitFinishReason::ClaimInvalid;
		}

		UE_LOG(LogTemp, Log, TEXT("QueueSlotWait: Entity %d leaving queue slot %d. Reason=%d TimedOut: %d, ClaimValid: %d."),
			MassContext.GetEntity().SerialNumber, InstanceData.ClaimedSlot.SlotHandle.GetSlotIndex(), static_cast<int32>(InstanceData.FinishReason), bTimedOut, bClaimStillValid);

		if (LeaveSignal != NAME_None)
		{
			SignalSubsystem.SignalEntity(LeaveSignal, MassContext.GetEntity());
		}
		SignalSubsystem.SignalEntity(UE::Mass::Signals::NewStateTreeTaskRequired, MassContext.GetEntity());

		Status = EStateTreeRunStatus::Succeeded;
	}

	return Status;
}
