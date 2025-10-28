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
				bool HasFrontCarAtCurLane=TrafficSimSubsystem->FindFrontVehicle(VehicleMovement.LaneLocation.LaneHandle.Index,
					VehicleMovement.NextLane, Context.GetEntity(i), FrontVehicleMovement);
				

				if (FrontVehicleMovement)//设置目标速度为前车速度
				{
					float DistanceToFrontVehicle = FVector::Distance(VehicleMovement.LaneLocation.Position, FrontVehicleMovement->LaneLocation.Position);
					float HalfLength = (VehicleMovement.VehicleLength + FrontVehicleMovement->VehicleLength) * 0.5f;
					//距离小于 车长+速度*5秒的距离
					
					if (DistanceToFrontVehicle < HalfLength*2.5 + VehicleMovement.Speed*500.0/18.0*5.0)
					{
						if(VehicleMovement.Speed> FrontVehicleMovement->Speed)
							VehicleMovement.TargetSpeed = FrontVehicleMovement->Speed;
						//跟车过近了
						if(DistanceToFrontVehicle < HalfLength * 3.0)
							VehicleMovement.TargetSpeed = 0;
					}
					else
					{
						float NewTargetSpeed = FMath::RandRange(VehicleMovement.MinSpeed, VehicleMovement.MaxSpeed);
						VehicleMovement.TargetSpeed = NewTargetSpeed;
					}
				}

				//避让合并车道的车辆
				//const FMassVehicleMovementFragment* AheadVehicle = nullptr;
				//if (TrafficSimSubsystem->WaitForMergeVehilce(&VehicleMovement, AheadVehicle))
				//{
				//	//if (AheadVehicle)
				//	//{
				//	//	float AheadGap = AheadVehicle->VehicleLength / 2 + AheadVehicle->LeftDistance + VehicleMovement.VehicleLength / 2;
				//	//	if(AheadGap< VehicleMovement.LeftDistance)
				//	//		
				//	//}
				//	VehicleMovement.TargetSpeed = 0;
				//}

				//第一辆车如果距离红绿灯小于500米 ，并且下一个车道是红灯，则停车 或者当前速度为0且下一车道位绿灯，则起步
				if (!HasFrontCarAtCurLane && (VehicleMovement.LeftDistance < VehicleMovement.VehicleLength || VehicleMovement.Speed==0))
				{
					bool IntersectionLane = false,OpenLane=true;

					TrafficLightSubsystem->QueryLaneOpenState(VehicleMovement.NextLane, OpenLane, IntersectionLane);

					if(IntersectionLane)
					{
						float NewTargetSpeed = FMath::RandRange(VehicleMovement.MinSpeed, VehicleMovement.MaxSpeed);
						VehicleMovement.TargetSpeed = OpenLane? NewTargetSpeed :0.f;
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

				if(VehicleMovement.Speed<=0.0f)
				{
					VehicleMovement.FreezeTime+=Context.GetDeltaTimeSeconds();
				}
				else
				{
					VehicleMovement.FreezeTime = 0.0f;
				}
			}
		});
}

