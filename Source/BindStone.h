#pragma once

#include "UseManager.h"

class CBindStone : public CWeenieObject
{
public:
	CBindStone();
	virtual ~CBindStone() override;

	virtual class std::shared_ptr<CBindStone> AsBindStone() { return std::static_pointer_cast<CBindStone>(GetPointer()); }

	virtual int Use(std::shared_ptr<CPlayerWeenie> ) override;
	virtual int DoUseResponse(std::shared_ptr<CWeenieObject> player) override;

protected:
};