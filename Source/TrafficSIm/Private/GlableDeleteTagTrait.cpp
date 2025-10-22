// Fill out your copyright notice in the Description page of Project Settings.


#include "GlableDeleteTagTrait.h"
#include "MassEntityTemplate.h"
#include "TrafficTypes.h"
#include "MassEntityTemplateRegistry.h"

void UGlableDeleteTagTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FMassGlobalDespawnTag>();
}
