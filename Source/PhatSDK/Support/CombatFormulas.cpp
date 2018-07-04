
#include "StdAfx.h"
#include "PhatSDK.h"
#include "CombatFormulas.h"

double GetImbueMultiplier(double currentSkill, double minEffectivenessSkill, double maxEffectivenessSkill, double maxMultiplier, bool allowNegative)
{
	double multiplier = (currentSkill - minEffectivenessSkill) / (maxEffectivenessSkill - minEffectivenessSkill);
	double value = multiplier * maxMultiplier;
	if (!allowNegative)
	{
		value = max(value, 0.0);
	}
	value = min(value, maxMultiplier);
	return value;
}

void CalculateDamage(DamageEventData *dmgEvent, SpellCastData *spellData)
{
	if (!dmgEvent)
	{
		return;
	}
	std::shared_ptr<CWeenieObject> pSource = dmgEvent->source.lock();
	if (!pSource)
	{
		return;
	}

	dmgEvent->damageBeforeMitigation = dmgEvent->damageAfterMitigation = dmgEvent->baseDamage;

	CalculateRendingAndMiscData(dmgEvent);
	CalculateAttributeDamageBonus(dmgEvent);
	CalculateSkillDamageBonus(dmgEvent, spellData);
	CalculateSlayerData(dmgEvent);


	double damageCalc = dmgEvent->baseDamage;
	damageCalc += dmgEvent->attributeDamageBonus;
	damageCalc += dmgEvent->skillDamageBonus;
	damageCalc += dmgEvent->slayerDamageBonus;

	dmgEvent->wasCrit = (Random::GenFloat(0.0, 1.0) < dmgEvent->critChance) ? true : false;
	if (dmgEvent->wasCrit)
	{
		damageCalc += damageCalc * dmgEvent->critMultiplier; //Leave the old formula for Melee/Missile crits.

		if (dmgEvent->damage_form == DF_MAGIC) //Multiply base spell damage by the critMultiplier before adding skill and slayer damage bonuses for Magic.
		{
			damageCalc = dmgEvent->baseDamage;
			damageCalc += damageCalc * dmgEvent->critMultiplier;
			damageCalc += dmgEvent->skillDamageBonus;
			damageCalc += dmgEvent->slayerDamageBonus;
		}
	}

	if (dmgEvent->damage_form == DF_MAGIC && !pSource->AsPlayer())
		damageCalc /= 2; //creatures do half magic damage. Unconfirmed but feels right. Should this be projectile spells only?


	dmgEvent->damageBeforeMitigation = dmgEvent->damageAfterMitigation = damageCalc;
}

void CalculateAttributeDamageBonus(DamageEventData *dmgEvent)
{
	if (!dmgEvent)
	{
		return;
	}
	std::shared_ptr<CWeenieObject> pSource = dmgEvent->source.lock();
	if (!pSource)
	{
		return;
	}

	switch (dmgEvent->damage_form)
	{
	case DF_MELEE:
	case DF_MISSILE:
	{
		DWORD attrib = 0;
		if (dmgEvent->attackSkill == FINESSE_WEAPONS_SKILL || dmgEvent->attackSkill == MISSILE_WEAPONS_SKILL)
		{
			pSource->m_Qualities.InqAttribute(COORDINATION_ATTRIBUTE, attrib, FALSE);
		}
		else
		{
			pSource->m_Qualities.InqAttribute(STRENGTH_ATTRIBUTE, attrib, FALSE);
		}

		double attribDamageMod;
		if (attrib >= 1000000) //this makes /godly characters use the old formula(huge damage!)
			attribDamageMod = ((int)attrib - 55.0) / 33.0;
		else
			attribDamageMod = 6.75*(1.0 - exp(-0.005*((int)attrib - 55)));
		if (attribDamageMod < 0 || dmgEvent->ignoreMagicArmor || dmgEvent->ignoreMagicResist) //half attribute bonus for hollow weapons.
			dmgEvent->attributeDamageBonus = dmgEvent->baseDamage * (attribDamageMod / 2.0);
		else
			dmgEvent->attributeDamageBonus = dmgEvent->baseDamage * (attribDamageMod - 1.0);
		break;
	}
	case DF_MAGIC:
		break;
	}
}

