
#include "StdAfx.h"
#include "AugmentationDevice.h"
#include "UseManager.h"
#include "Player.h"
#include "Qualities.h"

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

	switch (aug)
	{
	case 1:
		if (augInnates >= 10)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			break;
		}
		if (augInnates <= 9)
		{
			if (unassignedXP >= augCost)
			{
				Attribute strength;				
				player->m_Qualities.InqAttribute(STRENGTH_ATTRIBUTE, strength);
				if (strength._init_level == 100)
					player->SendText("You can't do this dipshit", LTT_DEFAULT);
				else
					strength._init_level = strength._init_level + 5;
				player->m_Qualities.SetInt(AUGMENTATION_INNATE_FAMILY_INT, + 1);
				DecrementStackOrStructureNum();
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Innate Strength augmentation.", LTT_DEFAULT);
				player->SendText(csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str()), LTT_WORLD_BROADCAST);

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
				DecrementStackOrStructureNum();
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Master of The Steel Circle augmentation.", LTT_DEFAULT);
				player->SendText(csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str()), LTT_WORLD_BROADCAST);
				//Need to cycle through the skills and notify the updated values here. Previous code was no good

				//for (PackableHashTableWithJson<STypeSkill, Skill>::iterator entry = player->m_Qualities._skillStatsTable->begin(); entry != player->m_Qualities._skillStatsTable->end(); entry++)
				//{
				//DWORD val = 5;
				//DWORD &valptr = val;
				//STypeSkill skill = entry->first;
				//player->InqSkill(skill, valptr, false);
				//player->NotifySkillStatUpdated(skill);

				//}

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
				DecrementStackOrStructureNum();
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Master of the Focused Eye augmentation.", LTT_DEFAULT);
				player->SendText(csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str()), LTT_WORLD_BROADCAST);
				//Need to cycle through the skills and notify the updated values here. Previous code was no good

				//for (PackableHashTableWithJson<STypeSkill, Skill>::iterator entry = player->m_Qualities._skillStatsTable->begin(); entry != player->m_Qualities._skillStatsTable->end(); entry++)
				//{
				//DWORD val = 5;
				//DWORD &valptr = val;
				//STypeSkill skill = entry->first;
				//player->InqSkill(skill, valptr, false);
				//player->NotifySkillStatUpdated(skill);

				//}

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
				DecrementStackOrStructureNum();
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Master of The Five Fold Path augmentation.", LTT_DEFAULT);
				player->SendText(csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str()), LTT_WORLD_BROADCAST);
				//Need to cycle through the skills and notify the updated values here. Previous code was no good

				//for (PackableHashTableWithJson<STypeSkill, Skill>::iterator entry = player->m_Qualities._skillStatsTable->begin(); entry != player->m_Qualities._skillStatsTable->end(); entry++)
				//{
				//DWORD val = 5;
				//DWORD &valptr = val;
				//STypeSkill skill = entry->first;
				//player->InqSkill(skill, valptr, false);
				//player->NotifySkillStatUpdated(skill);

				//}

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
				DecrementStackOrStructureNum();
				player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, unassignedXP - augCost);
				player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
				player->EmitEffect(159, 1.0f);
				player->SendText("Congratulations! You have succeeded in acquiring the Jack of All Trades augmentation.", LTT_DEFAULT);
				player->SendText(csprintf("%s has acquired the %s augmentation!", player->GetName().c_str(), GetName().c_str()), LTT_WORLD_BROADCAST);
				//Need to cycle through the skills and notify the updated values here. Previous code was no good

				//for (PackableHashTableWithJson<STypeSkill, Skill>::iterator entry = player->m_Qualities._skillStatsTable->begin(); entry != player->m_Qualities._skillStatsTable->end(); entry++)
				//{
					//DWORD val = 5;
					//DWORD &valptr = val;
					//STypeSkill skill = entry->first;
					//player->InqSkill(skill, valptr, false);
					//player->NotifySkillStatUpdated(skill);

				//}

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
