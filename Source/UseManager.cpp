
#include "StdAfx.h"
#include "UseManager.h"
#include "WeenieObject.h"
#include "World.h"
#include "Container.h"
#include "Player.h"

// TODO fix memory leak with use data

CUseEventData::CUseEventData()
{
}

void CUseEventData::Update()
{
	CheckTimeout();
}

void CUseEventData::SetupUse()
{	
	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (target)
	{
		_max_use_distance = target->InqFloatQuality(USE_RADIUS_FLOAT, 0.0);
	}

	_timeout = Timer::cur_time + 15.0;
}

void CUseEventData::Begin()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	SetupUse();

	if (_target_id)
	{
		std::shared_ptr<CWeenieObject> target = GetTarget();
		if (!target)
		{
			Cancel(WERROR_OBJECT_GONE);
			return;
		}

		if (target->IsContained())
		{
			bool bInViewedContainer = false;

			if (!pWeenie->FindContainedItem(target->GetID()))
			{
				bool bInViewedContainer = false;
				if (std::shared_ptr<CContainerWeenie> externalContainer = target->GetWorldTopLevelContainer())
				{
					if (externalContainer->_openedById == pWeenie->GetID())
					{
						bInViewedContainer = true;
					}
				}

				if (!bInViewedContainer)
				{
					Cancel(WERROR_OBJECT_GONE);
					return;
				}
			}

			OnReadyToUse();
		}
		else
		{
			if (InUseRange())
			{
				OnReadyToUse();
			}
			else
			{
				MoveToUse();
			}
		}
	}
	else
	{
		OnReadyToUse();
	}
}

void CUseEventData::MoveToUse()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	_move_to = true;

	MovementParameters params;
	params.min_distance = _max_use_distance; //a little leeway on item move to range
	params.action_stamp = ++_weenie->m_wAnimSequence;
	_weenie->last_move_was_autonomous = false;
	if (CWeenieObject *target = GetTarget())
	{
		if (target->AsPortal())
		{
			params.use_spheres = 0;
		}
	}

	_weenie->MoveToObject(_target_id, &params);
}

void CUseEventData::CheckTimeout()
{
	if (Timer::cur_time > _timeout)
	{
		if (_move_to)
			Cancel(WERROR_MOVED_TOO_FAR);
		else
			Cancel(0);
	}
}

void CUseEventData::Cancel(DWORD error)
{
	CancelMoveTo();

	_manager->OnUseCancelled(error);
}

void CUseEventData::CancelMoveTo()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	if (_move_to)
	{
		pWeenie->cancel_moveto();
		pWeenie->Animation_MoveToUpdate();

		_move_to = false;
	}
}

double CUseEventData::DistanceToTarget()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return DBL_MAX;
	}

	if (!_target_id || _target_id == pWeenie->GetID())
		return 0.0;

	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target)
		return FLT_MAX;
	if (target->AsPortal())
	{
		return _weenie->DistanceTo(target, false);
	}

	return _weenie->DistanceTo(target, true);
}

double CUseEventData::HeadingDifferenceToTarget()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return 0;
	}

	if (!_target_id || _target_id == pWeenie->GetID())
		return 0.0;

	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target)
		return 0.0;

	return pWeenie->get_heading();
}

bool CUseEventData::InUseRange()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return false;
	}

	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (target && (pWeenie->IsContainedWithinViewable(target->GetID())))
		return true;

	if ((_max_use_distance + F_EPSILON) < (DistanceToTarget()))
		return false;

	return true;
}

std::shared_ptr<CWeenieObject> CUseEventData::GetTarget()
{
	return g_pWorld->FindObject(_target_id);
}

std::shared_ptr<CWeenieObject> CUseEventData::GetTool()
{
	return g_pWorld->FindObject(_tool_id);
}

void CUseEventData::HandleMoveToDone(DWORD error)
{
	_move_to = false;

	if (error)
	{
		Cancel(error);
		return;
	}

	if (!InUseRange())
	{
		Cancel(WERROR_TOO_FAR);
		return;
	}

	OnReadyToUse();
}

void CUseEventData::OnMotionDone(DWORD motion, BOOL success)
{
	if (_move_to || !_active_use_anim)
		return;

	if (motion == _active_use_anim)
	{
		_active_use_anim = 0;

		if (success)
		{
			OnUseAnimSuccess(motion);
		}
		else
		{
			Cancel();
		}
	}	
}

