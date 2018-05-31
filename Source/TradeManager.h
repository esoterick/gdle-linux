#pragma once

#include "Player.h"

class TradeManager
{
public:
	static TradeManager* RegisterTrade(std::shared_ptr<CPlayerWeenie> initiator, std::shared_ptr<CPlayerWeenie> partner);

	void CloseTrade(std::weak_ptr<CPlayerWeenie> playerFrom, DWORD reason = 0x51);
	void OnCloseTrade(std::weak_ptr<CPlayerWeenie> player, DWORD reason = 0);

	void AddToTrade(std::shared_ptr<CPlayerWeenie> playerFrom, DWORD item);
	//void RemoveFromTrade(std::shared_ptr<CPlayerWeenie> playerFrom, DWORD item);

	void AcceptTrade(std::shared_ptr<CPlayerWeenie> playerFrom);
	bool OnTradeAccepted();

	void DeclineTrade(std::shared_ptr<CPlayerWeenie> playerFrom);

	void ResetTrade(std::shared_ptr<CPlayerWeenie> playerFrom);

	std::shared_ptr<CPlayerWeenie> GetOtherPlayer(std::shared_ptr<CPlayerWeenie> player);

	void CheckDistance();
private:
	// prevent anyone from initiating this outside of OpenTrade
	TradeManager(std::shared_ptr<CPlayerWeenie> initiator, std::shared_ptr<CPlayerWeenie> partner);

	void Delete();

	std::weak_ptr<CPlayerWeenie> _initiator;
	std::weak_ptr<CPlayerWeenie> _partner;

	bool m_bInitiatorAccepted = false;
	bool m_bPartnerAccepted = false;

	std::list<DWORD> m_lInitiatorItems;
	std::list<DWORD> m_lPartnerItems;

	// double check the trade is still legit
	bool CheckTrade();
};
