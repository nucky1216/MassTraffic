// Copyright Epic Games, Inc. All Rights Reserved.

#include "Crowd/Task/MassDebugLogTask.h"

#include "MassAIBehaviorTypes.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FMassDebugLogTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
    if (InstanceData.Message.IsEmpty())
    {
        UE_LOG(LogTemp,Log, TEXT("[DebugTask] Message is empty."));
    }
    else
    {
        UE_LOG(LogTemp,Log, TEXT("[DebugTask SN:%d] %s"), MassContext.GetEntity().SerialNumber, *InstanceData.Message);
    }

    return EStateTreeRunStatus::Running;
}

//EStateTreeRunStatus FMassDebugLogTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
//{
//    return EStateTreeRunStatus::Running;
//}