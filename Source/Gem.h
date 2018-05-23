
#pragma once

#include "WeenieObject.h"

class CGemWeenie : public CWeenieObject
{
public:
	CGemWeenie();
	virtual ~CGemWeenie() override;

	virtual std::shared_ptr<CGemWeenie> AsGem() override { return std::dynamic_pointer_cast<CGemWeenie>(m_spThis.lock()); }

	virtual int Use(std::shared_ptr<CPlayerWeenie> pOther) override;
	virtual int DoUseResponse(std::shared_ptr<CWeenieObject> player) override;
};

