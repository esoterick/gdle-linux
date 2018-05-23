#pragma once

#include "WeenieObject.h"

class CBookWeenie : public CWeenieObject
{
public:
	CBookWeenie();
	virtual ~CBookWeenie() override;

	virtual class std::shared_ptr<CBookWeenie> AsBook() { return std::dynamic_pointer_cast<CBookWeenie>(m_spThis.lock()); }

	virtual void ApplyQualityOverrides() override;
	virtual int Use(std::shared_ptr<CPlayerWeenie> ) override;

protected:
};

