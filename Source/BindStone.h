#pragma once

#include "UseManager.h"

class CBindStone : public CWeenieObject
{
public:
	CBindStone();
	virtual ~CBindStone() override;

	virtual class std::shared_ptr<CBindStone> AsBindStone() { return std::dynamic_pointer_cast<CBindStone>(m_spThis.lock()); }

	virtual int Use(std::shared_ptr<CPlayerWeenie> ) override;
	virtual int DoUseResponse(std::shared_ptr<CWeenieObject> player) override;

protected:
};