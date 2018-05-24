
#pragma once

#include "WeenieObject.h"

class CAttributeTransferDeviceWeenie : public CWeenieObject
{
public:
	CAttributeTransferDeviceWeenie();
	virtual ~CAttributeTransferDeviceWeenie() override;

	virtual std::shared_ptr<CAttributeTransferDeviceWeenie> AsAttributeTransferDevice() override { return GetPointer<CAttributeTransferDeviceWeenie>(); }

	virtual int Use(std::shared_ptr<CPlayerWeenie> pOther) override;
};

