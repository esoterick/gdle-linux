#pragma once

#include "combat/AttackEventData.h"

class CMissileAttackEvent : public CAttackEventData
{
public:
	virtual void Setup() override;
	virtual void PostCharge() override;
	virtual void OnReadyToAttack() override;
	virtual void OnAttackAnimSuccess(DWORD motion) override;
	void Finish();

	virtual void HandleAttackHook(const AttackCone &cone) override;

	void FireMissile();

	void CalculateAttackMotion();
	bool CalculateTargetPosition();
	bool CalculateSpawnPosition(float missileRadius);
	bool CalculateMissileVelocity(bool track = true, bool gravity = true, float speed = 20.0f);

	virtual class CMissileAttackEvent *AsMissileAttackEvent() { return this; }

	DWORD _do_attack_animation = 0;

	Position _missile_spawn_position;
	Position _missile_target_position;
	Vector _missile_velocity;
	float _missile_dist_to_target = 0.0f;
	bool m_bTurned = false;
};
