#include "stdafx.h"
#include "MAllegianceRecallHometown_02AB.h"
#include "AllegianceManager.h"

MAllegianceRecallHometown_02AB::MAllegianceRecallHometown_02AB(CPlayerWeenie * player)
{
	m_pPlayer = player;
}

void MAllegianceRecallHometown_02AB::Parse(BinaryReader * reader)
{
	Process();
}

void MAllegianceRecallHometown_02AB::Process()
{
	// TODO: move this somewhere more appropriate
	if (m_pPlayer->CheckPKActivity())
	{
		m_pPlayer->NotifyWeenieError(WERROR_PORTAL_PK_ATTACKED_TOO_RECENTLY);
		return;
	}

	AllegianceTreeNode *allegianceNode = g_pAllegianceManager->GetTreeNode(m_pPlayer->GetID());

	if (!allegianceNode)
	{
		m_pPlayer->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	AllegianceInfo *allegianceInfo = g_pAllegianceManager->GetInfo(allegianceNode->_monarchID);

	if (allegianceInfo && allegianceInfo->_info.m_BindPoint.objcell_id)
	{
		m_pPlayer->ExecuteUseEvent(new CAllegianceHometownRecallUseEvent());
	}
	else
	{
		m_pPlayer->NotifyWeenieError(WERROR_ALLEGIANCE_HOMETOWN_NOT_SET);
	}
}
