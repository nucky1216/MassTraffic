// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "GlableDeleteTagTrait.generated.h"

/**
 * 
 */
UCLASS(EditInlineNew, DisplayName = "Global Despawn Tag Trait")
class TRAFFICSIM_API UGlableDeleteTagTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()
public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

};
