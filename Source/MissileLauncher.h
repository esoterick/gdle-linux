
#pragma once

#include "WeenieObject.h"

class CMissileLauncherWeenie : public CWeenieObject
{
public:
	CMissileLauncherWeenie();

	virtual class std::shared_ptr<CMissileLauncherWeenie> AsMissileLauncher() { return std::dynamic_pointer_cast<CMissileLauncherWeenie>(m_spThis.lock()); }

	virtual COMBAT_MODE GetEquippedCombatMode() override { return COMBAT_MODE::MISSILE_COMBAT_MODE; }
};
