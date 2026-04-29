#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MassSmartObjectBehaviorDefinition.h"
#include "SOBD_QueueSlot.generated.h"

UENUM(BlueprintType)
enum class EQueueSlotRole : uint8
{
	Tail,
	Wait,
	Service,
	Exit
};

UCLASS(EditInlineNew)
class TRAFFICSIM_API USOBD_QueueSlot : public USmartObjectMassBehaviorDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Queue")
	EQueueSlotRole Role = EQueueSlotRole::Wait;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Queue", meta = (ClampMin = "0.0"))
	float ServiceDuration = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Queue")
	bool bSendAdvanceOnExit = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Queue")
	FGameplayTagContainer QueueTags;
};
