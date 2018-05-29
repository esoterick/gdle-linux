#include "StdAfx.h"
#include "AttackManager.h"
#include "WeenieObject.h"
#include "World.h"
#include "Player.h"
#include "WeenieFactory.h"
#include "Ammunition.h"
#include "CombatFormulas.h"

// TODO fix memory leak with attack data

const double DISTANCE_REQUIRED_FOR_MELEE_ATTACK = 2.0;
const double MAX_MELEE_ATTACK_CONE_ANGLE = 90.0;
const double MAX_MISSILE_ATTACK_CONE_ANGLE = 3.0;

#define MISSILE_SLOW_SPEED 0.9f
#define MISSILE_FAST_SPEED 1.25f

CAttackEventData::CAttackEventData()
{
}

void CAttackEventData::Update()
{
	if (_attack_charge_time >= 0.0 && Timer::cur_time >= _attack_charge_time)
	{
		PostCharge();
	}

	CheckTimeout();
}

void CAttackEventData::Setup()
{
	_max_attack_distance = DISTANCE_REQUIRED_FOR_MELEE_ATTACK;
	_max_attack_angle = MAX_MELEE_ATTACK_CONE_ANGLE;
	_timeout = Timer::cur_time + 15.0;
}

void CAttackEventData::PostCharge()
{
	_attack_charge_time = -1.0;

	if (InAttackCone())
	{
		OnReadyToAttack();
	}
	else
	{
		MoveToAttack();
	}
}

void CAttackEventData::Begin()
{
	Setup();

	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	if (target->HasOwner())
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	if (!target->IsAttackable())
	{
		Cancel();
		return;
	}
	
	if (_attack_charge_time < 0.0 || Timer::cur_time >= _attack_charge_time)
	{
		PostCharge();
	}
}

void CAttackEventData::MoveToAttack()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}

	_move_to = true;
	
	MovementParameters params;
	params.can_walk = 0;
	params.can_run = 0;
	params.can_sidestep = 0;
	params.can_walk_backwards = 0;
	params.move_away = 1;
	params.can_charge = m_bCanCharge || !pWeenie->AsPlayer() ? 1 : 0;
	params.fail_walk = 1;
	params.use_final_heading = 1;
	params.sticky = _use_sticky;

	params.min_distance = 0.1f; 
	params.distance_to_object = _max_attack_distance - 0.5f; // 0.5
	params.fail_distance = _fail_distance;
	params.speed = 1.5f;
	params.action_stamp = ++pWeenie->m_wAnimSequence;
	pWeenie->last_move_was_autonomous = false;
	pWeenie->MoveToObject(_target_id, &params);
}

void CAttackEventData::CheckTimeout()
{
	if (Timer::cur_time > _timeout)
	{
		if (_move_to)
			Cancel(WERROR_MOVED_TOO_FAR);
		else
			Cancel(0);
	}
}

void CAttackEventData::Cancel(DWORD error)
{
	CancelMoveTo();

	_manager->OnAttackCancelled(error);
}

void CAttackEventData::CancelMoveTo()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}

	if (_move_to)
	{
		pWeenie->cancel_moveto();
		pWeenie->Animation_MoveToUpdate();

		_move_to = false;
	}
}

double CAttackEventData::DistanceToTarget()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return FLT_MAX;
	}

	if (!_target_id || _target_id == pWeenie->GetID())
		return 0.0;

	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target)
		return FLT_MAX;

	return pWeenie->DistanceTo(target, true);
}

double CAttackEventData::HeadingToTarget(bool relative)
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return 0.0;
	}

	if (!_target_id || _target_id == pWeenie->GetID())
		return 0.0;

	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target)
		return 0.0;

	return pWeenie->HeadingTo(target, relative);
}

bool CAttackEventData::InAttackRange()
{
	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target || target->HasOwner())
		return true;

	if ((_max_attack_distance + F_EPSILON) < DistanceToTarget())
		return false;

	return true;
}

bool CAttackEventData::InAttackCone()
{
	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target || target->HasOwner())
		return true;

	if ((_max_attack_distance + F_EPSILON) < DistanceToTarget())
		return false;
	if ((_max_attack_angle + F_EPSILON) < HeadingToTarget())
		return false;

	return true;
}

std::shared_ptr<CWeenieObject> CAttackEventData::GetTarget()
{
	return g_pWorld->FindObject(_target_id);
}

void CAttackEventData::HandleMoveToDone(DWORD error)
{
	_move_to = false;

	if (error)
	{
		Cancel(error);
		return;
	}

	if (!InAttackRange())
	{
		Cancel(WERROR_TOO_FAR);
		return;
	}

	OnReadyToAttack();
}

void CAttackEventData::OnMotionDone(DWORD motion, BOOL success)
{
	if (_move_to || _turn_to || !_active_attack_anim)
		return;

	if (motion == _active_attack_anim)
	{
		_active_attack_anim = 0;

		if (success)
		{
			OnAttackAnimSuccess(motion);
		}
		else
		{
			Cancel();
		}
	}
}

void CAttackEventData::OnAttackAnimSuccess(DWORD motion)
{
	Done();
}

void CAttackEventData::Done(DWORD error)
{
	_manager->OnAttackDone(error);
}

bool CAttackEventData::IsValidTarget()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}
	std::shared_ptr<CWeenieObject> target = GetTarget();

	if (!target || !target->IsAttackable() || target->IsDead() || target->IsInPortalSpace() || target->ImmuneToDamage(pWeenie))
	{
		return false;
	}

	return true;
}

