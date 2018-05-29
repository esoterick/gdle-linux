
#include "StdAfx.h"
#include "EmoteManager.h"
#include "WeenieObject.h"
#include "ChatMsgs.h"
#include "World.h"
#include "WeenieFactory.h"
#include "Player.h"
#include "SpellcastingManager.h"
#include "Config.h"
#include "GameEventManager.h"
#include "MonsterAI.h"

EmoteManager::EmoteManager(std::shared_ptr<CWeenieObject> weenie)
{
	_weenie = weenie;
}

bool EmoteManager::ChanceExecuteEmoteSet(EmoteCategory category, std::string msg, DWORD target_id)
{
	std::shared_ptr<CWeenieObject> pWeenie = pWeenie;
	if (!pWeenie)
	{
		return false;
	}

	PackableList<EmoteSet> *emoteCategory = pWeenie->m_Qualities._emote_table->_emote_table.lookup(category);

	if (!emoteCategory)
		return false;

	float diceRoll = Random::RollDice(0.0, 1.0);

	for (auto &entry : *emoteCategory)
	{
		if (!_stricmp(entry.quest.c_str(), msg.c_str()) && diceRoll < entry.probability)
		{
			ExecuteEmoteSet(entry, target_id);
			return true;
		}
	}
	return false;
}

bool EmoteManager::ChanceExecuteEmoteSet(EmoteCategory category, DWORD target_id)
{
	std::shared_ptr<CWeenieObject> pWeenie = pWeenie;
	if (!pWeenie)
	{
		return false;
	}
	PackableList<EmoteSet> *emoteCategory =pWeenie->m_Qualities._emote_table->_emote_table.lookup(category);

	if (!emoteCategory)
		return false;

	float diceRoll = Random::RollDice(0.0, 1.0);

	for (auto &entry : *emoteCategory)
	{
		if (diceRoll < entry.probability)
		{
			ExecuteEmoteSet(entry, target_id);
			return true;
		}
	}
	return false;
}

void EmoteManager::ExecuteEmoteSet(const EmoteSet &emoteSet, DWORD target_id)
{
	if (_emoteEndTime < Timer::cur_time)
		_emoteEndTime = Timer::cur_time;

	double totalEmoteSetTime = 0.0;
	for (auto &emote : emoteSet.emotes)
	{
		totalEmoteSetTime += emote.delay;

		QueuedEmote qe;
		qe._data = emote;
		qe._target_id = target_id;
		qe._executeTime = Timer::cur_time + totalEmoteSetTime;
		_emoteQueue.push_back(qe);
	}
}

std::string EmoteManager::ReplaceEmoteText(const std::string &text, DWORD target_id, DWORD source_id)
{
	std::string result = text;

	if (result.find("%s") != std::string::npos)
	{
		std::string targetName;
		if (!g_pWorld->FindObjectName(target_id, targetName))
			return ""; // Couldn't resolve name, don't display this message.

		while (ReplaceString(result, "%s", targetName));
	}

	if (result.find("%tn") != std::string::npos)
	{
		std::string targetName;
		if (!g_pWorld->FindObjectName(target_id, targetName))
			return ""; // Couldn't resolve name, don't display this message.

		while (ReplaceString(result, "%tn", targetName));
	}

	if (result.find("%n") != std::string::npos)
	{
		std::string sourceName;
		if (!g_pWorld->FindObjectName(source_id, sourceName))
			return ""; // Couldn't resolve name, don't display this message.

		while (ReplaceString(result, "%n", sourceName));
	}

	if (result.find("%mn") != std::string::npos)
	{
		std::string sourceName;
		if (!g_pWorld->FindObjectName(source_id, sourceName))
			return ""; // Couldn't resolve name, don't display this message.

		while (ReplaceString(result, "%mn", sourceName));
	}

	if (result.find("%tqt") != std::string::npos)
	{
		while (ReplaceString(result, "%tqt", "some amount of time"));
	}

	return result;
}

