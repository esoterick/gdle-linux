
#pragma once

#include "WeenieObject.h"

class CMeleeWeaponWeenie : public CWeenieObject
{
public:
	CMeleeWeaponWeenie();

	virtual class std::shared_ptr<CMeleeWeaponWeenie> AsMeleeWeapon() { return std::static_pointer_cast<CMeleeWeaponWeenie>(GetPointer()); }

	virtual COMBAT_MODE GetEquippedCombatMode() override { return COMBAT_MODE::MELEE_COMBAT_MODE; }
};