void CAttackEventData::ExecuteAnimation(DWORD motion, MovementParameters *params)
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}
	assert(!_move_to);
	assert(!_turn_to);
	assert(!_active_attack_anim);

	if (pWeenie->IsDead() || pWeenie->IsInPortalSpace())
	{
		Cancel(WERROR_ACTIONS_LOCKED);
		return;
	}

	_active_attack_anim = motion;

	DWORD error = pWeenie->DoForcedMotion(motion, params);

	if (error)
	{
		Cancel(error);
	}
}

void CMeleeAttackEvent::Setup()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}
	DWORD attack_motion = 0;
	DWORD weapon_id = 0;

	if (!_do_attack_animation)
	{
		if (pWeenie->_combatTable)
		{
			std::shared_ptr<CWeenieObject> weapon = NULL;
			if (!pWeenie->GetWieldedCombat(COMBAT_USE_TWO_HANDED))
			{
				weapon = pWeenie->GetWieldedCombat(COMBAT_USE_MELEE);
			}
			else 
			{
				weapon = pWeenie->GetWieldedCombat(COMBAT_USE_TWO_HANDED);
			}

			if (weapon)
			{
				//_max_attack_distance = weapon->InqFloatQuality(WEAPON_LENGTH_FLOAT, 0.5); //todo: this would be interesting but something makes the character still move next to the target anyway. Is it the client?

				weapon_id = weapon->GetID();

				AttackType attack_type = (AttackType)weapon->InqIntQuality(ATTACK_TYPE_INT, 0);

				if (attack_type == (Thrust_AttackType | Slash_AttackType))
				{
					if (_attack_power >= 0.75f)
						attack_type = Slash_AttackType;
					else
						attack_type = Thrust_AttackType;
				}

				if (CombatManeuver *combat_maneuver = pWeenie->_combatTable->TryGetCombatManuever(pWeenie->get_minterp()->InqStyle(), attack_type, _attack_height))
				{
					attack_motion = combat_maneuver->motion;
				}
			}
		}

		if (!attack_motion)
		{
			switch (_attack_height)
			{
			case LOW_ATTACK_HEIGHT: attack_motion = Motion_AttackLow1; break;
			case MEDIUM_ATTACK_HEIGHT: attack_motion = Motion_AttackMed1; break;
			case HIGH_ATTACK_HEIGHT: attack_motion = Motion_AttackHigh1; break;
			default:
				{
					Cancel();
					return;
				}
			}

			if (_attack_power >= 0.25f)
				attack_motion += 3;
			if (_attack_power >= 0.75f)
				attack_motion += 3;

			if (_attack_power < 0.0f || _attack_power > 1.0f)
			{
				Cancel();
				return;
			}
		}
	
		// melee attacks can charge!
		m_bCanCharge = true;
		_do_attack_animation = attack_motion;
	}

	DWORD quickness = 0;
	pWeenie->m_Qualities.InqAttribute(QUICKNESS_ATTRIBUTE, quickness, FALSE);

	int weaponAttackTime = pWeenie->GetAttackTimeUsingWielded();
	int creatureAttackTime = max(0, 120 - (((int)quickness - 60) / 2)); //we reach 0 attack speed at 300 quickness

	int attackTime = (creatureAttackTime + weaponAttackTime) / 2; //our attack time is the average between our speed and the speed of our weapon.
	attackTime = max(0, min(120, attackTime));

	_attack_speed = 2.25f - (attackTime * (1.0 / 70.0));
	_attack_speed = max(min(_attack_speed, 2.25), 0.8);

	//old formula:
	//int attackTime = max(0, min(120, pWeenie->GetAttackTimeUsingWielded()));
	//_attack_speed = 1.0 / (1.0 / (1.0 + ((120 - attackTime) * (0.005))));

	CAttackEventData::Setup();
}

void CMeleeAttackEvent::OnReadyToAttack()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}

	if (_do_attack_animation)
	{
		MovementParameters params;
		params.sticky = 1;
		params.can_charge = 1;
		params.modify_interpreted_state = 1;
		params.speed = _attack_speed;
		params.action_stamp = ++pWeenie->m_wAnimSequence;
		params.autonomous = 0;
		pWeenie->stick_to_object(_target_id);

		ExecuteAnimation(_do_attack_animation, &params);
	}
	else
	{
		Finish();
	}
}

void CMeleeAttackEvent::OnAttackAnimSuccess(DWORD motion)
{
	Finish();
}

void CMeleeAttackEvent::Finish()
{
	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	Done();
}

