
#pragma once

class CWeenieObject;
class CMonsterWeenie;

// tolerance = always attack if in range
// tolerance = 1, never attack
// tolerance = 2, attack after attacked or identified
// tolerance = 0x40, attack after provoked, only the person that attacked

enum ToleranceEnum
{
	TolerateNothing = 0,
	TolerateEverything = 1,
	TolerateUnlessBothered = 2, // ID'd or attacked
	TolerateUnlessAttacked = 0x40,
};

enum MonsterAIState
{
	Idle,
	ReturningToSpawn,
	SeekNewTarget,
	MeleeModeAttack,
	MissileModeAttack,
	MagicModeAttack
};

class MonsterAIManager
{
public:
	MonsterAIManager(std::shared_ptr<CMonsterWeenie> pWeenie, const Position &HomePos);
	virtual ~MonsterAIManager();

	void SetHomePosition(const Position &pos);
	void Update();

	void SwitchState(int state);
	void EnterState(int state);

	void BeginIdle();
	void UpdateIdle();
	void EndIdle();

	void BeginReturningToSpawn();
	void UpdateReturningToSpawn();
	void EndReturningToSpawn();

	void BeginSeekNewTarget();
	void UpdateSeekNewTarget();
	void EndSeekNewTarget();

	void BeginMeleeModeAttack();
	void EndMeleeModeAttack();
	void UpdateMeleeModeAttack();

	void BeginMissileModeAttack();
	void EndMissileModeAttack();
	void UpdateMissileModeAttack();

	std::shared_ptr<CWeenieObject> GetTargetWeenie();
	void SetNewTarget(std::shared_ptr<CWeenieObject> pTarget);

	bool SeekTarget();

	bool IsValidTarget(std::shared_ptr<CWeenieObject> pWeenie);
	void OnDeath();
	void OnDealtDamage(DamageEventData &damageData);
	void OnTookDamage(DamageEventData &damageData);
	void OnResistSpell(std::shared_ptr<CWeenieObject> attacker);
	void OnEvadeAttack(std::shared_ptr<CWeenieObject> attacker);

	void AlertIdleFriendsToAggro(std::shared_ptr<CWeenieObject> pAttacker);

	void OnIdentifyAttempted(std::shared_ptr<CWeenieObject> other);
	void HandleAggro(std::shared_ptr<CWeenieObject> pAttacker);

	float DistanceToHome();
	bool ShouldSeekNewTarget();
	
	bool RollDiceCastSpell();
	bool DoCastSpell(DWORD spell_id);
	bool DoMeleeAttack();

	void GenerateRandomAttack(DWORD *motion, ATTACK_HEIGHT *height, float *power, std::shared_ptr<CWeenieObject> weapon = NULL);
	float GetChaseDistance() { return m_fChaseRange; }

	std::weak_ptr<CMonsterWeenie> m_pWeenie;

	Position m_HomePosition;
	double m_fAwarenessRange = 40.0f;
	double m_fChaseRange = 100.0f;
	double m_fMaxHomeRange = 150.0f;
	double m_fMinReturnStateDuration = 20.0;
	double m_fMinCombatStateDuration = 10.0;
	double m_fChaseTimeoutDuration = 30.0;
	double m_fMeleeAttackRange = 3.0;
	double m_fReturnTimeout = 30.0f;

	std::weak_ptr<CWeenieObject> _currentWeapon;
	std::weak_ptr<CWeenieObject> _currentShield;
	std::weak_ptr<CWeenieObject> _shield;
	std::weak_ptr<CWeenieObject> _meleeWeapon;
	std::weak_ptr<CWeenieObject> _missileWeapon;
	bool _hasUnarmedSkill = false;
	double _nextTaunt = -1.0;

	int m_State = MonsterAIState::Idle;

	// to check for targets nearby
	double m_fNextPVSCheck = 0.0;

	double m_fChaseTimeoutTime = 0.0;
	double m_fNextChaseTime = 0.0;
	double m_fNextAttackTime = 0.0;
	double m_fMinReturnStateTime = 0.0;
	double m_fMinCombatStateTime = 0.0;
	DWORD m_TargetID = 0;

	double m_fReturnTimeoutTime = 0.0;

	double m_fAggroTime = 0.0;
	double m_fNextCastTime = 0.0;

	double m_fLastWoundedTauntHP = 1.0;

	int _toleranceType = 0;
	int _aiOptions = 0;

	double _cachedVisualAwarenessRange = 0.0;
};