void CUseEventData::OnUseAnimSuccess(DWORD motion)
{
	Done();
}

void CUseEventData::Done(DWORD error)
{
	_manager->OnUseDone(error);
}

void CUseEventData::ExecuteUseAnimation(DWORD motion, MovementParameters *params)
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	assert (!_move_to);
	assert (!_active_use_anim);

	if (pWeenie->IsDead() || pWeenie->IsInPortalSpace())
	{
		Cancel(WERROR_ACTIONS_LOCKED);
		return;
	}

	_active_use_anim = motion;

	DWORD error = pWeenie->DoForcedMotion(motion, params);

	if (error)
	{
		Cancel(error);
	}
}

void CGenericUseEvent::OnReadyToUse()
{
	if (_do_use_animation)
	{
		ExecuteUseAnimation(_do_use_animation);
	}
	else
	{
		Finish();
	}
}

void CGenericUseEvent::OnUseAnimSuccess(DWORD motion)
{
	Finish();
}

void CGenericUseEvent::Finish()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	std::shared_ptr<CWeenieObject> tool = GetTool();
	if (!tool && _tool_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	int error = WERROR_NONE;

	if (target)
	{
		if (!InUseRange())
		{
			Cancel(WERROR_TOO_FAR);
			return;
		}
		
		if (_do_use_response)
		{
			if (_tool_id)
			{
				error = tool->DoUseWithResponse(pWeenie, target);
			}
			else
			{
				error = target->DoUseResponse(pWeenie);
			}
		}

		if (error == WERROR_NONE && _do_use_emote)
		{
			target->DoUseEmote(pWeenie);
		}

		if (_do_use_message)
		{
			if (error == WERROR_NONE)
			{
				std::string useMessage;
				if (target->m_Qualities.InqString(USE_MESSAGE_STRING, useMessage))
				{
					pWeenie->SendText(useMessage.c_str(), LTT_MAGIC);
				}
			}
			else
			{
				std::string failMessage;
				if (target->m_Qualities.InqString(ACTIVATION_FAILURE_STRING, failMessage))
				{
					pWeenie->SendText(failMessage.c_str(), LTT_MAGIC);
				}
			}
		}
	}

	Done(error);
}

void CActivationUseEvent::OnReadyToUse()
{
	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (target)
	{
		std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

		if (!pWeenie)
		{
			return;
		}
		target->Activate(pWeenie->GetID());
	}

	Done();
}

void CInventoryUseEvent::SetupUse()
{
	_max_use_distance = 1.0;
	_timeout = Timer::cur_time + 15.0;
}

//-------------------------------------------------------------------------------------

void CPickupInventoryUseEvent::OnReadyToUse()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	std::shared_ptr<CWeenieObject> target = GetTarget();

	if (target->HasOwner()) {
		
		if (this->_target_container_id == pWeenie->GetID())
			target = target->GetWorldTopLevelOwner();
		else
			target = g_pWorld->FindObject(this->_target_container_id);
	}

	float z1 = target->m_Position.frame.m_origin.z;
	float z2 = pWeenie->m_Position.frame.m_origin.z;

	if (z1 - z2 >= 1.9)
		ExecuteUseAnimation(Motion_Pickup20);
	else if (z1 - z2 >= 1.4)
		ExecuteUseAnimation(Motion_Pickup15);
	else if (z1 - z2 >= 0.9)
		ExecuteUseAnimation(Motion_Pickup10);
	else
		ExecuteUseAnimation(Motion_Pickup);
}

