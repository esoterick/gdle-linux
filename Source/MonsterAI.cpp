
#include "StdAfx.h"
#include "MonsterAI.h"
#include "WeenieObject.h"
#include "Monster.h"
#include "World.h"
#include "SpellcastingManager.h"
#include "EmoteManager.h"

#define DEFAULT_AWARENESS_RANGE 40.0

MonsterAIManager::MonsterAIManager(std::shared_ptr<CMonsterWeenie> pWeenie, const Position &HomePos)
{
	m_pWeenie = pWeenie;
	_toleranceType = pWeenie->InqIntQuality(TOLERANCE_INT, 0, TRUE);
	_aiOptions = pWeenie->InqIntQuality(AI_OPTIONS_INT, 0, TRUE);
	_cachedVisualAwarenessRange = pWeenie->InqFloatQuality(VISUAL_AWARENESS_RANGE_FLOAT, DEFAULT_AWARENESS_RANGE);

	if(!pWeenie->GetWieldedCombat(COMBAT_USE_TWO_HANDED))
	_meleeWeapon = pWeenie->GetWieldedCombat(COMBAT_USE_MELEE);
	else {
		_meleeWeapon = pWeenie->GetWieldedCombat(COMBAT_USE_TWO_HANDED);
	}
	_missileWeapon = pWeenie->GetWieldedCombat(COMBAT_USE_MISSILE);
	_shield = pWeenie->GetWieldedCombat(COMBAT_USE_SHIELD);

	SKILL_ADVANCEMENT_CLASS unarmedSkill;
	pWeenie->m_Qualities.InqSkillAdvancementClass(LIGHT_WEAPONS_SKILL, unarmedSkill);
	_hasUnarmedSkill = (unarmedSkill > UNTRAINED_SKILL_ADVANCEMENT_CLASS);

	// TODO these may need to be shared not weak
	std::shared_ptr<CWeenieObject> pMeleeWeapon = _meleeWeapon.lock();
	std::shared_ptr<CWeenieObject> pMissileWeapon = _meleeWeapon.lock();
	std::shared_ptr<CWeenieObject> pShield = _shield.lock();

	if ( pMeleeWeapon && pMissileWeapon ) //if we have both melee and missile weapons, favor missile
	{
		pWeenie->FinishMoveItemToContainer(pMeleeWeapon, pWeenie, 0, true);
		if (pShield && pMissileWeapon->InqIntQuality(DEFAULT_COMBAT_STYLE_INT, 0) != ThrownWeapon_CombatStyle)
			pWeenie->FinishMoveItemToContainer(pShield, pWeenie, 0, true);
		else
			_currentShield = _shield;
		_currentWeapon = _missileWeapon;
	}
	else if (pMissileWeapon)
	{
		_currentWeapon = _missileWeapon;
		if (pShield && pMissileWeapon->InqIntQuality(DEFAULT_COMBAT_STYLE_INT, 0) != ThrownWeapon_CombatStyle)
			pWeenie->FinishMoveItemToContainer(pShield, pWeenie, 0, true);
		else
			_currentShield = _shield;
	}
	else if (pMeleeWeapon)
	{
		_currentWeapon = _meleeWeapon;
		_currentShield = _shield;
	}

	if (pWeenie->m_Qualities._emote_table && pWeenie->m_Qualities._emote_table->_emote_table.lookup(Taunt_EmoteCategory))
		_nextTaunt = Timer::cur_time + Random::GenUInt(10, 30); //We have taunts so schedule them.

	// List of qualities that could be used to affect the AI behavior:
	// AI_CP_THRESHOLD_INT
	// AI_PP_THRESHOLD_INT
	// AI_ADVANCEMENT_STRATEGY_INT

	// AI_ALLOWED_COMBAT_STYLE_INT
	// COMBAT_TACTIC_INT
	// TARGETING_TACTIC_INT
	// HOMESICK_TARGETING_TACTIC_INT

	// AI_OPTIONS_INT
	// FRIEND_TYPE_INT
	// FOE_TYPE_INT

	// ATTACKER_AI_BOOL
	// AI_USES_MANA_BOOL
	// AI_USE_HUMAN_MAGIC_ANIMATIONS_BOOL
	// AI_IMMOBILE_BOOL
	// AI_ALLOW_TRADE_BOOL
	// AI_ACCEPT_EVERYTHING_BOOL
	// AI_USE_MAGIC_DELAY_FLOAT

	// AI_ACQUIRE_HEALTH_FLOAT
	// AI_ACQUIRE_STAMINA_FLOAT
	// AI_ACQUIRE_MANA_FLOAT

	// AI_COUNTERACT_ENCHANTMENT_FLOAT
	// AI_DISPEL_ENCHANTMENT_FLOAT

	// AI_TARGETED_DETECTION_RADIUS_FLOAT

	// SetHomePosition(HomePos); don't use preset home position
}

