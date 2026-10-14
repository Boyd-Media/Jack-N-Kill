#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ClearStalkerInvestigation.generated.h"

UCLASS()
class JACKNKILL_API UBTTask_ClearStalkerInvestigation : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_ClearStalkerInvestigation();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