void CPickupInventoryUseEvent::OnUseAnimSuccess(DWORD motion)
{

	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	if (!InUseRange())
	{
		Cancel(WERROR_TOO_FAR);
		return;
	}

	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	std::string questString;
	if (target->m_Qualities.InqString(QUEST_STRING, questString) && !questString.empty())
	{
		if (pWeenie->InqQuest(questString.c_str()))
		{
			int timeTilOkay = pWeenie->InqTimeUntilOkayToComplete(questString.c_str());

			if (timeTilOkay > 0)
			{
				int secs = timeTilOkay % 60;
				timeTilOkay /= 60;

				int mins = timeTilOkay % 60;
				timeTilOkay /= 60;

				int hours = timeTilOkay % 24;
				timeTilOkay /= 24;

				int days= timeTilOkay;

				pWeenie->SendText(csprintf("You cannot complete this quest for another %dd %dh %dm %ds.", days, hours, mins, secs), LTT_DEFAULT);
			}

			pWeenie->DoForcedStopCompletely();
			Cancel(WERROR_QUEST_SOLVED_TOO_RECENTLY);
			return;
		}

		pWeenie->StampQuest(questString.c_str());
		target->m_Qualities.SetString(QUEST_STRING, "");
	}

	bool success = pWeenie->AsPlayer()->MoveItemToContainer(_source_item_id, _target_container_id, _target_slot, true);

	pWeenie->DoForcedStopCompletely();

	if (!success)
		Cancel();
	else
		Done();
}

//-------------------------------------------------------------------------------------

void CDropInventoryUseEvent::OnReadyToUse()
{
	ExecuteUseAnimation(Motion_Pickup);
}

void CDropInventoryUseEvent::OnUseAnimSuccess(DWORD motion)
{

	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	if (!InUseRange())
	{
		Cancel(WERROR_TOO_FAR);
		return;
	}

	if (target->IsAttunedOrContainsAttuned())
	{
		Cancel(WERROR_ATTUNED_ITEM);
		return;
	}

	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	bool success = pWeenie->AsPlayer()->MoveItemTo3D(_target_id, true);

	pWeenie->DoForcedStopCompletely();

	if (!success)
		Cancel();
	else
		Done();
}

//-------------------------------------------------------------------------------------

void CMoveToWieldInventoryUseEvent::OnReadyToUse()
{
	ExecuteUseAnimation(Motion_Pickup);
}

void CMoveToWieldInventoryUseEvent::OnUseAnimSuccess(DWORD motion)
{

	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	if (!InUseRange())
	{
		Cancel(WERROR_TOO_FAR);
		return;
	}

	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	std::string questString;
	if (target->m_Qualities.InqString(QUEST_STRING, questString) && !questString.empty())
	{
		if (pWeenie->InqQuest(questString.c_str()))
		{
			int timeTilOkay = pWeenie->InqTimeUntilOkayToComplete(questString.c_str());

			if (timeTilOkay > 0)
			{
				int secs = timeTilOkay % 60;
				timeTilOkay /= 60;

				int mins = timeTilOkay % 60;
				timeTilOkay /= 60;

				int hours = timeTilOkay % 24;
				timeTilOkay /= 24;

				int days = timeTilOkay;

				pWeenie->SendText(csprintf("You cannot complete this quest for another %dd %dh %dm %ds.", days, hours, mins, secs), LTT_DEFAULT);
			}

			pWeenie->DoForcedStopCompletely();
			Cancel(WERROR_QUEST_SOLVED_TOO_RECENTLY);
			return;
		}

		pWeenie->StampQuest(questString.c_str());
		target->m_Qualities.SetString(QUEST_STRING, "");
	}

	bool success = pWeenie->AsPlayer()->MoveItemToWield(_sourceItemId, _targetLoc, true);

	pWeenie->DoForcedStopCompletely();

	if (!success)
		Cancel();
	else
		Done();
}

//-------------------------------------------------------------------------------------

void CStackMergeInventoryUseEvent::OnReadyToUse()
{
	ExecuteUseAnimation(Motion_Pickup);
}

void CStackMergeInventoryUseEvent::OnUseAnimSuccess(DWORD motion)
{
	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	if (!InUseRange())
	{
		Cancel(WERROR_TOO_FAR);
		return;
	}

	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	std::string questString;
	if (target->m_Qualities.InqString(QUEST_STRING, questString) && !questString.empty())
	{
		if (pWeenie->InqQuest(questString.c_str()))
		{
			int timeTilOkay = pWeenie->InqTimeUntilOkayToComplete(questString.c_str());

			if (timeTilOkay > 0)
			{
				int secs = timeTilOkay % 60;
				timeTilOkay /= 60;

				int mins = timeTilOkay % 60;
				timeTilOkay /= 60;

				int hours = timeTilOkay % 24;
				timeTilOkay /= 24;

				int days = timeTilOkay;

				pWeenie->SendText(csprintf("You cannot complete this quest for another %dd %dh %dm %ds.", days, hours, mins, secs), LTT_DEFAULT);
			}

			pWeenie->DoForcedStopCompletely();
			Cancel(WERROR_QUEST_SOLVED_TOO_RECENTLY);
			return;
		}

		pWeenie->StampQuest(questString.c_str());
		target->m_Qualities.SetString(QUEST_STRING, "");
	}

	bool success = pWeenie->AsPlayer()->MergeItem(_sourceItemId, _targetItemId, _amountToTransfer, true);

	pWeenie->DoForcedStopCompletely();

	if (!success)
		Cancel();
	else
		Done();
}

