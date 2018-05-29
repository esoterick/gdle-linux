
#include "StdAfx.h"
#include "WeenieObject.h"
#include "ManaStone.h"
#include "Player.h"

CManaStoneWeenie::CManaStoneWeenie()
{
	SetName("ManaStone");
	m_Qualities.m_WeenieType = ManaStone_WeenieType;
}

CManaStoneWeenie::~CManaStoneWeenie()
{
}

void CManaStoneWeenie::ApplyQualityOverrides()
{
}

int CManaStoneWeenie::UseWith(std::shared_ptr<CPlayerWeenie> player, std::shared_ptr<CWeenieObject> with)
{
	CManaStoneUseEvent *useEvent = new CManaStoneUseEvent;
	useEvent->_target_id = with->GetID();
	useEvent->_tool_id = GetID();
	useEvent->_max_use_distance = 1.0; // todo: change to 0?
	player->ExecuteUseEvent(useEvent);

	return WERROR_NONE;
}

/**
TODO: move logic from player useex to here
int CManaStoneWeenie::DoUseWithResponse(std::shared_ptr<CWeenieObject> player, std::shared_ptr<CWeenieObject> with) {

}
**/

void CManaStoneUseEvent::OnReadyToUse()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		Cancel(WERROR_OBJECT_GONE);
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
		error = tool->DoUseWithResponse(pWeenie, target);
	}

	Done(error);
}
