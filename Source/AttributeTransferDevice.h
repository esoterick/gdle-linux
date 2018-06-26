
#pragma once

#include "WeenieObject.h"

class CAttributeTransferDeviceWeenie : public CWeenieObject
{
public:
	CAttributeTransferDeviceWeenie();
	virtual ~CAttributeTransferDeviceWeenie() override;

	virtual std::shared_ptr<CAttributeTransferDeviceWeenie> AsAttributeTransferDevice() override { return std::static_pointer_cast<CAttributeTransferDeviceWeenie>(GetPointer()); }

	virtual int Use(std::shared_ptr<CPlayerWeenie> pOther) override;
};