//-------------------------------------------------------------------------------------

void CStackSplitToContainerInventoryUseEvent::OnReadyToUse()
{
	ExecuteUseAnimation(Motion_Pickup);
}

void CStackSplitToContainerInventoryUseEvent::OnUseAnimSuccess(DWORD motion)
{
	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	if (!InUseRange())
	{
		Cancel(WERROR_TOO_FAR);
		return;
	}

	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	std::string questString;
	if (target->m_Qualities.InqString(QUEST_STRING, questString) && !questString.empty())
	{
		std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

		if (!pWeenie)
		{
			return;
		}

		if (pWeenie->InqQuest(questString.c_str()))
		{
			int timeTilOkay = pWeenie->InqTimeUntilOkayToComplete(questString.c_str());

			if (timeTilOkay > 0)
			{
				int secs = timeTilOkay % 60;
				timeTilOkay /= 60;

				int mins = timeTilOkay % 60;
				timeTilOkay /= 60;

				int hours = timeTilOkay % 24;
				timeTilOkay /= 24;

				int days = timeTilOkay;

				pWeenie->SendText(csprintf("You cannot complete this quest for another %dd %dh %dm %ds.", days, hours, mins, secs), LTT_DEFAULT);
			}

			pWeenie->DoForcedStopCompletely();
			Cancel(WERROR_QUEST_SOLVED_TOO_RECENTLY);
			return;
		}

		pWeenie->StampQuest(questString.c_str());
		target->m_Qualities.SetString(QUEST_STRING, "");
	}

	bool success = pWeenie->AsPlayer()->SplitItemToContainer(_sourceItemId, _targetContainerId, _targetSlot, _amountToTransfer, true);

	pWeenie->DoForcedStopCompletely();

	if (!success)
		Cancel();
	else
		Done();
}

//-------------------------------------------------------------------------------------


void CStackSplitTo3DInventoryUseEvent::OnReadyToUse()
{
	ExecuteUseAnimation(Motion_Pickup);
}

void CStackSplitTo3DInventoryUseEvent::OnUseAnimSuccess(DWORD motion)
{
	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	if (!InUseRange())
	{
		Cancel(WERROR_TOO_FAR);
		return;
	}

	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	std::string questString;
	if (target->m_Qualities.InqString(QUEST_STRING, questString) && !questString.empty())
	{
		if (pWeenie->InqQuest(questString.c_str()))
		{
			int timeTilOkay = pWeenie->InqTimeUntilOkayToComplete(questString.c_str());

			if (timeTilOkay > 0)
			{
				int secs = timeTilOkay % 60;
				timeTilOkay /= 60;

				int mins = timeTilOkay % 60;
				timeTilOkay /= 60;

				int hours = timeTilOkay % 24;
				timeTilOkay /= 24;

				int days = timeTilOkay;

				pWeenie->SendText(csprintf("You cannot complete this quest for another %dd %dh %dm %ds.", days, hours, mins, secs), LTT_DEFAULT);
			}

			pWeenie->DoForcedStopCompletely();
			Cancel(WERROR_QUEST_SOLVED_TOO_RECENTLY);
			return;
		}

		pWeenie->StampQuest(questString.c_str());
		target->m_Qualities.SetString(QUEST_STRING, "");
	}

	bool success = pWeenie->AsPlayer()->SplitItemto3D(_sourceItemId, _amountToTransfer, true);

	pWeenie->DoForcedStopCompletely();

	if (!success)
		Cancel();
	else
		Done();
}

//-------------------------------------------------------------------------------------

