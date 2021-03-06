#include "StdAfx.h"
#include "AttackManager.h"
#include "WeenieObject.h"
#include "World.h"
#include "Player.h"
#include "WeenieFactory.h"
#include "Ammunition.h"
#include "CombatFormulas.h"
#include "combat/MeleeAttackEventData.h"

// TODO: Move these up to AttackEventData?
void CMeleeAttackEvent::CalculateAtt(CWeenieObject *weapon, STypeSkill& weaponSkill, DWORD& weaponSkillLevel)
{
	float offenseMod = weapon->GetOffenseMod();
	weaponSkill = SkillTable::OldToNewSkill((STypeSkill)weapon->InqIntQuality(WEAPON_SKILL_INT, LIGHT_WEAPONS_SKILL, TRUE));
	weaponSkillLevel = 0;

	if (_weenie->InqSkill(weaponSkill, weaponSkillLevel, FALSE))
	{
		weaponSkillLevel = (DWORD)(weaponSkillLevel * offenseMod);
	}
}

//float CMeleeAttackEvent::CalculateDef()
//{
//	CWeenieObject *weapon = _weenie->GetWieldedCombat(_combat_style);
//	if (weapon)
//	{
//		float defenseMod = weapon->GetMeleeDefenseMod();
//		return defenseMod;
//	}
//
//	return CAttackEventData::CalculateDef();
//}

