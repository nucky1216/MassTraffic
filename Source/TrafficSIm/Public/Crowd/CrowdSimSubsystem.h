#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "SmartObjectRuntime.h"
#include "CrowdSimSubsystem.generated.h"

UCLASS()
class TRAFFICSIM_API UCrowdSimSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void RegisterClaimedSlot(const FSmartObjectClaimHandle& ClaimedSlot, const FMassEntityHandle Entity);
	void UnregisterClaimedSlot(const FSmartObjectClaimHandle& ClaimedSlot);
	FMassEntityHandle FindEntityOnNextSlot(const FSmartObjectClaimHandle& ClaimedSlot) const;

	void RequestQueueAdvance(const FMassEntityHandle Entity);
	bool ConsumeQueueAdvance(const FMassEntityHandle Entity);

private:
	static FString MakeSlotKey(const FSmartObjectHandle SmartObjectHandle, const int32 SlotIndex);

	TMap<FString, FMassEntityHandle> QueueSlotRegistry;
	TSet<FMassEntityHandle> PendingQueueAdvanceEntities;
};
