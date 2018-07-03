
#pragma once

#include "WeenieObject.h"

class CAugmentationDeviceWeenie : public CWeenieObject
{
public:
	CAugmentationDeviceWeenie();
	virtual ~CAugmentationDeviceWeenie() override;

	virtual std::shared_ptr<CAugmentationDeviceWeenie> AsAugmentationDevice() override { return std::static_pointer_cast<CAugmentationDeviceWeenie>(GetPointer()); }

	virtual int Use(std::shared_ptr<CPlayerWeenie> pOther) override;
};

