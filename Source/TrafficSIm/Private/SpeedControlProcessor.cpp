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
	if (!TrafficLightSubsystem)
	{
		UE_LOG(LogTrafficSim, Error, TEXT("TrafficLightSubsystem is not initialized! Cannot execute VehicleMovementProcessor."));
		return;
	}
}

void USpeedControlProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(EntityManager, Context,
		[this, DeltaTime](FMassExecutionContext& Context)
		{
			const int32 NumEntities = Context.GetNumEntities();

			TArrayView<FMassVehicleMovementFragment> VehicleMovementList =
				Context.GetMutableFragmentView<FMassVehicleMovementFragment>();

			TArrayView<FTransformFragment> TransformList =
				Context.GetMutableFragmentView<FTransformFragment>();

			for (int32 i = 0; i < NumEntities; ++i)
			{
				FMassVehicleMovementFragment& VM = VehicleMovementList[i];

				/*------------------------------------------------------------
				 * 0. 每帧初始化加减速（非常重要，防止残留）
				 *------------------------------------------------------------*/
				float Accelaration = VM.Accelaration;
				float Decelaration = VM.Decelaration;

				/*------------------------------------------------------------
				 * 1. 查找真实前车
				 *------------------------------------------------------------*/
				const FMassVehicleMovementFragment* RealFront = nullptr;
				const bool bIsFirst = TrafficSimSubsystem->FindFrontVehicle(
					VM.LaneLocation.LaneHandle.Index,
					VM.NextLane,
					Context.GetEntity(i),
					RealFront);

				/*------------------------------------------------------------
				 * 2. 判断路口灯态（是否需要虚拟前车）
				 *------------------------------------------------------------*/
				bool bIntersection = false;
				bool bOpen = true;
				TrafficLightSubsystem->QueryLaneOpenState(
					VM.NextLane, bOpen, bIntersection);

				const bool bNeedVirtualFront =
					bIsFirst &&
					bIntersection &&
					!bOpen &&
					VM.LeftDistance > 0.f;
				/*------------------------------------------------------------
				 * 3. 选择 EffectiveFront（真实 or 虚拟）
				 *------------------------------------------------------------*/
				const FMassVehicleMovementFragment* EffectiveFront = RealFront;
				FMassVehicleMovementFragment VirtualFront;

				float DistToRealFront = FLT_MAX;
				if (RealFront)
				{
					FVector FrontRearPos =
						RealFront->LaneLocation.Position -
						RealFront->LaneLocation.Direction * (RealFront->VehicleLength * 0.5f);
					FVector SelfFrontPos =
						VM.LaneLocation.Position +
						VM.LaneLocation.Direction * (VM.VehicleLength * 0.5f);
					DistToRealFront= FVector::DotProduct(VM.LaneLocation.Direction,SelfFrontPos- FrontRearPos);
				}

				const float DistToStopLine = VM.LeftDistance;

				if (bNeedVirtualFront && DistToStopLine < DistToRealFront)
				{
					// 构造虚拟前车（停车线）
					VirtualFront = FMassVehicleMovementFragment();
					VirtualFront.LaneLocation.Position =
						VM.LaneLocation.Position +
						VM.LaneLocation.Direction * DistToStopLine;

					VirtualFront.Speed = 0.f;
					VirtualFront.VehicleLength = 0.f;

					EffectiveFront = &VirtualFront;
				}

				/*------------------------------------------------------------
				 * 4. IDM 速度控制（统一入口）
				 *------------------------------------------------------------*/
				const float v = FMath::Max(0.f, VM.Speed);
				const float v0 = VM.CruiseSpeed < KINDA_SMALL_NUMBER ? 100.f : VM.CruiseSpeed;

				// IDM 参数
				const float aMax = 50.f; //最大加速度
				const float bComfort = 50.f;//舒适减速度
				const float s0 = 300.f;// 最小安全间距
				const float T = 1.6f;	// 期望时间间隔
				const float delta = 4.f;//加速度指数

				float TargetSpeed = v;

				if (EffectiveFront)
				{
					const float vf = FMath::Max(0.f, EffectiveFront->Speed);

					const float dist =
						FVector::Distance(
							VM.LaneLocation.Position,
							EffectiveFront->LaneLocation.Position);

					const float halfLenSum =
						0.5f * (VM.VehicleLength + EffectiveFront->VehicleLength);

					float s = dist - halfLenSum;
					s = FMath::Max(s, 1.f);

					const float dv = v - vf;

					float sStar =
						s0 +
						v * T +
						(v * dv) / (2.f * FMath::Sqrt(aMax * bComfort));

					sStar = FMath::Max(sStar, s0);

					const float accelFree = 1.f - FMath::Pow(v / v0, delta);
					const float accelInt = FMath::Pow(sStar / s, 2.f);

					const float aIDM = aMax * (accelFree - accelInt);

					TargetSpeed = v + aIDM * DeltaTime;
					TargetSpeed = FMath::Clamp(TargetSpeed, 0.f, VM.MaxSpeed);

					if (aIDM >= 0.f)
					{
						Accelaration = aIDM;
					}
					else
					{
						Decelaration = -aIDM;
					}
				}
				else
				{
					// 自由流
					const float aFree =
						aMax * (1.f - FMath::Pow(v / v0, delta));

					TargetSpeed = v + aFree * DeltaTime;
					TargetSpeed = FMath::Clamp(TargetSpeed, 0.f, VM.MaxSpeed);

					Accelaration = aFree;
				}

				VM.TargetSpeed = TargetSpeed;

				// =====================================================
				// Emergency Brake: 绝对不能越线的兜底逻辑
				// =====================================================
				if (bNeedVirtualFront)
				{
					float vs = VM.Speed;
					float d = FMath::Max(VM.LeftDistance, 1.f);
					float bMax = VM.Decelaration;

					float dMinStop = (vs * vs) / (2.f * bMax);

					if (d <= dMinStop + VM.VehicleLength/2.0)
					{
						// 强制最大减速度
						VM.TargetSpeed = 0.f;
						Decelaration = bMax * 5.0;
					}
					//UE_LOG(LogTrafficSim, Warning, TEXT("EntitySN:%d Emergency Brake Applied: DistToStopLine=%.2f, MinStopDist=%.2f Decel:%.2f LeftDist:%f"),
					//	Context.GetEntity(i).SerialNumber, d, dMinStop,Decelaration,d);
				}


				/*------------------------------------------------------------
				 * 5. 速度积分
				 *------------------------------------------------------------*/
				if (VM.TargetSpeed > VM.Speed)
				{
					VM.Speed += Accelaration * DeltaTime;
					VM.Speed = FMath::Min(VM.Speed, VM.TargetSpeed);
				}
				else
				{
					VM.Speed -= Decelaration * DeltaTime;
					VM.Speed = FMath::Max(VM.Speed, VM.TargetSpeed);
				}

				VM.Speed = FMath::Clamp(VM.Speed, 0.f, VM.MaxSpeed);

				/*------------------------------------------------------------
				 * 6. 冻结时间统计
				 *------------------------------------------------------------*/
				if (VM.Speed <= KINDA_SMALL_NUMBER)
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
