#include "BPFLTools.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Async/ParallelFor.h"
#include "TrafficTypes.h"
#include "Async/Async.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

void UBPFLTools::JsonToDT(const TArray<uint8>& RawData, UPARAM(ref) UDataTable*& LaneMap, FOnDataTableFinished OnFinished)
{
	if (!LaneMap)
	{
		UE_LOG(LogTemp, Warning, TEXT("JsonToDT: LaneMap is null."));
		return;
	}

	// 校验 DataTable 的 RowStruct 是否匹配
	if (LaneMap->GetRowStruct() != FDTRoadGeoStatus::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("JsonToDT: LaneMap RowStruct mismatch. Expected FLaneMapRow."));
		return;
	}

	// 将字节流（UTF-8）转为字符串
	FString JsonStr;
	FFileHelper::BufferToString(JsonStr, RawData.GetData(), RawData.Num());


	// 解析 JSON 根对象
	TSharedPtr<FJsonObject> RootObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("JsonToDT: Deserialize JSON failed."));
		return;
	}

	// 提取 data 数组
	const TArray<TSharedPtr<FJsonValue>>* DataArrayPtr = nullptr;
	if (!RootObj->TryGetArrayField(TEXT("data"), DataArrayPtr) || !DataArrayPtr)
	{
		UE_LOG(LogTemp, Error, TEXT("JsonToDT: 'data' array missing."));
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>& DataArray = *DataArrayPtr;

	// 并行解析到中间数组（线程安全：按索引写入，避免 push_back 竞争）
	TArray<FDTRoadGeoStatus> Rows;
	TArray<FString> LinkIDs;

	const int32 Count = DataArray.Num();
	Rows.SetNum(Count);
	LinkIDs.SetNum(Count);
	
	ParallelFor(Count, [&](int32 Index)
		{
			const TSharedPtr<FJsonObject> ItemObj = DataArray[Index].IsValid() ? DataArray[Index]->AsObject() : nullptr;
			if (!ItemObj.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("JsonToDT: Failed to parse JSON object."));
				return;
			}

			FDTRoadGeoStatus Row;
			FString linkID;
			ItemObj->TryGetStringField(TEXT("linkid"), linkID);
			LinkIDs[Index] = linkID;

			ItemObj->TryGetStringField(TEXT("roadname"), Row.name);
			//ItemObj->TryGetStringField(TEXT("roadclass"), Row.RoadClass);

			int32 Tmp = 0;
			if (ItemObj->TryGetNumberField(TEXT("speed"), Tmp))       Row.speed = Tmp;
			if (ItemObj->TryGetNumberField(TEXT("state"), Tmp))       Row.state = Tmp;
			if (ItemObj->TryGetNumberField(TEXT("traveltime"), Tmp))  Row.traveltime = Tmp;

			const TArray<TSharedPtr<FJsonValue>>* PathArray = nullptr;
			if (ItemObj->TryGetArrayField(TEXT("path"), PathArray) && PathArray)
			{
				Row.coords.Reserve(PathArray->Num());
				for (const TSharedPtr<FJsonValue>& PairVal : *PathArray)
				{
					const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
					if (PairVal->TryGetArray(Pair) && Pair && Pair->Num() >= 2)
					{
						const double X = (*Pair)[0]->AsNumber();
						const double Y = (*Pair)[1]->AsNumber();
						Row.coords.Add(FVector(X, Y,0.0));
					}
				}
			}

			Rows[Index] = MoveTemp(Row);
		});

	// 回到游戏线程写入 DataTable
	auto CommitToDataTable = [LaneMap,NameIDs=MoveTemp(LinkIDs), Rows = MoveTemp(Rows),OnFinished]() mutable
		{
			LaneMap->EmptyTable();

			TSet<FName> NameSet;
			NameSet.Reserve(Rows.Num());

			for (int32 i = 0; i < Rows.Num(); ++i)
			{
				FName RowName = FName(*NameIDs[i]);

				// 避免重名
				if (NameSet.Contains(RowName))
				{
					RowName = FName(*FString::Printf(TEXT("%s_%d"), *RowName.ToString(), i));
				}
				NameSet.Add(RowName);

				// UDataTable 的 AddRow 在 UE5 支持以 FTableRowBase 子类添加
				LaneMap->AddRow(RowName, Rows[i]);
			}
			if(OnFinished.IsBound())
			{
				OnFinished.Execute(LaneMap);
			}
		};

	if (IsInGameThread())
	{
		CommitToDataTable();
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, MoveTemp(CommitToDataTable));
	}
}

void UBPFLTools::SendGetRequest(const FString& URL, const FString& BodyJson, FOnHttpResponseBP OnCompleted)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// Set JSON body
	Request->SetContentAsString(BodyJson);

	// Keep request alive until completion
	static TArray<TSharedRef<IHttpRequest, ESPMode::ThreadSafe>> PendingRequests;
	PendingRequests.Add(Request);


	
	Request->OnProcessRequestComplete().BindLambda([OnCompleted, Request](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		int32 StatusCode = Response.IsValid() ? Response->GetResponseCode() : 0;
		FString Content = Response.IsValid() ? Response->GetContentAsString() : FString();

		// Ensure delegate executes on game thread for BP
		AsyncTask(ENamedThreads::GameThread, [OnCompleted, StatusCode, Content, bWasSuccessful]() mutable
		{
			if (OnCompleted.IsBound())
			{
				OnCompleted.Execute(StatusCode, Content, bWasSuccessful);
			}
		});

		// Remove from pending
		PendingRequests.RemoveSingleSwap(Request);
	});

	Request->ProcessRequest();
}