void EmoteManager::ExecuteEmote(const Emote &emote, DWORD target_id)
{
	std::shared_ptr<CWeenieObject> pWeenie = pWeenie;
	if (!pWeenie)
	{
		return;
	}

	switch (emote.type)
	{
	default:
		{
#ifndef PUBLIC_BUILD
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (!target || !target->IsAdmin())
				break;

			target->SendText(csprintf("Unhandled emote %s (%u)", Emote::EmoteTypeToName(emote.type), emote.type), LTT_DEFAULT);
#endif
			break;
		}
	case Act_EmoteType:
	{
		std::string text = ReplaceEmoteText(emote.msg, target_id, pWeenie->GetID());

		if (!text.empty())
		{
			BinaryWriter *textMsg = ServerText(text.c_str(), LTT_EMOTE);

			std::list<std::shared_ptr<CWeenieObject> > results;
			g_pWorld->EnumNearbyPlayers(pWeenie->GetPosition(), 30.0f, &results);

			for (auto target : results)
			{
				if (target == pWeenie)
					continue;

				target->SendNetMessage(textMsg->GetData(), textMsg->GetSize(), PRIVATE_MSG, 0);
			}
			delete textMsg;
		}

		break;
	}
	case LocalBroadcast_EmoteType:
		{
			std::string text = ReplaceEmoteText(emote.msg, target_id, pWeenie->GetID());

			if (!text.empty())
			{
				BinaryWriter *textMsg = ServerText(text.c_str(), LTT_DEFAULT);
				g_pWorld->BroadcastPVS(pWeenie, textMsg->GetData(), textMsg->GetSize(), PRIVATE_MSG, 0, false);
				delete textMsg;
			}

			break;
		}
	case WorldBroadcast_EmoteType:
		{
			std::string text = ReplaceEmoteText(emote.msg, target_id, pWeenie->GetID());

			if (!text.empty())
			{
				BinaryWriter *textMsg = ServerText(text.c_str(), LTT_DEFAULT);
				g_pWorld->BroadcastGlobal(textMsg->GetData(), textMsg->GetSize(), PRIVATE_MSG, 0, false);
				delete textMsg;
			}

			break;
		}
	case AdminSpam_EmoteType:
	{
		std::string text = ReplaceEmoteText(emote.msg, target_id, pWeenie->GetID());

		if (!text.empty())
		{
			BinaryWriter *textMsg = ServerText(text.c_str(), LTT_ALL_CHANNELS);
			g_pWorld->BroadcastGlobal(textMsg->GetData(), textMsg->GetSize(), PRIVATE_MSG, 0, false);
			delete textMsg;
		}

		break;
	}
	case Activate_EmoteType:
		{
			if (DWORD activation_target_id = pWeenie->InqIIDQuality(ACTIVATION_TARGET_IID, 0))
			{
				std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(activation_target_id);
				if (target)
					target->Activate(target_id);
			}

			break;
		}
	case CastSpellInstant_EmoteType:
		{
			if (target_id == 0)
				target_id = pWeenie->GetID();
			pWeenie->MakeSpellcastingManager()->CastSpellInstant(target_id, emote.spellid);
			break;
		}
	case CastSpell_EmoteType:
		{
			if (target_id == 0)
				target_id = pWeenie->GetID();
			pWeenie->MakeSpellcastingManager()->CreatureBeginCast(target_id, emote.spellid);
			break;
		}
	case AwardXP_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			long long amount = emote.amount64;

			amount = (int)(amount * g_pConfig->RewardXPMultiplier(target->InqIntQuality(LEVEL_INT, 0)));

			if (amount < 0)
				amount = 0;

			target->GiveSharedXP(amount, true);
			break;
		}
	case AwardNoShareXP_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			long long amount = emote.amount64;

			amount = (int)(amount * g_pConfig->RewardXPMultiplier(target->InqIntQuality(LEVEL_INT, 0)));

			if (amount < 0)
				amount = 0;

			target->GiveXP(amount, true);
			break;
		}
	case AwardSkillXP_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			target->GiveSkillXP((STypeSkill)emote.stat, emote.amount, false);
			break;
		}
	case AwardLevelProportionalSkillXP_EmoteType:
	{
		std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
		if (!target)
			break;

		Skill skill;
		target->m_Qualities.InqSkill((STypeSkill)emote.stat, skill);

		if (skill._sac < TRAINED_SKILL_ADVANCEMENT_CLASS)
			break;

		int currentLevel = skill._level_from_pp;

		if (currentLevel <= 10)
			currentLevel += 5 - (currentLevel * 0.4);

		DWORD64 xp_to_next_level = ExperienceSystem::ExperienceToSkillLevel(TRAINED_SKILL_ADVANCEMENT_CLASS, currentLevel + 1) - ExperienceSystem::ExperienceToSkillLevel(TRAINED_SKILL_ADVANCEMENT_CLASS, currentLevel);

		DWORD64 xp_to_give = (DWORD64)round((long double)xp_to_next_level * emote.percent);
		if (emote.min > 0)
			xp_to_give = max(xp_to_give, emote.min);
		if (emote.max > 0)
			xp_to_give = min(xp_to_give, emote.max);

		target->GiveSkillXP((STypeSkill)emote.stat, xp_to_give, false);
		break;
	}
	case AwardSkillPoints_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			target->GiveSkillPoints((STypeSkill)emote.stat, emote.amount);
			break;
		}
	case AwardTrainingCredits_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			target->GiveSkillCredits(emote.amount, true);
			break;
		}
	case AwardLevelProportionalXP_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			int current_level = target->m_Qualities.GetInt(LEVEL_INT, 1);
			DWORD64 xp_to_next_level = ExperienceSystem::ExperienceToRaiseLevel(current_level, current_level + 1);

			DWORD64 xp_to_give = (DWORD64)round((long double)xp_to_next_level * emote.percent);
			if (emote.min64 > 0)
				xp_to_give = max(xp_to_give, emote.min64);
			if (emote.max64 > 0)
				xp_to_give = min(xp_to_give, emote.max64);
			target->GiveXP(xp_to_give, true, false);
			break;
		}
	case Give_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (!target)
				break;

			pWeenie->SimulateGiveObject(target, emote.cprof.wcid, emote.cprof.stack_size, emote.cprof.palette, emote.cprof.shade, emote.cprof.try_to_bond);
			break;
		}
	case Motion_EmoteType:
		{
			pWeenie->DoAutonomousMotion(OldToNewCommandID(emote.motion));
			break;
		}
	case ForceMotion_EmoteType:
		{
			pWeenie->DoForcedMotion(OldToNewCommandID(emote.motion));
			break;
		}
	case PhysScript_EmoteType:
	{
		std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
		if (target)
			target->EmitEffect(emote.pscript, 1.0);

		break;
	}
	case Say_EmoteType:
	{
		std::string text = ReplaceEmoteText(emote.msg, target_id, pWeenie->GetID());

		if (!text.empty())
			pWeenie->SpeakLocal(text.c_str(), LTT_EMOTE);

		break;
	}
	case Sound_EmoteType:
	{
		pWeenie->EmitSound(emote.sound, 1.0);

		break;
	}
	case Tell_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (target)
			{
				std::string text = ReplaceEmoteText(emote.msg, target_id, pWeenie->GetID());

				if (!text.empty())
					target->SendNetMessage(DirectChat(text.c_str(), pWeenie->GetName().c_str(), pWeenie->GetID(), target->GetID(), LTT_SPEECH_DIRECT), PRIVATE_MSG, TRUE);
			}

			break;
		}
	case InflictVitaePenalty_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (target)
			{
				target->UpdateVitaePool(0);
				target->ReduceVitae((float)emote.amount / 100.f);
				target->UpdateVitaeEnchantment();
			}

			break;
		}
	case TellFellow_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (target)
			{
				std::string text = ReplaceEmoteText(emote.msg, target_id, pWeenie->GetID());

				if (text.empty())
					break;

				Fellowship *fellow = target->GetFellowship();

				if (!fellow)
				{
					target->SendNetMessage(DirectChat(text.c_str(), pWeenie->GetName().c_str(), pWeenie->GetID(), target->GetID(), LTT_SPEECH_DIRECT), PRIVATE_MSG, TRUE);
				}
				else
				{
					for (auto &entry : fellow->_fellowship_table)
					{
						if (std::shared_ptr<CWeenieObject> member = g_pWorld->FindPlayer(entry.first))
						{
							member->SendNetMessage(DirectChat(text.c_str(), pWeenie->GetName().c_str(), pWeenie->GetID(), target->GetID(), LTT_SPEECH_DIRECT), PRIVATE_MSG, TRUE);
						}
					}
				}
			}

			break;
		}

	case LockFellow_EmoteType:
	{
		std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
		if (target)
		{
			Fellowship *fellow = target->GetFellowship();

			if (fellow)
				fellow->_locked = true;
		}

		break;
	}

	case TextDirect_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (target)
			{
				std::string text = ReplaceEmoteText(emote.msg, target_id, pWeenie->GetID());

				if (!text.empty())
					target->SendNetMessage(ServerText(text.c_str(), LTT_DEFAULT), PRIVATE_MSG, TRUE);
			}

			break;
		}
	case DirectBroadcast_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (target)
			{
				std::string text = ReplaceEmoteText(emote.msg, target_id, pWeenie->GetID());

				if (!text.empty())
					target->SendNetMessage(ServerText(text.c_str(), LTT_DEFAULT), PRIVATE_MSG, TRUE);
			}

			break;
		}
	case BLog_EmoteType:
	{
		std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
		if (target)
		{
			std::string text = ReplaceEmoteText(emote.msg, target_id, pWeenie->GetID());

			if (!text.empty())
				target->SendNetMessage(ServerText(text.c_str(), LTT_COMBAT), PRIVATE_MSG, TRUE);
		}

		break;
	}
	case FellowBroadcast_EmoteType:
		{
			std::shared_ptr<CPlayerWeenie> target = g_pWorld->FindPlayer(target_id);
			if (target)
			{
				std::string text = ReplaceEmoteText(emote.msg, target_id, pWeenie->GetID());

				if (text.empty())
					break;

				Fellowship *fellow = target->GetFellowship();

				if (!fellow)
				{
					target->SendNetMessage(ServerText(text.c_str(), LTT_DEFAULT), PRIVATE_MSG, TRUE);
				}
				else
				{
					for (auto &entry : fellow->_fellowship_table)
					{
						if (std::shared_ptr<CWeenieObject> member = g_pWorld->FindPlayer(entry.first))
						{
							member->SendNetMessage(ServerText(text.c_str(), LTT_DEFAULT), PRIVATE_MSG, TRUE);
						}
					}
				}
			}

			break;
		}
	case TurnToTarget_EmoteType:
		{
			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				MovementParameters params;
				pWeenie->TurnToObject(target_id, &params);
			}
			break;
		}
	case Turn_EmoteType:
		{
			MovementParameters params;
			params.desired_heading = emote.frame.get_heading();
			params.speed = 1.0f;
			params.action_stamp = ++pWeenie->m_wAnimSequence;
			params.modify_interpreted_state = 0;
			pWeenie->last_move_was_autonomous = false;

			pWeenie->cancel_moveto();
			pWeenie->TurnToHeading(&params);
			break;
		}
	case TeachSpell_EmoteType:
		{
			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				target->LearnSpell(emote.spellid, true);
			}
			break;
		}
	case InqEvent_EmoteType:
		{
			bool success = g_pGameEventManager->IsEventStarted(emote.msg.c_str());

			ChanceExecuteEmoteSet(success ? EventSuccess_EmoteCategory : EventFailure_EmoteCategory, emote.msg, target_id);
			break;
		}

	case StartEvent_EmoteType:
		{
			g_pGameEventManager->StartEvent(emote.msg.c_str());
			break;
		}

	case StopEvent_EmoteType:
		{
			g_pGameEventManager->StopEvent(emote.msg.c_str());
			break;
		}

	case InqQuest_EmoteType:
		{
			if (!pWeenie->m_Qualities._emote_table)
				break;

			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = target->InqQuest(emote.msg.c_str());

				ChanceExecuteEmoteSet(success ? QuestSuccess_EmoteCategory : QuestFailure_EmoteCategory, emote.msg, target_id);
			}

			break;
		}
	case InqFellowNum_EmoteType:
		{
		if (!pWeenie->m_Qualities._emote_table)
		{
			break;
		}

		std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
		if (target)
		{
			Fellowship *fellow = target->GetFellowship();

			if (!fellow)
				ChanceExecuteEmoteSet(TestNoFellow_EmoteCategory, emote.msg, target_id);
			else
			{
				bool success = false;

				int numberOfMembers = (int)fellow->_fellowship_table.size();

				if (numberOfMembers >= emote.min && numberOfMembers <= emote.max)
					success = true;

				ChanceExecuteEmoteSet(success ? NumFellowsSuccess_EmoteCategory : NumFellowsFailure_EmoteCategory, emote.msg, target_id);
			}
		}

		break;
	}
	case InqFellowQuest_EmoteType:
		{
			if (!pWeenie->m_Qualities._emote_table)
			{
				break;
			}

			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				Fellowship *fellow = target->GetFellowship();

				if (!fellow)
					ChanceExecuteEmoteSet(QuestNoFellow_EmoteCategory, emote.msg, target_id);
				else
				{
					bool success = false;

					for (auto &entry : fellow->_fellowship_table)
					{
						if (std::shared_ptr<CWeenieObject> member = g_pWorld->FindObject(entry.first))
						{
							success = member->InqQuest(emote.msg.c_str());

							if (success)
								break;
						}
					}

					ChanceExecuteEmoteSet(success ? QuestSuccess_EmoteCategory : QuestFailure_EmoteCategory, emote.msg, target_id);
				}
			}

			break;
		}
	case UpdateQuest_EmoteType:
		{
			if (!pWeenie->m_Qualities._emote_table)
				break;

			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = target->UpdateQuest(emote.msg.c_str());
					
				ChanceExecuteEmoteSet(success ? QuestSuccess_EmoteCategory : QuestFailure_EmoteCategory, emote.msg, target_id);
			}

			break;
		}
	case StampQuest_EmoteType:
		{
			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				target->StampQuest(emote.msg.c_str());
			}
			break;
		}
	case StampFellowQuest_EmoteType:
		{
			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				Fellowship *fellow = target->GetFellowship();

				if (fellow)
				{
					for (auto &entry : fellow->_fellowship_table)
					{
						if (std::shared_ptr<CWeenieObject> member = g_pWorld->FindObject(entry.first))
						{
							member->StampQuest(emote.msg.c_str());
						}
					}
				}
			}

			break;
		}
	case UpdateFellowQuest_EmoteType:
		{
			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				Fellowship *fellow = target->GetFellowship();
				if (!fellow)
					ChanceExecuteEmoteSet(QuestNoFellow_EmoteCategory, emote.msg, target_id);
				else
				{
					/*
					bool success = false;

					for (auto &entry : fellow->_fellowship_table)
					{
						if (std::shared_ptr<CWeenieObject> member = g_pWorld->FindObject(entry.first))
						{
							success = member->UpdateQuest(emote.msg.c_str());

							if (success)
								break;
						}
					}

					PackableList<EmoteSet> *emoteCategory = pWeenie->m_Qualities._emote_table->_emote_table.lookup(success ? QuestSuccess_EmoteCategory : QuestFailure_EmoteCategory);
					if (!emoteCategory)
						break;

					for (auto &entry : *emoteCategory)
					{
						if (!_stricmp(entry.quest.c_str(), emote.msg.c_str()))
						{
							// match
							ExecuteEmoteSet(entry, target_id);
							break;
						}
					}
					*/

					target->SendText("Unsupported quest logic, please report to Pea how you received this.", LTT_DEFAULT);

					ChanceExecuteEmoteSet(QuestFailure_EmoteCategory, emote.msg, target_id);
				}
			}

			break;
		}
	case IncrementQuest_EmoteType:
		{
			if (!pWeenie->m_Qualities._emote_table)
				break;

			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				target->IncrementQuest(emote.msg.c_str());
			}

			break;
		}
	case DecrementQuest_EmoteType:
		{
			if (!pWeenie->m_Qualities._emote_table)
				break;

			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				target->DecrementQuest(emote.msg.c_str());
			}

			break;
		}
	case EraseQuest_EmoteType:
		{
			if (!pWeenie->m_Qualities._emote_table)
				break;

			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				target->EraseQuest(emote.msg.c_str());
			}

			break;
		}
	case Goto_EmoteType:
		{
			ChanceExecuteEmoteSet(GotoSet_EmoteCategory, emote.msg, target_id);
		}
	case InqBoolStat_EmoteType:
	{
		if (!pWeenie->m_Qualities._emote_table)
			break;

		std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
		if (target)
		{
			bool success = false;
			bool hasQuality = false;

			int boolStat;
			if (target->m_Qualities.InqBool((STypeBool)emote.stat, boolStat))
			{
				hasQuality = true;
				if (boolStat > 0)
					success = true;
			}

			if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
				break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
			ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
		}

		break;
	}
	case InqIntStat_EmoteType:
		{
			if (!pWeenie->m_Qualities._emote_table)
				break;

			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = false;
				bool hasQuality = false;

				int intStat;
				if (target->m_Qualities.InqInt((STypeInt) emote.stat, intStat))
				{
					hasQuality = true;
					if (intStat >= emote.min && intStat <= emote.max)
					{
						success = true;
					}
				}

				if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
					break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
				ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
			}

			break;
		}
	case InqFloatStat_EmoteType:
	{
		if (!pWeenie->m_Qualities._emote_table)
			break;

		std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
		if (target)
		{
			bool success = false;
			bool hasQuality = false;

			double floatStat;
			if (target->m_Qualities.InqFloat((STypeFloat)emote.stat, floatStat))
			{
				hasQuality = true;
				if (floatStat >= emote.fmin && floatStat <= emote.fmax)
					success = true;
			}

			if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
				break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
			ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
		}

		break;
	}
	case InqStringStat_EmoteType:
	{
		if (!pWeenie->m_Qualities._emote_table)
			break;

		std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
		if (target)
		{
			bool success = false;
			bool hasQuality = false;

			std::string stringStat;
			if (target->m_Qualities.InqString((STypeString)emote.stat, stringStat))
			{
				hasQuality = true;
				if (stringStat == emote.teststring)
					success = true;
			}

			if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
				break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
			ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
		}

		break;
	}
	case InqAttributeStat_EmoteType:
	{
		if (!pWeenie->m_Qualities._emote_table)
			break;

		std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
		if (target)
		{
			bool success = false;
			bool hasQuality = false;

			DWORD attributeStat;
			if (target->m_Qualities.InqAttribute((STypeAttribute)emote.stat, attributeStat, FALSE))
			{
				hasQuality = true;
				if (attributeStat >= emote.min && attributeStat <= emote.max)
					success = true;
			}

			if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
				break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
			ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
		}

		break;
	}
	case InqRawAttributeStat_EmoteType:
	{
		if (!pWeenie->m_Qualities._emote_table)
			break;

		std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
		if (target)
		{
			bool success = false;
			bool hasQuality = false;

			DWORD attributeStat;
			if (target->m_Qualities.InqAttribute((STypeAttribute)emote.stat, attributeStat, TRUE))
			{
				hasQuality = true;
				if (attributeStat >= emote.min && attributeStat <= emote.max)
					success = true;
			}

			if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
				break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
			ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
		}

		break;
	}
	case InqSecondaryAttributeStat_EmoteType:
	{
		if (!pWeenie->m_Qualities._emote_table)
			break;

		std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
		if (target)
		{
			bool success = false;
			bool hasQuality = false;

			DWORD attribute2ndStat;
			if (target->m_Qualities.InqAttribute2nd((STypeAttribute2nd)emote.stat, attribute2ndStat, FALSE))
			{
				hasQuality = true;
				if (attribute2ndStat >= emote.min && attribute2ndStat <= emote.max)
					success = true;
			}

			if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
				break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
			ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
		}

		break;
	}
	case InqRawSecondaryAttributeStat_EmoteType:
	{
		if (!pWeenie->m_Qualities._emote_table)
			break;

		std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
		if (target)
		{
			bool success = false;
			bool hasQuality = false;

			DWORD attribute2ndStat;
			if (target->m_Qualities.InqAttribute2nd((STypeAttribute2nd)emote.stat, attribute2ndStat, TRUE))
			{
				hasQuality = true;
				if (attribute2ndStat >= emote.min && attribute2ndStat <= emote.max)
					success = true;
			}

			if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
				break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
			ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
		}

		break;
	}
	case InqSkillTrained_EmoteType:
		{
			if (!pWeenie->m_Qualities._emote_table)
				break;

			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = false;

				SKILL_ADVANCEMENT_CLASS sac = SKILL_ADVANCEMENT_CLASS::UNTRAINED_SKILL_ADVANCEMENT_CLASS;
				bool hasQuality = target->m_Qualities.InqSkillAdvancementClass((STypeSkill)emote.stat, sac);

				if (sac == TRAINED_SKILL_ADVANCEMENT_CLASS || sac == SPECIALIZED_SKILL_ADVANCEMENT_CLASS)
					success = true;

				if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
					break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
				ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
			}

			break;
		}
	case InqSkillSpecialized_EmoteType:
		{
			if (!pWeenie->m_Qualities._emote_table)
				break;

			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = false;

				SKILL_ADVANCEMENT_CLASS sac = SKILL_ADVANCEMENT_CLASS::UNTRAINED_SKILL_ADVANCEMENT_CLASS;
				bool hasQuality = target->m_Qualities.InqSkillAdvancementClass((STypeSkill)emote.stat, sac);

				if (sac == SPECIALIZED_SKILL_ADVANCEMENT_CLASS)
					success = true;

				if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
					break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
				ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
			}

			break;
		}
	case InqSkillStat_EmoteType:
		{
			if (!pWeenie->m_Qualities._emote_table)
				break;

			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = false;
				bool hasQuality = false;

				DWORD skillStat;
				if (target->m_Qualities.InqSkill((STypeSkill)emote.stat, skillStat, FALSE))
				{
					hasQuality = true;
					if (skillStat >= emote.min && skillStat <= emote.max)
					{
						success = true;
					}
				}

				if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
					break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
				ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
			}

			break;
		}
	case InqRawSkillStat_EmoteType:
		{
			if (!pWeenie->m_Qualities._emote_table)
				break;

			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = false;
				bool hasQuality = false;

				DWORD skillStat;
				if (target->m_Qualities.InqSkill((STypeSkill)emote.stat, skillStat, TRUE))
				{
					hasQuality = true;
					if (skillStat >= emote.min && skillStat <= emote.max)
					{
						success = true;
					}
				}

				if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
					break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
				ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
			}

			break;
		}
	case InqQuestSolves_EmoteType:
		{
			if (!pWeenie->m_Qualities._emote_table)
				break;

			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				bool success = false;

				int intQuestSolves = target->InqQuestSolves(emote.msg.c_str());

				if (intQuestSolves >= emote.min && intQuestSolves <= emote.max)
					success = true;

				ChanceExecuteEmoteSet(success ? QuestSuccess_EmoteCategory : QuestFailure_EmoteCategory, emote.msg, target_id);
			}

			break;
		}
	case SetIntStat_EmoteType:
		{
			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
				target->m_Qualities.SetInt((STypeInt)emote.stat, emote.amount);
			break;
		}
	case IncrementIntStat_EmoteType:
		{
			std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
			if (target)
			{
				int intStat = target->InqIntQuality((STypeInt)emote.stat, 0, TRUE) + 1;
				target->m_Qualities.SetInt((STypeInt)emote.stat, intStat);
			}
			break;
		}
	case DecrementIntStat_EmoteType:
	{
		std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
		if (target)
		{
			int intStat = target->InqIntQuality((STypeInt)emote.stat, 0, TRUE) - 1;
			target->m_Qualities.SetInt((STypeInt)emote.stat, intStat);
		}
		break;
	}
	case CreateTreasure_EmoteType:
	{
		std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
		if (target && target->AsContainer())
			target->AsContainer()->SpawnTreasureInContainer((eTreasureCategory)emote.treasure_type, emote.wealth_rating, emote.treasure_class);
		break;
	}
	case ResetHomePosition_EmoteType:
	{
		std::shared_ptr<CMonsterWeenie> monster = pWeenie->AsMonster();
		if (monster && monster->m_MonsterAI)
			monster->m_MonsterAI->SetHomePosition(monster->m_Position);

		pWeenie->SetInitialPosition(pWeenie->m_Position);
		break;
	}
	case MoveHome_EmoteType:
	{
		std::shared_ptr<CMonsterWeenie> monster = pWeenie->AsMonster();
		if (monster && monster->m_MonsterAI)
			monster->m_MonsterAI->SwitchState(ReturningToSpawn);
		else
		{
			//Position destination;
			//pWeenie->m_Qualities.InqPosition(INSTANTIATION_POSITION, destination);

			//if (destination.objcell_id)
			//{
			//	MovementParameters params;
			//	params.desired_heading = destination.frame.get_heading();

			//	MovementStruct mvs;
			//	mvs.type = MovementTypes::MoveToPosition;
			//	mvs.pos = destination;
			//	mvs.params = &params;

			//	pWeenie->last_move_was_autonomous = false;
			//	pWeenie->movement_manager->PerformMovement(mvs);
			//}

			//todo: make creatures that do not normally have an AI(vendors/etc) move home.
			//Position initialPos;
			//if (pWeenie->m_Qualities.InqPosition(INSTANTIATION_POSITION, initialPos) && initialPos.objcell_id)
			//{
			//	pWeenie->Movement_Teleport(initialPos, false);
			//}
		}
		break;
	}
	case Move_EmoteType:
	{
		//Position destination = pWeenie->m_Position.add_offset(emote.frame.m_origin);
		//destination.frame.m_origin.z = CalcSurfaceZ(destination.objcell_id, destination.frame.m_origin.x, destination.frame.m_origin.y);

		//MovementParameters params;
		//params.desired_heading = emote.frame.get_heading();

		//MovementStruct mvs;
		//mvs.type = MovementTypes::MoveToPosition;
		//mvs.pos = destination;
		//mvs.params = &params;

		//pWeenie->last_move_was_autonomous = false;
		//pWeenie->movement_manager->PerformMovement(mvs);

		break;
	}
	case SetSanctuaryPosition_EmoteType:
	{
		std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
		if (target)
		{
			target->SetInitialPosition(target->m_Position);
			target->m_Qualities.SetPosition(SANCTUARY_POSITION, target->m_Position);
		}
		break;
	}
	case InqInt64Stat_EmoteType:
	{
		if (!pWeenie->m_Qualities._emote_table)
			break;

		std::shared_ptr<CWeenieObject> target = g_pWorld->FindObject(target_id);
		if (target)
		{
			bool success = false;
			bool hasQuality = false;

			long long intStat64;
			if (target->m_Qualities.InqInt64((STypeInt64)emote.stat, intStat64))
			{
				hasQuality = true;
				if (intStat64 >= emote.min64 && intStat64 <= emote.max64)
				{
					success = true;
				}
			}

			if (!hasQuality && ChanceExecuteEmoteSet(TestNoQuality_EmoteCategory, emote.msg, target_id))
				break; //if we have a TestNoQuality_EmoteCategory break otherwise try the categories below.
			ChanceExecuteEmoteSet(success ? TestSuccess_EmoteCategory : TestFailure_EmoteCategory, emote.msg, target_id);
		}

		break;
	}
	}
}

bool EmoteManager::IsExecutingAlready()
{
	return !_emoteQueue.empty();
}

void EmoteManager::Tick()
{
	if (_emoteQueue.empty())
		return;

	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}

	for (std::list<QueuedEmote>::iterator i = _emoteQueue.begin(); i != _emoteQueue.end();)
	{
		if (i->_executeTime > Timer::cur_time || pWeenie->IsBusyOrInAction() || pWeenie->IsMovingTo())
			break;

		ExecuteEmote(i->_data, i->_target_id);
		i = _emoteQueue.erase(i);
		if (i != _emoteQueue.end())
			i->_executeTime = Timer::cur_time + i->_data.delay;
	}
}

void EmoteManager::Cancel()
{
	_emoteQueue.clear();
}

void EmoteManager::OnDeath(DWORD killer_id)
{
	std::shared_ptr<CWeenieObject> pWeenie = _weenie.lock();
	if (!pWeenie)
	{
		return;
	}

	Cancel();

	if (pWeenie->m_Qualities._emote_table)
		ChanceExecuteEmoteSet(Death_EmoteCategory, killer_id);
}


