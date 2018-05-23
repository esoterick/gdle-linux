#pragma once

#include "WeenieObject.h"

class CFoodWeenie : public CWeenieObject // CWeenieObject
{
public:
	CFoodWeenie();
	virtual ~CFoodWeenie() override;

	virtual class std::shared_ptr<CFoodWeenie> AsFood() { return std::dynamic_pointer_cast<CFoodWeenie>(m_spThis.lock()); }

	virtual void ApplyQualityOverrides() override;
	virtual int Use(std::shared_ptr<CPlayerWeenie> ) override;
	virtual int DoUseResponse(std::shared_ptr<CWeenieObject> other) override;

protected:
};

