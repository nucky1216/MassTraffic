// Copyright Epic Games, Inc. All Rights Reserved.

#include "Crowd/Task/MassDebugLogTask.h"

#include "MassAIBehaviorTypes.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FMassDebugLogTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    if (InstanceData.Message.IsEmpty())
    {
        UE_LOG(LogTemp,Log, TEXT("[DebugTask] Message is empty."));
    }
    else
    {
        UE_LOG(LogTemp,Log, TEXT("[DebugTask] %s"), *InstanceData.Message);
    }

    return EStateTreeRunStatus::Succeeded;
}