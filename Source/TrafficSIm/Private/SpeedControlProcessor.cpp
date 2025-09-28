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
				bool HasFrontCar=TrafficSimSubsystem->FindFrontVehicle(VehicleMovement.LaneLocation.LaneHandle.Index,
					VehicleMovement.NextLane, Context.GetEntity(i), FrontVehicleMovement);
				

				if (FrontVehicleMovement)//����Ŀ���ٶ�Ϊǰ���ٶ�
				{
					float DistanceToFrontVehicle = FVector::Distance(VehicleMovement.LaneLocation.Position, FrontVehicleMovement->LaneLocation.Position);
					float HalfLength = (VehicleMovement.VehicleLength + FrontVehicleMovement->VehicleLength) * 0.5f;
					if (DistanceToFrontVehicle < HalfLength * 2.0)
					{
						if(VehicleMovement.Speed> FrontVehicleMovement->Speed)
							VehicleMovement.TargetSpeed = FrontVehicleMovement->Speed;
						//����������
						if(DistanceToFrontVehicle < HalfLength * 1.5)
							VehicleMovement.TargetSpeed = VehicleMovement.Speed-100.f;
					}
					else
					{
						VehicleMovement.TargetSpeed = VehicleMovement.MaxSpeed;
					}
				
				}

				//��һ�������������̵�С��500�� ��������һ�������Ǻ�ƣ���ͣ�� ���ߵ�ǰ�ٶ�Ϊ0����һ����λ�̵ƣ�����
				if (!HasFrontCar && (VehicleMovement.LeftDistance < VehicleMovement.VehicleLength || VehicleMovement.Speed==0))
				{
					bool IntersectionLane = false,OpenLane=true;

					TrafficLightSubsystem->QueryLaneOpenState(VehicleMovement.NextLane, OpenLane, IntersectionLane);

					if(IntersectionLane)
					{
						VehicleMovement.TargetSpeed = OpenLane?VehicleMovement.MaxSpeed:0.f;
					}
				}

				//������Ŀ���ٶ�
				if (VehicleMovement.TargetSpeed > VehicleMovement.Speed)
				{
					VehicleMovement.Speed += VehicleMovement.Accelaration * DeltaTime;
				}
				//������Ŀ���ٶ�
				else if (VehicleMovement.TargetSpeed < VehicleMovement.Speed)
				{
					VehicleMovement.Speed -= VehicleMovement.Decelaration * DeltaTime;
				}
				VehicleMovement.Speed= FMath::Clamp(VehicleMovement.Speed,0, VehicleMovement.MaxSpeed);
			}
		});
}

