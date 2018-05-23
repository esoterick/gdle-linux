
#pragma once

#include "WeenieObject.h"

class CAttributeTransferDeviceWeenie : public CWeenieObject
{
public:
	CAttributeTransferDeviceWeenie();
	virtual ~CAttributeTransferDeviceWeenie() override;

	virtual std::shared_ptr<CAttributeTransferDeviceWeenie> AsAttributeTransferDevice() override { return std::dynamic_pointer_cast<CAttributeTransferDeviceWeenie>(m_spThis.lock()); }

	virtual int Use(std::shared_ptr<CPlayerWeenie> pOther) override;
};

