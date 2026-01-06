// Fill out your copyright notice in the Description page of Project Settings.


#include "VehDeletorProcessor.h"

UVehDeletorProcessor::UVehDeletorProcessor():EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = FName(TEXT("TrafficSim"));
	ExecutionOrder.ExecuteAfter.Add("VehicleMovementProcessor");
	bAutoRegisterWithProcessingPhases = true;
}

void UVehDeletorProcessor::Initialize(UObject& Owner)
{
	TrafficSimSubsystem = UWorld::GetSubsystem<UTrafficSimSubsystem>(GetWorld());
	if (!TrafficSimSubsystem)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("UVehDeletorProcessor::TrafficSimSubsystem is not initialized! Cannot execute VehicleMovementProcessor."));
		return;
	}
}

void UVehDeletorProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FVehicleDeleteTag>(EMassFragmentPresence::All);
}

void UVehDeletorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<FMassEntityHandle> EntitiesToDelete;
	EntitiesToDelete.Reserve(50); // 预留一些容量，减少重复分配

	TArray<FName> VehIDsToDelete;

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&EntitiesToDelete,&VehIDsToDelete](FMassExecutionContext& Context)
		{
			TArrayView<FMassVehicleMovementFragment> VehFrags = Context.GetMutableFragmentView<FMassVehicleMovementFragment>();
			const int32 EntityCount = Context.GetNumEntities();
			for (int32 i = 0; i < EntityCount; ++i)
			{
				EntitiesToDelete.Add(Context.GetEntity(i));

				if (!VehFrags[i].VehID.IsNone())
				{
					VehIDsToDelete.Add(VehFrags[i].VehID);
				}
			}
		});

	if (EntitiesToDelete.Num() > 0)
	{
		// 通过延迟命令批量删除
		EntityManager.Defer().DestroyEntities(EntitiesToDelete);
	}
	if(VehIDsToDelete.Num()>0)
	{
		TrafficSimSubsystem->BroadcastEntityDestoryEvent(VehIDsToDelete);
	}
}
