// Fill out your copyright notice in the Description page of Project Settings.


#include "Crowd/SOBD_DestroyEntity.h"

#include "MassCommandBuffer.h"
#include "MassSmartObjectFragments.h"
#include "SmartObjectSubsystem.h"
#include "VisualLogger/VisualLogger.h"

void USOBD_DestroyEntity::Activate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const
{
	Super::Activate(CommandBuffer, EntityContext);
}

void USOBD_DestroyEntity::Deactivate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const
{
	const FMassEntityHandle Entity = EntityContext.EntityView.GetEntity();
	const FMassSmartObjectUserFragment& User = EntityContext.EntityView.GetFragmentData<FMassSmartObjectUserFragment>();

	// Deactivate is called before status gets updated to TaskCompleted/Aborted in StopInteraction.
	// Destroy only when interaction completed normally.
	if (User.InteractionStatus == EMassSmartObjectInteractionStatus::BehaviorCompleted)
	{
		UE_VLOG(&EntityContext.SmartObjectSubsystem, LogSmartObject, Log, TEXT("Destroy entity after smart object use."));
		CommandBuffer.DestroyEntity(Entity);
	}

	Super::Deactivate(CommandBuffer, EntityContext);
}

