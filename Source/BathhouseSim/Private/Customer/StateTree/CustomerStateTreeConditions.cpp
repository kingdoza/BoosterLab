#include "Customer/StateTree/CustomerStateTreeConditions.h"

#include "Customer/CustomerSessionComponent.h"
#include "StateTreeExecutionContext.h"

bool FCustomerSessionStateCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	bool bResult = false;
	if (Data.Session)
	{
		switch (Data.Condition)
		{
		case ECustomerSessionCondition::HasAssignedKey:
			bResult = Data.Session->HasAssignedKey();
			break;
		case ECustomerSessionCondition::CheckInTimedOut:
			bResult = Data.Session->DidCheckInTimeOut();
			break;
		case ECustomerSessionCondition::QueueFront:
			bResult = Data.Session->IsQueueFront();
			break;
		case ECustomerSessionCondition::HasFacility:
			bResult = Data.Session->HasCurrentFacility();
			break;
		case ECustomerSessionCondition::BathStayExpired:
			bResult = Data.Session->IsBathStayExpired();
			break;
		case ECustomerSessionCondition::CashClaimed:
			bResult = Data.Session->IsCashClaimed();
			break;
		case ECustomerSessionCondition::TechnicalAbort:
			bResult = Data.Session->IsTechnicalAbort();
			break;
		case ECustomerSessionCondition::Finished:
			bResult = Data.Session->IsFinished();
			break;
		default:
			break;
		}
	}
	return Data.bInvert ? !bResult : bResult;
}

bool FCustomerBathTimeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	const bool bResult = Data.Session && !Data.Session->IsBathStayExpired() && Data.Session->GetRemainingBathStaySeconds() > 0.0f;
	return Data.bInvert ? !bResult : bResult;
}
