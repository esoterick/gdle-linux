
#pragma once

#include "WeenieObject.h"

class CSkillAlterationDeviceWeenie : public CWeenieObject
{
public:
	CSkillAlterationDeviceWeenie();
	virtual ~CSkillAlterationDeviceWeenie() override;

	virtual std::shared_ptr<CSkillAlterationDeviceWeenie> AsSkillAlterationDevice() override { return std::dynamic_pointer_cast<CSkillAlterationDeviceWeenie>(m_spThis.lock()); }

	virtual int Use(std::shared_ptr<CPlayerWeenie> pOther) override;
};

