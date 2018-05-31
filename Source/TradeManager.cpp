#include "StdAfx.h"

#include "TradeManager.h"
#include "World.h"

TradeManager* TradeManager::RegisterTrade(std::shared_ptr<CPlayerWeenie>  initiator, std::shared_ptr<CPlayerWeenie>  partner)
{
	return new TradeManager(initiator, partner);
}

TradeManager::TradeManager(std::shared_ptr<CPlayerWeenie> initiator, std::shared_ptr<CPlayerWeenie> partner)
{
	_initiator = initiator;
	_partner = partner;

	BinaryWriter openTrade;
	openTrade.Write<DWORD>(0x1FD);
	openTrade.Write<DWORD>(initiator->GetID());
	openTrade.Write<DWORD>(partner->GetID());
	openTrade.Write<long>(0); // "some kind of stamp"?
	initiator->SendNetMessage(&openTrade, PRIVATE_MSG, TRUE, FALSE);


	BinaryWriter openTradePartner;
	openTradePartner.Write<DWORD>(0x1FD);
	openTradePartner.Write<DWORD>(partner->GetID());
	openTradePartner.Write<DWORD>(initiator->GetID());
	openTradePartner.Write<long>(0); // "some kind of stamp"?
	partner->SendNetMessage(&openTradePartner, PRIVATE_MSG, TRUE, FALSE);
	
	// it's possible for users to have items in the trade window before starting so clear it
	ResetTrade(initiator);
}

void TradeManager::CloseTrade(std::weak_ptr<CPlayerWeenie> playerFrom, DWORD reason)
{
	OnCloseTrade(_initiator, reason);
	OnCloseTrade(_partner, reason);

	Delete();
}

void TradeManager::OnCloseTrade(std::weak_ptr<CPlayerWeenie> player, DWORD reason)
{
	std::shared_ptr<CWeenieObject> pPlayer = player.lock();
	if (pPlayer)
	{
		BinaryWriter closeTrade;
		closeTrade.Write<DWORD>(0x1FF);
		closeTrade.Write<DWORD>(reason);
		pPlayer->SendNetMessage(&closeTrade, PRIVATE_MSG, TRUE, FALSE);
	}
}

void TradeManager::AddToTrade(std::shared_ptr<CPlayerWeenie> playerFrom, DWORD item)
{
	if (!CheckTrade())
		return;

	std::shared_ptr<CWeenieObject> pItem = g_pWorld->FindWithinPVS(playerFrom, item);

	if (!pItem || pItem->GetWorldTopLevelOwner() != playerFrom || pItem->IsAttunedOrContainsAttuned() || pItem->IsWielded())
	{
		playerFrom->SendText("You cannot trade that item!", LTT_ERROR);
		BinaryWriter cannotTrade;
		cannotTrade.Write<DWORD>(0x207);
		cannotTrade.Write<DWORD>(item);
		cannotTrade.Write<DWORD>(0);
		playerFrom->SendNetMessage(&cannotTrade, PRIVATE_MSG, TRUE, FALSE);
		return;
	}

	std::list<DWORD> *itemList;

	if (playerFrom == _initiator.lock())
	{
		itemList = &m_lInitiatorItems;
	}
	else
	{
		itemList = &m_lPartnerItems;
	}

	itemList->push_back(item);

	m_bInitiatorAccepted = false;
	m_bPartnerAccepted = false;

	std::shared_ptr<CPlayerWeenie> pOther = GetOtherPlayer(playerFrom);

	pOther->MakeAware(pItem, true);

	BinaryWriter addToTrade;
	addToTrade.Write<DWORD>(0x200);
	addToTrade.Write<DWORD>(item);
	addToTrade.Write<DWORD>(0x1);
	addToTrade.Write<DWORD>(0);
	playerFrom->SendNetMessage(&addToTrade, PRIVATE_MSG, TRUE, FALSE);

	BinaryWriter addToTradeOther;
	addToTradeOther.Write<DWORD>(0x200);
	addToTradeOther.Write<DWORD>(item);
	addToTradeOther.Write<DWORD>(0x2);
	addToTradeOther.Write<DWORD>(0);
	pOther->SendNetMessage(&addToTradeOther, PRIVATE_MSG, TRUE, FALSE);
}