MonsterAIManager::~MonsterAIManager()
{
}

void MonsterAIManager::SetHomePosition(const Position &pos)
{
	m_HomePosition = pos;
}

void MonsterAIManager::Update()
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	if (!m_HomePosition.objcell_id)
	{
		// make sure we set a home position
		if (!(pWeenie->transient_state & ON_WALKABLE_TS))
			return;

		SetHomePosition(pWeenie->m_Position);
	}

	switch (m_State)
	{
	case MonsterAIState::Idle:
		UpdateIdle();
		break;

	case MonsterAIState::MeleeModeAttack:
		UpdateMeleeModeAttack();
		break;

	case MonsterAIState::MissileModeAttack:
		UpdateMissileModeAttack();
		break;

	case MonsterAIState::ReturningToSpawn:
		UpdateReturningToSpawn();
		break;

	case MonsterAIState::SeekNewTarget:
		UpdateSeekNewTarget();
		break;
	}
}

void MonsterAIManager::SwitchState(int state)
{
	if (state == m_State)
		return;

	switch (state)
	{
	case MonsterAIState::Idle:
		EndIdle();
		break;

	case MonsterAIState::MeleeModeAttack:
		EndMeleeModeAttack();
		break;

	case MonsterAIState::MissileModeAttack:
		EndMissileModeAttack();
		break;

	case MonsterAIState::ReturningToSpawn:
		EndReturningToSpawn();
		break;

	case MonsterAIState::SeekNewTarget:
		EndSeekNewTarget();
		break;
	}

	EnterState(state);
}

void MonsterAIManager::EnterState(int state)
{
	m_State = state;

	switch (state)
	{
	case MonsterAIState::Idle:
		BeginIdle();
		break;

	case MonsterAIState::MeleeModeAttack:
		BeginMeleeModeAttack();
		break;

	case MonsterAIState::MissileModeAttack:
		BeginMissileModeAttack();
		break;

	case MonsterAIState::ReturningToSpawn:
		BeginReturningToSpawn();
		break;

	case MonsterAIState::SeekNewTarget:
		BeginSeekNewTarget();
		break;
	}
}

void MonsterAIManager::BeginIdle()
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	m_fNextPVSCheck = Timer::cur_time;
	pWeenie->ChangeCombatMode(COMBAT_MODE::NONCOMBAT_COMBAT_MODE, false);
}

void MonsterAIManager::EndIdle()
{
}

void MonsterAIManager::UpdateIdle()
{
	if (_toleranceType == TolerateNothing)
	{
		SeekTarget();
	}
}

bool MonsterAIManager::SeekTarget()
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return false;
	}

	if (m_fNextPVSCheck <= Timer::cur_time)
	{
		m_fNextPVSCheck = Timer::cur_time + 2.0f;

		std::list<std::shared_ptr<CWeenieObject> > results;
		g_pWorld->EnumNearbyPlayers(pWeenie, _cachedVisualAwarenessRange, &results); // m_HomePosition

		std::list<std::shared_ptr<CWeenieObject> > validTargets;

		std::shared_ptr<CWeenieObject> pClosestWeenie = NULL;
		double fClosestWeenieDist = FLT_MAX;

		for (auto weenie : results)
		{
			if (weenie == pWeenie)
				continue;

			if (!weenie->_IsPlayer()) // only attack players
				continue;

			if (!weenie->IsAttackable())
				continue;

			if (weenie->ImmuneToDamage(pWeenie)) // only attackable players (not dead, not in portal space, etc.
				continue;

			validTargets.push_back(weenie);

			/*
			double fWeenieDist = pWeenie->DistanceTo(weenie);
			if (pClosestWeenie && fWeenieDist >= fClosestWeenieDist)
			continue;

			pClosestWeenie = weenie;
			fClosestWeenieDist = fWeenieDist;
			*/
		}

		/*
		if (pClosestWeenie)
		SetNewTarget(pClosestWeenie);
		*/

		if (!validTargets.empty())
		{
			// Random target
			std::list<std::shared_ptr<CWeenieObject> >::iterator i = validTargets.begin();
			std::advance(i, Random::GenInt(0, (unsigned int)(validTargets.size() - 1)));
			SetNewTarget(*i);
			return true;
		}
	}

	return false;
}