void CalculateSkillDamageBonus(DamageEventData *dmgEvent, SpellCastData *spellData)
{
	if (!dmgEvent)
	{
		return;
	}
	std::shared_ptr<CWeenieObject> pSource = dmgEvent->source.lock();
	if (!pSource)
	{
		return;
	}

	switch (dmgEvent->damage_form)
	{
	case DF_MELEE:
	case DF_MISSILE:
		return;
	case DF_MAGIC:
		if (!spellData)
			return;

		if (dmgEvent->attackSkill == WAR_MAGIC_SKILL)
		{
			ProjectileSpellEx *meta = (ProjectileSpellEx *)spellData->spellEx->_meta_spell._spell;
			//Skill based damage bonus: This additional damage will be a constant percentage of the minimum damage value.
			//The percentage is determined by comparing the level of the spell against the buffed war magic skill of the character.
			//Note that creatures do not receive this bonus.
			if (pSource->AsPlayer())
			{
				float minDamage = (float)meta->_baseIntensity;

				float difficulty = spellData->spell->_power - 100; // add fudge factor
				if (spellData->spell->_power == 400)
				{
					difficulty -= 75; // Adjust for level 8s
				}

				float skillDamageMod = ((int)spellData->current_skill - difficulty) / 1000.0; //better made up formula.
				if (skillDamageMod > 0)
					dmgEvent->skillDamageBonus = minDamage * skillDamageMod;
			}
		}
		return;
	}
}

void CalculateCriticalHitData(DamageEventData *dmgEvent, SpellCastData *spellData)
{
	if (!dmgEvent)
	{
		return;
	}
	std::shared_ptr<CWeenieObject> pSource = dmgEvent->source.lock();
	if (!pSource)
	{
		return;
	}
	std::shared_ptr<CWeenieObject> pTarget = dmgEvent->target.lock();
	if (!pTarget)
	{
		return;
	}
	std::shared_ptr<CWeenieObject> pWeapon = dmgEvent->weapon.lock();
	if (!pWeapon)
	{
		return;
	}

	DWORD imbueEffects;

	switch (dmgEvent->damage_form)
	{
	case DF_MELEE:
		dmgEvent->critChance = 0.1;
		dmgEvent->critMultiplier = 1.0;

		if (!pWeapon)
			return;

		imbueEffects = pWeapon->GetImbueEffects();

		if (pWeapon->GetBitingStrikeFrequency())
			dmgEvent->critChance = pWeapon->GetBitingStrikeFrequency();

		if (pWeapon->GetCrushingBlowMultiplier())
			dmgEvent->critMultiplier += pWeapon->GetCrushingBlowMultiplier();

		if (imbueEffects & CriticalStrike_ImbuedEffectType)
			dmgEvent->critChance += GetImbueMultiplier(dmgEvent->attackSkillLevel, 150, 400, 0.5);

		if (imbueEffects & CripplingBlow_ImbuedEffectType)
			dmgEvent->critMultiplier += GetImbueMultiplier(dmgEvent->attackSkillLevel, 150, 400, 6);

		dmgEvent->critMultiplier = min(max(dmgEvent->critMultiplier, 0.0), 7.0);
		dmgEvent->critChance = min(max(dmgEvent->critChance, 0.0), 1.0);
		return;
	case DF_MISSILE:
		dmgEvent->critChance = 0.1;
		dmgEvent->critMultiplier = 1.0;

		if (!pWeapon)
			return;

		imbueEffects = pWeapon->GetImbueEffects();

		if (pWeapon->GetBitingStrikeFrequency())
			dmgEvent->critChance = pWeapon->GetBitingStrikeFrequency();

		if (pWeapon->GetCrushingBlowMultiplier())
			dmgEvent->critMultiplier += pWeapon->GetCrushingBlowMultiplier();

		if (imbueEffects & CriticalStrike_ImbuedEffectType)
			dmgEvent->critChance += GetImbueMultiplier(dmgEvent->attackSkillLevel, 125, 360, 0.5);

		if (imbueEffects & CripplingBlow_ImbuedEffectType)
			dmgEvent->critMultiplier += GetImbueMultiplier(dmgEvent->attackSkillLevel, 125, 360, 6);

		dmgEvent->critMultiplier = min(max(dmgEvent->critMultiplier, 0.0), 7.0);
		dmgEvent->critChance = min(max(dmgEvent->critChance, 0.0), 1.0);
		return;
	case DF_MAGIC:
		dmgEvent->critChance = 0.05;
		dmgEvent->critMultiplier = 0.5;

		if (!pWeapon)
		{
			return;
		}
		if (!spellData)
		{
			return;
		}

		imbueEffects = pWeapon->GetImbueEffects();

		if (pWeapon->GetBitingStrikeFrequency())
			dmgEvent->critChance = pWeapon->GetBitingStrikeFrequency();

		if (pWeapon->GetCrushingBlowMultiplier())
			dmgEvent->critMultiplier += pWeapon->GetCrushingBlowMultiplier();

		if (dmgEvent->attackSkill == WAR_MAGIC_SKILL)
		{
			ProjectileSpellEx *meta = (ProjectileSpellEx *)spellData->spellEx->_meta_spell._spell;
			//Imbue and slayer effects for War Magic now scale from minimum effectiveness at 125 to 
			//maximum effectiveness at 360 skill instead of from 150 to 400 skill(PvM only).

			bool isPvP = pSource->AsPlayer() && pTarget->AsPlayer();

			if (imbueEffects & CriticalStrike_ImbuedEffectType)
			{
				//Critical Strike for War Magic scales from 5% critical hit chance to 50% critical hit chance at maximum effectiveness.
				//PvP: Critical Strike for War Magic scales from 5% critical hit chance to 25% critical hit chance at maximum effectiveness.
				if (isPvP)
					dmgEvent->critChance += GetImbueMultiplier(dmgEvent->attackSkillLevel, 150, 400, 0.25);
				else
					dmgEvent->critChance += GetImbueMultiplier(dmgEvent->attackSkillLevel, 125, 360, 0.5);
			}

			if (imbueEffects & CripplingBlow_ImbuedEffectType)
			{
				//Crippling Blow for War Magic currently scales from adding 50% of the spells damage
				//on critical hits to adding 500% at maximum effectiveness.
				//PvP: Crippling Blow for War Magic currently scales from adding 50 % of the spells damage on critical hits 
				//to adding 100 % at maximum effectiveness
				if (isPvP)
					dmgEvent->critMultiplier += GetImbueMultiplier(dmgEvent->attackSkillLevel, 150, 400, 0.5);
				else
					dmgEvent->critMultiplier += GetImbueMultiplier(dmgEvent->attackSkillLevel, 125, 360, 5.0);
			}
		}
		dmgEvent->critMultiplier = min(max(dmgEvent->critMultiplier, 0.0), 7.0);
		dmgEvent->critChance = min(max(dmgEvent->critChance, 0.0), 1.0);
		return;
	}
}

