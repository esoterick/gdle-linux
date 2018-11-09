#pragma once

#include "GameEnums.h"
#include "combat/AttackEventData.h"
#include "combat/MeleeAttackEventData.h"

class CDualWieldAttackEvent : public CMeleeAttackEvent
{
public:
	CDualWieldAttackEvent() :
		CMeleeAttackEvent(COMBAT_USE_MELEE),
		_hand(0), _main_attack_motion(0), _offhand_attack_motion(0)
	{ }

	virtual void Setup() override;

	//virtual void OnReadyToAttack() override;
	//virtual void OnAttackAnimSuccess(DWORD motion) override;
	//void Finish();

	virtual void HandleAttackHook(const AttackCone &cone) override;

	//void HandlePerformAttack(CWeenieObject *target, DamageEventData dmgEvent);

protected:
	virtual void CalculateAtt(CWeenieObject *weapon, STypeSkill& weaponSkill, DWORD& weaponSkillLevel) override;
	virtual int CalculateDef(CWeenieObject *weapon) override;

	int _hand;

	int _main_attack_motion;
	int _offhand_attack_motion;
};
