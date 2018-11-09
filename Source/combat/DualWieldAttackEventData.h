#pragma once

#include "GameEnums.h"
#include "combat/AttackEventData.h"
#include "combat/MeleeAttackEventData.h"

class CDualWieldAttackEvent : public CMeleeAttackEvent
{
public:
	CDualWieldAttackEvent() : CMeleeAttackEvent(COMBAT_USE_OFFHAND) { }

	//virtual void Setup() override;

	//virtual void OnReadyToAttack() override;
	//virtual void OnAttackAnimSuccess(DWORD motion) override;
	//void Finish();

	//virtual void HandleAttackHook(const AttackCone &cone) override;
	//void HandlePerformAttack(CWeenieObject *target, DamageEventData dmgEvent);


};
