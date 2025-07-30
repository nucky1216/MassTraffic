// Fill out your copyright notice in the Description page of Project Settings.


#include "VehicleMovementProcessor.h"
#include "MassVehicleMovementFragment.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"

UVehicleMovementProcessor::UVehicleMovementProcessor():EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add("VehicleParamsInitProcessor");
	bAutoRegisterWithProcessingPhases = true;
}

void UVehicleMovementProcessor::Initialize(UObject& Owner)
{
}

void UVehicleMovementProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);;
}

void UVehicleMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		TConstArrayView<FMassVehicleMovementFragment> VehicleMovementFragments = Context.GetFragmentView<FMassVehicleMovementFragment>();
		const TArrayView<FTransformFragment> TransformFragments = Context.GetMutableFragmentView<FTransformFragment>();

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			FMassVehicleMovementFragment& VehicleMovementFragment = VehicleMovementFragments[EntityIndex];
			FTransformFragment& TransformFragment = TransformFragments[EntityIndex];
			// Update the transform based on the vehicle movement fragment
			TransformFragment.Transform.SetLocation(VehicleMovementFragment.CurrentLocation);
			TransformFragment.Transform.SetRotation(VehicleMovementFragment.CurrentRotation.Quaternion());
		}
		});
}
