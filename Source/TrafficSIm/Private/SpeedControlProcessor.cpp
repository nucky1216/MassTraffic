// Fill out your copyright notice in the Description page of Project Settings.


#include "SpeedControlProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"

USpeedControlProcessor::USpeedControlProcessor() :EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = FName(TEXT("TrafficSim"));
	ExecutionOrder.ExecuteAfter.Add(FName(TEXT("CollectLaneVehiclesProcessor")));
	ExecutionOrder.ExecuteBefore.Add(FName(TEXT("VehicleMovementProcessor")));
	bAutoRegisterWithProcessingPhases = true;
}

void USpeedControlProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void USpeedControlProcessor::Initialize(UObject& Owner)
{
	TrafficSimSubsystem = UWorld::GetSubsystem<UTrafficSimSubsystem>(GetWorld());
	if (!TrafficSimSubsystem)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("TrafficSimSubsystem is not initialized! Cannot execute VehicleMovementProcessor."));
		return;
	}
}

void USpeedControlProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UE_LOG(LogTrafficSim, VeryVerbose, TEXT("SpeedControlProcessor::Executing.."));
	float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, DeltaTime](FMassExecutionContext& Context)
		{
			int32 NumEntities = Context.GetNumEntities();
			TArrayView<FMassVehicleMovementFragment> VehicleMovementList = Context.GetMutableFragmentView<FMassVehicleMovementFragment>();
			TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
			for(int32 i = 0; i < NumEntities; ++i)
			{
				FMassVehicleMovementFragment& VehicleMovement = VehicleMovementList[i];
				//FTransformFragment				

				const FMassVehicleMovementFragment* FrontVehicleMovement = nullptr;
				bool front=TrafficSimSubsystem->FindFrontVehicle(VehicleMovement.LaneLocation.LaneHandle.Index, Context.GetEntity(i), FrontVehicleMovement);
				
				//减速至前车速度
				if (front && FrontVehicleMovement)
				{
					float DistanceToFrontVehicle = FVector::Distance(VehicleMovement.LaneLocation.Position, FrontVehicleMovement->LaneLocation.Position);
					if (FrontVehicleMovement->Speed < VehicleMovement.Speed && DistanceToFrontVehicle < VehicleMovement.MinGap * 2.0)
					{
						VehicleMovement.TargetSpeed = FrontVehicleMovement->Speed;
					}
				
				}
				//加速至目标速度
				if (VehicleMovement.TargetSpeed > VehicleMovement.Speed)
				{
					VehicleMovement.Speed += VehicleMovement.Accelaration * DeltaTime;
				}
				//减速至目标速度
				else if (VehicleMovement.TargetSpeed < VehicleMovement.Speed)
				{
					VehicleMovement.Speed -= VehicleMovement.Decelaration * DeltaTime;
				}
			}
		});
}

