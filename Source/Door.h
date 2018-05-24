
#pragma once

class CBaseDoor : public CWeenieObject
{
public:
	CBaseDoor();
	virtual ~CBaseDoor() override;
	
	virtual class std::shared_ptr<CBaseDoor> AsDoor() { return GetPointer<CBaseDoor>(); }

	virtual int Activate(DWORD activator_id) override;
	virtual int Use(std::shared_ptr<CPlayerWeenie> ) override;

	virtual void PostSpawn() override;
	virtual void ResetToInitialState() override;

	void CloseDoor();
	void OpenDoor();
	
	bool IsClosed();

protected:
	bool m_bOpen = false;
	bool m_bInitiallyLocked = false;
};