void MonsterAIManager::SetNewTarget(std::shared_ptr<CWeenieObject> pTarget)
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie || !pTarget)
	{
		return;
	}

	m_TargetID = pTarget->GetID();

	if (_missileWeapon.lock() )
	{
		double fTargetDist = pWeenie->DistanceTo(pTarget, true);
		if(fTargetDist > 5 || (!_meleeWeapon.lock() && !_hasUnarmedSkill))
			SwitchState(MissileModeAttack);
		else
			SwitchState(MeleeModeAttack);
	}
	else
		SwitchState(MeleeModeAttack);

	pWeenie->ChanceExecuteEmoteSet(pTarget->GetID(), NewEnemy_EmoteCategory);
}

std::shared_ptr<CWeenieObject> MonsterAIManager::GetTargetWeenie()
{
	return g_pWorld->FindObject(m_TargetID);
}

float MonsterAIManager::DistanceToHome()
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return FLT_MAX;
	}

	if (!m_HomePosition.objcell_id)
		return FLT_MAX;

	return m_HomePosition.distance(pWeenie->m_Position);
}

bool MonsterAIManager::ShouldSeekNewTarget()
{
	if (DistanceToHome() >= m_fMaxHomeRange)
		return false;

	return true;
}

bool MonsterAIManager::RollDiceCastSpell()
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return false;
	}

	if (m_fNextCastTime > Timer::cur_time)
	{
		return false;
	}

	if (pWeenie->m_Qualities._spell_book)
	{
		/* not correct, these must be independent events (look at wisps)
		float dice = Random::RollDice(0.0f, 1.0f);

		auto spellIterator = pWeenie->m_Qualities._spell_book->_spellbook.begin();

		while (spellIterator != pWeenie->m_Qualities._spell_book->_spellbook.end())
		{
			float likelihood = spellIterator->second._casting_likelihood;

			if (dice <= likelihood)
			{
				return DoCastSpell(spellIterator->first);
			}

			dice -= likelihood;
			spellIterator++;
		}
		*/
		
		auto spellIterator = pWeenie->m_Qualities._spell_book->_spellbook.begin();

		while (spellIterator != pWeenie->m_Qualities._spell_book->_spellbook.end())
		{
			float dice = Random::RollDice(0.0f, 1.0f);
			float likelihood = spellIterator->second._casting_likelihood;

			if (dice <= likelihood)
			{
				m_fNextCastTime = Timer::cur_time + pWeenie->m_Qualities.GetFloat(AI_USE_MAGIC_DELAY_FLOAT, 0.0);
				return DoCastSpell(spellIterator->first);
			}

			spellIterator++;
		}
	}

	return false;
}

bool MonsterAIManager::DoCastSpell(DWORD spell_id)
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return false;
	}

	std::shared_ptr<CWeenieObject> pTarget = GetTargetWeenie();
	pWeenie->MakeSpellcastingManager()->CreatureBeginCast(pTarget ? pTarget->GetID() : 0, spell_id);
	return true;
}

bool MonsterAIManager::DoMeleeAttack()
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return false;
	}

	DWORD motion = 0;
	ATTACK_HEIGHT height = ATTACK_HEIGHT::UNDEF_ATTACK_HEIGHT;
	float power = 0.0f;
	GenerateRandomAttack(&motion, &height, &power);
	if (!motion)
	{
		return false;
	}

	std::shared_ptr<CWeenieObject> pTarget = GetTargetWeenie();
	if (!pTarget)
	{
		return false;
	}

	pWeenie->TryMeleeAttack(pTarget->GetID(), height, power, motion);

	m_fNextAttackTime = Timer::cur_time + 2.0f;
	m_fNextChaseTime = Timer::cur_time; // chase again anytime
	m_fMinCombatStateTime = Timer::cur_time + m_fMinCombatStateDuration;

	return true;
}

