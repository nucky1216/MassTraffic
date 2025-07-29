// Fill out your copyright notice in the Description page of Project Settings.


#include "VehicleDeletorProcessor.h"
#include "MassVehicleMovementFragment.h"
#include "MassExecutionContext.h"

UVehicleDeletorProcessor::UVehicleDeletorProcessor():EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UVehicleDeletorProcessor::Initialize(UObject& Owner)
{
}

void UVehicleDeletorProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadOnly);
}

void UVehicleDeletorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const int32 TargetLane = LaneToDelete;

	TArray<FMassEntityHandle> EntitiesToDelete;

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [TargetLane, &EntitiesToDelete](FMassExecutionContext& Context)
	{

		const int32 EntityCount = Context.GetNumEntities();

		const TArrayView<FMassVehicleMovementFragment> VehicleMovementFragments = Context.GetMutableFragmentView<FMassVehicleMovementFragment>();

		for (int32 i = 0; i < EntityCount; ++i)
		{
			if(VehicleMovementFragments[i].LaneLocation.LaneHandle.Index == TargetLane)
			{
				EntitiesToDelete.Add(Context.GetEntity(i));
			}
		}
		});

	if (EntitiesToDelete.Num() > 0)
	{
		EntityManager.Defer().DestroyEntities(EntitiesToDelete);  // 异步批量删除实体
	}
}
