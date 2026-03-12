// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "MassDebugLogTask.generated.h"

struct FStateTreeTransitionResult;
struct FStateTreeExecutionContext;

USTRUCT()
struct TRAFFICSIM_API FMassDebugLogTaskInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = Input)
    FString Message;
};

USTRUCT(meta = (DisplayName = "Debug Log"))
struct TRAFFICSIM_API FMassDebugLogTask : public FMassStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FMassDebugLogTaskInstanceData;

protected:
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};