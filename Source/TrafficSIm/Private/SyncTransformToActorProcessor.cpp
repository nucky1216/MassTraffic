#include "SyncTransformToActorProcessor.h"
#include "MassRepresentationSubsystem.h"
#include "MassRepresentationTypes.h"
#include "MassProcessor.h"
#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"

USyncTransformToActorProcessor::USyncTransformToActorProcessor() :EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Representation;
	bAutoRegisterWithProcessingPhases = true;
	// �ؼ���Ҫ������Ϸ�߳�ִ�У����Ⲣ�������߳������ Actor
	bRequiresGameThreadExecution = true;
}



void USyncTransformToActorProcessor::ConfigureQueries()
{
	// ��������ǰ�� Actor ��ʽ��ʾ��ʵ��
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
}

void USyncTransformToActorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& ExecContext)
		{
			const int32 NumEntities = ExecContext.GetNumEntities();

			const TConstArrayView<FTransformFragment>& TransformFragments = ExecContext.GetMutableFragmentView<FTransformFragment>();
			const TArrayView<FMassActorFragment>& ActorFragments = ExecContext.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FMassRepresentationFragment>& RepFragments = ExecContext.GetMutableFragmentView<FMassRepresentationFragment>();

			for (int32 i = 0; i < NumEntities; ++i)
			{
				// ������ǰ��ʾΪ Actor ʱͬ��
				if (RepFragments[i].CurrentRepresentation == EMassRepresentationType::StaticMeshInstance|| RepFragments[i].CurrentRepresentation == EMassRepresentationType::None)
				{
					continue;
				}
				//UE_LOG(LogTemp, Log, TEXT("Visit Actor Entity at:%d"),i);
				const FTransform& EntityTransform = TransformFragments[i].GetTransform();
				AActor*  Actor = ActorFragments[i].GetMutable(FMassActorFragment::EActorAccess::OnlyWhenAlive);

				if (IsValid(Actor))
				{
					Actor->SetActorTransform(EntityTransform, /*bSweep*/ false, /*OutHit*/ nullptr, ETeleportType::TeleportPhysics);
				}
			}
		});
}
