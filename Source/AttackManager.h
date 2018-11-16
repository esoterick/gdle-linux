
#pragma once

#include "combat/AttackEventData.h"

//#define CLEAVING_ATTACK_ANGLE 178

class AttackManager
{
public:
	AttackManager(class CWeenieObject *weenie);
	~AttackManager();

	void Update();
	void Cancel();

	void BeginMeleeAttack(DWORD target_id, ATTACK_HEIGHT height, float power, float chase_distance = 15.0f, DWORD motion = 0);
	void BeginMissileAttack(DWORD target_id, ATTACK_HEIGHT height, float power, DWORD motion = 0);

	void BeginAttack(CAttackEventData *data);

	void OnAttackCancelled(DWORD error = 0);
	void OnAttackDone(DWORD error = 0);

	bool RepeatAttacks();

	void OnDeath(DWORD killer_id);
	void HandleMoveToDone(DWORD error);
	void HandleAttackHook(const AttackCone &cone);
	void OnMotionDone(DWORD motion, BOOL success);

	bool IsAttacking();

	void MarkForCleanup(CAttackEventData *data);

	float GetDefenseMod()
	{
		if (_attackData)
			return _attackData->CalculateDef();
		return 1.0f;
	}

private:
	class CWeenieObject *_weenie = NULL;

	double _next_allowed_attack = 0.0;
	CAttackEventData *_attackData = NULL;
	CAttackEventData *_queuedAttackData = NULL;
	CAttackEventData *_cleanupData = NULL;
};