void CMeleeAttackEvent::HandleAttackHook(const AttackCone &cone)
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}
	std::shared_ptr<CWeenieObject> target = GetTarget();

	if (!target || !IsValidTarget())
	{
		return;
	}

	int preVarianceDamage;
	float variance;

	STypeSkill weaponSkill = STypeSkill::UNDEF_SKILL;
	DAMAGE_TYPE damageType = DAMAGE_TYPE::UNDEF_DAMAGE_TYPE;

	double offenseMod = 1.0;

	bool isBodyPart = false;
	std::shared_ptr<CWeenieObject> weapon = NULL;
	if (!pWeenie->GetWieldedCombat(COMBAT_USE_TWO_HANDED))
		weapon = pWeenie->GetWieldedCombat(COMBAT_USE_MELEE);
	else {
		weapon = pWeenie->GetWieldedCombat(COMBAT_USE_TWO_HANDED);
	}

	if (!weapon) //if we still don't have a weapon use our body parts
	{
		weapon = pWeenie;
		if (pWeenie->m_Qualities._body)
		{
			BodyPart *part = pWeenie->m_Qualities._body->_body_part_table.lookup(cone.part_index);
			if (part)
			{
				isBodyPart = true;
				damageType = part->_dtype;
				preVarianceDamage = part->_dval;
				variance = part->_dvar;
			}
		}

		std::shared_ptr<CWeenieObject> gloverOrBoots;
		if (_attack_power >= 0.75f) //this is a kick
			gloverOrBoots = pWeenie->GetWielded(FOOT_WEAR_LOC);
		else //this is a punch
			gloverOrBoots = pWeenie->GetWielded(HAND_WEAR_LOC);

		if (gloverOrBoots)
		{
			damageType = (DAMAGE_TYPE)gloverOrBoots->InqIntQuality(DAMAGE_TYPE_INT, damageType);
			preVarianceDamage += gloverOrBoots->GetAttackDamage();
			variance = gloverOrBoots->InqFloatQuality(DAMAGE_VARIANCE_FLOAT, variance);
		}
	}

	if (!isBodyPart)
	{
		preVarianceDamage = weapon->GetAttackDamage();
		variance = weapon->InqFloatQuality(DAMAGE_VARIANCE_FLOAT, 0.0f);
		damageType = weapon->InqDamageType();
	}
	offenseMod = weapon->GetOffenseMod();
	weaponSkill = SkillTable::OldToNewSkill((STypeSkill)weapon->InqIntQuality(WEAPON_SKILL_INT, LIGHT_WEAPONS_SKILL, TRUE));

	//todo: maybe handle this differently as to integrate all possible damage type combos
	if (damageType == (DAMAGE_TYPE::SLASH_DAMAGE_TYPE|DAMAGE_TYPE::PIERCE_DAMAGE_TYPE))
	{
		if (_attack_power >= 0.75f)
			damageType = DAMAGE_TYPE::SLASH_DAMAGE_TYPE;
		else
			damageType = DAMAGE_TYPE::PIERCE_DAMAGE_TYPE;
	}
	else if (damageType == (DAMAGE_TYPE::SLASH_DAMAGE_TYPE | DAMAGE_TYPE::FIRE_DAMAGE_TYPE))
	{
		//todo: as far as I know only the Mattekar Claw had this, figure out what it did exactly, was it like this? or was it a bit of both damages?
		//or even a chance for fire damage?
		if (_attack_power >= 0.75f)
			damageType = DAMAGE_TYPE::SLASH_DAMAGE_TYPE;
		else
			damageType = DAMAGE_TYPE::FIRE_DAMAGE_TYPE;
	}

	std::shared_ptr<CWeenieObject> shield = pWeenie->GetWieldedCombat(COMBAT_USE::COMBAT_USE_SHIELD);

	int burden = 0;
	if (weapon != NULL && weapon != pWeenie)
		burden += weapon->InqIntQuality(ENCUMB_VAL_INT, 0);

	if (shield != NULL)
		burden += shield->InqIntQuality(ENCUMB_VAL_INT, 0);

	int necessaryStamina;
	if (_attack_power < 0.33)
		necessaryStamina = max(round(burden / 900.0f), 1);
	else if (_attack_power < 0.66)
		necessaryStamina = max(round(burden / 600.0f), 1);
	else
		necessaryStamina = max(round(burden / 300.0f), 1);

	if (pWeenie->AsPlayer())
	{
		//the higher a player's Endurance, the less stamina one uses while attacking. 
		//This benefit is tied to Endurance only, and it caps out at around 50% less stamina used per attack. 
		//The minimum stamina used per attack remains one. 
		DWORD endurance = 0;
		pWeenie->m_Qualities.InqAttribute(ENDURANCE_ATTRIBUTE, endurance, true);
		float necessaryStaminaMod = 1.0 - ((float)endurance - 100.0) / 600.0; //made up formula: 50% reduction at 400 endurance.
		necessaryStaminaMod = min(max(necessaryStaminaMod, 0.5), 1.0);
		necessaryStamina = round((float)necessaryStamina * necessaryStaminaMod);
	}
	necessaryStamina = max(necessaryStamina, 1);

	bool hadEnoughStamina = true;
	if (pWeenie->GetStamina() < necessaryStamina)
		hadEnoughStamina = false;

	pWeenie->AdjustStamina(-necessaryStamina);

	DWORD weaponSkillLevel = 0;
	if (pWeenie->InqSkill(weaponSkill, weaponSkillLevel, FALSE))
	{
		weaponSkillLevel = (DWORD)(weaponSkillLevel * offenseMod);

		if (!hadEnoughStamina)
		{
			pWeenie->NotifyWeenieError(WERROR_STAMINA_TOO_LOW);
			weaponSkillLevel *= 0.5; //50% penalty to our attack skill when we don't have enough to perform it.
		}
	}

	// okay, we're attacking. check for pvp interactions
	if (target->AsPlayer() && pWeenie->AsPlayer())
	{
		target->AsPlayer()->UpdatePKActivity();
		pWeenie->AsPlayer()->UpdatePKActivity();
	}

	DWORD meleeDefense = 0;
	if (target->InqSkill(MELEE_DEFENSE_SKILL, meleeDefense, FALSE) && meleeDefense > 0)
	{
		if (target->TryMeleeEvade(weaponSkillLevel))
		{
			target->OnEvadeAttack(pWeenie);

			// send evasion message
			BinaryWriter attackerEvadeEvent;
			attackerEvadeEvent.Write<DWORD>(0x01B3);
			attackerEvadeEvent.WriteString(target->GetName());
			pWeenie->SendNetMessage(&attackerEvadeEvent, PRIVATE_MSG, TRUE, FALSE);

			BinaryWriter attackedEvadeEvent;
			attackedEvadeEvent.Write<DWORD>(0x01B4);
			attackedEvadeEvent.WriteString(pWeenie->GetName());
			target->SendNetMessage(&attackedEvadeEvent, PRIVATE_MSG, TRUE, FALSE);
			return;
		}
	}

	DAMAGE_QUADRANT hitQuadrant = DAMAGE_QUADRANT::DQ_UNDEF;
	switch (_attack_height)
	{
	case HIGH_ATTACK_HEIGHT:
		hitQuadrant = DAMAGE_QUADRANT::DQ_HIGH;
		break;
	default:
	case MEDIUM_ATTACK_HEIGHT:
		hitQuadrant = DAMAGE_QUADRANT::DQ_MEDIUM;
		break;
	case LOW_ATTACK_HEIGHT:
		hitQuadrant = DAMAGE_QUADRANT::DQ_LOW;
		break;
	}

	double angle = pWeenie->HeadingFrom(_target_id, false);
	if (angle <= 45)
		hitQuadrant = (DAMAGE_QUADRANT)(hitQuadrant | DAMAGE_QUADRANT::DQ_FRONT);
	else if (angle > 45 && angle <= 135)
		hitQuadrant = (DAMAGE_QUADRANT)(hitQuadrant | DAMAGE_QUADRANT::DQ_RIGHT);
	else if (angle > 135 && angle <= 225)
		hitQuadrant = (DAMAGE_QUADRANT)(hitQuadrant | DAMAGE_QUADRANT::DQ_BACK);
	else if (angle > 225 && angle <= 315)
		hitQuadrant = (DAMAGE_QUADRANT)(hitQuadrant | DAMAGE_QUADRANT::DQ_LEFT);
	else
		hitQuadrant = (DAMAGE_QUADRANT)(hitQuadrant | DAMAGE_QUADRANT::DQ_FRONT);

	DamageEventData dmgEvent;
	dmgEvent.source = pWeenie;
	dmgEvent.target = target;
	dmgEvent.weapon = weapon;
	dmgEvent.damage_form = DF_MELEE;
	dmgEvent.damage_type = damageType;
	dmgEvent.hit_quadrant = hitQuadrant;
	dmgEvent.attackSkill = weaponSkill;
	dmgEvent.attackSkillLevel = weaponSkillLevel;
	dmgEvent.preVarianceDamage = preVarianceDamage;
	dmgEvent.baseDamage = preVarianceDamage * (1.0f - Random::GenFloat(0.0f, variance)) * (0.5 + _attack_power);

	CalculateDamage(&dmgEvent);

	pWeenie->TryToDealDamage(dmgEvent);
}


