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
//#include "../../../UE_5.3/Engine/Plugins/Runtime/ZoneGraph/Source/ZoneGraphEditor/Private/ZoneShapeComponentVisualizer.h"
#include "IDetailChildrenBuilder.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "FZoneShapeEditorExtensionsModule"

class FZoneShapeComponentVisualizer;

class FZoneShapePointDetails : public IDetailCustomNodeBuilder, public TSharedFromThis<FZoneShapePointDetails>
{
public:
    FZoneShapePointDetails(UZoneShapeComponent& InOwningComponent)
        : ShapeComp(&InOwningComponent)
    {
        // 获取 Visualizer
        TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(InOwningComponent.GetClass());
        ShapeCompVisualizer = StaticCastSharedPtr<FZoneShapeComponentVisualizer>(Visualizer);

    }

    virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override {}
    virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override
    {
        // 展示选中点数量
        ChildrenBuilder.AddCustomRow(FText::FromString("Selected Points"))
            [SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("当前选中控制点数量: %d"), SelectedPoints.Num())))];
    }

    virtual void Tick(float DeltaTime) override
    {
        // 每帧监听选中点变化
        if (ShapeCompVisualizer.IsValid())
        {
            SelectedPoints = ShapeCompVisualizer->GetSelectedPoints();
        }
    }

    virtual bool RequiresTick() const override { return true; }
    virtual FName GetName() const override { return FName("ZoneShapePointDetails"); }

private:
    UZoneShapeComponent* ShapeComp;
    TSet<int32> SelectedPoints;
    TSharedPtr<FZoneShapeComponentVisualizer> ShapeCompVisualizer;
};

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
        UE_LOG(LogTemp, Warning, TEXT("EditorUtilities: Read to Selected object class"));
        for (auto& Obj : Objects)
        {
            UE_LOG(LogTemp, Warning, TEXT("Selected object class: %s"), *Obj->GetClass()->GetName());
            // 情况 1: Actor
            if (AZoneShape* ZoneShapeActor = Cast<AZoneShape>(Obj.Get()))
            {
                AddEdgeConfigCategory(DetailBuilder, ZoneShapeActor->GetComponentByClass<UZoneShapeComponent>(), ZoneShapeActor->GetName());
            }
            // 情况 2: Component
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

        CustomCategory.AddCustomRow(LOCTEXT("PolygonEdgeConfig", "Polygon Edge Config"))
            .WholeRowContent()
            [
                SNew(SButton)
                    .Text(LOCTEXT("EditEdgeConfigButton", "编辑边配置"))
                    .OnClicked_Lambda([ZoneShapeComp]()
                        {
                            OpenEdgeConfigWindow(ZoneShapeComp);
                            return FReply::Handled();
                        })
            ];
    }

    static void OpenEdgeConfigWindow(UZoneShapeComponent* ZoneShape)
    {
        // 创建一个简单的窗口
        TSharedRef<SWindow> Window = SNew(SWindow)
            .Title(LOCTEXT("EdgeConfigWindowTitle", "Polygon Edge Config"))
            .ClientSize(FVector2D(400, 300))
            .SupportsMinimize(false)
            .SupportsMaximize(false);

        Window->SetContent(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("编辑 %s 的边配置"), *ZoneShape->GetName())))
            ]
            + SVerticalBox::Slot().FillHeight(1.f)
            [
                SNew(STextBlock)
                    .Text(LOCTEXT("EdgeListPlaceholder", "这里可以用 SListView 展示每条边和 Lane 配置"))
            ]
        );

        FSlateApplication::Get().AddWindow(Window);
    }
};

void FEditorUtilitiesModule::StartupModule()
{
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

    // 注册 Actor 的自定义 Details
    PropertyModule.RegisterCustomClassLayout(
        "ZoneShape",
        FOnGetDetailCustomizationInstance::CreateStatic(&FZoneShapeDetailsCustomization::MakeInstance)
    );

    // 注册 Component 的自定义 Details
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
