#pragma once

#include "WeenieObject.h"
#include "UseManager.h"

class CScrollUseEvent : public CUseEventData
{
public:
	virtual void OnReadyToUse() override;
	virtual void OnUseAnimSuccess(DWORD motion) override;

	DWORD _spell_id = 0;
};

class CScrollWeenie : public CWeenieObject // CWeenieObject
{
public:
	CScrollWeenie();
	virtual ~CScrollWeenie() override;

	virtual class std::shared_ptr<CScrollWeenie> AsScroll() { return GetPointer<CScrollWeenie>(); }

	virtual void ApplyQualityOverrides() override;
	virtual int Use(std::shared_ptr<CPlayerWeenie> player) override;

	const CSpellBase *GetSpellBase();

protected:
};

