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

	if (SOUser.InteractionHandle.IsValid())
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


	if (!InstanceData.SearchRequestID.IsSet())
	{
		// 1. 如果实体现在已经合法占据了一个排队槽位，我们只需向前寻找或者等待
		if (SOUser.InteractionHandle.IsValid())
		{
			const int32 PreviousSlotIndex = SOUser.InteractionHandle.SlotHandle.GetSlotIndex() - 1;
			if (PreviousSlotIndex >= 0)
			{
				TArray<FSmartObjectSlotHandle> AllSlots;
				SmartObjectSubsystem.GetAllSlots(SOUser.InteractionHandle.SmartObjectHandle, AllSlots);

				if (AllSlots.IsValidIndex(PreviousSlotIndex) && SmartObjectSubsystem.CanBeClaimed(AllSlots[PreviousSlotIndex]))
				{
					// 找到前一个空闲的Slot，可以排队前进了
					InstanceData.FoundCandidateSlots.NumSlots = 1;
					InstanceData.FoundCandidateSlots.Slots[0] = FSmartObjectCandidateSlot(FSmartObjectRequestResult(SOUser.InteractionHandle.SmartObjectHandle, AllSlots[PreviousSlotIndex]), 0.0f);
					InstanceData.bHasCandidateSlots = true;
				}
			}
			else
			{
				// 我们已经在首位
				InstanceData.FoundCandidateSlots.Reset();
				InstanceData.bHasCandidateSlots = false;
			}

			// 只在队列内部行动时，不需要去发起完全异步的Smart Object扫描
			return EStateTreeRunStatus::Running;
		}

		// 2. 检查是否有组件外部指示要求强制占据某个特定的后续槽位
		if (const FMassQueuedAdvanceSlotFragment* QueuedAdvanceSlot = Context.GetExternalDataPtr(QueuedAdvanceSlotHandle))
		{
			if (QueuedAdvanceSlot->bHasPendingSlot && QueuedAdvanceSlot->PendingSlot.SmartObjectHandle.IsValid())
			{
				if (SmartObjectSubsystem.CanBeClaimed(QueuedAdvanceSlot->PendingSlot.SlotHandle))
				{
					InstanceData.FoundCandidateSlots.NumSlots = 1;
					InstanceData.FoundCandidateSlots.Slots[0] = FSmartObjectCandidateSlot(QueuedAdvanceSlot->PendingSlot, 0.0f);
					InstanceData.bHasCandidateSlots = true;
				}
				// 既然有指定的候选对象或者是等待目标，不再发起全部异步查找
				return EStateTreeRunStatus::Running;
			}
		}

		if (InstanceData.bHasCandidateSlots)
		{
			return EStateTreeRunStatus::Running;
		}

		// 3. 所有情况都不满足时，发起异步排队队尾节点的检索
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