void MonsterAIManager::GenerateRandomAttack(DWORD *motion, ATTACK_HEIGHT *height, float *power, std::shared_ptr<CWeenieObject> weapon)
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	*motion = 0;
	*height = ATTACK_HEIGHT::UNDEF_ATTACK_HEIGHT;
	*power = Random::GenFloat(0, 1);

	if (pWeenie->_combatTable)
	{
		if(weapon == NULL)
		if (!pWeenie->GetWieldedCombat(COMBAT_USE_TWO_HANDED))
			weapon = pWeenie->GetWieldedCombat(COMBAT_USE_MELEE);
		else {
			weapon = pWeenie->GetWieldedCombat(COMBAT_USE_TWO_HANDED);
		}
		if (weapon)
		{
			AttackType attackType = (AttackType)weapon->InqIntQuality(ATTACK_TYPE_INT, 0);

			if (attackType == (Thrust_AttackType | Slash_AttackType))
			{
				if (*power >= 0.75f)
					attackType = Slash_AttackType;
				else
					attackType = Thrust_AttackType;
			}

			CombatManeuver *combatManeuver;
			
			// some monster have undef'd attack heights (hollow?) which is index 0
			combatManeuver = pWeenie->_combatTable->TryGetCombatManuever(pWeenie->get_minterp()->InqStyle(), attackType, (ATTACK_HEIGHT)Random::GenUInt(0, 3));

			if (!combatManeuver)
			{
				// and some don't
				combatManeuver = pWeenie->_combatTable->TryGetCombatManuever(pWeenie->get_minterp()->InqStyle(), attackType, (ATTACK_HEIGHT)Random::GenUInt(1, 3));

				if (!combatManeuver)
				{
					combatManeuver = pWeenie->_combatTable->TryGetCombatManuever(pWeenie->get_minterp()->InqStyle(), attackType, ATTACK_HEIGHT::HIGH_ATTACK_HEIGHT);
				
					if (!combatManeuver)
					{
						combatManeuver = pWeenie->_combatTable->TryGetCombatManuever(pWeenie->get_minterp()->InqStyle(), attackType, ATTACK_HEIGHT::MEDIUM_ATTACK_HEIGHT);
				
						if (!combatManeuver)
						{
							combatManeuver = pWeenie->_combatTable->TryGetCombatManuever(pWeenie->get_minterp()->InqStyle(), attackType, ATTACK_HEIGHT::LOW_ATTACK_HEIGHT);
						}
					}
				}
			}

			if (combatManeuver)
			{
				*motion = combatManeuver->motion;
				*height = combatManeuver->attack_height;
			}
		}
		else
		{
			AttackType attackType;
			
			if (*power >= 0.75f)
			{
				attackType = Kick_AttackType;
			}
			else
			{
				attackType = Punch_AttackType;
			}

			CombatManeuver *combatManeuver;

			// some monster have undef'd attack heights (hollow?) which is index 0
			combatManeuver = pWeenie->_combatTable->TryGetCombatManuever(pWeenie->get_minterp()->InqStyle(), attackType, (ATTACK_HEIGHT)Random::GenUInt(0, 3));

			if (!combatManeuver)
			{
				// and some don't
				combatManeuver = pWeenie->_combatTable->TryGetCombatManuever(pWeenie->get_minterp()->InqStyle(), attackType, (ATTACK_HEIGHT)Random::GenUInt(1, 3));

				if (!combatManeuver)
				{
					combatManeuver = pWeenie->_combatTable->TryGetCombatManuever(pWeenie->get_minterp()->InqStyle(), attackType, ATTACK_HEIGHT::HIGH_ATTACK_HEIGHT);

					if (!combatManeuver)
					{
						combatManeuver = pWeenie->_combatTable->TryGetCombatManuever(pWeenie->get_minterp()->InqStyle(), attackType, ATTACK_HEIGHT::MEDIUM_ATTACK_HEIGHT);

						if (!combatManeuver)
						{
							combatManeuver = pWeenie->_combatTable->TryGetCombatManuever(pWeenie->get_minterp()->InqStyle(), attackType, ATTACK_HEIGHT::LOW_ATTACK_HEIGHT);
						}
					}
				}
			}

			if (combatManeuver)
			{
				*motion = combatManeuver->motion;
				*height = combatManeuver->attack_height;
			}
		}
	}

	if (!*motion)
	{
		*motion = Motion_AttackHigh1;
	}
}

