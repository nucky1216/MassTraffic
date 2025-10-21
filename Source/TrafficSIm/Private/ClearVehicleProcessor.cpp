// Fill out your copyright notice in the Description page of Project Settings.


#include "ClearVehicleProcessor.h"
#include "MassVehicleMovementFragment.h"

UClearVehicleProcessor::UClearVehicleProcessor() :EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;
}

void UClearVehicleProcessor::Initialize(UObject& Owner) 
{
	
}

void UClearVehicleProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadOnly);
}

void UClearVehicleProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> EntitiesToDelete;

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [ &EntitiesToDelete](FMassExecutionContext& Context)
		{

			const int32 EntityCount = Context.GetNumEntities();

			const TArrayView<FMassVehicleMovementFragment> VehicleMovementFragments = Context.GetMutableFragmentView<FMassVehicleMovementFragment>();

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
