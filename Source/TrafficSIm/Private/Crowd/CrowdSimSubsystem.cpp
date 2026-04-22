#include "Crowd/CrowdSimSubsystem.h"

void UCrowdSimSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	QueueSlotRegistry.Reset();
	PendingQueueAdvanceEntities.Reset();
}

void UCrowdSimSubsystem::Deinitialize()
{
	QueueSlotRegistry.Reset();
	PendingQueueAdvanceEntities.Reset();
	Super::Deinitialize();
}

void UCrowdSimSubsystem::RegisterClaimedSlot(const FSmartObjectClaimHandle& ClaimedSlot, const FMassEntityHandle Entity)
{
	if (!ClaimedSlot.IsValid())
	{
		return;
	}

	QueueSlotRegistry.FindOrAdd(MakeSlotKey(ClaimedSlot.SmartObjectHandle, ClaimedSlot.SlotHandle.GetSlotIndex())) = Entity;
}

void UCrowdSimSubsystem::UnregisterClaimedSlot(const FSmartObjectClaimHandle& ClaimedSlot)
{
	if (!ClaimedSlot.IsValid())
	{
		return;
	}

	QueueSlotRegistry.Remove(MakeSlotKey(ClaimedSlot.SmartObjectHandle, ClaimedSlot.SlotHandle.GetSlotIndex()));
}

FMassEntityHandle UCrowdSimSubsystem::FindEntityOnNextSlot(const FSmartObjectClaimHandle& ClaimedSlot) const
{
	if (!ClaimedSlot.IsValid())
	{
		return FMassEntityHandle();
	}

	const FString Key = MakeSlotKey(ClaimedSlot.SmartObjectHandle, ClaimedSlot.SlotHandle.GetSlotIndex() + 1);
	if (const FMassEntityHandle* Found = QueueSlotRegistry.Find(Key))
	{
		return *Found;
	}

	return FMassEntityHandle();
}

void UCrowdSimSubsystem::RequestQueueAdvance(const FMassEntityHandle Entity)
{
	if (Entity.IsSet())
	{
		PendingQueueAdvanceEntities.Add(Entity);
	}
}

bool UCrowdSimSubsystem::ConsumeQueueAdvance(const FMassEntityHandle Entity)
{
	if (!Entity.IsSet())
	{
		return false;
	}

	return PendingQueueAdvanceEntities.Remove(Entity) > 0;
}

FString UCrowdSimSubsystem::MakeSlotKey(const FSmartObjectHandle SmartObjectHandle, const int32 SlotIndex)
{
	return FString::Printf(TEXT("%s:%d"), *LexToString(SmartObjectHandle), SlotIndex);
}
