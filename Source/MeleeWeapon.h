
#pragma once

#include "WeenieObject.h"

class CMeleeWeaponWeenie : public CWeenieObject
{
public:
	CMeleeWeaponWeenie();

	virtual class std::shared_ptr<CMeleeWeaponWeenie> AsMeleeWeapon() { return std::dynamic_pointer_cast<CMeleeWeaponWeenie>(m_spThis.lock()); }

	virtual COMBAT_MODE GetEquippedCombatMode() override { return COMBAT_MODE::MELEE_COMBAT_MODE; }
};
