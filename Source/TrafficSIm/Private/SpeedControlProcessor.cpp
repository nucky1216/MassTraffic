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
					// 当前两车中心距离
					const float dist = FVector::Distance(VM.LaneLocation.Position, FrontVM->LaneLocation.Position);
					// 两车半长和安全缓冲
					const float halfLenSum = 0.5f * (VM.VehicleLength + FrontVM->VehicleLength);
					const float SafetyBuffer = 5.f; // 安全缓冲（单位与距离一致，可按需要调优）
					// 有效可用间距（前车尾到我车头之间）
					float gapEff = dist - halfLenSum - SafetyBuffer;

					// 若 gapEff 小于 0，立即设为强制停车场景
					if (gapEff <= 0.f)
					{
						VM.TargetSpeed = 0.f;
						VM.Decelaration = FMath::Max(VM.Decelaration, 3000.f); // 强制高减速度以快速停车
					}
					else
					{
						// 当前速度
						const float v = FMath::Max(0.f, VM.Speed);

						// 期望跟驰速度（对齐前车速度）
						const float FollowTarget = FrontVM->Speed;

						// 计算为避免碰撞所需的最小减速度：a_req = v^2 / (2 * gapEff)
						// 如果 v 很小，则无需强减速
						float a_req = (v > 1e-3f) ? (v * v) / (2.f * gapEff) : 0.f;

						// 限幅减速度
						const float aMin = 50.f;   // 最小可用减速度（防止过小，单位与项目一致）
						const float aMax = 8000.f;  // 最大允许减速度
						a_req = FMath::Clamp(a_req, aMin, aMax);

						// 距离是否过近：额外触发“目标速度=0”的策略
						const bool bTooClose = (dist < halfLenSum * 1.5f);

						// 如果距离不足以安全跟驰（或过近），收敛到停车（或至少不快于前车）
						if (bTooClose)
						{
							VM.TargetSpeed = 0.f;
						}
						else
						{
							// 不允许比前车快（避免追尾）
							VM.TargetSpeed = FMath::Min(FollowTarget, VM.TargetSpeed);
							// 若目前比前车快，也收敛至前车速度
							if (VM.Speed > FollowTarget)
							{
								VM.TargetSpeed = FollowTarget;
							}
						}

						// 更新使用的减速度：至少满足 a_req
						VM.Decelaration = FMath::Max(VM.Decelaration, a_req);

						// 若距离很充裕（远大于安全距离），允许恢复到巡航目标
						const bool bFarEnough = (gapEff > halfLenSum * 2.0f);
						if (bFarEnough && VM.TargetSpeed <= FollowTarget)
						{
							VM.TargetSpeed = CruiseTarget;
						}
					}
				}
				else
				{
					// 无前车：自由巡航（可加速至随机目标）
					VM.TargetSpeed = CruiseTarget;
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

