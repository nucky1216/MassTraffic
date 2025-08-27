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
	TrafficLightSubsystem = UWorld::GetSubsystem<UTrafficLightSubsystem>(GetWorld());
	if(!TrafficLightSubsystem)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("TrafficLightSubsystem is not initialized! Cannot execute VehicleMovementProcessor."));
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
				bool front=TrafficSimSubsystem->FindFrontVehicle(VehicleMovement.LaneLocation.LaneHandle.Index,
					VehicleMovement.NextLane, Context.GetEntity(i), FrontVehicleMovement);
				

				if (front && FrontVehicleMovement)//设置目标速度为前车速度
				{
					float DistanceToFrontVehicle = FVector::Distance(VehicleMovement.LaneLocation.Position, FrontVehicleMovement->LaneLocation.Position);
					if (FrontVehicleMovement->Speed < VehicleMovement.Speed && DistanceToFrontVehicle < VehicleMovement.MinGap * 2.0)
					{
						VehicleMovement.TargetSpeed = FrontVehicleMovement->Speed;
					}
					else
					{
						VehicleMovement.TargetSpeed = VehicleMovement.MaxSpeed;
					}
				
				}

				//如果距离红绿灯小于500米，并且下一个车道是红灯，则停车
				if (VehicleMovement.LeftDistance < VehicleMovement.CrossStopDistance)
				{
					bool IntersectionLane = false,OpenLane=true;

					TrafficLightSubsystem->QueryLaneOpenState(VehicleMovement.NextLane, OpenLane, IntersectionLane);

					if(IntersectionLane)
					{
						VehicleMovement.TargetSpeed = OpenLane?VehicleMovement.MaxSpeed:0.f;
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
				VehicleMovement.Speed= FMath::Clamp(VehicleMovement.Speed,0, VehicleMovement.MaxSpeed);
			}
		});
}

