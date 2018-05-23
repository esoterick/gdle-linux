
#pragma once

#include "WeenieObject.h"

class CKeyWeenie : public CWeenieObject
{
public:
	CKeyWeenie();
	virtual ~CKeyWeenie() override;

	virtual std::shared_ptr<CKeyWeenie> AsKey() override { return std::dynamic_pointer_cast<CKeyWeenie>(m_spThis.lock()); }

	virtual int UseWith(std::shared_ptr<CPlayerWeenie> player, std::shared_ptr<CWeenieObject> with) override;
	virtual int DoUseWithResponse(std::shared_ptr<CWeenieObject> player, std::shared_ptr<CWeenieObject> with) override;
};