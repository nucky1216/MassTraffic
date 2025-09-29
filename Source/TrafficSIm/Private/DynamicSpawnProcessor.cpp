// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicSpawnProcessor.h"
#include "TrafficSimSubsystem.h"
#include "TrafficCommonFragments.h"
#include "MassCommonFragments.h"
#include "TrafficTypes.h"
#include "MassExecutionContext.h"
#include "VehicleParamsInitProcessor.h"
#include "MassCommands.h" // added for deferred create command
#include "MassRepresentationFragments.h" // for representation fragment if needed

UDynamicSpawnProcessor::UDynamicSpawnProcessor() :EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = FName(TEXT("TrafficSim"));
	//ExecutionOrder.ExecuteAfter.Add(FName(TEXT("VehicleMovementProcessor")));
	ExecutionOrder.ExecuteBefore.Add(FName(TEXT("VehicleParamsInitProcessor")));
	ProcessingPhase = EMassProcessingPhase::EndPhysics; // �޸�Ϊ PostPhysics �׶�
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
	EntityQuery.AddRequirement<FMassSpawnPointFragment>(EMassFragmentAccess::ReadOnly);
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
			float R = FMath::FRand(); // ���� 0~1 �������
			for (int32 i = 0; i < PrefixSum.Num(); i++)
			{
				if (R <= PrefixSum[i])
				{
					return i; // ����ѡ�е�Ԫ��
				}
			}
			return PrefixSum.Num() - 1; // �����ϲ����ߵ�����
		};

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& Context)
		{
			auto LaneLocations = Context.GetMutableFragmentView<FMassSpawnPointFragment>();
			int32 EntityCount = Context.GetNumEntities();
			for (int32 i = 0; i < EntityCount; i++)
			{
				auto& Frag = LaneLocations[i];
				FZoneGraphLaneLocation& LaneLocation = Frag.LaneLocation;
				TConstArrayView<FLaneVehicle> LaneVehicles;
				TrafficSimSubsystem->GetLaneVehicles(LaneLocation.LaneHandle.Index, LaneVehicles);

				if (LaneVehicles.Num() == 0)
				{
					UE_LOG(LogTrafficSim, Log, TEXT("Get Empty LaneVehicles from LaneIndex:%d"), LaneLocation.LaneHandle.Index);
					continue;
				}

				//ȡ�����һ����
				FLaneVehicle LastVehicle = LaneVehicles[0];
				float LeftSpace = LastVehicle.VehicleMovementFragment->LaneLocation.DistanceAlongLane - LastVehicle.VehicleMovementFragment->VehicleLength;

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

			}
		}

	);


	if (1 && ReadySpawnLocs.Num() > 0)
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
			// Capture by value to ensure safety until deferred execution
			int32 ConfigIndexCopy = Key;
			TArray<FZoneGraphLaneLocation> LaneLocationsCopy = LaneLocations;
			TWeakObjectPtr<UTrafficSimSubsystem> WeakTrafficSim = TrafficSimSubsystem;
			FMassEntityTemplate EntityTemplateCopy = *EntityTemplatePtr; // full copy (immutable data)
			EntityManager.Defer().PushCommand<FMassDeferredCreateCommand>([EntityTemplateCopy, LaneLocationsCopy = MoveTemp(LaneLocationsCopy), ConfigIndexCopy, WeakTrafficSim](FMassEntityManager& System) mutable
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
					MoveFrag.LaneLocation = LaneLoc;
					TransformFrag.SetTransform(FTransform(LaneLoc.Direction.ToOrientationQuat(), LaneLoc.Position));
					MoveFrag.VehicleHandle = NewEntities[i];
					MoveFrag.TargetSpeed = FMath::RandRange(MoveFrag.MinSpeed, MoveFrag.MaxSpeed);
					MoveFrag.Speed = FMath::RandRange(MoveFrag.MinSpeed, MoveFrag.TargetSpeed);

					TArray<int32> NextLanes;
					MoveFrag.NextLane= WeakTrafficSim->ChooseNextLane(LaneLoc.LaneHandle.Index, NextLanes);

				}
				//UE_LOG(LogTrafficSim, Log, TEXT("Deferred Spawned Entity Num:%d ConfigIndex:%d"), NewEntities.Num(), ConfigIndexCopy);
			});
		}
	}
	// Removed MassSpawnerSubsystem spawning & initializer processor usage; deferred commands will flush later.
	// Removed invalid call to EntityManager.Defer().BatchCreateEntities();
}
