#include "AI/BTTask_ClearStalkerInvestigation.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_ClearStalkerInvestigation::UBTTask_ClearStalkerInvestigation()
{
    NodeName = TEXT("Clear Stalker Investigation");
}

EBTNodeResult::Type UBTTask_ClearStalkerInvestigation::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB)
    {
        return EBTNodeResult::Failed;
    }
    BB->SetValueAsBool(TEXT("HasInvestigation"), false);
    BB->ClearValue(TEXT("InvestigationLocation"));
    return EBTNodeResult::Succeeded;
}
