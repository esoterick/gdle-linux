
#pragma once

#include "WeenieObject.h"

class CCasterWeenie : public CWeenieObject
{
public:
	CCasterWeenie();

	virtual class std::shared_ptr<CCasterWeenie> AsCaster() { return std::static_pointer_cast<CCasterWeenie>(GetPointer()); }

	virtual void ApplyQualityOverrides() override;
	virtual int Use(std::shared_ptr<CPlayerWeenie> other) override;

	virtual COMBAT_MODE GetEquippedCombatMode() override { return COMBAT_MODE::MAGIC_COMBAT_MODE; }
};
