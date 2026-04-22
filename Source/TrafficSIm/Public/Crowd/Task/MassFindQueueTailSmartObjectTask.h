#pragma once

#include "MassSmartObjectRequest.h"
#include "MassStateTreeTypes.h"
#include "TrafficCommonFragments.h"
#include "MassFindQueueTailSmartObjectTask.generated.h"

struct FMassSmartObjectUserFragment;
struct FMassZoneGraphLaneLocationFragment;
struct FTransformFragment;
class USmartObjectSubsystem;
class UMassSignalSubsystem;
struct FStateTreeExecutionContext;

USTRUCT()
struct TRAFFICSIM_API FMassFindQueueTailSmartObjectTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Output)
	FMassSmartObjectCandidateSlots FoundCandidateSlots;

	UPROPERTY(VisibleAnywhere, Category = Output)
	bool bHasCandidateSlots = false;

	UPROPERTY()
	FMassSmartObjectRequestID SearchRequestID;

	UPROPERTY()
	double NextUpdate = 0.;

	UPROPERTY()
	FZoneGraphLaneHandle LastLane;
};

USTRUCT(meta = (DisplayName = "Find Queue Tail Smart Object"))
struct TRAFFICSIM_API FMassFindQueueTailSmartObjectTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMassFindQueueTailSmartObjectTaskInstanceData;

	FMassFindQueueTailSmartObjectTask();

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void StateCompleted(FStateTreeExecutionContext& Context, const EStateTreeRunStatus CompletionStatus, const FStateTreeActiveStates& CompletedActiveStates) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;
	TStateTreeExternalDataHandle<FTransformFragment> EntityTransformHandle;
	TStateTreeExternalDataHandle<FMassSmartObjectUserFragment> SmartObjectUserHandle;
	TStateTreeExternalDataHandle<FMassZoneGraphLaneLocationFragment, EStateTreeExternalDataRequirement::Optional> LocationHandle;
	TStateTreeExternalDataHandle<FMassQueuedAdvanceSlotFragment, EStateTreeExternalDataRequirement::Optional> QueuedAdvanceSlotHandle;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagQuery ActivityRequirements;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float SearchInterval = 10.0f;
};
