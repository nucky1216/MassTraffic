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

	// У�� DataTable �� RowStruct �Ƿ�ƥ��
	if (LaneMap->GetRowStruct() != FDTRoadGeoStatus::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("JsonToDT: LaneMap RowStruct mismatch. Expected FLaneMapRow."));
		return;
	}

	// ���ֽ�����UTF-8��תΪ�ַ���
	FString JsonStr;
	FFileHelper::BufferToString(JsonStr, RawData.GetData(), RawData.Num());


	// ���� JSON ������
	TSharedPtr<FJsonObject> RootObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("JsonToDT: Deserialize JSON failed."));
		return;
	}

	// ��ȡ data ����
	const TArray<TSharedPtr<FJsonValue>>* DataArrayPtr = nullptr;
	if (!RootObj->TryGetArrayField(TEXT("data"), DataArrayPtr) || !DataArrayPtr)
	{
		UE_LOG(LogTemp, Error, TEXT("JsonToDT: 'data' array missing."));
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>& DataArray = *DataArrayPtr;

	// ���н������м����飨�̰߳�ȫ��������д�룬���� push_back ������
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

	// �ص���Ϸ�߳�д�� DataTable
	auto CommitToDataTable = [LaneMap,NameIDs=MoveTemp(LinkIDs), Rows = MoveTemp(Rows),OnFinished]() mutable
		{
			LaneMap->EmptyTable();

			TSet<FName> NameSet;
			NameSet.Reserve(Rows.Num());

			for (int32 i = 0; i < Rows.Num(); ++i)
			{
				FName RowName = FName(*NameIDs[i]);

				// ��������
				if (NameSet.Contains(RowName))
				{
					RowName = FName(*FString::Printf(TEXT("%s_%d"), *RowName.ToString(), i));
				}
				NameSet.Add(RowName);

				// UDataTable �� AddRow �� UE5 ֧���� FTableRowBase �������
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
	// ���������ŵ���̨����ֵ���� JsonObject ȷ�����������ڸ���
	Async(EAsyncExecution::ThreadPool, [JsonObject, VehTypesDT, OnFinished]() mutable
		{
			// �ں�̨�߳�����ȡ�б������ʹ����ʧЧ�� List
			FJsonLibraryList List = JsonObject.GetList(TEXT("lane_flows"));
			const int32 Count = List.Count();

			// ��������ں�̨�߳��ڴ��������
			TArray<FOutterLaneVehicle> VehicleInitDatas;
			VehicleInitDatas.SetNum(Count);

			// �ȳ���������󸱱������� ParallelFor ���ٷ��� Json ��
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
							VehicleData.VehicleTypeIndices.Add(0); // �Ҳ���������Ĭ��Ϊ 0
					}

				}, EParallelForFlags::Unbalanced);

			// �ص� GameThread ������ͼί��
			AsyncTask(ENamedThreads::GameThread, [VehicleInitDatas = MoveTemp(VehicleInitDatas), OnFinished]() mutable
				{
					if (OnFinished.IsBound() && IsValid(OnFinished.GetUObject()))
					{
						OnFinished.Execute(VehicleInitDatas);
					}
				});
		});
}

void UBPFLTools::AddInstanceComponent(AActor* Actor, UActorComponent* CompIn)
{
	if (!Actor|| !CompIn) return;

	Actor->Modify();
	CompIn->Modify();

	Actor->AddInstanceComponent(CompIn);
	CompIn->RegisterComponent();
	CompIn->OnComponentCreated();
	CompIn->MarkPackageDirty();

	Actor->MarkPackageDirty();


}
