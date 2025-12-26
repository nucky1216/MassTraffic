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


void USpeedControlProcessor::Initialize(UObject& Owner)
{
	TrafficSimSubsystem = UWorld::GetSubsystem<UTrafficSimSubsystem>(GetWorld());
	if (!TrafficSimSubsystem)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("TrafficSimSubsystem is not initialized! Cannot execute VehicleMovementProcessor."));
		return;
	}
	TrafficLightSubsystem = UWorld::GetSubsystem<UTrafficLightSubsystem>(GetWorld());
	if (!TrafficLightSubsystem)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("TrafficLightSubsystem is not initialized! Cannot execute VehicleMovementProcessor."));
		return;
	}
}

void USpeedControlProcessor::ConfigureQueries()
{
    EntityQuery.AddRequirement<FMassVehicleMovementFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void USpeedControlProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UE_LOG(LogTrafficSim, VeryVerbose, TEXT("SpeedControlProcessor::Executing.."));
    const float DeltaTime = Context.GetDeltaTimeSeconds();

    EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, DeltaTime](FMassExecutionContext& Context)
        {
            const int32 NumEntities = Context.GetNumEntities();
            TArrayView<FMassVehicleMovementFragment> VehicleMovementList = Context.GetMutableFragmentView<FMassVehicleMovementFragment>();
            // 获取只读视图，避免与查询权限不匹配产生未定义行为
            TArrayView<const FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();

            for (int32 i = 0; i < NumEntities; ++i)
            {
                FMassVehicleMovementFragment& VehicleMovement = VehicleMovementList[i];

                const FMassVehicleMovementFragment* FrontVehicleMovement = nullptr;
                const bool FirstVehAtLane = TrafficSimSubsystem->FindFrontVehicle(
                    VehicleMovement.LaneLocation.LaneHandle.Index,
                    VehicleMovement.NextLane,
                    Context.GetEntity(i),
                    FrontVehicleMovement);

                const float NewTargetSpeed = VehicleMovement.CruiseSpeed;

                if (FrontVehicleMovement)
                {
                    const float DistanceToFrontVehicle = FVector::Distance(
                        VehicleMovement.LaneLocation.Position,
                        FrontVehicleMovement->LaneLocation.Position);

                    const float HalfLength = (VehicleMovement.VehicleLength + FrontVehicleMovement->VehicleLength) * 0.5f;

                    if (DistanceToFrontVehicle < HalfLength * 2.5f)
                    {
                        if (VehicleMovement.Speed > FrontVehicleMovement->Speed || VehicleMovement.TargetSpeed > FrontVehicleMovement->Speed)
                        {
                            VehicleMovement.TargetSpeed = FrontVehicleMovement->Speed;
                        }
                        if (DistanceToFrontVehicle < HalfLength * 1.5f)
                        {
                            VehicleMovement.TargetSpeed = 0.0f;
                        }
                        UE_LOG(LogTrafficSim, VeryVerbose, TEXT("===Deccelaraiton--> VehicleSN:%d, DistanceToFrontVehicle:%f, HalfLength:%f, FrontVehSN:%d, FrontVehSpeed:%f TargetSpeed:%.2f"),
                            Context.GetEntity(i).SerialNumber,
                            DistanceToFrontVehicle, HalfLength,
                            FrontVehicleMovement->VehicleHandle.SerialNumber,
                            FrontVehicleMovement->Speed, VehicleMovement.TargetSpeed);
                    }
                    else if (DistanceToFrontVehicle > HalfLength * 1.2f)
                    {
                        VehicleMovement.TargetSpeed = NewTargetSpeed;
                        UE_LOG(LogTrafficSim, VeryVerbose, TEXT("===Accelaraiton--> VehicleSN:%d, DistanceToFrontVehicle:%f, HalfLength:%f, FrontVehSN:%d, FrontVehSpeed:%f,TargetSpeed:%.2f"),
                            Context.GetEntity(i).SerialNumber,
                            DistanceToFrontVehicle, HalfLength,
                            FrontVehicleMovement->VehicleHandle.SerialNumber,
                            FrontVehicleMovement->Speed, VehicleMovement.TargetSpeed);
                    }
                }

                if (FirstVehAtLane && (VehicleMovement.LeftDistance < VehicleMovement.VehicleLength || VehicleMovement.Speed <= KINDA_SMALL_NUMBER))
                {
                    bool IntersectionLane = false, OpenLane = true;

                    TrafficLightSubsystem->QueryLaneOpenState(VehicleMovement.NextLane, OpenLane, IntersectionLane);

                    if (IntersectionLane && VehicleMovement.LeftDistance < VehicleMovement.VehicleLength)
                    {
                        VehicleMovement.TargetSpeed = OpenLane ? NewTargetSpeed : 0.0f;
                        UE_LOG(LogTrafficSim, VeryVerbose, TEXT("+++Intersection Stop+++ VehicleSN:%d, LeftDistance:%f, OpenLane:%d, TargetSpeed:%f"),
                            Context.GetEntity(i).SerialNumber, VehicleMovement.LeftDistance, OpenLane, VehicleMovement.TargetSpeed);
                    }
                    else if (VehicleMovement.Speed <= KINDA_SMALL_NUMBER && !FrontVehicleMovement)
                    {
                        VehicleMovement.TargetSpeed = NewTargetSpeed;
                        UE_LOG(LogTrafficSim, VeryVerbose, TEXT("+++Start Moving+++ VehicleSN:%d, LeftDistance:%f, OpenLane:%d, TargetSpeed:%f"),
                            Context.GetEntity(i).SerialNumber, VehicleMovement.LeftDistance, OpenLane, VehicleMovement.TargetSpeed);
                    }
                }

                if (VehicleMovement.TargetSpeed > VehicleMovement.Speed)
                {
                    VehicleMovement.Speed += VehicleMovement.Accelaration * DeltaTime;
                }
                else if (VehicleMovement.TargetSpeed < VehicleMovement.Speed)
                {
                    VehicleMovement.Speed -= VehicleMovement.Decelaration * DeltaTime;
                }

                // 去掉无效的 Clamp 调用或改为赋值
                VehicleMovement.Speed = FMath::Clamp(VehicleMovement.Speed, 0.0f, VehicleMovement.MaxSpeed);

                UE_LOG(LogTrafficSim, VeryVerbose, TEXT("1.VehicleSN:%d, Speed:%f,Decelaration:%f,DeltaDece:%f TargetSpeed:%f bFront:%d,bFirst:%d, DeltaTime:%f"),
                    Context.GetEntity(i).SerialNumber,
                    VehicleMovement.Speed, VehicleMovement.Decelaration,
                    VehicleMovement.Decelaration * DeltaTime,
                    VehicleMovement.TargetSpeed, FrontVehicleMovement != nullptr,
                    FirstVehAtLane, DeltaTime);

                if (VehicleMovement.Speed <= KINDA_SMALL_NUMBER)
                {
                    VehicleMovement.FreezeTime += DeltaTime;
                }
                else
                {
                    VehicleMovement.FreezeTime = 0.0f;
                }
            }
        });
}