// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ZoneGraphData.h"
#include "TagFilter.generated.h"

/**
 * 
 */
UCLASS()
class TRAFFICSIM_API UTagFilter : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (ToolTip = "Intersection Tag Filter"))
	FZoneGraphTagFilter TagFilter;
	
};