void TradeManager::AcceptTrade(std::shared_ptr<CPlayerWeenie> playerFrom)
{
	if (!CheckTrade())
		return;

	if (playerFrom == _initiator.lock())
		m_bInitiatorAccepted = true;
	else
		m_bPartnerAccepted = true;

	if (m_bInitiatorAccepted && m_bPartnerAccepted)
	{
		OnTradeAccepted();
	}
	else
	{
		BinaryWriter acceptTrade;
		acceptTrade.Write<DWORD>(0x202);
		acceptTrade.Write<DWORD>(playerFrom->GetID());
		//playerFrom->SendNetMessage(&acceptTrade, PRIVATE_MSG, TRUE, FALSE);
		GetOtherPlayer(playerFrom)->SendNetMessage(&acceptTrade, PRIVATE_MSG, TRUE, FALSE);
	}
}

bool TradeManager::OnTradeAccepted()
{
	std::shared_ptr<CPlayerWeenie> pInitiator = _initiator.lock();
	if (!pInitiator)
	{
		return false;
	}
	std::shared_ptr<CPlayerWeenie> pPartner = _partner.lock();
	if (!pPartner)
	{
		return false;
	}

	bool bError = false;
	if (m_lPartnerItems.size() > pInitiator->Container_GetNumFreeMainPackSlots())
	{
		pInitiator->SendText("You do not have enough pack space to complete this trade!", LTT_ERROR);
		pPartner->SendText((pInitiator->GetName() = " does not have enough pack space to complete this trade!").c_str(), LTT_ERROR);
		bError = true;
	}
	if (m_lInitiatorItems.size() > pPartner->Container_GetNumFreeMainPackSlots())
	{
		pPartner->SendText("You do not have enough pack space to complete this trade!", LTT_ERROR);
		pInitiator->SendText((pPartner->GetName() = " does not have enough pack space to complete this trade!").c_str(), LTT_ERROR);
		bError = true;
	}

	// TODO check if this takes player over 300% burden

	std::list<std::shared_ptr<CWeenieObject>> lpInitiatorItems;
	for (auto it = m_lInitiatorItems.begin(); it != m_lInitiatorItems.end(); ++it)
	{
		std::shared_ptr<CWeenieObject> pItem = g_pWorld->FindWithinPVS(pInitiator, *it);
		lpInitiatorItems.push_back(pItem);

		if (!pItem)
		{
			pInitiator->SendText("Invalid item in trade!", LTT_ERROR);
			pPartner->SendText("Invalid item in trade!", LTT_ERROR);
			bError = true;
			break;
		}
		else if (pItem->GetWorldTopLevelOwner() != pInitiator || pItem->IsWielded() || pItem->IsAttunedOrContainsAttuned())
		{
			pInitiator->SendText(("You cannot trade " + pItem->GetName() + "!").c_str(), LTT_ERROR);
			pPartner->SendText((pInitiator->GetName() + " put invalid items in the trade!").c_str(), LTT_ERROR);
			bError = true;
			break;
		}
	}

	std::list<std::shared_ptr<CWeenieObject>> lpPartnerItems;
	for (auto it = m_lPartnerItems.begin(); it != m_lPartnerItems.end(); ++it)
	{
		std::shared_ptr<CWeenieObject> pItem = g_pWorld->FindWithinPVS(pPartner, *it);
		lpPartnerItems.push_back(pItem);

		if (!pItem)
		{
			pInitiator->SendText("Invalid item in trade!", LTT_ERROR);
			pPartner->SendText("Invalid item in trade!", LTT_ERROR);
			bError = true;
			break;
		}
		else if (pItem->GetWorldTopLevelOwner() != pPartner || pItem->IsWielded() || pItem->IsAttunedOrContainsAttuned())
		{
			pPartner->SendText(("You cannot trade " + pItem->GetName() + "!").c_str(), LTT_ERROR);
			pInitiator->SendText((pPartner->GetName() + " put invalid items in the trade!").c_str(), LTT_ERROR);
			bError = true;
			break;
		}
	}

	if (!bError)
	{
		// Swap items
		for (auto it = lpInitiatorItems.begin(); it != lpInitiatorItems.end(); ++it)
		{
			pPartner->OnReceiveInventoryItem(pInitiator, *it, 0);


			BinaryWriter removeItem;
			removeItem.Write<DWORD>(0x24);
			removeItem.Write<DWORD>((*it)->GetID());
			pInitiator->SendNetMessage(&removeItem, PRIVATE_MSG, TRUE, FALSE);
		}
		for (auto it = lpPartnerItems.begin(); it != lpPartnerItems.end(); ++it)
		{
			pInitiator->OnReceiveInventoryItem(pPartner, *it, 0);

			BinaryWriter removeItem;
			removeItem.Write<DWORD>(0x24);
			removeItem.Write<DWORD>((*it)->GetID());
			pPartner->SendNetMessage(&removeItem, PRIVATE_MSG, TRUE, FALSE);
		}


		// Trade Complete!
		pInitiator->NotifyWeenieError(0x529);
		pPartner->NotifyWeenieError(0x529);

		// reset the trade to continue trading
		ResetTrade(pInitiator);

		return true;
	}
	

	// Unfortunately, you cannot un-accept a completed trade as the accepting client bugs out
	CloseTrade(pInitiator, 0);

	return false;
}

