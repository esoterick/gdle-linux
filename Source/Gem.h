
#pragma once

#include "WeenieObject.h"

class CGemWeenie : public CWeenieObject
{
public:
	CGemWeenie();
	virtual ~CGemWeenie() override;

	virtual std::shared_ptr<CGemWeenie> AsGem() override { return std::static_pointer_cast<CGemWeenie>(GetPointer()); }

	virtual int Use(CPlayerWeenie *pOther) override;
	virtual int DoUseResponse(CWeenieObject *player) override;
	double cooldown = 0;
};

