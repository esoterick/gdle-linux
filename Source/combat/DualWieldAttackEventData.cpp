#include "StdAfx.h"
#include "AttackManager.h"
#include "WeenieObject.h"
#include "World.h"
#include "Player.h"
#include "WeenieFactory.h"
#include "Ammunition.h"
#include "CombatFormulas.h"
#include "combat/DualWieldAttackEventData.h"

void CDualWieldAttackEvent::CalculateAtt(CWeenieObject *weapon, STypeSkill& weaponSkill, DWORD& weaponSkillLevel)
{
	CWeenieObject *main = _weenie->GetWieldedCombat(COMBAT_USE_MELEE);
	CWeenieObject *left = _weenie->GetWieldedCombat(COMBAT_USE_OFFHAND);

	float mainMod = main->GetOffenseMod();
	float leftMod = left->GetOffenseMod();
	
	float offenseMod = max(mainMod, leftMod);

	weaponSkill = SkillTable::OldToNewSkill((STypeSkill)weapon->InqIntQuality(WEAPON_SKILL_INT, LIGHT_WEAPONS_SKILL, TRUE));
	weaponSkillLevel = 0;

	_weenie->InqSkill(weaponSkill, weaponSkillLevel, FALSE);

	if (_hand)
	{
		DWORD dualSkillLevel = 0;
		_weenie->InqSkill(STypeSkill::DUAL_WIELD_SKILL, dualSkillLevel, FALSE);
		weaponSkillLevel = min(weaponSkillLevel, dualSkillLevel);
	}

	weaponSkillLevel = (DWORD)(weaponSkillLevel * offenseMod);
}

int CDualWieldAttackEvent::CalculateDef(CWeenieObject *weapon)
{
	return 0;
}

void CDualWieldAttackEvent::Setup()
{
	CMeleeAttackEvent::Setup();

	if (!_main_attack_motion)
		_main_attack_motion = _do_attack_animation;

	// DW is 20% faster
	_attack_speed *= 0.8f;
	_attack_charge_time *= 0.8f;

	if (!_offhand_attack_motion)
	{

		DWORD attack_motion = 0;
		DWORD weapon_id = 0;

		if (_weenie->_combatTable)
		{
			CWeenieObject *weapon = _weenie->GetWieldedCombat(COMBAT_USE_OFFHAND);

			if (weapon)
			{
				weapon_id = weapon->GetID();

				AttackType attack_type = (AttackType)weapon->InqIntQuality(ATTACK_TYPE_INT, 0);

				if (attack_type == (Thrust_AttackType | Slash_AttackType))
				{
					if (_attack_power >= 0.25f)
						attack_type = Slash_AttackType;
					else
						attack_type = Thrust_AttackType;
				}

				attack_type = (AttackType)(attack_type * 0x100);

				if (CombatManeuver *combat_maneuver = _weenie->_combatTable->TryGetCombatManuever(_weenie->get_minterp()->InqStyle(), attack_type, _attack_height))
				{
					//don't load attack_motion for UA full speed attacks (Low and Med only) so that attack power is used to calculate motion instead.
					if ((weapon->m_Qualities.GetInt(DEFAULT_COMBAT_STYLE_INT, 0) != 1) || _attack_power >= 0.25f || _attack_height == 1)
						attack_motion = combat_maneuver->motion;
				}
			}
		}

		if (!attack_motion)
		{
			switch (_attack_height)
			{
			case LOW_ATTACK_HEIGHT: attack_motion = Motion_AttackLow4; break;
			case MEDIUM_ATTACK_HEIGHT: attack_motion = Motion_AttackMed4; break;
			case HIGH_ATTACK_HEIGHT: attack_motion = Motion_AttackHigh4; break;
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

		_offhand_attack_motion = attack_motion;
	}
}

void CDualWieldAttackEvent::HandleAttackHook(const AttackCone &cone)
{
	CMeleeAttackEvent::HandleAttackHook(cone);

	_hand ^= -1;

	DWORD attack_motion = 0;

	if (_hand)
	{
		_do_attack_animation = _offhand_attack_motion;
	}
	else
	{
		_do_attack_animation = _main_attack_motion;
	}

}