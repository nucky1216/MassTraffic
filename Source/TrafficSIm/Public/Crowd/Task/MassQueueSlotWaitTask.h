#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassStateTreeTypes.h"
#include "SmartObjectRuntime.h"
#include "TrafficCommonFragments.h"
#include "MassQueueSlotWaitTask.generated.h"

class USmartObjectSubsystem;
class UMassSignalSubsystem;
struct FMassSmartObjectUserFragment;
struct FStateTreeExecutionContext;

UENUM(BlueprintType)
enum class EQueueSlotWaitFinishReason : uint8
{
	NotSet,
	TimedOut,
	ClaimInvalid
};

USTRUCT()
struct TRAFFICSIM_API FMassQueueSlotWaitTaskInstanceData
{
	GENERATED_BODY()

	/** Claimed slot for the current entity in queue (Input). */
	UPROPERTY(EditAnywhere, Category = Input)
	FSmartObjectClaimHandle ClaimedSlot;

	/** Wait time in seconds. <= 0 means wait until slot becomes invalid/released. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float WaitDuration = 0.f;

	UPROPERTY(Transient)
	float ElapsedTime = 0.f;

	UPROPERTY(EditAnywhere, Category = Output)
	EQueueSlotWaitFinishReason FinishReason = EQueueSlotWaitFinishReason::NotSet;
};

USTRUCT(meta = (DisplayName = "Queue Slot Wait"))
struct TRAFFICSIM_API FMassQueueSlotWaitTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMassQueueSlotWaitTaskInstanceData;

	FMassQueueSlotWaitTask()
	{
		bShouldStateChangeOnReselect = false;
	}

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;
	TStateTreeExternalDataHandle<FMassSmartObjectUserFragment> SmartObjectUserHandle;

	/** Signal emitted to self when this wait task decides the entity should leave current queue slot. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FName LeaveSignal = FName(TEXT("QueueLeave"));
};
