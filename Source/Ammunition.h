
#pragma once

#include "WeenieObject.h"

class CAmmunitionWeenie : public CWeenieObject
{
public:
	CAmmunitionWeenie();
	virtual ~CAmmunitionWeenie() override;

	virtual class std::shared_ptr<CAmmunitionWeenie> AsAmmunition() { return std::static_pointer_cast<CAmmunitionWeenie>(GetPointer()); }

	virtual void ApplyQualityOverrides() override;

	virtual void PostSpawn() override;
	virtual int DoCollision(const class EnvCollisionProfile &prof) override;
	virtual int DoCollision(const class AtkCollisionProfile &prof) override;
	virtual int DoCollision(const class ObjCollisionProfile &prof) override;
	virtual void DoCollisionEnd(DWORD object_id) override;

	virtual DWORD GetPhysicsTargetID() override { return _targetID; }

	void MakeIntoMissile();
	void MakeIntoAmmo();

	void HandleNonTargetCollision();
	void HandleTargetCollision();

	DWORD _sourceID = 0;
	DWORD _launcherID = 0;
	DWORD _targetID = 0;

	float _attackPower = 0.0f;
	STypeSkill _weaponSkill = UNDEF_SKILL;
	DWORD _weaponSkillLevel = 0;
};