void MonsterAIManager::BeginReturningToSpawn()
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	// pWeenie->DoForcedStopCompletely();

	MovementParameters params;
	params.can_walk = 0;

	MovementStruct mvs;
	mvs.type = MovementTypes::MoveToPosition;	
	mvs.pos = m_HomePosition;
	mvs.params = &params;

	pWeenie->movement_manager->PerformMovement(mvs);

	m_fReturnTimeoutTime = Timer::cur_time + m_fReturnTimeout;

	pWeenie->ChanceExecuteEmoteSet(m_TargetID, Homesick_EmoteCategory);
}

void MonsterAIManager::EndReturningToSpawn()
{
}

void MonsterAIManager::UpdateReturningToSpawn()
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	float fDistToHome = m_HomePosition.distance(pWeenie->m_Position);

	if (fDistToHome < 5.0f)
	{
		SwitchState(Idle);
		return;
	}
	
	if (m_fReturnTimeoutTime <= Timer::cur_time)
	{
		// teleport back to spawn
		pWeenie->Movement_Teleport(m_HomePosition);

		SwitchState(Idle);
		return;
	}
}

void MonsterAIManager::OnDeath()
{
}

bool MonsterAIManager::IsValidTarget(std::shared_ptr<CWeenieObject> pWeenie)
{
	if (!pWeenie)
		return false;

	if (pWeenie == pWeenie)
		return false;

	if (!pWeenie->_IsPlayer()) // only attack players
		return false;

	if (pWeenie->ImmuneToDamage(pWeenie)) // only attackable players (not dead, not in portal space, etc.
		return false;

	return true;
}

void MonsterAIManager::AlertIdleFriendsToAggro(std::shared_ptr<CWeenieObject> pAttacker)
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	std::list<std::shared_ptr<CWeenieObject> > results;
	g_pWorld->EnumNearby(pWeenie, 20.0f, &results);

	int ourType = pWeenie->InqIntQuality(CREATURE_TYPE_INT, 0);
	int ourFriendType = pWeenie->InqIntQuality(FRIEND_TYPE_INT, 0);
	
	for (auto weenie : results)
	{
		if (weenie == pWeenie)
			continue;

		if (!weenie->IsCreature())
			continue;

		std::shared_ptr<CMonsterWeenie> creature = weenie->AsMonster();

		if (!creature->m_MonsterAI)
			continue;

		switch (creature->m_MonsterAI->m_State)
		{
		case Idle:
		case ReturningToSpawn:
		case SeekNewTarget:
			break;

		default:
			continue;
		}

		if (!creature->m_MonsterAI->IsValidTarget(pAttacker))
			continue;

		if (creature->m_MonsterAI->_toleranceType == TolerateEverything)
			continue;

		if (creature->m_MonsterAI->_toleranceType == TolerateUnlessAttacked && _aiOptions != 1) // _aiOptions = 1 creatures do not tolerate their own kind/friends being attacked.
			continue;

		int theirType = creature->InqIntQuality(CREATURE_TYPE_INT, 0);
		int theirFriendType = creature->InqIntQuality(FRIEND_TYPE_INT, 0);

		if (ourType > 0 && theirType > 0)
		{
			if (ourType != theirType && ourType != theirFriendType && theirType != ourFriendType) //we only help our own kind or friends
				continue;
		}

		creature->m_MonsterAI->m_fAggroTime = Timer::cur_time + 10.0;
		creature->m_MonsterAI->SetNewTarget(pAttacker);
	}
}

void MonsterAIManager::OnResistSpell(std::shared_ptr<CWeenieObject> attacker)
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	if(attacker)
		pWeenie->ChanceExecuteEmoteSet(attacker->GetID(), ResistSpell_EmoteCategory);
}

void MonsterAIManager::OnEvadeAttack(std::shared_ptr<CWeenieObject> attacker)
{

}

void MonsterAIManager::OnDealtDamage(DamageEventData &damageData)
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	if (_nextTaunt > 0 && _nextTaunt <= Timer::cur_time)
	{

		std::shared_ptr<CWeenieObject> pTarget = damageData.target.lock();

		if (pTarget)
			pWeenie->ChanceExecuteEmoteSet(pTarget->GetID(), Taunt_EmoteCategory);
		_nextTaunt = Timer::cur_time + Random::GenUInt(10, 30);
	}
}

