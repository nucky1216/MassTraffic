// ZoneShapeEditorExtensions.cpp

#include "EditorUtilities.h"
#include "PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "ZoneShapeActor.h"
#include "ZoneShapeComponent.h"
#include "ZoneShapeEdgeConfig.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "TrafficTypes.h"
#include "ZoneShapeActor.h"
#include "ZoneShapeComponent.h"
#include "TrafficLightSubsystem.h"

#define LOCTEXT_NAMESPACE "FZoneShapeEditorExtensionsModule"

// =============================
// 自定义 Details
// =============================
class FZoneShapeDetailsCustomization : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance()
    {
        return MakeShareable(new FZoneShapeDetailsCustomization);
    }

    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
    {
        // 保存原有属性


        // 添加自定义分类
        IDetailCategoryBuilder& CustomCategory = DetailBuilder.EditCategory(
            "Polygon Edge Config",
            FText::FromString("Polygon Edge Config"),
            ECategoryPriority::Important
        );

        // 获取被选中的 Actor
        TArray<TWeakObjectPtr<UObject>> Objects;
        DetailBuilder.GetObjectsBeingCustomized(Objects);

        UZoneShapeComponent* ZoneShapeComp = nullptr;
        for (auto& Obj : Objects)
        {
            if (AZoneShape* ZoneShapeActor = Cast<AZoneShape>(Obj.Get()))
            {
                ZoneShapeComp = ZoneShapeActor->GetComponentByClass<UZoneShapeComponent>();
                break;
            }
            else if (UZoneShapeComponent* Comp = Cast<UZoneShapeComponent>(Obj.Get()))
            {
                ZoneShapeComp = Comp;
                break;
			}
        }
        if(!ZoneShapeComp)
        {
			UE_LOG(LogTemp, Warning, TEXT("FZoneShapeDetailsCustomization: No ZoneShapeComponent found."));
            return; // 没有找到 ZoneShapeComponent，退出
		}

        // 初始化枚举下拉选项
        TurnTypeOptions.Empty();
        TurnTypeOptions.Add(MakeShared<ESlotTurnType>(ESlotTurnType::RightTurn));
        TurnTypeOptions.Add(MakeShared<ESlotTurnType>(ESlotTurnType::StraightRight));
        TurnTypeOptions.Add(MakeShared<ESlotTurnType>(ESlotTurnType::Straight));
        TurnTypeOptions.Add(MakeShared<ESlotTurnType>(ESlotTurnType::LeftTurn));
        TurnTypeOptions.Add(MakeShared<ESlotTurnType>(ESlotTurnType::StraightLeft));

        // 模拟固定数据：四条边，每条边三个slot
        if (EdgeTurnTypes.Num() == 0)
        {
            EdgeTurnTypes.SetNum(4); // 4 edges
            for (int32 EdgeIdx = 0; EdgeIdx < 4; ++EdgeIdx)
            {
                EdgeTurnTypes[EdgeIdx].SetNum(3); // 每条边3个slot
                for (int32 SlotIdx = 0; SlotIdx < 3; ++SlotIdx)
                {
                    EdgeTurnTypes[EdgeIdx][SlotIdx] = ESlotTurnType::Straight;
                }
            }
        }
        
        // 为每个 Edge 构建 UI
        for (int32 EdgeIndex = 0; EdgeIndex < EdgeTurnTypes.Num(); ++EdgeIndex)
        {
            IDetailGroup& EdgeGroup = CustomCategory.AddGroup(
                *FString::Printf(TEXT("Edge_%d"), EdgeIndex),
                FText::FromString(FString::Printf(TEXT("Edge %d"), EdgeIndex))
            );

            for (int32 SlotIndex = 0; SlotIndex < EdgeTurnTypes[EdgeIndex].Num(); ++SlotIndex)
            {
                EdgeGroup.AddWidgetRow()
                    .NameContent()
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("Slot %d"), SlotIndex)))
                            .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                    .ValueContent()
                    .MinDesiredWidth(200.0f)
                    [
                        SNew(SComboBox<TSharedPtr<ESlotTurnType>>)
                            .OptionsSource(&TurnTypeOptions)
                            .OnGenerateWidget(this, &FZoneShapeDetailsCustomization::MakeTurnTypeWidget)
                            .OnSelectionChanged_Lambda([this, EdgeIndex, SlotIndex](TSharedPtr<ESlotTurnType> NewSelection, ESelectInfo::Type)
                                {
                                    if (NewSelection.IsValid())
                                    {
                                        EdgeTurnTypes[EdgeIndex][SlotIndex] = *NewSelection;
                                        UE_LOG(LogTemp, Log, TEXT("Edge %d Slot %d changed to %s"),
                                            EdgeIndex, SlotIndex, *GetTurnTypeText(*NewSelection).ToString());
                                    }
                                })
                            .InitiallySelectedItem(GetSharedTurnType(EdgeTurnTypes[EdgeIndex][SlotIndex]))
                            [
                                SNew(STextBlock)
                                    .Text_Lambda([this, EdgeIndex, SlotIndex]()
                                        {
                                            return GetTurnTypeText(EdgeTurnTypes[EdgeIndex][SlotIndex]);
                                        })
                            ]
                    ];
            }
        }
    }

private:
    /** 二维数组：Edges -> Slots */
    TArray<TArray<ESlotTurnType>> EdgeTurnTypes;

    /** ComboBox 下拉选项 */
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
		case ESlotTurnType::StraightLeft: return LOCTEXT("StraightLeft", "Straight Left");
		case ESlotTurnType::StraightRight: return LOCTEXT("StraightRight", "Straight Right");
        default:                        return LOCTEXT("Unknown", "Unknown");
        }
    }

    /** 根据值找到共享指针 */
    TSharedPtr<ESlotTurnType> GetSharedTurnType(ESlotTurnType TurnType) const
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

// =============================
// 模块启动 / 关闭
// =============================
void FEditorUtilitiesModule::StartupModule()
{
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

    PropertyModule.RegisterCustomClassLayout(
        TEXT("ZoneShape"),
        FOnGetDetailCustomizationInstance::CreateStatic(&FZoneShapeDetailsCustomization::MakeInstance));

    PropertyModule.RegisterCustomClassLayout(
        TEXT("ZoneShapeComponent"),
        FOnGetDetailCustomizationInstance::CreateStatic(&FZoneShapeDetailsCustomization::MakeInstance));

    PropertyModule.NotifyCustomizationModuleChanged();
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