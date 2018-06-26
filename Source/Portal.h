
#pragma once

#include "WeenieObject.h"
#include "UseManager.h"

class CPortalUseEvent : public CUseEventData
{
public:
	virtual void OnReadyToUse() override;
};

class CPortal : public CWeenieObject
{
public:
	CPortal();
	virtual ~CPortal();

	virtual class std::shared_ptr<CPortal> AsPortal() { return std::static_pointer_cast<CPortal>(GetPointer()); }

	virtual int Use(std::shared_ptr<CPlayerWeenie> ) override;
	virtual void Tick() override;

	virtual int DoCollision(const class ObjCollisionProfile &prof) override;

	void CheckedTeleport(std::shared_ptr<CWeenieObject> pOther);
	void Teleport(std::shared_ptr<CWeenieObject> pTarget);

#if 0 // deprecated
	void ProximityThink();
#endif

	virtual bool GetDestination(Position &position);

private:
#if 0 // deprecated
	double m_fLastCacheClear;
	std::set<DWORD> m_RecentlyTeleported;
#endif
};