void MonsterAIManager::OnTookDamage(DamageEventData &damageData)
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	std::shared_ptr<CWeenieObject> source = damageData.source.lock();
	if (!source)
	{
		return;
	}

	unsigned int damage = damageData.outputDamageFinal;

	HandleAggro(source);

	if(damageData.wasCrit)
		pWeenie->ChanceExecuteEmoteSet(source->GetID(), ReceiveCritical_EmoteCategory);

	if (pWeenie->m_Qualities._emote_table && !pWeenie->IsExecutingEmote())
	{
		PackableList<EmoteSet> *emoteSetList = pWeenie->m_Qualities._emote_table->_emote_table.lookup(WoundedTaunt_EmoteCategory);

		if (emoteSetList)
		{
			double healthPercent = pWeenie->GetHealthPercent();
			if (m_fLastWoundedTauntHP > healthPercent)
			{
				double dice = Random::GenFloat(0.0, 1.0);

				for (auto &emoteSet : *emoteSetList)
				{
					// ignore probability?
					if (healthPercent >= emoteSet.minhealth && healthPercent < emoteSet.maxhealth)
					{
						m_fLastWoundedTauntHP = healthPercent;

						pWeenie->MakeEmoteManager()->ExecuteEmoteSet(emoteSet, source->GetID());
					}
				}
			}
		}
	}
}

void MonsterAIManager::OnIdentifyAttempted(std::shared_ptr<CWeenieObject> other)
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	if (_toleranceType != TolerateUnlessBothered)
	{
		return;
	}

	if (pWeenie->DistanceTo(other, true) >= 60.0)
	{
		return;
	}

	HandleAggro(other);
}

void MonsterAIManager::HandleAggro(std::shared_ptr<CWeenieObject> pAttacker)
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	if (_toleranceType == TolerateEverything)
	{
		return;
	}

	if (!pWeenie->IsDead())
	{
		switch (m_State)
		{
		case Idle:
		case ReturningToSpawn:
		case SeekNewTarget:
			{
				if (IsValidTarget(pAttacker))
				{
					//if (pWeenie->DistanceTo(pAttacker, true) <= pWeenie->InqFloatQuality(VISUAL_AWARENESS_RANGE_FLOAT, DEFAULT_AWARENESS_RANGE))
					//{
					SetNewTarget(pAttacker);
					//}

					pWeenie->ChanceExecuteEmoteSet(pAttacker->GetID(), Scream_EmoteCategory);
					m_fAggroTime = Timer::cur_time + 10.0;
				}

				break;
			}
		}
	}

	AlertIdleFriendsToAggro(pAttacker);
}

void MonsterAIManager::BeginSeekNewTarget()
{
	if (!ShouldSeekNewTarget() || !SeekTarget())
	{
		SwitchState(ReturningToSpawn);
	}
}

void MonsterAIManager::UpdateSeekNewTarget()
{
	SwitchState(ReturningToSpawn);
}

void MonsterAIManager::EndSeekNewTarget()
{
}

void MonsterAIManager::BeginMeleeModeAttack()
{
	std::shared_ptr<CMonsterWeenie> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}


	std::shared_ptr<CWeenieObject> pShield = _shield.lock();
	std::shared_ptr<CWeenieObject> pCurrentShield = _currentShield.lock();


	if (pShield && !pCurrentShield)
	{
		if (pWeenie->FinishMoveItemToWield(pShield, SHIELD_LOC)) //make sure our shield is equipped
			_currentShield = _shield;
	}


	std::shared_ptr<CWeenieObject> pMeleeWeapon = _meleeWeapon.lock();
	std::shared_ptr<CWeenieObject> pCurrentWeapon = _currentWeapon.lock();

	if (pCurrentWeapon != pMeleeWeapon)
	{
		if (pCurrentWeapon)
		{
			pWeenie->FinishMoveItemToContainer(pCurrentWeapon, pWeenie, 0, true);
		}
		if (pMeleeWeapon)
		{
			if (pWeenie->FinishMoveItemToWield(pMeleeWeapon, MELEE_WEAPON_LOC))
			{
				_currentWeapon = pMeleeWeapon;
			}
			else
			{
				_currentWeapon = std::weak_ptr<CWeenieObject>();
			}
		}
	}

	pWeenie->ChangeCombatMode(COMBAT_MODE::MELEE_COMBAT_MODE, false);

	m_fChaseTimeoutTime = Timer::cur_time + m_fChaseTimeoutDuration;
	m_fNextAttackTime = Timer::cur_time;
	m_fNextChaseTime = Timer::cur_time;
	m_fMinCombatStateTime = Timer::cur_time + m_fMinCombatStateDuration;
	m_fMinReturnStateTime = Timer::cur_time + m_fMinReturnStateDuration;
}