void CMissileAttackEvent::Setup()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}

	if (_attack_charge_time >= 0.0)
	{
		_attack_charge_time += 1; // for reload animation
	}

	DWORD attack_motion = 0;
	DWORD weapon_id = 0;

	if (!_do_attack_animation)
	{
		/*
		if (pWeenie->_combatTable)
		{
			std::shared_ptr<CWeenieObject> weapon = pWeenie->GetWieldedCombat(COMBAT_USE_MISSILE);
			if (weapon)
			{
				weapon_id = weapon->GetID();

				AttackType attack_type = (AttackType)weapon->InqIntQuality(ATTACK_TYPE_INT, 0);

				if (CombatManeuver *combat_maneuver = pWeenie->_combatTable->TryGetCombatManuever(pWeenie->get_minterp()->InqStyle(), attack_type, _attack_height))
				{
					attack_motion = combat_maneuver->motion;
				}
			}
		}
		*/

		if (!attack_motion)
		{
			switch (_attack_height)
			{
			case LOW_ATTACK_HEIGHT: attack_motion = Motion_AimLevel; break;
			case MEDIUM_ATTACK_HEIGHT: attack_motion = Motion_AimLevel; break;
			case HIGH_ATTACK_HEIGHT: attack_motion = Motion_AimLevel; break;
			default:
				{
					Cancel();
					return;
				}
			}

			if (_attack_power < 0.0f || _attack_power > 1.0f)
			{
				Cancel();
				return;
			}
		}

		_do_attack_animation = attack_motion;
	}

	DWORD quickness = 0;
	pWeenie->m_Qualities.InqAttribute(QUICKNESS_ATTRIBUTE, quickness, FALSE);

	int weaponAttackTime = pWeenie->GetAttackTimeUsingWielded();
	int creatureAttackTime = max(0, 120 - (((int)quickness - 60) / 2)); //we reach 0 attack speed at 300 quickness

	int attackTime = (creatureAttackTime + weaponAttackTime) / 2; //our attack time is the average between our speed and the speed of our weapon.
	attackTime = max(0, min(120, attackTime));

	_attack_speed = 2.5f - (attackTime * (1.0 / 70.0));
	_attack_speed = max(min(_attack_speed, 2.5), 0.8);

	//old formula:
	//int attackTime = max(0, min(120, pWeenie->GetAttackTimeUsingWielded()));
	//_attack_speed = 1.0 / (1.0 / (1.0 + ((120 - attackTime) * (0.005))));

	CAttackEventData::Setup();

	_max_attack_distance = 80.0;
	_max_attack_angle = MAX_MISSILE_ATTACK_CONE_ANGLE;
	_timeout = Timer::cur_time + 15.0;
	_use_sticky = false;
}

