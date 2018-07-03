#pragma once

#include "WeenieObject.h"
#include "UseManager.h"

class CHealerUseEvent : public CUseEventData
{
public:
	virtual void OnReadyToUse() override;
	virtual void OnUseAnimSuccess(DWORD motion) override;
};

class CHealerWeenie : public CWeenieObject // CWeenieObject
{
public:
	CHealerWeenie();
	virtual ~CHealerWeenie() override;

	virtual class std::shared_ptr<CHealerWeenie> AsHealer() { return std::static_pointer_cast<CHealerWeenie>(GetPointer()); }

	virtual void ApplyQualityOverrides() override;
	virtual int UseWith(std::shared_ptr<CPlayerWeenie> player, std::shared_ptr<CWeenieObject> with) override;

protected:
};