void MonsterAIManager::EndMeleeModeAttack()
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}
	pWeenie->unstick_from_object();
}

void MonsterAIManager::UpdateMeleeModeAttack()
{
	std::shared_ptr<CWeenieObject> pWeenie = m_pWeenie.lock();
	if (!pWeenie || pWeenie->IsBusyOrInAction())
	{
		// still animating or busy (attacking, etc.)
		return;
	}

	// rules:
	// dont switch targets to one that is farther than visual awareness range, unless attacked
	// dont chase a target that is outside the chase range, unless attacked
	// dont chase any new target, even if attacked, outside home range

	std::shared_ptr<CWeenieObject> pTarget = GetTargetWeenie();
	if (!pTarget || pTarget->IsDead() || !pTarget->IsAttackable() || pTarget->ImmuneToDamage(pWeenie) || pWeenie->DistanceTo(pTarget) >= m_fChaseRange)
	{
		if (ShouldSeekNewTarget())
		{
			SwitchState(SeekNewTarget);
		}
		else
		{
			SwitchState(ReturningToSpawn);
		}
		return;
	}

	if (DistanceToHome() >= m_fMaxHomeRange)
	{
		SwitchState(ReturningToSpawn);
		return;
	}

	double fTargetDist = pWeenie->DistanceTo(pTarget, true);
	if (fTargetDist >= pWeenie->InqFloatQuality(VISUAL_AWARENESS_RANGE_FLOAT, DEFAULT_AWARENESS_RANGE) && m_fAggroTime <= Timer::cur_time)
	{
		SwitchState(ReturningToSpawn);
		return;
	}

	if (m_fNextAttackTime > Timer::cur_time)
	{
		return;
	}

	if (!RollDiceCastSpell() && pWeenie->DistanceTo(pTarget) < m_fChaseRange)
	{
		// do physics attack
		DWORD motion = 0;
		ATTACK_HEIGHT height = ATTACK_HEIGHT::UNDEF_ATTACK_HEIGHT;
		float power = 0.0f;
		GenerateRandomAttack(&motion, &height, &power);
		pWeenie->TryMeleeAttack(pTarget->GetID(), height, power, motion);

		m_fNextAttackTime = Timer::cur_time + 2.0f;
		m_fNextChaseTime = Timer::cur_time; // chase again anytime
		m_fMinCombatStateTime = Timer::cur_time + m_fMinCombatStateDuration;
	}
}

void MonsterAIManager::BeginMissileModeAttack()
{
	std::shared_ptr<CMonsterWeenie> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	std::shared_ptr<CWeenieObject> pMissileWeapon = _missileWeapon.lock();

	if (!pMissileWeapon)
	{
		SwitchState(MeleeModeAttack);
		return;
	}
	else
	{
		std::shared_ptr<CWeenieObject> pShield = _shield.lock();
		std::shared_ptr<CWeenieObject> pCurrentShield = _currentShield.lock();

		if (pMissileWeapon->InqIntQuality(DEFAULT_COMBAT_STYLE_INT, 0) != ThrownWeapon_CombatStyle)
		{
			std::shared_ptr<CWeenieObject> equippedAmmo = pWeenie->GetWieldedCombat(COMBAT_USE::COMBAT_USE_AMMO);
			if (!equippedAmmo)
			{
				//we don't have ammo, disable missile mode and switch to melee.
				_missileWeapon = std::weak_ptr<CWeenieObject>();
				SwitchState(MeleeModeAttack);
				return;
			}

			if (pCurrentShield)
			{
				pWeenie->FinishMoveItemToContainer(pCurrentShield, pWeenie, 0, true); //get rid of the shield.
				_currentShield = std::weak_ptr<CWeenieObject>();
			}
		}
		else if (pShield && !pCurrentShield)
		{
			if (pWeenie->FinishMoveItemToWield(pShield, SHIELD_LOC)) //shields can be wielded with thrown weapons.
			{
				_currentShield = pShield;
			}
		}
	}
	std::shared_ptr<CWeenieObject> pCurrentWeapon = _currentWeapon.lock();

	if (pCurrentWeapon && pCurrentWeapon != pMissileWeapon)
	{
		pWeenie->FinishMoveItemToContainer(pCurrentWeapon, pWeenie, 0, true);
		if (pWeenie->FinishMoveItemToWield(pMissileWeapon, MISSILE_WEAPON_LOC))
			_currentWeapon = _missileWeapon;
		else
		{
			_currentWeapon = std::weak_ptr<CWeenieObject>();
			SwitchState(MeleeModeAttack);
		}
	}

	pWeenie->ChangeCombatMode(COMBAT_MODE::MISSILE_COMBAT_MODE, false);

	m_fChaseTimeoutTime = Timer::cur_time + m_fChaseTimeoutDuration;
	m_fNextAttackTime = Timer::cur_time;
	m_fNextChaseTime = Timer::cur_time;
	m_fMinCombatStateTime = Timer::cur_time + m_fMinCombatStateDuration;
	m_fMinReturnStateTime = Timer::cur_time + m_fMinReturnStateDuration;
}

