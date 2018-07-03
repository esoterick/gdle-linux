
#pragma once

#include "WeenieObject.h"

class CSwitchWeenie : public CWeenieObject
{
public:
	CSwitchWeenie();
	virtual ~CSwitchWeenie() override;

	virtual class std::shared_ptr<CSwitchWeenie> AsSwitch() { return std::static_pointer_cast<CSwitchWeenie>(GetPointer()); }

	virtual void ApplyQualityOverrides() override;

	virtual int Activate(DWORD activator_id) override;
	virtual int Use(std::shared_ptr<CPlayerWeenie> ) override;

	void PlaySwitchMotion();

	double m_fNextSwitchActivation = 0.0;
};

