#pragma once

#include "UseManager.h"

class CBaseLifestone : public CWeenieObject
{
public:
	CBaseLifestone();
	virtual ~CBaseLifestone() override;

	virtual class std::shared_ptr<CBaseLifestone> AsLifestone() { return std::static_pointer_cast<CBaseLifestone>(GetPointer()); }

	virtual void ApplyQualityOverrides() override;
	virtual int Use(std::shared_ptr<CPlayerWeenie> ) override;
	virtual int DoUseResponse(std::shared_ptr<CWeenieObject> player) override;

protected:
};