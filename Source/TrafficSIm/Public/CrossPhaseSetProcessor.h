#pragma once
#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "TrafficLightSubsystem.h"
#include "CrossPhaseSetProcessor.generated.h"

UCLASS()
class TRAFFICSIM_API UCrossPhaseSetProcessor : public UMassProcessor
{
	GENERATED_BODY();
public:
	UCrossPhaseSetProcessor();
	virtual void ConfigureQueries() override;
	virtual void Initialize(UObject& Owner) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	UTrafficSimSubsystem* TrafficSimSubsystem;
	FMassEntityHandle EntityHandle;
};

