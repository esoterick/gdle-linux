
#pragma once

#include "WeenieObject.h"

class CMeleeWeaponWeenie : public CWeenieObject
{
public:
	CMeleeWeaponWeenie();

	virtual class std::shared_ptr<CMeleeWeaponWeenie> AsMeleeWeapon() { return GetPointer<CMeleeWeaponWeenie>(); }

	virtual COMBAT_MODE GetEquippedCombatMode() override { return COMBAT_MODE::MELEE_COMBAT_MODE; }
};
