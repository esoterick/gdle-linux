
#pragma once

#include "WeenieObject.h"

class CAugmentationDeviceWeenie : public CWeenieObject
{
public:
	CAugmentationDeviceWeenie();
	virtual ~CAugmentationDeviceWeenie() override;

	virtual std::shared_ptr<CAugmentationDeviceWeenie> AsAugmentationDevice() override { return GetPointer<CAugmentationDeviceWeenie>(); }

	virtual int Use(std::shared_ptr<CPlayerWeenie> pOther) override;
};