void CMissileAttackEvent::OnReadyToAttack()
{
	if (_do_attack_animation)
	{
		MovementParameters params;
		params.modify_interpreted_state = 1;
		params.speed = _attack_speed;
		params.autonomous = 0;

		CalculateAttackMotion();

		ExecuteAnimation(_do_attack_animation, &params);
	}
	else
	{
		Finish();
	}
}

void CMissileAttackEvent::CalculateAttackMotion()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}

	std::shared_ptr<CWeenieObject> weapon = pWeenie->GetWieldedCombat(COMBAT_USE::COMBAT_USE_MISSILE);

	if (!weapon)
	{
		pWeenie->DoForcedStopCompletely();
		return;
	}

	std::shared_ptr<CWeenieObject> equippedAmmo;

	bool isThrownWeapon = (weapon->InqIntQuality(DEFAULT_COMBAT_STYLE_INT, 0) == ThrownWeapon_CombatStyle);
	bool isAtlatl = (weapon->InqIntQuality(DEFAULT_COMBAT_STYLE_INT, 0) == Atlatl_CombatStyle);

	if (isThrownWeapon)
		equippedAmmo = weapon;
	else
		equippedAmmo = pWeenie->GetWieldedCombat(COMBAT_USE::COMBAT_USE_AMMO);

	if (!equippedAmmo)
	{
		pWeenie->DoForcedStopCompletely();
		pWeenie->NotifyWeenieError(WERROR_COMBAT_OUT_OF_AMMO);
		return;
	}

	CalculateTargetPosition();
	CalculateSpawnPosition(equippedAmmo->GetRadius());

	bool bTrack = true;
	float fSpeed = weapon->InqFloatQuality(MAXIMUM_VELOCITY_FLOAT, 20.0);
	if (std::shared_ptr<CPlayerWeenie> pPlayer = pWeenie->AsPlayer())
	{
		bTrack = pPlayer->GetCharacterOptions2() & LeadMissileTargets_CharacterOptions2;
		fSpeed *= pPlayer->GetCharacterOptions2() & UseFastMissiles_CharacterOptions2 ? MISSILE_FAST_SPEED : MISSILE_SLOW_SPEED;
	}
	else
	{
		fSpeed *= MISSILE_SLOW_SPEED;
	}

	CalculateMissileVelocity(bTrack, true, fSpeed);

	float fVertAngle = RAD2DEG(asin(_missile_velocity.z / _missile_velocity.magnitude()));

	int motions[] = { Motion_AimLevel, Motion_AimHigh15, Motion_AimHigh30, Motion_AimHigh45, Motion_AimHigh60, Motion_AimHigh75, Motion_AimHigh90, Motion_AimLow15, Motion_AimLow30, Motion_AimLow45, Motion_AimLow60, Motion_AimLow75, Motion_AimLow90 };
	int iMotionIndex = 0;

	if (fVertAngle > 7.5)
	{
		iMotionIndex = min(floor((fVertAngle + 7.55) / 15), 6);
	}
	else if (fVertAngle < -7.5)
	{
		iMotionIndex = -min(ceil((fVertAngle - 7.55) / 15), 6) + 6;
	}

	_do_attack_animation = motions[min(max(0, iMotionIndex), 12)];
}

bool CMissileAttackEvent::CalculateTargetPosition()
{
	std::shared_ptr<CWeenieObject> target = GetTarget();
	assert(target);

	if (!target || !target->InValidCell())
	{
		return false;
	}

	_missile_target_position = target->GetPosition();

	switch (_attack_height)
	{
	case ATTACK_HEIGHT::LOW_ATTACK_HEIGHT:
		_missile_target_position.frame.m_origin.z += target->GetHeight() * (1.0 / 6.0); // 0.25;
		break;

	default:
	case ATTACK_HEIGHT::MEDIUM_ATTACK_HEIGHT:
		_missile_target_position.frame.m_origin.z += target->GetHeight() * 0.5;
		break;

	case ATTACK_HEIGHT::HIGH_ATTACK_HEIGHT:
		_missile_target_position.frame.m_origin.z += target->GetHeight() * (5.0 / 6.0); // 0.75;
		break;
	}

	return true;
}

bool CMissileAttackEvent::CalculateSpawnPosition(float missileRadius)
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}

	if (!pWeenie->InValidCell())
	{
		return false;
	}

	_missile_spawn_position = pWeenie->GetPosition();
	_missile_spawn_position = _missile_spawn_position.add_offset(Vector(0, 0, pWeenie->GetHeight() * 0.75)); //(2.0 / 3.0))); // 0.75f));
	
	Vector targetOffset = _missile_spawn_position.get_offset(_missile_target_position);
	Vector targetDir = targetOffset;

	if (targetDir.normalize_check_small())
	{
		targetDir = _missile_spawn_position.frame.get_vector_heading();

		// spawnPosition.frame.m_origin += targetDir * minSpawnDist;
		_missile_spawn_position.frame.set_vector_heading(targetDir);
		_missile_dist_to_target = 0.0;
	}
	else
	{
		float minSpawnDist = (pWeenie->GetRadius() + missileRadius) + 0.1f;

		_missile_spawn_position.frame.m_origin += targetDir * minSpawnDist;
		_missile_spawn_position.frame.set_vector_heading(targetDir);

		_missile_dist_to_target = targetOffset.magnitude();
	}

	return true;
}

