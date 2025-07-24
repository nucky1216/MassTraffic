// Fill out your copyright notice in the Description page of Project Settings.


#include "VehicleParamsInitProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "EngineUtils.h"
#include "MassVehicleMovementFragment.h"
#include "ZoneGraphQuery.h"

UVehicleParamsInitProcessor::UVehicleParamsInitProcessor():EntityQuery(*this)
{
	ObservedType = FMassVehicleMovementFragment::StaticStruct();
	Operation = EMassObservedOperation::Add;
}

void UVehicleParamsInitProcessor::Initialize(UObject& Owner)
{

}

void UVehicleParamsInitProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
}

void UVehicleParamsInitProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UE_LOG(LogTemp,Log,TEXT("VehicleInitProcessor.."));


	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
	{
	    const int32 EntityCount = Context.GetNumEntities();

		const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FMassVehicleMovementFragment> VehicleMovementList = Context.GetMutableFragmentView<FMassVehicleMovementFragment>();

		UTrafficSimSubsystem* TrafficSimSubsystem = UWorld::GetSubsystem<UTrafficSimSubsystem>(Context.GetWorld());
		if (!TrafficSimSubsystem)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to find TrafficSimSubsystem!"));
		}

		for (int32 EntityIndex = 0; EntityIndex < EntityCount; ++EntityIndex)
		{
			FMassVehicleMovementFragment& VehicleMovement = VehicleMovementList[EntityIndex];
			FVector SpawnLocation = TransformList[EntityIndex].GetTransform().GetLocation();

			FZoneGraphLaneLocation LaneLocation;


			FBox QueryBox = FBox::BuildAABB(SpawnLocation, VehicleMovement.QueryExtent);

			TrafficSimSubsystem->FindEntityLaneByQuery(QueryBox, VehicleMovement.LaneFilter, LaneLocation);

			VehicleMovement.LaneLocation = LaneLocation;
			
			//UE_LOG(LogTemp, Log, TEXT("Found Lane: %s"), *LaneLocation.LaneHandle.ToString());

			VehicleMovement.NextLane = TrafficSimSubsystem->ChooseNextLane(LaneLocation);

		}
	});
}
