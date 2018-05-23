
#pragma once

#include "WeenieObject.h"

class CPKModifierWeenie : public CWeenieObject
{
public:
	CPKModifierWeenie();
	virtual ~CPKModifierWeenie() override;

	virtual class std::shared_ptr<CPKModifierWeenie> AsPKModifier() { return std::dynamic_pointer_cast<CPKModifierWeenie>(m_spThis.lock()); }

	virtual int Use(std::shared_ptr<CPlayerWeenie> player) override;
	virtual int DoUseResponse(std::shared_ptr<CWeenieObject> player) override;
};
