
#pragma once

#include "WeenieObject.h"

class CHotSpotWeenie : public CWeenieObject
{
public:
	CHotSpotWeenie();
	virtual ~CHotSpotWeenie() override;

	virtual class std::shared_ptr<CHotSpotWeenie> AsHotSpot() { return std::dynamic_pointer_cast<CHotSpotWeenie>(m_spThis.lock()); }

	virtual void ApplyQualityOverrides() override;
	virtual void Tick() override;

	virtual void PostSpawn() override;
	virtual int DoCollision(const class ObjCollisionProfile &prof) override;
	virtual void DoCollisionEnd(DWORD object_id) override;

protected:
	void SetNextCycleTime();
	void DoCycle();
	void DoCycleDamage(std::shared_ptr<CWeenieObject> other);

	double m_fNextCycleTime = 0.0;
	std::set<DWORD> m_ContactedWeenies;
};

