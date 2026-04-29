#pragma once

#include "MassSmartObjectRequest.h"
#include "MassStateTreeTypes.h"
#include "MassClaimQueueSmartObjectTask.generated.h"

struct FStateTreeExecutionContext;
struct FMassSmartObjectUserFragment;
class USmartObjectSubsystem;
class UMassSignalSubsystem;

USTRUCT()
struct TRAFFICSIM_API FMassClaimQueueSmartObjectTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Input, meta = (BaseStruct = "/Script/MassSmartObjects.MassSmartObjectCandidateSlots"))
	FStateTreeStructRef CandidateSlots;

	UPROPERTY(VisibleAnywhere, Category = Output)
	FSmartObjectClaimHandle ClaimedSlot;
};

USTRUCT(meta = (DisplayName = "Claim Queue SmartObject"))
struct TRAFFICSIM_API FMassClaimQueueSmartObjectTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMassClaimQueueSmartObjectTaskInstanceData;

	FMassClaimQueueSmartObjectTask();

protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	TStateTreeExternalDataHandle<FMassSmartObjectUserFragment> SmartObjectUserHandle;
	TStateTreeExternalDataHandle<USmartObjectSubsystem> SmartObjectSubsystemHandle;
	TStateTreeExternalDataHandle<UMassSignalSubsystem> MassSignalSubsystemHandle;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float InteractionCooldown = 0.f;
};
