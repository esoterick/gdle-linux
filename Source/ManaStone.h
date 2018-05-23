#pragma once

#include "WeenieObject.h"
#include "UseManager.h"

class CManaStoneUseEvent : public CUseEventData
{
public:
	virtual void OnReadyToUse() override;
};

class CManaStoneWeenie : public CWeenieObject // CWeenieObject
{
public:
	CManaStoneWeenie();
	virtual ~CManaStoneWeenie() override;

	virtual class std::shared_ptr<CManaStoneWeenie> AsManaStone() { return std::dynamic_pointer_cast<CManaStoneWeenie>(m_spThis.lock()); }

	virtual void ApplyQualityOverrides() override;
	virtual int UseWith(std::shared_ptr<CPlayerWeenie> player, std::shared_ptr<CWeenieObject> with) override;

protected:
};

