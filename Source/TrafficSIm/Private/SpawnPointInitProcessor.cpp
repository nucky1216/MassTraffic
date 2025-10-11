#include "SpawnPointInitProcessor.h"
#include "TrafficTypes.h"
#include "MassCommonFragments.h"
#include "MassVehicleMovementFragment.h"
#include "ZoneGraphQuery.h"
#include "TrafficCommonFragments.h"
#include "MassRepresentationFragments.h"

USpawnPointInitProcessor::USpawnPointInitProcessor():EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = false;
}

void USpawnPointInitProcessor::Initialize(UObject& Owner)
{
}

void USpawnPointInitProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassSpawnPointFragment>(EMassFragmentAccess::ReadWrite);
}

void USpawnPointInitProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UE_LOG(LogTrafficSim, VeryVerbose, TEXT("VehicleInitProcessor.."));
	if (!ensure(Context.ValidateAuxDataType<FVehicleInitData>()))
	{
		UE_LOG(LogTrafficLight, Error, TEXT("TrafficLightInitProcessor requires FSpawnPointInitData to be set in the context."));
	}

	FVehicleInitData& InitData = Context.GetMutableAuxData().GetMutable<FVehicleInitData>();

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [&](FMassExecutionContext& Context)
	{
		const int32 EntityCount = Context.GetNumEntities();
		const TArrayView<FTransformFragment> TransformList = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FMassSpawnPointFragment> SpawnPointList = Context.GetMutableFragmentView<FMassSpawnPointFragment>();
		for (int32 EntityIndex = 0; EntityIndex < EntityCount; ++EntityIndex)
		{
			FTransformFragment& Transform = TransformList[EntityIndex];
			FMassSpawnPointFragment& SpawnPoint = SpawnPointList[EntityIndex];
			FZoneGraphLaneLocation& LaneLocation = InitData.LaneLocations[EntityIndex];

			Transform.SetTransform(FTransform(LaneLocation.Direction.ToOrientationQuat(),LaneLocation.Position));
			SpawnPoint.LaneLocation = LaneLocation;
			SpawnPoint.Clock = SpawnPoint.Duration+FMath::RandRange(0.f,SpawnPoint.RandOffset);
			
			//Debug
			//DrawDebugPoint(GetWorld(), LaneLocation.Position, 20.0f, FColor::Red, true, 20.0f);
		}
		});
}
