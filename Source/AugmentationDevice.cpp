
#include "StdAfx.h"
#include "AugmentationDevice.h"
#include "UseManager.h"
#include "Player.h"
#include "Qualities.h"
#include "ChatMsgs.h"
#include "World.h"

CAugmentationDeviceWeenie::CAugmentationDeviceWeenie()
{
}

CAugmentationDeviceWeenie::~CAugmentationDeviceWeenie()
{
}

int CAugmentationDeviceWeenie::Use(CPlayerWeenie *player)
{
	if (!player->FindContainedItem(GetID()))
	{
		player->NotifyUseDone(WERROR_NONE);
		return WERROR_NONE;
	}

	long long augCost = InqInt64Quality(AUGMENTATION_COST_INT64, 0);
	long long unassignedXP = player->InqInt64Quality(AVAILABLE_EXPERIENCE_INT64, 0);
	int aug = InqIntQuality(AUGMENTATION_STAT_INT, 0);
	int augJackOfTrades = player->InqIntQuality(AUGMENTATION_JACK_OF_ALL_TRADES_INT, 0);
	int augSkilledMelee = player->InqIntQuality(AUGMENTATION_SKILLED_MELEE_INT, 0);
	int augSkilledMissile = player->InqIntQuality(AUGMENTATION_SKILLED_MISSILE_INT, 0);
	int augSkilledMagic = player->InqIntQuality(AUGMENTATION_SKILLED_MAGIC_INT, 0);
	int augInnates = player->InqIntQuality(AUGMENTATION_INNATE_FAMILY_INT, 0);
	int augIncreasedBurden = player->InqIntQuality(AUGMENTATION_INCREASED_CARRYING_CAPACITY_INT, 0);
	int augSpecSalvage = player->InqIntQuality(AUGMENTATION_SPECIALIZE_SALVAGING_INT, 0);
	int augSpecItemTink = player->InqIntQuality(AUGMENTATION_SPECIALIZE_ITEM_TINKERING_INT, 0);
	int augSpecArmorTink = player->InqIntQuality(AUGMENTATION_SPECIALIZE_ARMOR_TINKERING_INT, 0);
	int augSpecMagicTink = player->InqIntQuality(AUGMENTATION_SPECIALIZE_MAGIC_ITEM_TINKERING_INT, 0);
	int augSpecWeaponTink = player->InqIntQuality(AUGMENTATION_SPECIALIZE_WEAPON_TINKERING_INT, 0);
	int augExtraPack = player->InqIntQuality(AUGMENTATION_EXTRA_PACK_SLOT_INT, 0);
	int augDropLess = player->InqIntQuality(AUGMENTATION_LESS_DEATH_ITEM_LOSS_INT, 0);
	int augCreatureFoci = player->InqIntQuality(AUGMENTATION_INFUSED_CREATURE_MAGIC_INT, 0);
	int augItemFoci = player->InqIntQuality(AUGMENTATION_INFUSED_ITEM_MAGIC_INT, 0);
	int augLifeFoci = player->InqIntQuality(AUGMENTATION_INFUSED_LIFE_MAGIC_INT, 0);
	int augWarFoci = player->InqIntQuality(AUGMENTATION_INFUSED_WAR_MAGIC_INT, 0);

	switch (aug)
	{
	case 1:
		if (augInnates == 10)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augInnates < 10)
		{
			if (unassignedXP >= augCost)
			{
				Attribute strength;
				player->m_Qualities.InqAttribute(STRENGTH_ATTRIBUTE, strength);

				if (strength._init_level == 100)
				{

					player->SendText("You are already at the maximum innate level.", LTT_DEFAULT);
					break;

				}
				else
				{
					player->m_Qualities.SetAttribute(STRENGTH_ATTRIBUTE, strength._init_level + 5);
					player->m_Qualities.SetInt(AUGMENTATION_INNATE_FAMILY_INT, augInnates + 1);
					player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
					player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
					player->NotifyAttributeStatUpdated(STRENGTH_ATTRIBUTE);

					player->EmitEffect(159, 1.0f);
					player->SendText("Congratulations! You have succeeded in acquiring the Reinforcement of the Lugians augmentation.", LTT_DEFAULT);
					std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
					if (!text.empty())
					{
						g_pWorld->BroadcastLocal(player->GetLandcell(), text);
					}


					DecrementStackOrStructureNum();
					break;
				}
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 2:
		if (augInnates == 10)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augInnates < 10)
		{
			if (unassignedXP >= augCost)
			{
				Attribute endurance;
				player->m_Qualities.InqAttribute(ENDURANCE_ATTRIBUTE, endurance);

				if (endurance._init_level == 100)
				{

					player->SendText("You are already at the maximum innate level.", LTT_DEFAULT);
					break;

				}
				else
				{
					player->m_Qualities.SetAttribute(ENDURANCE_ATTRIBUTE, endurance._init_level + 5);
					player->m_Qualities.SetInt(AUGMENTATION_INNATE_FAMILY_INT, augInnates + 1);
					player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
					player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
					player->NotifyAttributeStatUpdated(ENDURANCE_ATTRIBUTE);

					player->EmitEffect(159, 1.0f);
					player->SendText("Congratulations! You have succeeded in acquiring the Bleeargh's Fortitude augmentation.", LTT_DEFAULT);
					std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
					if (!text.empty())
					{
						g_pWorld->BroadcastLocal(player->GetLandcell(), text);
					}


					DecrementStackOrStructureNum();
					break;
				}
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 3:
		if (augInnates == 10)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augInnates < 10)
		{
			if (unassignedXP >= augCost)
			{
				Attribute coordination;
				player->m_Qualities.InqAttribute(COORDINATION_ATTRIBUTE, coordination);

				if (coordination._init_level == 100)
				{

					player->SendText("You are already at the maximum innate level.", LTT_DEFAULT);
					break;

				}
				else
				{
					player->m_Qualities.SetAttribute(COORDINATION_ATTRIBUTE, coordination._init_level + 5);
					player->m_Qualities.SetInt(AUGMENTATION_INNATE_FAMILY_INT, augInnates + 1);
					player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
					player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
					player->NotifyAttributeStatUpdated(COORDINATION_ATTRIBUTE);

					player->EmitEffect(159, 1.0f);
					player->SendText("Congratulations! You have succeeded in acquiring the Oswald's Enhancement augmentation.", LTT_DEFAULT);
					std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
					if (!text.empty())
					{
						g_pWorld->BroadcastLocal(player->GetLandcell(), text);
					}


					DecrementStackOrStructureNum();
					break;
				}
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 4:
		if (augInnates == 10)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augInnates < 10)
		{
			if (unassignedXP >= augCost)
			{
				Attribute quickness;
				player->m_Qualities.InqAttribute(QUICKNESS_ATTRIBUTE, quickness);

				if (quickness._init_level == 100)
				{

					player->SendText("You are already at the maximum innate level.", LTT_DEFAULT);
					break;

				}
				else
				{
					player->m_Qualities.SetAttribute(QUICKNESS_ATTRIBUTE, quickness._init_level + 5);
					player->m_Qualities.SetInt(AUGMENTATION_INNATE_FAMILY_INT, augInnates + 1);
					player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
					player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
					player->NotifyAttributeStatUpdated(QUICKNESS_ATTRIBUTE);

					player->EmitEffect(159, 1.0f);
					player->SendText("Congratulations! You have succeeded in acquiring the Siraluun's Blessing augmentation.", LTT_DEFAULT);
					std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
					if (!text.empty())
					{
						g_pWorld->BroadcastLocal(player->GetLandcell(), text);
					}


					DecrementStackOrStructureNum();
					break;
				}
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 5:
		if (augInnates == 10)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augInnates < 10)
		{
			if (unassignedXP >= augCost)
			{
				Attribute focus;
				player->m_Qualities.InqAttribute(FOCUS_ATTRIBUTE, focus);

				if (focus._init_level == 100)
				{

					player->SendText("You are already at the maximum innate level.", LTT_DEFAULT);
					break;

				}
				else
				{
					player->m_Qualities.SetAttribute(FOCUS_ATTRIBUTE, focus._init_level + 5);
					player->m_Qualities.SetInt(AUGMENTATION_INNATE_FAMILY_INT, augInnates + 1);
					player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
					player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
					player->NotifyAttributeStatUpdated(FOCUS_ATTRIBUTE);

					player->EmitEffect(159, 1.0f);
					player->SendText("Congratulations! You have succeeded in acquiring the Enduring Calm augmentation.", LTT_DEFAULT);
					std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
					if (!text.empty())
					{
						g_pWorld->BroadcastLocal(player->GetLandcell(), text);
					}


					DecrementStackOrStructureNum();
					break;
				}
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 6:
		if (augInnates == 10)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augInnates < 10)
		{
			if (unassignedXP >= augCost)
			{
				Attribute self;
				player->m_Qualities.InqAttribute(SELF_ATTRIBUTE, self);

				if (self._init_level == 100)
				{

					player->SendText("You are already at the maximum innate level.", LTT_DEFAULT);
					break;

				}
				else
				{
					player->m_Qualities.SetAttribute(SELF_ATTRIBUTE, self._init_level + 5);
					player->m_Qualities.SetInt(AUGMENTATION_INNATE_FAMILY_INT, augInnates + 1);
					player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
					player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
					player->NotifyAttributeStatUpdated(SELF_ATTRIBUTE);

					player->EmitEffect(159, 1.0f);
					player->SendText("Congratulations! You have succeeded in acquiring the Steadfast Will augmentation.", LTT_DEFAULT);
					std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
					if (!text.empty())
					{
						g_pWorld->BroadcastLocal(player->GetLandcell(), text);
					}


					DecrementStackOrStructureNum();
					break;
				}
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 7:
		if (augSpecSalvage == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augSpecSalvage == 0)
		{
			if (unassignedXP >= augCost)
			{
				Skill skill;
				player->m_Qualities.InqSkill(SALVAGING_SKILL, skill);
				if (skill._sac <= 1)
				{
					player->SendText("You are not trained in this skill!", LTT_DEFAULT);
					break;
				}

				else
				{
					skill.SetSkillAdvancementClass (SPECIALIZED_SKILL_ADVANCEMENT_CLASS);
					skill._level_from_pp = ExperienceSystem::SkillLevelFromExperience(skill._sac, skill._pp);
					skill._init_level = 10;
					player->m_Qualities.SetSkill(SALVAGING_SKILL, skill);
					player->m_Qualities.SetInt(AUGMENTATION_SPECIALIZE_SALVAGING_INT, augSpecSalvage + 1);
					player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
					player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
					player->NotifySkillStatUpdated(SALVAGING_SKILL);

					player->EmitEffect(159, 1.0f);
					player->SendText("Congratulations! You have succeeded in acquiring the Ciandra's Essence augmentation.", LTT_DEFAULT);
					std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
					if (!text.empty())
					{
						g_pWorld->BroadcastLocal(player->GetLandcell(), text);
					}

					DecrementStackOrStructureNum();

					break;
				}
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 8:
		if (augSpecSalvage == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augSpecItemTink == 0)
		{
			if (unassignedXP >= augCost)
			{
				Skill skill;
				player->m_Qualities.InqSkill(ITEM_APPRAISAL_SKILL, skill);
				if (skill._sac <= 1)
				{
					player->SendText("You are not trained in this skill!", LTT_DEFAULT);
					break;
				}

				else
				{
					skill.SetSkillAdvancementClass(SPECIALIZED_SKILL_ADVANCEMENT_CLASS);
					skill._level_from_pp = ExperienceSystem::SkillLevelFromExperience(skill._sac, skill._pp);
					skill._init_level = 10;
					player->m_Qualities.SetSkill(ITEM_APPRAISAL_SKILL, skill);
					player->m_Qualities.SetInt(AUGMENTATION_SPECIALIZE_ITEM_TINKERING_INT, augSpecItemTink + 1);
					player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
					player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
					player->NotifySkillStatUpdated(ITEM_APPRAISAL_SKILL);

					player->EmitEffect(159, 1.0f);
					player->SendText("Congratulations! You have succeeded in acquiring the Yoshi's Essence augmentation.", LTT_DEFAULT);
					std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
					if (!text.empty())
					{
						g_pWorld->BroadcastLocal(player->GetLandcell(), text);
					}

					DecrementStackOrStructureNum();

					break;
				}
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 9:
		if (augSpecArmorTink == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augSpecArmorTink == 0)
		{
			if (unassignedXP >= augCost)
			{
				Skill skill;
				player->m_Qualities.InqSkill(ARMOR_APPRAISAL_SKILL, skill);
				if (skill._sac <= 1)
				{
					player->SendText("You are not trained in this skill!", LTT_DEFAULT);
					break;
				}

				else
				{
					skill.SetSkillAdvancementClass(SPECIALIZED_SKILL_ADVANCEMENT_CLASS);
					skill._level_from_pp = ExperienceSystem::SkillLevelFromExperience(skill._sac, skill._pp);
					skill._init_level = 10;
					player->m_Qualities.SetSkill(ARMOR_APPRAISAL_SKILL, skill);
					player->m_Qualities.SetInt(AUGMENTATION_SPECIALIZE_ARMOR_TINKERING_INT, augSpecArmorTink + 1);
					player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
					player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
					player->NotifySkillStatUpdated(ARMOR_APPRAISAL_SKILL);

					player->EmitEffect(159, 1.0f);
					player->SendText("Congratulations! You have succeeded in acquiring the Jibril's Essence augmentation.", LTT_DEFAULT);
					std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
					if (!text.empty())
					{
						g_pWorld->BroadcastLocal(player->GetLandcell(), text);
					}

					DecrementStackOrStructureNum();
					break;
				}
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 10:
		if (augSpecMagicTink == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augSpecMagicTink == 0)
		{
			if (unassignedXP >= augCost)
			{
				Skill skill;
				player->m_Qualities.InqSkill(MAGIC_ITEM_APPRAISAL_SKILL, skill);
				if (skill._sac <= 1)
				{
					player->SendText("You are not trained in this skill!", LTT_DEFAULT);
					break;
				}

				else
				{
					skill.SetSkillAdvancementClass(SPECIALIZED_SKILL_ADVANCEMENT_CLASS);
					skill._level_from_pp = ExperienceSystem::SkillLevelFromExperience(skill._sac, skill._pp);
					skill._init_level = 10;
					player->m_Qualities.SetSkill(MAGIC_ITEM_APPRAISAL_SKILL, skill);
					player->m_Qualities.SetInt(AUGMENTATION_SPECIALIZE_MAGIC_ITEM_TINKERING_INT, augSpecMagicTink + 1);
					player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
					player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
					player->NotifySkillStatUpdated(MAGIC_ITEM_APPRAISAL_SKILL);

					player->EmitEffect(159, 1.0f);
					player->SendText("Congratulations! You have succeeded in acquiring the Celdiseth's Essence augmentation.", LTT_DEFAULT);
					std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
					if (!text.empty())
					{
						g_pWorld->BroadcastLocal(player->GetLandcell(), text);
					}
					DecrementStackOrStructureNum();
					break;
				}
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 11:
		if (augSpecWeaponTink == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augSpecWeaponTink == 0)
		{
			if (unassignedXP >= augCost)
			{
				Skill skill;
				player->m_Qualities.InqSkill(WEAPON_APPRAISAL_SKILL, skill);
				if (skill._sac <= 1)
				{
					player->SendText("You are not trained in this skill!", LTT_DEFAULT);
					break;
				}

				else
				{
					skill.SetSkillAdvancementClass(SPECIALIZED_SKILL_ADVANCEMENT_CLASS);
					skill._level_from_pp = ExperienceSystem::SkillLevelFromExperience(skill._sac, skill._pp);
					skill._init_level = 10;
					player->m_Qualities.SetSkill(WEAPON_APPRAISAL_SKILL, skill);
					player->m_Qualities.SetInt(AUGMENTATION_SPECIALIZE_WEAPON_TINKERING_INT, augSpecWeaponTink + 1);
					player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
					player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
					player->NotifySkillStatUpdated(WEAPON_APPRAISAL_SKILL);

					player->EmitEffect(159, 1.0f);
					player->SendText("Congratulations! You have succeeded in acquiring the Koga's Essence augmentation.", LTT_DEFAULT);
					std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
					if (!text.empty())
					{
						g_pWorld->BroadcastLocal(player->GetLandcell(), text);
					}
					DecrementStackOrStructureNum();
					break;
				}
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 12:
		if (augExtraPack == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augExtraPack == 0)
		{
			if (unassignedXP >= augCost)
			{
				player->m_Qualities.SetInt(AUGMENTATION_EXTRA_PACK_SLOT_INT, augExtraPack + 1);
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->m_Qualities.SetInt(CONTAINERS_CAPACITY_INT, 8);
				player->NotifyIntStatUpdated(CONTAINERS_CAPACITY_INT);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Shadow of The Seventh Mule augmentation.", LTT_DEFAULT);
				std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(player->GetLandcell(), text);
				}

				DecrementStackOrStructureNum();

				break;
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 13:
		if (augIncreasedBurden == 5)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augIncreasedBurden < 5)
		{
			if (unassignedXP >= augCost)
			{
				player->m_Qualities.SetInt(AUGMENTATION_INCREASED_CARRYING_CAPACITY_INT, augIncreasedBurden + 1);
				DecrementStackOrStructureNum();
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->NotifyIntStatUpdated(ENCUMB_CAPACITY_INT);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Might of The Seventh Mule augmentation.", LTT_DEFAULT);
				std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(player->GetLandcell(), text);
				}

				DecrementStackOrStructureNum();

				break;
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 14:
		if (augDropLess == 3)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augDropLess < 3)
		{
			if (unassignedXP >= augCost)
			{
				player->m_Qualities.SetInt(AUGMENTATION_LESS_DEATH_ITEM_LOSS_INT, augDropLess + 1);
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->NotifyIntStatUpdated(AUGMENTATION_LESS_DEATH_ITEM_LOSS_INT);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Clutch of the Miser augmentation.", LTT_DEFAULT);
				std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(player->GetLandcell(), text);
				}

				DecrementStackOrStructureNum();

				break;
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 29:
		if (augCreatureFoci == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augDropLess == 0)
		{
			if (unassignedXP >= augCost)
			{
				player->m_Qualities.SetInt(AUGMENTATION_INFUSED_CREATURE_MAGIC_INT, augCreatureFoci + 1);
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->NotifyIntStatUpdated(AUGMENTATION_INFUSED_CREATURE_MAGIC_INT);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Infused Creature Magic augmentation.", LTT_DEFAULT);
				std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(player->GetLandcell(), text);
				}

				DecrementStackOrStructureNum();

				break;
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 30:
		if (augItemFoci == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augDropLess == 0)
		{
			if (unassignedXP >= augCost)
			{
				player->m_Qualities.SetInt(AUGMENTATION_INFUSED_ITEM_MAGIC_INT, augItemFoci + 1);
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->NotifyIntStatUpdated(AUGMENTATION_INFUSED_ITEM_MAGIC_INT);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Infused Item Magic augmentation.", LTT_DEFAULT);
				std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(player->GetLandcell(), text);
				}

				DecrementStackOrStructureNum();

				break;
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 31:
		if (augLifeFoci == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augDropLess == 0)
		{
			if (unassignedXP >= augCost)
			{
				player->m_Qualities.SetInt(AUGMENTATION_INFUSED_LIFE_MAGIC_INT, augLifeFoci + 1);
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->NotifyIntStatUpdated(AUGMENTATION_INFUSED_LIFE_MAGIC_INT);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Infused Life Magic augmentation.", LTT_DEFAULT);
				std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(player->GetLandcell(), text);
				}

				DecrementStackOrStructureNum();

				break;
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 32:
		if (augWarFoci == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augDropLess == 0)
		{
			if (unassignedXP >= augCost)
			{
				player->m_Qualities.SetInt(AUGMENTATION_INFUSED_WAR_MAGIC_INT, augWarFoci + 1);
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->NotifyIntStatUpdated(AUGMENTATION_INFUSED_WAR_MAGIC_INT);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Infused War Magic augmentation.", LTT_DEFAULT);
				std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(player->GetLandcell(), text);
				}

				DecrementStackOrStructureNum();

				break;
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 35:
		if (augSkilledMelee == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augSkilledMelee == 0)
		{
			if (unassignedXP >= augCost)
			{
				player->m_Qualities.SetInt(AUGMENTATION_SKILLED_MELEE_INT, 1);
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Master of The Steel Circle augmentation.", LTT_DEFAULT);
				std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(player->GetLandcell(), text);
				}

				DecrementStackOrStructureNum();


				break;
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 36:
		if (augSkilledMissile == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augSkilledMissile == 0)
		{
			if (unassignedXP >= augCost)
			{
				player->m_Qualities.SetInt(AUGMENTATION_SKILLED_MISSILE_INT, 1);
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Master of the Focused Eye augmentation.", LTT_DEFAULT);
				std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(player->GetLandcell(), text);
				}

				DecrementStackOrStructureNum();


				break;
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 37:
		if (augSkilledMagic == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augSkilledMagic == 0)
		{
			if (unassignedXP >= augCost)
			{
				player->m_Qualities.SetInt(AUGMENTATION_SKILLED_MAGIC_INT, 1);
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Master of The Five Fold Path augmentation.", LTT_DEFAULT);
				std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(player->GetLandcell(), text);
				}

				DecrementStackOrStructureNum();


				break;
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	case 40:
		if (augJackOfTrades == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augJackOfTrades == 0)
		{
			if (unassignedXP >= augCost)
			{
				player->m_Qualities.SetInt(AUGMENTATION_JACK_OF_ALL_TRADES_INT, 1);
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Jack of All Trades augmentation.", LTT_DEFAULT);
				player->SendText(csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str()), LTT_WORLD_BROADCAST);
				std::string text = csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(player->GetLandcell(), text);
				}

				DecrementStackOrStructureNum();


				break;
			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
			break;
		}
	default:
		player->SendText("This Augmentation is not supported at this time!", LTT_DEFAULT);
		break;
	}

	player->NotifyUseDone(WERROR_NONE);
	return WERROR_NONE;

}
