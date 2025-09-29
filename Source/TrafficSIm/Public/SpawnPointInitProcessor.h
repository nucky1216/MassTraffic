#pragma once
#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassObserverProcessor.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneGraphData.h"
#include "TrafficSimSubsystem.h"
#include "SpawnPointInitProcessor.generated.h"


UCLASS()
class TRAFFICSIM_API USpawnPointInitProcessor : public UMassProcessor
{
	GENERATED_BODY()
	
public:
	USpawnPointInitProcessor();

	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery EntityQuery;
};

