// ZoneShapeEditorExtensions.cpp
#include "EditorUtilities.h"
#include "PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "ZoneShapeEdgeConfig.h"
#include "ZoneShapeActor.h"
#include "ZoneShapeComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "UnrealEdGlobals.h"
#include "Widgets/SWindow.h"
#include "ComponentVisualizer.h"
#include "IDetailChildrenBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SComboBox.h"

#define LOCTEXT_NAMESPACE "FZoneShapeEditorExtensionsModule"

class FZoneShapeDetailsCustomization : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance()
    {
        return MakeShareable(new FZoneShapeDetailsCustomization);
    }

    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
    {
        TArray<TWeakObjectPtr<UObject>> Objects;
        DetailBuilder.GetObjectsBeingCustomized(Objects);

        UE_LOG(LogTemp, Warning, TEXT("EditorUtilities: Ready to Selected object class"));

        for (auto& Obj : Objects)
        {
            UE_LOG(LogTemp, Warning, TEXT("Selected object class: %s"), *Obj->GetClass()->GetName());

            if (AZoneShape* ZoneShapeActor = Cast<AZoneShape>(Obj.Get()))
            {
                AddEdgeConfigCategory(DetailBuilder, ZoneShapeActor->GetComponentByClass<UZoneShapeComponent>(), ZoneShapeActor->GetName());
            }
            else if (UZoneShapeComponent* ZoneShapeComp = Cast<UZoneShapeComponent>(Obj.Get()))
            {
                AddEdgeConfigCategory(DetailBuilder, ZoneShapeComp, ZoneShapeComp->GetName());
            }
        }
    }

    void AddEdgeConfigCategory(IDetailLayoutBuilder& DetailBuilder, UZoneShapeComponent* ZoneShapeComp, const FString& DisplayName)
    {
        if (!ZoneShapeComp) return;

        IDetailCategoryBuilder& CustomCategory = DetailBuilder.EditCategory(
            "Polygon Edge Config",
            FText::FromString("Polygon Edge Config"),
            ECategoryPriority::Important
        );

        UE_LOG(LogTemp, Warning, TEXT("CustomizeDetails triggered for %s"), *DisplayName);

        // 初始化选项
        TurnTypeOptions.Empty();
        TurnTypeOptions.Add(MakeShared<ESlotTurnType>(ESlotTurnType::LeftTurn));
        TurnTypeOptions.Add(MakeShared<ESlotTurnType>(ESlotTurnType::Straight));
        TurnTypeOptions.Add(MakeShared<ESlotTurnType>(ESlotTurnType::RightTurn));

        // 模拟二维数组数据（4条边，每条边有不同 slot 数量）
        if (TurnTypes2D.Num() == 0)
        {
            TurnTypes2D = {
                { ESlotTurnType::LeftTurn, ESlotTurnType::Straight },
                { ESlotTurnType::Straight, ESlotTurnType::RightTurn },
                { ESlotTurnType::LeftTurn, ESlotTurnType::RightTurn, ESlotTurnType::Straight },
                { ESlotTurnType::Straight }
            };
        }

        // 遍历外层：Edge
        for (int32 EdgeIndex = 0; EdgeIndex < TurnTypes2D.Num(); ++EdgeIndex)
        {
            // 每条 Edge 加个标题
            CustomCategory.AddCustomRow(FText::FromString(FString::Printf(TEXT("Edge_%d"), EdgeIndex)))
                .WholeRowContent()
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("Edge %d"), EdgeIndex)))
                        .Font(IDetailLayoutBuilder::GetDetailFontBold())
                ];

            // 遍历内层：Slot
            for (int32 SlotIndex = 0; SlotIndex < TurnTypes2D[EdgeIndex].Num(); ++SlotIndex)
            {
                CustomCategory.AddCustomRow(FText::FromString(FString::Printf(TEXT("Edge_%d_Slot_%d"), EdgeIndex, SlotIndex)))
                    .NameContent()
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("Slot %d"), SlotIndex)))
                            .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                    .ValueContent()
                    .MinDesiredWidth(250.0f)
                    [
                        SNew(SComboBox<TSharedPtr<ESlotTurnType>>)
                            .OptionsSource(&TurnTypeOptions)
                            .OnGenerateWidget(this, &FZoneShapeDetailsCustomization::MakeTurnTypeWidget)
                            .OnSelectionChanged_Lambda([this, EdgeIndex, SlotIndex](TSharedPtr<ESlotTurnType> NewSelection, ESelectInfo::Type)
                                {
                                    if (NewSelection.IsValid())
                                    {
                                        TurnTypes2D[EdgeIndex][SlotIndex] = *NewSelection;
                                        UE_LOG(LogTemp, Log, TEXT("Edge %d Slot %d changed to %s"),
                                            EdgeIndex, SlotIndex, *GetTurnTypeText(*NewSelection).ToString());
                                    }
                                })
                            .InitiallySelectedItem(GetSharedTurnType(TurnTypes2D[EdgeIndex][SlotIndex]))
                            [
                                SNew(STextBlock)
                                    .Text_Lambda([this, EdgeIndex, SlotIndex]()
                                        {
                                            return GetTurnTypeText(TurnTypes2D[EdgeIndex][SlotIndex]);
                                        })
                            ]
                    ];
            }
        }
    }

private:
    /** 当前配置的转向类型数组 */
    TArray<ESlotTurnType> TurnTypes;

    /** 二维转向配置：TurnTypes2D[EdgeIndex][SlotIndex] */
    TArray<TArray<ESlotTurnType>> TurnTypes2D;

    /** ComboBox 可选项 */
    TArray<TSharedPtr<ESlotTurnType>> TurnTypeOptions;


    /** 渲染选项 */
    TSharedRef<SWidget> MakeTurnTypeWidget(TSharedPtr<ESlotTurnType> InOption) const
    {
        return SNew(STextBlock).Text(GetTurnTypeText(*InOption));
    }

    /** 转向类型转文本 */
    static FText GetTurnTypeText(ESlotTurnType TurnType)
    {
        switch (TurnType)
        {
        case ESlotTurnType::LeftTurn:   return LOCTEXT("LeftTurn", "Left Turn");
        case ESlotTurnType::Straight:   return LOCTEXT("Straight", "Straight");
        case ESlotTurnType::RightTurn:  return LOCTEXT("RightTurn", "Right Turn");
        default:                        return LOCTEXT("Unknown", "Unknown");
        }
    }

    /** 根据值找到共享指针 */
    TSharedPtr<ESlotTurnType> GetSharedTurnType(ESlotTurnType TurnType)
    {
        for (auto& Option : TurnTypeOptions)
        {
            if (*Option == TurnType)
            {
                return Option;
            }
        }
        return nullptr;
    }
};

void FEditorUtilitiesModule::StartupModule()
{
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

    PropertyModule.RegisterCustomClassLayout(
        "ZoneShape",
        FOnGetDetailCustomizationInstance::CreateStatic(&FZoneShapeDetailsCustomization::MakeInstance)
    );

    PropertyModule.RegisterCustomClassLayout(
        "ZoneShapeComponent",
        FOnGetDetailCustomizationInstance::CreateStatic(&FZoneShapeDetailsCustomization::MakeInstance)
    );

    UE_LOG(LogTemp, Warning, TEXT("EditorUtilities:Registering ZoneShapeActor customization..."));
}

void FEditorUtilitiesModule::ShutdownModule()
{
    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
    {
        FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

        PropertyModule.UnregisterCustomClassLayout("ZoneShape");
        PropertyModule.UnregisterCustomClassLayout("ZoneShapeComponent");
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEditorUtilitiesModule, EditorUtilities)
