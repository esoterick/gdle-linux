#pragma once

#include "WeenieObject.h"

class CBookWeenie : public CWeenieObject
{
public:
	CBookWeenie();
	virtual ~CBookWeenie() override;

	virtual class std::shared_ptr<CBookWeenie> AsBook() { return std::static_pointer_cast<CBookWeenie>(GetPointer()); }

	virtual void ApplyQualityOverrides() override;
	virtual int Use(std::shared_ptr<CPlayerWeenie> ) override;

protected:
};