bool CMissileAttackEvent::CalculateMissileVelocity(bool track, bool gravity, float speed)
{
	std::shared_ptr<CWeenieObject> target = GetTarget();

	if (!target)
	{
		return false;
	}

	Vector targetOffset = _missile_spawn_position.get_offset(_missile_target_position);
	double targetDist = targetOffset.magnitude();

	if (!track)
	{
		double t = targetDist / speed;
		Vector v = targetOffset / t;

		if (gravity)
		{
			v.z += (9.8*t) / 2.0f;
		}

		//Vector targetDir = v;
		//targetDir.normalize();

		_missile_velocity = v;
		return true;
	}

	Vector P0 = targetOffset;
	Vector P1(0, 0, 0);

	float s0 = target->get_velocity().magnitude();
	Vector V0 = target->get_velocity();
	if (V0.normalize_check_small())
	{
		V0 = Vector(0, 0, 0);
	}

	float s1 = speed;

	double a = (V0.x * V0.x) + (V0.y * V0.y) - (s1 * s1);
	double b = 2 * ((P0.x * V0.x) + (P0.y * V0.y) - (P1.x * V0.x) - (P1.y * V0.y));
	double c = (P0.x * P0.x) + (P0.y * P0.y) + (P1.x * P1.x) + (P1.y * P1.y) - (2 * P1.x * P0.x) - (2 * P1.y * P0.y);

	double t1 = (-b + sqrt((b * b) - (4 * a * c))) / (2 * a);
	double t2 = (-b - sqrt((b * b) - (4 * a * c))) / (2 * a);

	if (t1 < 0)
	{
		t1 = FLT_MAX;
	}

	if (t2 < 0)
	{
		t2 = FLT_MAX;
	}

	double t = min(t1, t2);
	if (t >= 100.0)
	{
		return CalculateMissileVelocity(false, true, speed);
	}

	Vector v;
	v.x = (P0.x + (t * s0 * V0.x)) / (t); // * s1);
	v.y = (P0.y + (t * s0 * V0.y)) / (t); // * s1);
	v.z = (P0.z + (t * s0 * V0.z)) / (t); // * s1);

	if (gravity)
	{
		// add z to velocity for gravity
		v.z += (9.8*t) / 2.0f;
	}

	_missile_velocity = v;

	return true;
}