void CMeleeAttackEvent::Setup()
{
	DWORD attack_motion = 0;
	DWORD weapon_id = 0;
	DWORD style = _weenie->get_minterp()->InqStyle();

	if (!_do_attack_animation)
	{
		if (_weenie->_combatTable)
		{
			CWeenieObject *weapon = _weenie->GetWieldedCombat(_combat_style);

			if (weapon)
			{
				//_max_attack_distance = weapon->InqFloatQuality(WEAPON_LENGTH_FLOAT, 0.5); //todo: this would be interesting but something makes the character still move next to the target anyway. Is it the client?

				weapon_id = weapon->GetID();

				AttackType attack_type = (AttackType)weapon->InqIntQuality(ATTACK_TYPE_INT, 0);

			if (attack_type == (Thrust_AttackType | Slash_AttackType))
			{
				if (_attack_power >= 0.25f)
					attack_type = Slash_AttackType;
				else
					attack_type = Thrust_AttackType;
			}

			// Different rules for Dual Wield vs Onehand and shield.
			if (style == Motion_DualWieldCombat)
			{
				// Dual Wield can use both slash and thrust animations based on power.
				switch (attack_type)
				{
				case DoubleThrust_AttackType | DoubleSlash_AttackType:
					if (_attack_power >= 0.25f)
						attack_type = DoubleSlash_AttackType;
					else
						attack_type = DoubleThrust_AttackType;
					break;

				case TripleThrust_AttackType | TripleSlash_AttackType:
					if (_attack_power >= 0.25f)
						attack_type = TripleSlash_AttackType;
					else
						attack_type = TripleThrust_AttackType;
					break;
				}
			}
			else if (style == Motion_SwordShieldCombat)
			{
				// Force Thrust animation when use a shield with a multi-strike weapon.
				switch (attack_type)
				{
				case DoubleThrust_AttackType | DoubleSlash_AttackType:
					attack_type = DoubleThrust_AttackType;
					break;

				case TripleThrust_AttackType | TripleSlash_AttackType:
					attack_type = TripleThrust_AttackType;
					break;
				}
			}



			if (CombatManeuver *combat_maneuver = _weenie->_combatTable->TryGetCombatManuever(style, attack_type, _attack_height))
			{
				attack_motion = combat_maneuver->motion;

				// UA full speed attacks (Low only) need 3 removed to get the correct animation.
				if (weapon && weapon->m_Qualities.GetInt(DEFAULT_COMBAT_STYLE_INT,0) == Unarmed_CombatStyle && _attack_power <= 0.25f && _attack_height == 3)
					attack_motion -= 3;
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
	_weenie->m_Qualities.InqAttribute(QUICKNESS_ATTRIBUTE, quickness, FALSE);

	int weaponAttackTime = _weenie->GetAttackTimeUsingWielded();
	int creatureAttackTime = max(0, 120 - (((int)quickness - 60) / 2)); //we reach 0 attack speed at 300 quickness

	int attackTime = (creatureAttackTime + weaponAttackTime) / 2; //our attack time is the average between our speed and the speed of our weapon.
	attackTime = max(0, min(120, attackTime));

	_attack_speed = 2.25f - (attackTime * (1.0 / 70.0));
	_attack_speed = max(min(_attack_speed, 2.25f), 0.8f);

	//old formula:
	//int attackTime = max(0, min(120, _weenie->GetAttackTimeUsingWielded()));
	//_attack_speed = 1.0 / (1.0 / (1.0 + ((120 - attackTime) * (0.005))));

	CAttackEventData::Setup();
}

void CMeleeAttackEvent::OnReadyToAttack()
{
	if (_do_attack_animation)
	{
		MovementParameters params;
		params.sticky = 1;
		params.can_charge = 1;
		params.modify_interpreted_state = 1;
		params.speed = _attack_speed;
		params.action_stamp = ++_weenie->m_wAnimSequence;
		params.autonomous = 0;
		_weenie->stick_to_object(_target_id);

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
	CWeenieObject *target = GetTarget();
	if (!target && _target_id)
	{
		Cancel(WERROR_OBJECT_GONE);
		return;
	}

	Done();
}

void CMeleeAttackEvent::HandleAttackHook(const AttackCone &cone)
{
	CWeenieObject *target = GetTarget();

	if (!target || !IsValidTarget())
	{
		return;
	}

	int preVarianceDamage;
	float variance;

	DAMAGE_TYPE damageType = DAMAGE_TYPE::UNDEF_DAMAGE_TYPE;

	bool isBodyPart = false;
	CWeenieObject *weapon = _weenie->GetWieldedCombat(_combat_style);

	if (!weapon) //if we still don't have a weapon use our body parts
	{
		weapon = _weenie;
		if (_weenie->m_Qualities._body)
		{
			BodyPart *part = _weenie->m_Qualities._body->_body_part_table.lookup(cone.part_index);
			if (part)
			{
				isBodyPart = true;
				damageType = part->_dtype;
				preVarianceDamage = part->_dval;
				variance = part->_dvar;
			}
		}

		CWeenieObject *gloverOrBoots;
		if (_attack_power >= 0.75f) //this is a kick
			gloverOrBoots = _weenie->GetWielded(FOOT_WEAR_LOC);
		else //this is a punch
			gloverOrBoots = _weenie->GetWielded(HAND_WEAR_LOC);

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

	//todo: maybe handle this differently as to integrate all possible damage type combos
	if (damageType == (DAMAGE_TYPE::SLASH_DAMAGE_TYPE | DAMAGE_TYPE::PIERCE_DAMAGE_TYPE))
	{
		// Damage type should always be Pierce for multi-strike Thrust animations, not slashing.
		if (_attack_power >= 0.25f && !(_do_attack_animation >= Motion_DoubleThrustLow && _do_attack_animation <= Motion_TripleThrustHigh))
			damageType = DAMAGE_TYPE::SLASH_DAMAGE_TYPE;
		else
			damageType = DAMAGE_TYPE::PIERCE_DAMAGE_TYPE;
	}
	else if (damageType == (DAMAGE_TYPE::SLASH_DAMAGE_TYPE | DAMAGE_TYPE::FIRE_DAMAGE_TYPE))
	{
		//todo: as far as I know only the Mattekar Claw had this, figure out what it did exactly, was it like this? or was it a bit of both damages?
		//or even a chance for fire damage?
		if (_attack_power >= 0.25f)
			damageType = DAMAGE_TYPE::SLASH_DAMAGE_TYPE;
		else
			damageType = DAMAGE_TYPE::FIRE_DAMAGE_TYPE;
	}

	CWeenieObject *shield = _weenie->GetWieldedCombat(COMBAT_USE::COMBAT_USE_SHIELD);

	constexpr float weaponBurdenFactor = 1.0f / 450.0f;
	constexpr float shieldBurdenFactor = 1.0f / 680.0f;

	float weaponBurden = 0.0f;
	float shieldBurden = 0.0f;
	if (weapon != NULL && weapon != _weenie)
		weaponBurden = (float)weapon->InqIntQuality(ENCUMB_VAL_INT, 0) * weaponBurdenFactor;

	if (shield != NULL) // TODO: Consider adding burden calc to AttackEventData
	{
		shieldBurden = (float)shield->InqIntQuality(ENCUMB_VAL_INT, 0);
		shieldBurden *= (shield->GetPlacementFrameID() == Shield) ? shieldBurdenFactor : weaponBurdenFactor;
	}

	float powerStamMod = 0.25 + _attack_power * 0.75;
	powerStamMod = powerStamMod > 1.0 ? 1.0 : powerStamMod;

	float equipStamCost = 2.0 + (weaponBurden + shieldBurden);
	int necessaryStam = min(1, (int)(equipStamCost * powerStamMod + 0.5));

	if (_weenie->AsPlayer())
	{
		// this lowers the max endurance needed for getting top bonus to 300 vs 400
		DWORD endurance = 0;
		float necStamMod = 1.0;
		_weenie->m_Qualities.InqAttribute(ENDURANCE_ATTRIBUTE, endurance, true);

		if (endurance >= 50)
		{
			necStamMod = ((float)(endurance * endurance) * -0.000003175) - ((float)endurance * 0.0008889) + 1.052;
			necStamMod = min(max(necStamMod, 0.5f), 1.0f);
			necessaryStam = (int)(necessaryStam * necStamMod + Random::RollDice(0.0, 1.0)); // little sprinkle of luck 
		}
	}
	necessaryStam = max(necessaryStam, 1);

	bool hadEnoughStamina = true;
	if (_weenie->GetStamina() < necessaryStam)
	{
		hadEnoughStamina = false;
		_attack_power = 0.00f;
		_weenie->SetStamina(0, true); // you lose all current stam
	}
	else
		_weenie->AdjustStamina(-necessaryStam);

	STypeSkill weaponSkill = STypeSkill::UNDEF_SKILL;
	DWORD weaponSkillLevel = 0;
	CalculateAtt(weapon, weaponSkill, weaponSkillLevel);

	if (!hadEnoughStamina)
	{
		if (CPlayerWeenie *pPlayer = _weenie->AsPlayer())
		{
			pPlayer->SendText("You're exhausted!", LTT_ERROR);
		}
	}

	DamageEventData dmgEvent;
	dmgEvent.source = _weenie;
	dmgEvent.weapon = weapon;
	dmgEvent.damage_form = DF_MELEE;
	dmgEvent.damage_type = damageType;
	dmgEvent.attackSkill = weaponSkill;
	dmgEvent.attackSkillLevel = weaponSkillLevel;
	dmgEvent.preVarianceDamage = preVarianceDamage;
	dmgEvent.variance = variance;

	HandlePerformAttack(target, dmgEvent);

	int cleaveTargets = weapon->InqIntQuality(CLEAVING_INT, 1) - 1;
	if (cleaveTargets)
	{
		std::list<CWeenieObject *> lpNearby;
		g_pWorld->EnumNearby(dmgEvent.source, _max_attack_distance, &lpNearby);

		int numTargets = cleaveTargets;
		for (auto tg : lpNearby)
		{
			if (!numTargets)
				break;

			if (tg == target)
				continue;

			if (_weenie->m_Qualities.id != 1 && tg->m_Qualities.id != 1) // Don't cleave mobs if we are a mob. Where 1 is the WCID for a player (always 1).
				continue;

			if (!tg->IsAttackable() || (tg->_IsPlayer() && _weenie->_IsPlayer() && ((!_weenie->IsPK() || !tg->IsPK()) && (!_weenie->IsPKLite() || !tg->IsPKLite()))))
				continue;

			if (tg->HeadingFrom(_weenie, true) < CLEAVING_ATTACK_ANGLE / 2)
			{
				HandlePerformAttack(tg, dmgEvent);
				dmgEvent.killingBlow = false;
				numTargets--;
			}
		}
	}
}

void CMeleeAttackEvent::HandlePerformAttack(CWeenieObject *target, DamageEventData dmgEvent)
{

	// okay, we're attacking. check for pvp interactions
	if (target->AsPlayer() && _weenie->AsPlayer())
	{
		target->AsPlayer()->UpdatePKActivity();
		_weenie->AsPlayer()->UpdatePKActivity();
	}

	if (_weenie->AsPlayer())
		_weenie->AsPlayer()->CancelLifestoneProtection();

	DWORD meleeDefense = 0;
	if (target->InqSkill(MELEE_DEFENSE_SKILL, meleeDefense, FALSE) && meleeDefense > 0)
	{
		if (target->TryMeleeEvade(dmgEvent.attackSkillLevel))
		{
			target->OnEvadeAttack(_weenie);

			// send evasion message
			BinaryWriter attackerEvadeEvent;
			attackerEvadeEvent.Write<DWORD>(0x01B3);
			attackerEvadeEvent.WriteString(target->GetName());
			_weenie->SendNetMessage(&attackerEvadeEvent, PRIVATE_MSG, TRUE, FALSE);

			BinaryWriter attackedEvadeEvent;
			attackedEvadeEvent.Write<DWORD>(0x01B4);
			attackedEvadeEvent.WriteString(_weenie->GetName());
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

	double angle = _weenie->HeadingFrom(_target_id, false);
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

	dmgEvent.hit_quadrant = hitQuadrant;

	dmgEvent.target = target;

	CalculateCriticalHitData(&dmgEvent, NULL);
	dmgEvent.wasCrit = (Random::GenFloat(0.0, 1.0) < dmgEvent.critChance) ? true : false;
	
	if (dmgEvent.wasCrit)
		dmgEvent.baseDamage = dmgEvent.preVarianceDamage * (0.5 + _attack_power);//Calculate baseDamage with no variance (uses max dmg on weapon)

	else 
		dmgEvent.baseDamage = dmgEvent.preVarianceDamage * (1.0f - Random::GenFloat(0.0f, dmgEvent.variance)) * (0.5 + _attack_power); // not a crit so include variance in base damage

	//cast on strike
	if (dmgEvent.weapon->InqDIDQuality(PROC_SPELL_DID, 0))
	{
		double procChance = dmgEvent.weapon->InqFloatQuality(PROC_SPELL_RATE_FLOAT, 0.0f);

		bool proc = (Random::GenFloat(0.0, 1.0) < procChance) ? true : false;

		if (proc && target)
		{
			DWORD targetid = target->GetID();
			DWORD procspell = dmgEvent.weapon->InqDIDQuality(PROC_SPELL_DID, 0);

			dmgEvent.weapon->TryCastSpell(targetid, procspell);
		}
	}
	CalculateDamage(&dmgEvent);

	_weenie->TryToDealDamage(dmgEvent);
}