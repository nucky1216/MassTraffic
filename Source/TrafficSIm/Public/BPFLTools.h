// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CesiumGeoreference.h"
#include "Engine/DataTable.h"
#include "BPFLTools.generated.h"

/**
 * 
 */
UCLASS()
class TRAFFICSIM_API UBPFLTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	

public:
	static void MapRoadToLanes(UDataTable* RoadGeo,UDataTable*& LaneMap);
};