void CMissileAttackEvent::FireMissile()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}

	std::shared_ptr<CWeenieObject> weapon = pWeenie->GetWieldedCombat(COMBAT_USE::COMBAT_USE_MISSILE);

	if (!weapon)
	{
		pWeenie->DoForcedStopCompletely();
		return;
	}

	std::shared_ptr<CWeenieObject> equippedAmmo;
	bool isThrownWeapon = (weapon->InqIntQuality(DEFAULT_COMBAT_STYLE_INT, 0) == ThrownWeapon_CombatStyle);
	bool isAtlatl = (weapon->InqIntQuality(DEFAULT_COMBAT_STYLE_INT, 0) == Atlatl_CombatStyle);
	if (isThrownWeapon)
		equippedAmmo = weapon;
	else
		equippedAmmo = pWeenie->GetWieldedCombat(COMBAT_USE::COMBAT_USE_AMMO);

	if (!equippedAmmo)
	{
		pWeenie->DoForcedStopCompletely();
		pWeenie->NotifyWeenieError(WERROR_COMBAT_OUT_OF_AMMO);
		return;
	}

	std::shared_ptr<CWeenieObject> missileAsWeenieObject = g_pWeenieFactory->CloneWeenie(equippedAmmo);

	if (!missileAsWeenieObject)
	{
		pWeenie->DoForcedStopCompletely();
		pWeenie->NotifyWeenieError(WERROR_COMBAT_MISFIRE);
		return;
	}

	std::shared_ptr<CAmmunitionWeenie> missile = missileAsWeenieObject->AsAmmunition();

	if (!missile)
	{
		pWeenie->DoForcedStopCompletely();
		pWeenie->NotifyWeenieError(WERROR_COMBAT_MISFIRE);
		return;
	}

	int stackSize = 0;
	if (missile->m_Qualities.InqInt(STACK_SIZE_INT, stackSize))
	{
		missile->SetStackSize(1);
	}

	missile->m_Qualities.SetInstanceID(WIELDER_IID, 0);
	missile->m_Qualities.SetInstanceID(CONTAINER_IID, 0);
	missile->_cachedHasOwner = false;

	missile->m_Qualities.SetInt(CURRENT_WIELDED_LOCATION_INT, 0);
	missile->m_Qualities.SetInt(PARENT_LOCATION_INT, 0);

	missile->SetInitialPhysicsState(INELASTIC_PS | GRAVITY_PS | PATHCLIPPED_PS | ALIGNPATH_PS | MISSILE_PS | REPORT_COLLISIONS_PS);
	missile->SetInitialPosition(pWeenie->GetPosition());
	missile->InitPhysicsObj();

	CalculateTargetPosition();
	CalculateSpawnPosition(missile->GetRadius());

	bool bTrack = true;
	float fSpeed = weapon->InqFloatQuality(MAXIMUM_VELOCITY_FLOAT, 20.0);
	if (std::shared_ptr<CPlayerWeenie> pPlayer = pWeenie->AsPlayer())
	{
		bTrack = pPlayer->GetCharacterOptions2() & LeadMissileTargets_CharacterOptions2;
		fSpeed *= pPlayer->GetCharacterOptions2() & UseFastMissiles_CharacterOptions2 ? MISSILE_FAST_SPEED : MISSILE_SLOW_SPEED;
	}
	else
	{
		fSpeed *= MISSILE_SLOW_SPEED;
	}

	CalculateMissileVelocity(bTrack, true, fSpeed);

	missile->m_Position = _missile_spawn_position;
	missile->set_velocity(_missile_velocity, FALSE);

	std::shared_ptr<CWeenieObject> launcher = pWeenie->GetWieldedCombat(COMBAT_USE_MISSILE);
	std::shared_ptr<CWeenieObject> target = GetTarget();
	missile->_sourceID = pWeenie->GetID();
	missile->_launcherID = launcher ? launcher->GetID() : 0;
	missile->_targetID = target ? target->GetID() : 0;
	missile->_attackPower = _attack_power;
	missile->_timeToRot = Timer::cur_time + 5.0;

	std::shared_ptr<CWeenieObject> shield = pWeenie->GetWieldedCombat(COMBAT_USE::COMBAT_USE_SHIELD); //thrown weapons users can have a shield

	int burden = weapon->InqIntQuality(ENCUMB_VAL_INT, 0);
	if (shield != NULL)
		burden += shield->InqIntQuality(ENCUMB_VAL_INT, 0);

	int necessaryStamina;
	if (_attack_power < 0.33)
		necessaryStamina = max(round(burden / 900.0f), 1);
	else if (_attack_power < 0.66)
		necessaryStamina = max(round(burden / 600.0f), 1);
	else
		necessaryStamina = max(round(burden / 300.0f), 1);

	if (pWeenie->AsPlayer())
	{
		//the higher a player's Endurance, the less stamina one uses while attacking. 
		//This benefit is tied to Endurance only, and it caps out at around 50% less stamina used per attack. 
		//The minimum stamina used per attack remains one. 

		DWORD endurance = 0;
		pWeenie->m_Qualities.InqAttribute(ENDURANCE_ATTRIBUTE, endurance, true);
		float necessaryStaminaMod = 1.0 - ((float)endurance - 100.0) / 600.0; //made up formula: 50% reduction at 400 endurance.
		necessaryStaminaMod = min(max(necessaryStaminaMod, 0.5), 1.0);
		necessaryStamina = round((float)necessaryStamina * necessaryStaminaMod);
	}
	necessaryStamina = max(necessaryStamina, 1);

	bool hadEnoughStamina = true;
	if (pWeenie->GetStamina() < necessaryStamina)
		hadEnoughStamina = false;

	pWeenie->AdjustStamina(-necessaryStamina);

	missile->_weaponSkill = SkillTable::OldToNewSkill((STypeSkill)weapon->InqIntQuality(WEAPON_SKILL_INT, UNDEF_SKILL, false));
	if (pWeenie->InqSkill(missile->_weaponSkill, missile->_weaponSkillLevel, false))
	{
		double offenseMod = weapon->GetOffenseMod();
		missile->_weaponSkillLevel = (DWORD)(missile->_weaponSkillLevel * offenseMod);

		if (!hadEnoughStamina)
		{
			pWeenie->NotifyWeenieError(WERROR_STAMINA_TOO_LOW);
			missile->_weaponSkillLevel *= 0.5; //50% penalty to our attack skill when we don't have enough to perform it.
		}
	}

	if (g_pWorld->CreateEntity(missile))
		missile->EmitEffect(PS_Launch, 0.0f);

	if (!isAtlatl)
	{
		int ammoType = weapon->InqIntQuality(AMMO_TYPE_INT, 0);

		switch (ammoType)
		{
		case AMMO_TYPE::AMMO_ARROW:
		case AMMO_TYPE::AMMO_ARROW_CHORIZITE:
		case AMMO_TYPE::AMMO_ARROW_CRYSTAL:
			pWeenie->EmitSound(Sound_BowRelease, 1.0f);
			break;

		case AMMO_TYPE::AMMO_BOLT:
		case AMMO_TYPE::AMMO_BOLT_CHORIZITE:
		case AMMO_TYPE::AMMO_BOLT_CRYSTAL:
			pWeenie->EmitSound(Sound_CrossbowRelease, 1.0f);
			break;
		}
	}

	if (equippedAmmo->IsWorldAware())
	{
		equippedAmmo->_position_timestamp++;

		BinaryWriter removeFrom3D;
		removeFrom3D.Write<DWORD>(0xF74A);
		removeFrom3D.Write<DWORD>(equippedAmmo->GetID());
		removeFrom3D.Write<WORD>(equippedAmmo->_instance_timestamp);
		removeFrom3D.Write<WORD>(equippedAmmo->_position_timestamp);
		g_pWorld->BroadcastPVS(equippedAmmo->GetWorldTopLevelOwner()->GetLandcell(), removeFrom3D.GetData(), removeFrom3D.GetSize());

		equippedAmmo->m_Qualities.SetInt(PARENT_LOCATION_INT, 0);
		equippedAmmo->unset_parent();
	}

	if (pWeenie->AsPlayer()) //only players use ammo.
	{
		if (equippedAmmo->InqIntQuality(STACK_SIZE_INT, 1) <= 1)
		{
			//we're about to run out of ammo, exit combat mode instead of trying to reload.
			pWeenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
			pWeenie->NotifyWeenieError(WERROR_COMBAT_OUT_OF_AMMO);
		}

		//and consume the ammo.
		equippedAmmo->DecrementStackNum(true);
	}
}