void UBPFLTools::AddShapeTag(const FZoneGraphTagMask AddedTag, FZoneGraphTagMask OriginTag,FZoneGraphTagMask& NewTag)
{
	OriginTag.Add(AddedTag);
	NewTag = OriginTag;
}

void UBPFLTools::MarkActorModified(AActor* Actor)
{
	if (!Actor) return;	

	Actor->Modify();
	Actor->MarkPackageDirty();
	ULevel* Level =Actor->GetWorld()->GetCurrentLevel();

	if (Level)
	{
		Level->Actors.AddUnique(Actor);
		Level->Modify();
	}

	Level->MarkPackageDirty();

	
}

void UBPFLTools::MarkSplineModified(USplineComponent* Spline)
{
	if (!Spline) return;

	Spline->bSplineHasBeenEdited = true;

}

void UBPFLTools::AddRowToDT(UDataTable* DataTable, const FName& RowName, FCrossPhaseLaneData RowData)
{
	//FCrossPhaseLaneData Row = *(FCrossPhaseLaneData*)RowData;
	if (!DataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid DataTable Input!"));
		return;
	}
	DataTable->AddRow(RowName, RowData);
}

void UBPFLTools::ClearDT(UDataTable* DataTable)
{
	DataTable->EmptyTable();
}

void UBPFLTools::DeserializeOutterLaneVehicles(FJsonLibraryObject JsonObject, UDataTable* VehTypesDT,  FOnJsonDeserializeFinished OnFinished)
{
	// 整个解析放到后台，按值捕获 JsonObject 确保其生命周期覆盖
	Async(EAsyncExecution::ThreadPool, [JsonObject, VehTypesDT, OnFinished]() mutable
		{
			// 在后台线程重新取列表，避免使用已失效的 List
			FJsonLibraryList List = JsonObject.GetList(TEXT("lane_flows"));
			const int32 Count = List.Count();

			// 结果数组在后台线程内创建与填充
			TArray<FOutterLaneVehicle> VehicleInitDatas;
			VehicleInitDatas.SetNum(Count);

			// 先抽出轻量对象副本，避免 ParallelFor 里再访问 Json 库
			TArray<FJsonLibraryObject> Items;
			Items.SetNum(Count);
			for (int32 i = 0; i < Count; ++i)
			{
				Items[i] = List.GetObject(i);
			}
			UE_LOG(LogTemp, Log, TEXT("DeserializeOutterLaneVehicles: Processing %d items."), Count);
			ParallelFor(Count, [&](int32 Index)
				{
					const FJsonLibraryObject& LaneObj = Items[Index];
					FOutterLaneVehicle& VehicleData = VehicleInitDatas[Index];

					VehicleData.LaneSectID = FName(*LaneObj.GetString(TEXT("lane_sect_id")));
					VehicleData.FlowSpeed = LaneObj.GetFloat(TEXT("lane_speed"));

					const TArray<FString> StrVehIDs = LaneObj.GetList(TEXT("vehicle_ids")).ToStringArray();
					VehicleData.VehicleIDs.Reset();
					VehicleData.VehicleIDs.Reserve(StrVehIDs.Num());
					for (const FString& S : StrVehIDs)
					{
						VehicleData.VehicleIDs.Emplace(*S);
					}

					TArray<FString> VehTypes= LaneObj.GetList(TEXT("lane_types")).ToStringArray();
					
					VehicleData.VehicleTypeIndices.Reset();
					VehicleData.VehicleTypeIndices.Reserve(VehTypes.Num());
					UE_LOG(LogTemp, Log, TEXT("DeserializeOutterLaneVehicles: Lane %s has %d vehicle types."), *VehicleData.LaneSectID.ToString(), VehTypes.Num());
					for(auto TypeStr: VehTypes)
					{

							FVehTypeRow* Row=VehTypesDT->FindRow<FVehTypeRow>(FName(TypeStr),TEXT("Find VehType Indice"));
							if (Row&&Row->TypeIndices.Num()>0)
							{
				
								int32 TypeIndex = FMath::RandHelper(Row->TypeIndices.Num());
								VehicleData.VehicleTypeIndices.Add(Row->TypeIndices[TypeIndex]);
								continue;
							}
							VehicleData.VehicleTypeIndices.Add(0); // 找不到类型则默认为 0
					}

				}, EParallelForFlags::Unbalanced);

			// 回到 GameThread 触发蓝图委托
			AsyncTask(ENamedThreads::GameThread, [VehicleInitDatas = MoveTemp(VehicleInitDatas), OnFinished]() mutable
				{
					if (OnFinished.IsBound() && IsValid(OnFinished.GetUObject()))
					{
						OnFinished.Execute(VehicleInitDatas);
					}
				});
		});
}