void MonsterAIManager::EndMissileModeAttack()
{
}

void MonsterAIManager::UpdateMissileModeAttack()
{
	std::shared_ptr<CMonsterWeenie> pWeenie = m_pWeenie.lock();
	if (!pWeenie)
	{
		return;
	}

	if (pWeenie->IsBusyOrInAction() || pWeenie->motions_pending())
	{
		// still animating or busy (attacking, etc.)
		return;
	}

	// rules:
	// dont switch targets to one that is farther than visual awareness range, unless attacked
	// dont chase a target that is outside the chase range, unless attacked
	// dont chase any new target, even if attacked, outside home range

	std::shared_ptr<CWeenieObject> pTarget = GetTargetWeenie();
	if (!pTarget || pTarget->IsDead() || !pTarget->IsAttackable() || pTarget->ImmuneToDamage(pWeenie) || pWeenie->DistanceTo(pTarget) >= m_fChaseRange)
	{
		if (ShouldSeekNewTarget())
		{
			SwitchState(SeekNewTarget);
		}
		else
		{
			SwitchState(ReturningToSpawn);
		}
		return;
	}

	if (DistanceToHome() >= m_fMaxHomeRange)
	{
		SwitchState(ReturningToSpawn);
		return;
	}

	float weaponMinRange = 1;
	float weaponMaxRange = 60; //todo: get the value from the weapon? Players currently have 60 as a fixed max value.

	double fTargetDist = pWeenie->DistanceTo(pTarget, true);
	if (fTargetDist >= max(pWeenie->InqFloatQuality(VISUAL_AWARENESS_RANGE_FLOAT, DEFAULT_AWARENESS_RANGE), weaponMaxRange) && m_fAggroTime <= Timer::cur_time)
	{
		SwitchState(ReturningToSpawn);
		return;
	}

	if (m_fNextAttackTime > Timer::cur_time)
	{
		return;
	}


	std::shared_ptr<CWeenieObject> pMeleeWeapon = _meleeWeapon.lock();

	if (pMeleeWeapon || _hasUnarmedSkill) //we also have a melee weapon(or know how to fight without one)
	{
		double roll = Random::GenFloat(0.0, 1.0);
		if (fTargetDist < weaponMinRange && roll < 0.3) //the target is too close, let's go melee
		{
			SwitchState(MeleeModeAttack);
			return;
		}

		if (roll < 0.02) //we're tired of doing missile attacks, let's switch it up
		{
			SwitchState(MeleeModeAttack);
			return;
		}
	}

	if (!RollDiceCastSpell())
	{
		if (pWeenie->DistanceTo(pTarget) < weaponMaxRange)
		{
			// do physics attack
			DWORD motion = 0;
			ATTACK_HEIGHT height = (ATTACK_HEIGHT)Random::GenUInt(1, 3);
			float power = Random::GenFloat(0, 1);

			pWeenie->TryMissileAttack(pTarget->GetID(), height, power);

			m_fNextAttackTime = Timer::cur_time + 2.0f;
			m_fNextChaseTime = Timer::cur_time; // chase again anytime
			m_fMinCombatStateTime = Timer::cur_time + m_fMinCombatStateDuration;
		}
	}
}