void TradeManager::DeclineTrade(std::shared_ptr<CPlayerWeenie> playerFrom)
{
	if (!CheckTrade())
		return;

	if (playerFrom == _initiator.lock())
		m_bInitiatorAccepted = false;
	else
		m_bPartnerAccepted = false;

	BinaryWriter declineTrade;
	declineTrade.Write<DWORD>(0x203);
	declineTrade.Write<DWORD>(playerFrom->GetID());
	//playerFrom->SendNetMessage(&declineTrade, PRIVATE_MSG, TRUE, FALSE);
	GetOtherPlayer(playerFrom)->SendNetMessage(&declineTrade, PRIVATE_MSG, TRUE, FALSE);
}

void TradeManager::ResetTrade(std::shared_ptr<CPlayerWeenie> playerFrom)
{
	if (!CheckTrade())
		return;

	// remove all items
	m_lInitiatorItems.clear();
	m_lPartnerItems.clear();

	std::shared_ptr<CPlayerWeenie> other = GetOtherPlayer(playerFrom);

	BinaryWriter resetTrade;
	resetTrade.Write<DWORD>(0x205);
	resetTrade.Write<DWORD>(playerFrom->GetID());
	playerFrom->SendNetMessage(&resetTrade, PRIVATE_MSG, TRUE, FALSE);
	GetOtherPlayer(playerFrom)->SendNetMessage(&resetTrade, PRIVATE_MSG, TRUE, FALSE);
}

std::shared_ptr<CPlayerWeenie> TradeManager::GetOtherPlayer(std::shared_ptr<CPlayerWeenie> player)
{
	std::shared_ptr<CPlayerWeenie> pInitiator = _initiator.lock();
	std::shared_ptr<CPlayerWeenie> pPartner = _partner.lock();

	if (player == pInitiator)
		return pPartner;

	return pInitiator;
};

// Checks whether trade is still legit. True is so.
bool TradeManager::CheckTrade()
{
	std::shared_ptr<CPlayerWeenie> pInitiator = _initiator.lock();
	std::shared_ptr<CPlayerWeenie> pPartner = _partner.lock();
	// not currently trading
	if (!pInitiator || !pPartner)
	{
		OnCloseTrade(_initiator);
		OnCloseTrade(_partner);
		Delete();
		return false;
	}

	return true;
}

// Removes references and then removes this from memory
void TradeManager::Delete()
{
	std::shared_ptr<CPlayerWeenie> pInitiator = _initiator.lock();
	std::shared_ptr<CPlayerWeenie> pPartner = _partner.lock();

	// Delete all references to this
	if (pInitiator)
	{
		pInitiator->SetTradeManager(NULL);
	}
	if (pPartner)
	{
		pPartner->SetTradeManager(NULL);
	}

	// MUST be the final thing done in this class
	delete this;
}

void TradeManager::CheckDistance()
{
	if (!CheckTrade())
		return;

	std::shared_ptr<CPlayerWeenie> pInitiator = _initiator.lock();
	std::shared_ptr<CPlayerWeenie> pPartner = _partner.lock();

	if (pInitiator->DistanceTo(pPartner, true) >= 1 )
	{
		pInitiator->SendText((pPartner->GetName() + " is too far away to trade!").c_str(), LTT_ERROR);
		pPartner->SendText((pInitiator->GetName() + " is too far away to trade!").c_str(), LTT_ERROR);

		CloseTrade(_initiator, 1);
	}
}