void CStackSplitToWieldInventoryUseEvent::OnReadyToUse()
{
	ExecuteUseAnimation(Motion_Pickup);
}

void CStackSplitToWieldInventoryUseEvent::OnUseAnimSuccess(DWORD motion)
{
	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	if (!InUseRange())
	{
		Cancel(WERROR_TOO_FAR);
		return;
	}
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

	if (!pWeenie)
	{
		return;
	}

	std::string questString;
	if (target->m_Qualities.InqString(QUEST_STRING, questString) && !questString.empty())
	{
		if (pWeenie->InqQuest(questString.c_str()))
		{
			int timeTilOkay = pWeenie->InqTimeUntilOkayToComplete(questString.c_str());

			if (timeTilOkay > 0)
			{
				int secs = timeTilOkay % 60;
				timeTilOkay /= 60;

				int mins = timeTilOkay % 60;
				timeTilOkay /= 60;

				int hours = timeTilOkay % 24;
				timeTilOkay /= 24;

				int days = timeTilOkay;

				pWeenie->SendText(csprintf("You cannot complete this quest for another %dd %dh %dm %ds.", days, hours, mins, secs), LTT_DEFAULT);
			}

			pWeenie->DoForcedStopCompletely();
			Cancel(WERROR_QUEST_SOLVED_TOO_RECENTLY);
			return;
		}

		pWeenie->StampQuest(questString.c_str());
		target->m_Qualities.SetString(QUEST_STRING, "");
	}

	bool success = pWeenie->AsPlayer()->SplitItemToWield(_sourceItemId, _targetLoc, _amountToTransfer, true);

	pWeenie->DoForcedStopCompletely();

	if (!success)
		Cancel();
	else
		Done();
}

//-------------------------------------------------------------------------------------

UseManager::UseManager(std::shared_ptr<CWeenieObject> weenie)
{
	_weenie = weenie;
}

UseManager::~UseManager()
{
	SafeDelete(_useData);
	SafeDelete(_cleanupData);
}

void UseManager::MarkForCleanup(CUseEventData *data)
{
	if (_cleanupData && _cleanupData != data)
	{
		delete _cleanupData;
	}

	_cleanupData = data;
}

void UseManager::Cancel()
{
	if (_useData)
	{
		_useData->Cancel();
	}
}

void UseManager::OnUseCancelled(DWORD error)
{

	if (_useData)
	{
		std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

		if (!pWeenie)
		{
			return;
		}

		if (!_useData->IsInventoryEvent())
		{
			pWeenie->NotifyUseDone(error);
		}
		else
		{
			pWeenie->NotifyInventoryFailedEvent(_useData->_target_id, error);
		}

		MarkForCleanup(_useData);
		_useData = NULL;
	}
}

void UseManager::OnUseDone(DWORD error)
{
	if (_useData)
	{
		std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

		if (!pWeenie)
		{
			return;
		}

		if (!_useData->IsInventoryEvent())
		{
			pWeenie->NotifyUseDone(error);
		}
		else
		{
		}

		MarkForCleanup(_useData);
		_useData = NULL;
	}
}

void UseManager::Update()
{
	if (_useData)
	{
		_useData->Update();
	}

	SafeDelete(_cleanupData);
}

void UseManager::OnDeath(DWORD killer_id)
{
	Cancel();
}

void UseManager::HandleMoveToDone(DWORD error)
{
	if (_useData)
	{
		_useData->HandleMoveToDone(error);
	}
}

void UseManager::OnMotionDone(DWORD motion, BOOL success)
{
	if (_useData)
	{
		_useData->OnMotionDone(motion, success);
	}
}

bool UseManager::IsUsing()
{
	return _useData != NULL ? true : false;
}

void UseManager::BeginUse(CUseEventData *data)
{
	if (_useData)
	{
		std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();

		if (!pWeenie)
		{
			return;
		}

		// already busy
		if (!data->IsInventoryEvent())
		{
			pWeenie->NotifyWeenieError(WERROR_ACTIONS_LOCKED);
			pWeenie->NotifyUseDone(0);
		}
		else
		{
			pWeenie->NotifyInventoryFailedEvent(_useData->_target_id, WERROR_ACTIONS_LOCKED);
		}

		delete data;
		return;
	}

	_useData = data;
	_useData->Begin();
}

