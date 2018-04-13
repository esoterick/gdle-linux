
#include "StdAfx.h"
#include "AugmentationDevice.h"
#include "UseManager.h"
#include "Player.h"

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

	switch (aug)
	{
	case 40:
	{
		int augJackOfTrades = player->InqIntQuality(AUGMENTATION_JACK_OF_ALL_TRADES_INT, 0);
		if (augJackOfTrades == 1)
		{
			player->SendText("This augmentation is already active.", LTT_DEFAULT);
			player->NotifyUseDone(WERROR_NONE);
			return WERROR_NONE;
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
				//Cycle through the skills and notify the updated values
				if (m_Qualities._skillStatsTable)
				{
					for (PackableHashTableWithJson<STypeSkill, Skill>::iterator entry = m_Qualities._skillStatsTable->begin(); entry != m_Qualities._skillStatsTable->end(); entry++)
					{
						STypeSkill skill = entry->first;
						player->NotifySkillStatUpdated(skill);
					}
				}

			}
			else
				player->SendText("You do not have enough experience to use this augmentation gem.", LTT_DEFAULT);
		}
	}
	}

	player->NotifyUseDone(WERROR_NONE);
	return WERROR_NONE;
}
