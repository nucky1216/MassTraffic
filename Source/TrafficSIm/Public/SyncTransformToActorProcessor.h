#pragma once

#include "MassProcessor.h"
#include "MassRepresentationFragments.h"
#include "SyncTransformToActorProcessor.generated.h"



/** Processor: 根据实体上的 FToggleISMCollisionFragment 临时开关对应 ISM Component 的碰撞 */
UCLASS()
class TRAFFICSIM_API USyncTransformToActorProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	USyncTransformToActorProcessor();

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery; 
};
