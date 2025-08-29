#pragma once

#include "CoreMinimal.h"

class FEditorUtilities : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
