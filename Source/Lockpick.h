
#pragma once

#include "WeenieObject.h"

class CLockpickWeenie : public CWeenieObject
{
public:
	CLockpickWeenie();
	virtual ~CLockpickWeenie() override;

	virtual std::shared_ptr<CLockpickWeenie> AsLockpick() override { return std::static_pointer_cast<CLockpickWeenie>(GetPointer()); }

	virtual int UseWith(std::shared_ptr<CPlayerWeenie> player, std::shared_ptr<CWeenieObject> with) override;
	virtual int DoUseWithResponse(std::shared_ptr<CWeenieObject> player, std::shared_ptr<CWeenieObject> with) override;
};