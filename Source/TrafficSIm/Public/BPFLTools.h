// Copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CesiumGeoreference.h"
#include "Engine/DataTable.h"
#include "ZoneGraphTypes.h"
#include "JsonLibraryHelpers.h"
#include "TrafficTypes.h"
#include "Components/SplineComponent.h"
#include "BPFLTools.generated.h"

/**
 * 
 */
UCLASS()
class TRAFFICSIM_API UBPFLTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	// Blueprint dynamic delegate for HTTP responses
	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnHttpResponseBP, int32, StatusCode, const FString&, Content, bool, bWasSuccessful);
	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnDataTableFinished, UDataTable*, RoadDT);
	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnJsonDeserializeFinished, const TArray<FOutterLaneVehicle>&, VehicleInitDatas);

	UFUNCTION(BlueprintCallable, Category = "DataTable")
	static void JsonToDT(const TArray<uint8>& RawData , UPARAM(ref)UDataTable*& LaneMap,FOnDataTableFinished OnFinished);

	// Send HTTP POST with JSON body and return via Blueprint callback
	UFUNCTION(BlueprintCallable, Category = "HTTP", meta=(AutoCreateRefTerm="OnCompleted,BodyJson"))
	static void SendGetRequest(const FString& URL, const FString& BodyJson, FOnHttpResponseBP OnCompleted);

	UFUNCTION(BlueprintCallable, BlueprintPure,Category = "ZoneShapeTag")
	static void AddShapeTag(const FZoneGraphTagMask AddedTag, FZoneGraphTagMask OriginTag, FZoneGraphTagMask& NewTag);

	UFUNCTION(BlueprintCallable, Category = "ActorUtility")
	static void MarkActorModified(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "ActorUtility")
	static void MarkSplineModified(USplineComponent* Spline);

	UFUNCTION(BlueprintCallAble,Category="MatchCrossLanes")
	static void AddRowToDT(UDataTable* DataTable, const FName& RowName, FCrossPhaseLaneData RowData);

	UFUNCTION(BlueprintCallAble, Category = "MatchCrossLanes")
	static void ClearDT(UDataTable* DataTable);

	UFUNCTION(BlueprintCallAble, Category = "Json/Deserialize")
	static void DeserializeOutterLaneVehicles(FJsonLibraryObject JsonObject,UDataTable* VehTypesDT, FOnJsonDeserializeFinished OnFinished);
};