void CalculateSlayerData(DamageEventData *dmgEvent)
{
	if (!dmgEvent)
	{
		return;
	}
	std::shared_ptr<CWeenieObject> pSource = dmgEvent->source.lock();
	if (!pSource)
	{
		return;
	}
	std::shared_ptr<CWeenieObject> pTarget = dmgEvent->target.lock();
	if (!pTarget)
	{
		return;
	}
	std::shared_ptr<CWeenieObject> pWeapon = dmgEvent->weapon.lock();
	if (!pWeapon)
	{
		return;
	}

	if (dmgEvent->damage_form == DF_MAGIC && !dmgEvent->isProjectileSpell)
	{
		return; //non projectile spells do not benefit from the slayer property.
	}

	double slayerDamageMod = 0.0;
	int slayerType = pWeapon->InqIntQuality(SLAYER_CREATURE_TYPE_INT, 0, TRUE);
	if (slayerType && slayerType == pTarget->InqIntQuality(CREATURE_TYPE_INT, 0, TRUE))
		slayerDamageMod = pWeapon->InqFloatQuality(SLAYER_DAMAGE_BONUS_FLOAT, 0.0, FALSE);

	if (slayerDamageMod > 0.0)
		dmgEvent->slayerDamageBonus = dmgEvent->baseDamage * (slayerDamageMod - 1.0);
}

