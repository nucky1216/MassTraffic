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
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, DeltaTime](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();
			TArrayView<FMassVehicleMovementFragment> VehicleMovementList = Context.GetMutableFragmentView<FMassVehicleMovementFragment>();
			TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();

			for (int32 i = 0; i < NumEntities; ++i)
			{
				FMassVehicleMovementFragment& VM = VehicleMovementList[i];

				const FMassVehicleMovementFragment* FrontVM = nullptr;
				const bool bIsFirst = TrafficSimSubsystem->FindFrontVehicle(
					VM.LaneLocation.LaneHandle.Index, VM.NextLane, Context.GetEntity(i), FrontVM);

				// 随机巡航目标（无前车或距离很充裕时使用）
				const float CruiseTarget = VM.CruiseSpeed;

				if (FrontVM)
				{
					// 前车距离
					const float dist = FVector::Distance(VM.LaneLocation.Position, FrontVM->LaneLocation.Position);
					// 两车半长之和
					const float halfLenSum = 0.5f * (VM.VehicleLength + FrontVM->VehicleLength);

					// 有效车间距 (车尾到车头)
					float s = dist - halfLenSum;
					s = FMath::Max(s, 1.f);  // 避免除零

					// 当前速度 v，前车速度 vf
					const float v = FMath::Max(0.f, VM.Speed);
					const float vf = FMath::Max(0.f, FrontVM->Speed);
					const float dv = v - vf; // 相对速度

					// ==== IDM 参数 ====
					const float v0 = VM.CruiseSpeed; // 期望速度
					const float aMax = 300.f;        // 最大加速度
					const float bComfort = 400.f;    // 舒适减速度
					const float s0 = 300.f;          // 静态距离
					const float T = 1.6f;            // 时间头距
					const float delta = 4.0f;

					// ==== 动态安全距离 s* ====
					float sStar = s0 + v * T + (v * dv) / (2.f * FMath::Sqrt(aMax * bComfort));
					sStar = FMath::Max(sStar, s0);

					// ==== IDM 加速度公式 ====
					float accelFree = 1.f - FMath::Pow(v / v0, delta);
					float accelInt = FMath::Pow(sStar / s, 2.f);

					float aIDM = aMax * (accelFree - accelInt);

					// ==== 将 IDM 加速度转换为目标速度 ====
					float newSpeed = v + aIDM * DeltaTime;
					newSpeed = FMath::Clamp(newSpeed, 0.f, VM.MaxSpeed);

					VM.TargetSpeed = newSpeed;

					// ==== 设置 VM.Deceleration / VM.Acceleration 用于你的积分部分 ====
					if (aIDM < 0.f)
					{
						VM.Decelaration = -aIDM;
					}
					else
					{
						VM.Accelaration = aIDM;
					}
				}
				else
				{
					// 无前车：自由巡航（IDM 自由流方程）
					const float v = VM.Speed;
					const float v0 = VM.CruiseSpeed;
					const float aMax = 300.f;
					const float delta = 4.f;

					float aFree = aMax * (1 - FMath::Pow(v / v0, delta));
					float newSpeed = v + aFree * DeltaTime;
					newSpeed = FMath::Clamp(newSpeed, 0.f, VM.MaxSpeed);

					VM.TargetSpeed = newSpeed;
				}


				// 红绿灯一车之条件处理（保留你的逻辑）
				if (bIsFirst && (VM.LeftDistance < VM.VehicleLength || VM.Speed == 0.f))
				{
					bool bIntersection = false, bOpen = true;
					TrafficLightSubsystem->QueryLaneOpenState(VM.NextLane, bOpen, bIntersection);

					if (bIntersection && VM.LeftDistance < VM.VehicleLength)
					{
						VM.TargetSpeed = bOpen ? CruiseTarget : 0.f;
					}
					else if (VM.Speed == 0.f && !FrontVM)
					{
						VM.TargetSpeed = CruiseTarget;
					}
				}

				// 速度积分（加速/减速）
				if (VM.TargetSpeed > VM.Speed)
				{
					VM.Speed += VM.Accelaration * DeltaTime;
					VM.Speed = FMath::Min(VM.Speed, VM.TargetSpeed);
				}
				else if (VM.TargetSpeed < VM.Speed)
				{
					VM.Speed -= VM.Decelaration * DeltaTime;
					VM.Speed = FMath::Max(VM.Speed, VM.TargetSpeed);
				}

				// Clamp 最终速度
				VM.Speed = FMath::Clamp(VM.Speed, 0.f, VM.MaxSpeed);

				// 冻结时间统计
				if (VM.Speed <= 0.f)
				{
					VM.FreezeTime += DeltaTime;
				}
				else
				{
					VM.FreezeTime = 0.f;
				}
			}
		});
}

