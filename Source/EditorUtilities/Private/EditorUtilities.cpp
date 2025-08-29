// ZoneShapeEditorExtensions.cpp
#include "ZoneShapeEditorExtensions.h"
#include "PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "ZoneShapeEdgeConfig.h"
#include "Components/ZoneShapeComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"

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

        for (auto& Obj : Objects)
        {
            if (UZoneShapeComponent* ZoneShape = Cast<UZoneShapeComponent>(Obj.Get()))
            {
                IDetailCategoryBuilder& CustomCategory = DetailBuilder.EditCategory("Polygon Edge Config");

                CustomCategory.AddCustomRow(LOCTEXT("PolygonEdgeConfig", "Polygon Edge Config"))
                .WholeRowContent()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("EditEdgeConfigButton", "编辑边配置"))
                    .OnClicked_Lambda([ZoneShape]()
                    {
                        OpenEdgeConfigWindow(ZoneShape);
                        return FReply::Handled();
                    })
                ];
            }
        }
    }

    static void OpenEdgeConfigWindow(UZoneShapeComponent* ZoneShape)
    {
        // 创建一个简单的窗口
        TSharedRef<SWindow> Window = SNew(SWindow)
            .Title(LOCTEXT("EdgeConfigWindowTitle", "Polygon Edge Config"))
            .ClientSize(FVector2D(400, 300))
            .SupportsMinimize(false)
            .SupportsMaximize(false);

        // 在这里你可以展示你的 Edge 配置列表，这里给一个简单示例
        Window->SetContent(
            SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("编辑 %s 的边配置"), *ZoneShape->GetName())))
            ]
            +SVerticalBox::Slot().FillHeight(1.f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("EdgeListPlaceholder", "这里可以用 SListView 展示每条边和 Lane 配置"))
            ]
        );

        FSlateApplication::Get().AddWindow(Window);
    }
};

void FEditorUtilities::StartupModule()
{
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyModule.RegisterCustomClassLayout(
        "ZoneShapeComponent",
        FOnGetDetailCustomizationInstance::CreateStatic(&FZoneShapeDetailsCustomization::MakeInstance)
    );
}

void FEditorUtilities::ShutdownModule()
{
    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
    {
        FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PropertyModule.UnregisterCustomClassLayout("ZoneShapeComponent");
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FZoneShapeEditorExtensionsModule, ZoneShapeEditorExtensions)
