
#pragma once

#include "Monster.h"

class CTownCrier : public CMonsterWeenie
{
public:
	CTownCrier();
	virtual ~CTownCrier() override;

	virtual class std::shared_ptr<CTownCrier> AsTownCrier() { return std::dynamic_pointer_cast<CTownCrier>(m_spThis.lock()); }

	virtual int DoUseResponse(std::shared_ptr<CWeenieObject> player) override;
	virtual int Use(std::shared_ptr<CPlayerWeenie> other) override;
	virtual void HandleMoveToDone(DWORD error) override;

	virtual DWORD OnReceiveInventoryItem(std::shared_ptr<CWeenieObject> source, std::shared_ptr<CWeenieObject> item, DWORD desired_slot) override;

	std::string GetNewsText(bool paid);
};