
#pragma once

#include "WeenieObject.h"

class CAugmentationDeviceWeenie : public CWeenieObject
{
public:
	CAugmentationDeviceWeenie();
	virtual ~CAugmentationDeviceWeenie() override;

	virtual std::shared_ptr<CAugmentationDeviceWeenie> AsAugmentationDevice() override { return std::dynamic_pointer_cast<CAugmentationDeviceWeenie>(m_spThis.lock()); }

	virtual int Use(std::shared_ptr<CPlayerWeenie> pOther) override;
};

