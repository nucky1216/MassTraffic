// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicSpawnProcessor.h"
#include "TrafficSimSubsystem.h"
#include "TrafficCommonFragments.h"
#include "MassCommonFragments.h"
#include "TrafficTypes.h"
#include "MassExecutionContext.h"
#include "ZoneGraphQuery.h"
#include "VehicleParamsInitProcessor.h"
#include "MassCommands.h" // added for deferred create command
#include "MassRepresentationFragments.h" // for representation fragment if needed
#include "MassRepresentationSubsystem.h"

UDynamicSpawnProcessor::UDynamicSpawnProcessor() :EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = FName(TEXT("TrafficSim"));
	//ExecutionOrder.ExecuteAfter.Add(FName(TEXT("VehicleMovementProcessor")));
	ExecutionOrder.ExecuteBefore.Add(FName(TEXT("VehicleParamsInitProcessor")));
	ProcessingPhase = EMassProcessingPhase::EndPhysics; // 修改为 PostPhysics 阶段
	bAutoRegisterWithProcessingPhases = true;
}

void UDynamicSpawnProcessor::Initialize(UObject& Owner)
{
	TrafficSimSubsystem = GetWorld()->GetSubsystem<UTrafficSimSubsystem>();
	if (!TrafficSimSubsystem)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("Failed to Get TrafficSubsystem"));
		return;
	}

	MassSpawnerSubsystem = GetWorld()->GetSubsystem<UMassSpawnerSubsystem>();
	if (!MassSpawnerSubsystem)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("Failed to Get MassSpawnerSubsystem!"));
		return;
	}

	TrafficSimSubsystem->GetVehicleConfigs(VehicleLenth,PrefixSum);
}

void UDynamicSpawnProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassSpawnPointFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UDynamicSpawnProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UE_LOG(LogTrafficSim, VeryVerbose, TEXT("DynamicSpawnProcessor.."));

	if (VehicleLenth.Num() == 0)
	{
		UE_LOG(LogTrafficSim, Warning, TEXT("No Vehicle Config"));
		return;
	}
	TMultiMap<int32,FZoneGraphLaneLocation> ReadySpawnLocs;
	auto SelectRandomItem = [&]()
		{
			float R = FMath::FRand(); // 生成 0~1 的随机数
			for (int32 i = 0; i < PrefixSum.Num(); i++)
			{
				if (R <= PrefixSum[i])
				{
					return i; // 返回选中的元素
				}
			}
			return PrefixSum.Num() - 1; // 理论上不会走到这里
		};

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& Context)
		{
			auto LaneLocations = Context.GetMutableFragmentView<FMassSpawnPointFragment>();
			auto Configs = Context.GetMutableFragmentView<FMassSpawnPointFragment>();
			float DeltaTime = Context.GetDeltaTimeSeconds();
			int32 EntityCount = Context.GetNumEntities();
			for (int32 i = 0; i < EntityCount; i++)
			{
				auto& Frag = LaneLocations[i];
				float& Clock = Configs[i].Clock;

				Clock -= DeltaTime;
				if (Clock <= 0)
				{
					FZoneGraphLaneLocation& LaneLocation = Frag.LaneLocation;
					TConstArrayView<FLaneVehicle> LaneVehicles;
					TrafficSimSubsystem->GetLaneVehicles(LaneLocation.LaneHandle.Index, LaneVehicles);

					float LeftSpace = 0;
					if (LaneVehicles.Num() == 0)
					{
						//UE_LOG(LogTrafficSim, Log, TEXT("DynamicSpawnProcessor::Get Empty LaneVehicles from LaneIndex:%d"), LaneLocation.LaneHandle.Index);
						//continue;
						UE::ZoneGraph::Query::GetLaneLength(*TrafficSimSubsystem->ZoneGraphStorage, LaneLocation.LaneHandle.Index, LeftSpace);
					}
					else
					{
						//取出最后一辆车
						FLaneVehicle LastVehicle = LaneVehicles[0];
						LeftSpace = LastVehicle.VehicleMovementFragment.LaneLocation.DistanceAlongLane - LastVehicle.VehicleMovementFragment.VehicleLength;
					}

					if (Frag.NextVehicleType < 0)
					{
						Frag.NextVehicleType = SelectRandomItem();
					}

					if (LeftSpace > VehicleLenth[Frag.NextVehicleType])
					{
						ReadySpawnLocs.Add(Frag.NextVehicleType, LaneLocation);
						Frag.NextVehicleType = -1;

						//TArray<FColor> Colors = {FColor::Red,FColor::Blue,FColor::Green};
						//DrawDebugBox(Context.GetWorld(), LaneLocation.Position, FVector(VehicleLenth[SpawnConfigIndex], 20, 20), Colors[SpawnConfigIndex]
						//	, false, 20.f, 0, 10.f);
					}

					Clock = Configs[i].Duration+FMath::RandRange(0.f,Configs[i].RandOffset);
				}


			}
		}

	);


	if (ReadySpawnLocs.Num() > 0)
	{
		TArray<int32> Keys;
		ReadySpawnLocs.GetKeys(Keys);
		for (auto Key : Keys)
		{
			TArray<FZoneGraphLaneLocation> LaneLocations;
			ReadySpawnLocs.MultiFind(Key, LaneLocations);

			if (LaneLocations.Num() <= 0)
			{
				continue;
			}

			// Copy needed data for lambda capture
			const FMassEntityTemplate* EntityTemplatePtr = TrafficSimSubsystem->EntityTemplates.IsValidIndex(Key) ? TrafficSimSubsystem->EntityTemplates[Key] : nullptr;
			if (!EntityTemplatePtr || !EntityTemplatePtr->IsValid())
			{
				UE_LOG(LogTrafficSim, Warning, TEXT("Invalid EntityTemplate for Key %d"), Key);
				continue;
			}

			UMassRepresentationSubsystem* RepSubsystem = UWorld::GetSubsystem<UMassRepresentationSubsystem>(GetWorld());
			if (!RepSubsystem)
			{
				UE_LOG(LogTemp, Warning, TEXT("No RepresentationSubsystem found"));
				return;
			}
			

			// Capture by value to ensure safety until deferred execution
			int32 ConfigIndexCopy = Key;
			TArray<FZoneGraphLaneLocation> LaneLocationsCopy = LaneLocations;
			TWeakObjectPtr<UTrafficSimSubsystem> WeakTrafficSim = TrafficSimSubsystem;
			TWeakObjectPtr<UMassRepresentationSubsystem> WeakRepSubsystem = RepSubsystem;

			FMassEntityTemplate EntityTemplateCopy = *EntityTemplatePtr; // full copy (immutable data)
			EntityManager.Defer().PushCommand<FMassDeferredCreateCommand>([EntityTemplateCopy,  LaneLocationsCopy = MoveTemp(LaneLocationsCopy), ConfigIndexCopy, WeakTrafficSim, WeakRepSubsystem](FMassEntityManager& System) mutable
			{
				// 1. Create with shared fragments
				TArray<FMassEntityHandle> NewEntities;
				TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = System.BatchCreateEntities(
					EntityTemplateCopy.GetArchetype(),
					EntityTemplateCopy.GetSharedFragmentValues(),
					LaneLocationsCopy.Num(),
					NewEntities);

				// 2. Apply template initial fragment values
				System.BatchSetEntityFragmentsValues(CreationContext->GetEntityCollection(), EntityTemplateCopy.GetInitialFragmentValues());

				// 3. Runtime custom init (similar to VehicleParamsInitProcessor essentials)
				UWorld* World = System.GetWorld();
				UTrafficSimSubsystem* TrafficSimSubsystemLocal = World ? World->GetSubsystem<UTrafficSimSubsystem>() : nullptr;

				for (int32 i = 0; i < NewEntities.Num(); ++i)
				{
					const FZoneGraphLaneLocation& LaneLoc = LaneLocationsCopy[i];
					FMassVehicleMovementFragment& MoveFrag = System.GetFragmentDataChecked<FMassVehicleMovementFragment>(NewEntities[i]);
					FTransformFragment& TransformFrag = System.GetFragmentDataChecked<FTransformFragment>(NewEntities[i]);
					const FMassRepresentationFragment& Representation = System.GetFragmentDataChecked<FMassRepresentationFragment>(NewEntities[i]);
					const FMassRepresentationLODFragment& RepresentationLOD = System.GetFragmentDataChecked<FMassRepresentationLODFragment>(NewEntities[i]);

					MoveFrag.LaneLocation = LaneLoc;
					TransformFrag.SetTransform(FTransform(LaneLoc.Direction.ToOrientationQuat(), LaneLoc.Position));
					MoveFrag.VehicleHandle = NewEntities[i];

					const FZoneLaneData& LaneData = WeakTrafficSim->ZoneGraphStorage->Lanes[LaneLoc.LaneHandle.Index];
					WeakTrafficSim->GetLaneSpeedByTag(
						LaneData.Tags,
						MoveFrag.MaxSpeed,
						MoveFrag.MinSpeed,
						MoveFrag.LaneSpeedTag);
					MoveFrag.TargetSpeed = FMath::RandRange(MoveFrag.MinSpeed, MoveFrag.MaxSpeed);
					MoveFrag.Speed = FMath::RandRange(MoveFrag.MinSpeed, MoveFrag.TargetSpeed);

					if(MoveFrag.Speed==0.0 || MoveFrag.TargetSpeed==0.0)
					{
						UE_LOG(LogTrafficSim, Warning, TEXT("Spawned Vehicle with Zero Speed on Lane %d"), LaneLoc.LaneHandle.Index);
					}

					TArray<int32> NextLanes;
					MoveFrag.NextLane= WeakTrafficSim->ChooseNextLane(LaneLoc.LaneHandle.Index, NextLanes);

					if (TrafficSim::MoveFrag::Debug::bEnbaleCustomData)
					{
						auto SMInfos = WeakRepSubsystem->GetMutableInstancedStaticMeshInfos();

						// 获取 InstancedStaticMesh 的索引
						const int32 StaticMeshInstanceIndex = Representation.StaticMeshDescIndex;
						if (StaticMeshInstanceIndex == INDEX_NONE)
						{
							continue; // 无效索引，跳过
						}

						// 获取 InstancedStaticMesh 信息
						FMassInstancedStaticMeshInfo& MeshInfo = SMInfos[StaticMeshInstanceIndex];

						const float LODSignificance = RepresentationLOD.LODSignificance;
						struct FVehicleISMCustomData
						{
							float SerialNumber;
							float EntityId;
							float Speed;
						};
						static_assert((sizeof(FVehicleISMCustomData) % sizeof(float)) == 0, "custom data must be float-packed");
						float testFloat = 100.f;
						const FVehicleISMCustomData Custom{
							static_cast<float>(NewEntities[i].SerialNumber),
							MoveFrag.TargetSpeed,
							MoveFrag.Speed
						};

						//UE_LOG(LogTemp, Log, TEXT("MeshIndex:%d"), StaticMeshInstanceIndex);
						MeshInfo.AddBatchedCustomData(Custom, LODSignificance);
					}

				}
				//UE_LOG(LogTrafficSim, Log, TEXT("Deferred Spawned Entity Num:%d ConfigIndex:%d"), NewEntities.Num(), ConfigIndexCopy);
			});
		}
	}
	// Removed MassSpawnerSubsystem spawning & initializer processor usage; deferred commands will flush later.
	// Removed invalid call to EntityManager.Defer().BatchCreateEntities();
}
