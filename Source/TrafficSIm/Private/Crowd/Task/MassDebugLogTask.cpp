// Copyright Epic Games, Inc. All Rights Reserved.

#include "Crowd/Task/MassDebugLogTask.h"

#include "MassAIBehaviorTypes.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "EngineUtils.h"

EStateTreeRunStatus FMassDebugLogTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    if (InstanceData.Message.IsEmpty())
    {
        MASSBEHAVIOR_LOG(Log, TEXT("[DebugTask] Message is empty."));
    }
    else
    {
        MASSBEHAVIOR_LOG(Log, TEXT("[DebugTask] %s"), *InstanceData.Message);
    }

    return EStateTreeRunStatus::Succeeded;
}