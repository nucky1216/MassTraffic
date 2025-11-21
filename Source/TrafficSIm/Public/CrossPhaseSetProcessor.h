#pragma once
#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "TrafficLightSubsystem.h"
#include "CrossPhaseSetProcessor.generated.h"

UCLASS()
class TRAFFICSIM_API UCrossPhaseSetProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UCrossPhaseSetProcessor();
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	TArray<TTuple<FName, double, double>> PhaseArray;
	FMassEntityHandle EntityHandle;
private:
	UTrafficSimSubsystem* TrafficSimSubsystem;
	FMassEntityQuery EntityQuery;

};

