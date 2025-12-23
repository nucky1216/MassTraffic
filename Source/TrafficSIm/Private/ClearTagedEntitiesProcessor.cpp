// Fill out your copyright notice in the Description page of Project Settings.


#include "ClearTagedEntitiesProcessor.h"
#include "MassVehicleMovementFragment.h"
#include "TrafficTypes.h"

UClearTagedEntitiesProcessor::UClearTagedEntitiesProcessor() :EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;
}

void UClearTagedEntitiesProcessor::Initialize(UObject& Owner)
{
	TrafficSimSubsystem = GetWorld()->GetSubsystem<UTrafficSimSubsystem>();
	if(!TrafficSimSubsystem)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("TrafficSimSubsystem is not initialized! Cannot execute ClearTagedEntitiesProcessor."));
		return;
	}
}

void UClearTagedEntitiesProcessor::ConfigureQueries()
{
	EntityQuery.AddTagRequirement<FMassGlobalDespawnTag>(EMassFragmentPresence::All);
}

void UClearTagedEntitiesProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> EntitiesToDelete;

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [ &EntitiesToDelete](FMassExecutionContext& Context)
		{

			const int32 EntityCount = Context.GetNumEntities();
			for (int32 i = 0; i < EntityCount; ++i)
			{
				EntitiesToDelete.Add(Context.GetEntity(i));
				
			}
		});

	if (EntitiesToDelete.Num() > 0)
	{
		EntityManager.Defer().DestroyEntities(EntitiesToDelete);  // 异步批量删除实体
	}
}
