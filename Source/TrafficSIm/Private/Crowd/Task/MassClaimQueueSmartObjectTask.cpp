#include "Crowd/Task/MassClaimQueueSmartObjectTask.h"

#include "Crowd/CrowdSimSubsystem.h"
#include "MassAIBehaviorTypes.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectFragments.h"
#include "MassSmartObjectHandler.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FMassClaimQueueSmartObjectTask::FMassClaimQueueSmartObjectTask()
{
	bShouldStateChangeOnReselect = false;
}

bool FMassClaimQueueSmartObjectTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectUserHandle);
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	Linker.LinkExternalData(MassSignalSubsystemHandle);

	return true;
}

EStateTreeRunStatus FMassClaimQueueSmartObjectTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
	UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const FMassSmartObjectCandidateSlots* CandidateSlots = InstanceData.CandidateSlots.GetPtr<FMassSmartObjectCandidateSlots>();
	if (CandidateSlots == nullptr)
	{
		MASSBEHAVIOR_LOG(Log, TEXT("Candidate slots not set"));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ClaimedSlot.Invalidate();

	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FMassSmartObjectHandler MassSmartObjectHandler(MassContext.GetEntityManager(), MassContext.GetEntitySubsystemExecutionContext(), SmartObjectSubsystem, SignalSubsystem);

	// 如果实体身上已经有正在交互的 Slot，先将其停止并释放，以防在 ClaimCandidate 中触发 assert 奔溃
	if (SOUser.InteractionHandle.IsValid())
	{
		FSmartObjectClaimHandle OldHandle = SOUser.InteractionHandle;

		// 如果还在交互中，需要先停止交互
		if (SOUser.InteractionStatus == EMassSmartObjectInteractionStatus::InProgress)
		{
			MassSmartObjectHandler.StopUsingSmartObject(MassContext.GetEntity(), SOUser, EMassSmartObjectInteractionStatus::Aborted);
		}
		else
		{
			// 手动重置状态
			SOUser.InteractionHandle.Invalidate();
			SOUser.InteractionStatus = EMassSmartObjectInteractionStatus::Unset;
		}

		// 从我们的队列 Subsystem 中注销
		if (UCrowdSimSubsystem* CrowdSimSubsystem = Context.GetWorld()->GetSubsystem<UCrowdSimSubsystem>())
		{
			CrowdSimSubsystem->UnregisterClaimedSlot(OldHandle);
		}

		// 释放 SmartObject 槽位
		MassSmartObjectHandler.ReleaseSmartObject(MassContext.GetEntity(), SOUser, OldHandle);
	}

	InstanceData.ClaimedSlot = MassSmartObjectHandler.ClaimCandidate(MassContext.GetEntity(), SOUser, *CandidateSlots);

	SOUser.InteractionCooldownEndTime = Context.GetWorld()->GetTimeSeconds() + InteractionCooldown;

	if (!InstanceData.ClaimedSlot.IsValid())
	{
		MASSBEHAVIOR_LOG(Log, TEXT("Failed to claim smart object slot from %d candidates"), CandidateSlots->NumSlots);
		return EStateTreeRunStatus::Failed;
	}

	if (UCrowdSimSubsystem* CrowdSimSubsystem = Context.GetWorld()->GetSubsystem<UCrowdSimSubsystem>())
	{
		CrowdSimSubsystem->RegisterClaimedSlot(InstanceData.ClaimedSlot, MassContext.GetEntity());
	}

	return EStateTreeRunStatus::Running;
}

void FMassClaimQueueSmartObjectTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	SOUser.InteractionCooldownEndTime = Context.GetWorld()->GetTimeSeconds() + InteractionCooldown;

	if (UCrowdSimSubsystem* CrowdSimSubsystem = Context.GetWorld()->GetSubsystem<UCrowdSimSubsystem>())
	{
		CrowdSimSubsystem->UnregisterClaimedSlot(InstanceData.ClaimedSlot);
	}

	if (InstanceData.ClaimedSlot.IsValid())
	{
		const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
		const FMassSmartObjectHandler MassSmartObjectHandler(MassContext.GetEntityManager(), MassContext.GetEntitySubsystemExecutionContext(), SmartObjectSubsystem, SignalSubsystem);

		if (SmartObjectSubsystem.IsClaimedSmartObjectValid(InstanceData.ClaimedSlot) && SmartObjectSubsystem.GetSlotState(InstanceData.ClaimedSlot.SlotHandle) != ESmartObjectSlotState::Free)
		{
			if (SOUser.InteractionHandle == InstanceData.ClaimedSlot && SOUser.InteractionStatus == EMassSmartObjectInteractionStatus::InProgress)
			{
				MassSmartObjectHandler.StopUsingSmartObject(MassContext.GetEntity(), SOUser, EMassSmartObjectInteractionStatus::Aborted);
			}
			MassSmartObjectHandler.ReleaseSmartObject(MassContext.GetEntity(), SOUser, InstanceData.ClaimedSlot);
		}
	}
	else
	{
		MASSBEHAVIOR_LOG(VeryVerbose, TEXT("Exiting state with an invalid ClaimHandle: nothing to do."));
	}
}

EStateTreeRunStatus FMassClaimQueueSmartObjectTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);

	SOUser.InteractionCooldownEndTime = Context.GetWorld()->GetTimeSeconds() + InteractionCooldown;

	if (InstanceData.ClaimedSlot.IsValid())
	{
		const USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		if (!SmartObjectSubsystem.IsClaimedSmartObjectValid(InstanceData.ClaimedSlot))
		{
			if (UCrowdSimSubsystem* CrowdSimSubsystem = Context.GetWorld()->GetSubsystem<UCrowdSimSubsystem>())
			{
				CrowdSimSubsystem->UnregisterClaimedSlot(InstanceData.ClaimedSlot);
			}

			InstanceData.ClaimedSlot.Invalidate();
		}
	}

	return InstanceData.ClaimedSlot.IsValid() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}