void CMissileAttackEvent::OnAttackAnimSuccess(DWORD motion)
{
	FireMissile();
	Finish();
}

void CMissileAttackEvent::Finish()
{
	std::shared_ptr<CWeenieObject> target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	Done();
}

void CMissileAttackEvent::HandleAttackHook(const AttackCone &cone)
{
}

AttackManager::AttackManager(std::shared_ptr<CWeenieObject> weenie)
{
	_weenie = weenie;
}

AttackManager::~AttackManager()
{
	SafeDelete(_attackData);
	SafeDelete(_cleanupData);
}

void AttackManager::MarkForCleanup(CAttackEventData *data)
{
	if (_cleanupData && _cleanupData != data)
	{
		delete _cleanupData;
	}

	_cleanupData = data;
}

void AttackManager::Cancel()
{
	if (_attackData)
		_attackData->Cancel();
}

void AttackManager::OnAttackCancelled(DWORD error)
{
	if (_attackData)
	{
		std::shared_ptr<CWeenieObject> pWeenie = pWeenie;

		if (pWeenie)
		{
			pWeenie->NotifyAttackDone();
			//pWeenie->DoForcedStopCompletely();
			pWeenie->unstick_from_object();
		}

		MarkForCleanup(_attackData);
		_attackData = NULL;
	}
}

bool AttackManager::RepeatAttacks()
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}

	if (std::shared_ptr<CPlayerWeenie> player = pWeenie->AsPlayer())
	{
		return player->ShouldRepeatAttacks();
	}

	return false;
}

void AttackManager::OnAttackDone(DWORD error)
{
	//if(pWeenie->_blockNewAttacksUntil < Timer::cur_time) //fix for cancelling reload animation making attacking faster 
		//pWeenie->_blockNewAttacksUntil = Timer::cur_time + 1.0;
	if (_attackData)
	{
		std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
		if (!pWeenie)
		{
			return;
		}

		if (RepeatAttacks() && _attackData->IsValidTarget())
		{
			pWeenie->NotifyAttackDone();
			
			if (_queuedAttackData != NULL)
			{
				//we have a queued attack, change to that.
				SafeDelete(_attackData);
				_attackData = _queuedAttackData;
				_queuedAttackData = NULL;
			}
			
			if (_attackData->AsMissileAttackEvent())
			{
				//This is needed for missile attacks otherwise the client won't start the next shot.
				//Although this makes it so the client's power bar doesn't animate(it starts filled up to the attack power chosen).
				//So we don't want it for melee weapons as those work correctly without this. Did missile attacks animate the power bar on retail?
				pWeenie->NotifyCommenceAttack();
			}

			_attackData->_attack_charge_time = Timer::cur_time + (_attackData->_attack_power);
			_attackData->Begin();
		}
		else
		{
			pWeenie->NotifyAttackDone();

			MarkForCleanup(_attackData);
			_attackData = NULL;
		}
	}
}

void AttackManager::Update()
{
	if (_attackData)
	{
		_attackData->Update();
	}

	SafeDelete(_cleanupData);
}

void AttackManager::OnDeath(DWORD killer_id)
{
	Cancel();
}

void AttackManager::HandleMoveToDone(DWORD error)
{
	if (_attackData)
	{
		_attackData->HandleMoveToDone(error);
	}
}

void AttackManager::HandleAttackHook(const AttackCone &cone)
{
	if (_attackData)
	{
		_attackData->HandleAttackHook(cone);
	}
}

void AttackManager::OnMotionDone(DWORD motion, BOOL success)
{
	if (_attackData)
	{
		_attackData->OnMotionDone(motion, success);
	}
}

bool AttackManager::IsAttacking()
{
	return _attackData != NULL ? true : false;
}

void AttackManager::BeginAttack(CAttackEventData *data)
{
	if (_attackData != NULL)
	{
		//we're already attacking, queue this, or change current queued attack to this.
		SafeDelete(_queuedAttackData);
		_queuedAttackData = data;
	}
	else
	{
		_attackData = data;
		_attackData->Begin();
	}
}

void AttackManager::BeginMeleeAttack(DWORD target_id, ATTACK_HEIGHT height, float power, float chase_distance, DWORD motion)
{
	CMeleeAttackEvent *attackEvent = new CMeleeAttackEvent();

	attackEvent->_weenie = _weenie.lock();
	attackEvent->_manager = this;
	attackEvent->_target_id = target_id;
	attackEvent->_attack_height = height;
	attackEvent->_attack_power = power;
	attackEvent->_do_attack_animation = motion;
	attackEvent->_fail_distance = chase_distance;

	BeginAttack(attackEvent);
}

void AttackManager::BeginMissileAttack(DWORD target_id, ATTACK_HEIGHT height, float power, DWORD motion)
{
	CMissileAttackEvent *attackEvent = new CMissileAttackEvent();

	attackEvent->_weenie = _weenie.lock();
	attackEvent->_manager = this;
	attackEvent->_target_id = target_id;
	attackEvent->_attack_height = height;
	attackEvent->_attack_power = power;
	attackEvent->_do_attack_animation = motion;

	BeginAttack(attackEvent);
}