void CalculateRendingAndMiscData(DamageEventData *dmgEvent)
{
	if (!dmgEvent)
	{
		return;
	}
	std::shared_ptr<CWeenieObject> pSource = dmgEvent->source.lock();
	if (!pSource)
	{
		return;
	}
	if (!pSource)
		return;
	std::shared_ptr<CWeenieObject> pWeapon = dmgEvent->weapon.lock();
	if (!pWeapon)
	{
		return;
	}


	dmgEvent->ignoreMagicResist = pSource->InqBoolQuality(IGNORE_MAGIC_RESIST_BOOL, FALSE);
	dmgEvent->ignoreMagicArmor = pSource->InqBoolQuality(IGNORE_MAGIC_ARMOR_BOOL, FALSE);

	if (!dmgEvent->ignoreMagicResist)
	{
		dmgEvent->ignoreMagicResist = pWeapon->InqBoolQuality(IGNORE_MAGIC_RESIST_BOOL, FALSE);
	}

	if (!dmgEvent->ignoreMagicArmor)
	{
		dmgEvent->ignoreMagicArmor = pWeapon->InqBoolQuality(IGNORE_MAGIC_ARMOR_BOOL, FALSE);
	}

	DWORD imbueEffects = pWeapon->GetImbueEffects();

	if (imbueEffects & IgnoreAllArmor_ImbuedEffectType)
	{
		dmgEvent->ignoreArmorEntirely = true;
	}

	if (imbueEffects & ArmorRending_ImbuedEffectType)
	{
		dmgEvent->isArmorRending = true;
	}

	switch (dmgEvent->damage_type)
	{
	case SLASH_DAMAGE_TYPE:
		if (imbueEffects & SlashRending_ImbuedEffectType)
		{
			dmgEvent->isElementalRending = true;
		}
		break;
	case PIERCE_DAMAGE_TYPE:
		if (imbueEffects & PierceRending_ImbuedEffectType)
		{
			dmgEvent->isElementalRending = true;
		}
		break;
	case BLUDGEON_DAMAGE_TYPE:
		if (imbueEffects & BludgeonRending_ImbuedEffectType)
		{
			dmgEvent->isElementalRending = true;
		}
		break;
	case COLD_DAMAGE_TYPE:
		if (imbueEffects & ColdRending_ImbuedEffectType)
		{
			dmgEvent->isElementalRending = true;
		}
		break;
	case FIRE_DAMAGE_TYPE:
		if (imbueEffects & FireRending_ImbuedEffectType)
		{
			dmgEvent->isElementalRending = true;
		}
		break;
	case ACID_DAMAGE_TYPE:
		if (imbueEffects & AcidRending_ImbuedEffectType)
		{
			dmgEvent->isElementalRending = true;
		}
		break;
	case ELECTRIC_DAMAGE_TYPE:
		if (imbueEffects & ElectricRending_ImbuedEffectType)
		{
			dmgEvent->isElementalRending = true;
		}
		break;
	}

	if (dmgEvent->isElementalRending)
	{
		switch (dmgEvent->damage_form)
		{
		case DF_MELEE:
			dmgEvent->rendingMultiplier = max(GetImbueMultiplier(dmgEvent->attackSkillLevel, 0, 400, 2.5), 1.0f);
			break;
		case DF_MISSILE:
			dmgEvent->rendingMultiplier = max(0.25 + GetImbueMultiplier(dmgEvent->attackSkillLevel, 0, 360, 2.25), 1.0f);
			break;
		case DF_MAGIC:
			dmgEvent->rendingMultiplier = max(0.25 + GetImbueMultiplier(dmgEvent->attackSkillLevel, 0, 360, 2.25), 1.0f);
			break;
		default:
			return;
		}
	}

	if (pWeapon->InqIntQuality(RESISTANCE_MODIFIER_TYPE_INT, 0, FALSE))
		dmgEvent->isResistanceCleaving = TRUE;

	if (dmgEvent->isResistanceCleaving)
	{
		switch (dmgEvent->damage_form)
		{
		case DF_MELEE:
			dmgEvent->rendingMultiplier = 2.5;
			break;
		case DF_MISSILE:
			dmgEvent->rendingMultiplier = 2.25;
			break;
		case DF_MAGIC:
			dmgEvent->rendingMultiplier = 2.25;
			break;
		default:
			return;
		}
	}

	if (dmgEvent->isArmorRending)
	{
		switch (dmgEvent->damage_form)
		{
		case DF_MELEE:
			dmgEvent->armorRendingMultiplier = 1.0 / max(GetImbueMultiplier(dmgEvent->attackSkillLevel, 0, 400, 2.5), 1.0f);
		case DF_MISSILE:
			dmgEvent->armorRendingMultiplier = 1.0 / max(0.25 + GetImbueMultiplier(dmgEvent->attackSkillLevel, 0, 360, 2.25), 1.0f);
			break;
		case DF_MAGIC:
		default:
			return;
		}
	}

	if (pWeapon->InqFloatQuality(IGNORE_ARMOR_FLOAT, 0, FALSE))
		dmgEvent->isArmorCleaving = TRUE;

	if (dmgEvent->isArmorCleaving)
	{
		switch (dmgEvent->damage_form)
		{
		case DF_MELEE:
			dmgEvent->armorRendingMultiplier = 1.0 /  2.5;
		case DF_MISSILE:
			dmgEvent->armorRendingMultiplier = 1.0 / 2.25;
			break;
		case DF_MAGIC:
		default:
			return;
		}
	}
}