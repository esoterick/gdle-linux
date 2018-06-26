
#include "StdAfx.h"

#include "Client.h"
#include "ClientCommands.h"
#include "ClientEvents.h"
#include "World.h"

#include "Database.h"
#include "DatabaseIO.h"
#include "Database2.h"

#include "ChatMsgs.h"
#include "ObjectMsgs.h"

#include "WeenieObject.h"
#include "Monster.h"
#include "Player.h"
#include "ChatMsgs.h"
#include "Movement.h"
#include "MovementManager.h"
#include "Vendor.h"
#include "AllegianceManager.h"
#include "House.h"
#include "SpellcastingManager.h"
#include "TradeManager.h"

#include "Config.h"

CClientEvents::CClientEvents(CClient *parent)
{
	m_pClient = parent;
	m_pPlayer = std::shared_ptr<CPlayerWeenie>();
}

CClientEvents::~CClientEvents()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();

	if (pPlayer)
	{
		pPlayer->BeginLogout();
		pPlayer->DetachClient();

		DetachPlayer();
	}
}

void CClientEvents::DetachPlayer()
{
	m_pPlayer = std::shared_ptr<CPlayerWeenie>();
}

DWORD CClientEvents::GetPlayerID()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();

	if (!pPlayer)
		return 0;

	return pPlayer->GetID();
}

std::shared_ptr<CPlayerWeenie> CClientEvents::GetPlayer()
{
	return m_pPlayer.lock();
}

void CClientEvents::ExitWorld()
{
	DetachPlayer();
	m_pClient->ExitWorld();
}

void CClientEvents::Think()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();

	if (pPlayer)
	{
		if (m_bSendAllegianceUpdates)
		{
			if (m_fNextAllegianceUpdate <= g_pGlobals->Time())
			{
				SendAllegianceUpdate();
				m_fNextAllegianceUpdate = g_pGlobals->Time() + 5.0f;

				if (!m_bSentFirstAllegianceUpdate)
				{
					m_bSendAllegianceUpdates = FALSE;
					m_bSentFirstAllegianceUpdate = TRUE;
				}
			}
		}
	}
}

void CClientEvents::BeginLogout()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (pPlayer && !pPlayer->IsLoggingOut())
	{
		if (pPlayer->IsBusyOrInAction())
		{
			pPlayer->NotifyWeenieError(WERROR_ACTIONS_LOCKED);
			return;
		}

		pPlayer->BeginLogout();
	}
}

void CClientEvents::ForceLogout()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (pPlayer && !pPlayer->IsLoggingOut())
	{
		pPlayer->BeginLogout();
	}
}

void CClientEvents::OnLogoutCompleted()
{
	ExitWorld();
}

void CClientEvents::LoginError(int iError)
{
	DWORD ErrorPackage[2];

	ErrorPackage[0] = 0xF659;
	ErrorPackage[1] = iError;

	m_pClient->SendNetMessage(ErrorPackage, sizeof(ErrorPackage), PRIVATE_MSG);
}

void CClientEvents::LoginCharacter(DWORD char_weenie_id, const char *szAccount)
{
	

	if (!m_pClient->HasCharacter(char_weenie_id))
	{
		LoginError(13); // update error codes
		SERVER_WARN << szAccount << "attempting to log in with a character that doesn't belong to this account";
		return;
	}

	if (m_pPlayer.lock() || g_pWorld->FindPlayer(char_weenie_id))
	{
		// LOG(Temp, Normal, "Character already logged in!\n");
		LoginError(13); // update error codes
		SERVER_WARN << szAccount << "Login request, but character already logged in!";
		return;
	}

	/*
	if (_stricmp(szAccount, m_pClient->GetAccountInfo().username))
	{
		LoginError(15);
		LOG(Client, Warning, "Bad account for login: \"%s\" \"%s\"\n", szAccount, m_pClient->GetAccountInfo().username);
		return;
	}
	*/

	std::shared_ptr<CPlayerWeenie> pPlayer = (new CPlayerWeenie(m_pClient, char_weenie_id, m_pClient->IncCharacterInstanceTS(char_weenie_id)))->GetPointer(true)->AsPlayer();;

	m_pPlayer = pPlayer;

	if (!pPlayer->Load())
	{
		LoginError(13); // update error codes
		SERVER_WARN << szAccount << "Login request, but character failed to load!";

		return;
	}

	pPlayer->SetLoginPlayerQualities(); // overrides
	pPlayer->RecalculateEncumbrance();
	pPlayer->LoginCharacter();

	//temporarily send a purge all enchantments packet on login to wipe stacked characters.
	if (pPlayer->m_Qualities._enchantment_reg)
	{
		PackableList<DWORD> removed;

		if (pPlayer->m_Qualities._enchantment_reg->_add_list)
		{
			for (auto it = pPlayer->m_Qualities._enchantment_reg->_add_list->begin(); it != pPlayer->m_Qualities._enchantment_reg->_add_list->end();)
			{
				if (it->_duration == -1.0)
				{
					removed.push_back(it->_id);
					it = pPlayer->m_Qualities._enchantment_reg->_add_list->erase(it);
				}
				else
				{
					it++;
				}
			}
		}

		if (pPlayer->m_Qualities._enchantment_reg->_mult_list)
		{
			for (auto it = pPlayer->m_Qualities._enchantment_reg->_mult_list->begin(); it != pPlayer->m_Qualities._enchantment_reg->_mult_list->end();)
			{
				if (it->_duration == -1.0)
				{
					removed.push_back(it->_id);
					it = pPlayer->m_Qualities._enchantment_reg->_mult_list->erase(it);
				}
				else
				{
					it++;
				}
			}
		}

		if (removed.size())
		{
			// m_Qualities._enchantment_reg->PurgeEnchantments();

			BinaryWriter expireMessage;
			expireMessage.Write<DWORD>(0x2C8);
			removed.Pack(&expireMessage);

			m_pClient->SendNetMessage(&expireMessage, PRIVATE_MSG, TRUE, FALSE);
		}
	}

	for (auto wielded : pPlayer->m_Wielded)
	{
		std::shared_ptr<CWeenieObject> pWielded = wielded.lock();
		// Updates shields to have SHIELD_VALUE_INT
		if(pWielded->InqIntQuality(ITEM_TYPE_INT, 0) == TYPE_ARMOR && pWielded->InqIntQuality(LOCATIONS_INT, 0) == SHIELD_LOC)
		{
			pWielded->m_Qualities.SetInt(SHIELD_VALUE_INT, pWielded->InqIntQuality(ARMOR_LEVEL_INT, 0));
		}

		// Weeping wand nerf
		if (pWielded->m_Qualities.m_WeenieType == 35 && pWielded->InqStringQuality(NAME_STRING, "") == "Weeping Wand"
			&& (pWielded->InqDIDQuality(SPELL_DID, 0) > 0 || pWielded->InqFloatQuality(SLAYER_DAMAGE_BONUS_FLOAT, 0) != 1.4))
		{
			pWielded->m_Qualities.RemoveDataID(SPELL_DID);
			pWielded->m_Qualities.SetFloat(SLAYER_DAMAGE_BONUS_FLOAT, 1.4);
		}

	}

	for (auto item : pPlayer->m_Items)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		// Updates shields to have SHIELD_VALUE_INT
		if (pItem->InqIntQuality(ITEM_TYPE_INT, 0) == TYPE_ARMOR && pItem->InqIntQuality(LOCATIONS_INT, 0) == SHIELD_LOC)
		{
			pItem->m_Qualities.SetInt(SHIELD_VALUE_INT, pItem->InqIntQuality(ARMOR_LEVEL_INT, 0));
		}

		// Weeping wand nerf
		if (pItem->m_Qualities.m_WeenieType == 35 && pItem->InqStringQuality(NAME_STRING, "") == "Weeping Wand"
			&& (pItem->InqDIDQuality(SPELL_DID, 0) > 0 || pItem->InqFloatQuality(SLAYER_DAMAGE_BONUS_FLOAT, 0) != 1.4))
		{
			pItem->m_Qualities.RemoveDataID(SPELL_DID);
			pItem->m_Qualities.SetFloat(SLAYER_DAMAGE_BONUS_FLOAT, 1.4);
		}
	}

	for (auto pack : pPlayer->m_Packs)
	{
		std::shared_ptr<CWeenieObject> pPack = pack.lock();
		
		if (pPack->m_Qualities.id != W_PACKCREATUREESSENCE_CLASS && pPack->m_Qualities.id != W_PACKITEMESSENCE_CLASS && pPack->m_Qualities.id != W_PACKLIFEESSENCE_CLASS &&
			pPack->m_Qualities.id != W_PACKWARESSENCE_CLASS )
		{
			
			for (auto item : pPack->AsContainer()->m_Items)
			{
				std::shared_ptr<CWeenieObject> pItem = item.lock();

				// Updates shields to have SHIELD_VALUE_INT
				if (pItem->InqIntQuality(ITEM_TYPE_INT, 0) == TYPE_ARMOR && pItem->InqIntQuality(LOCATIONS_INT, 0) == SHIELD_LOC)
				{
					pItem->m_Qualities.SetInt(SHIELD_VALUE_INT, pItem->InqIntQuality(ARMOR_LEVEL_INT, 0));
				}

				// Weeping wand nerf
				if (pItem->m_Qualities.m_WeenieType == 35 && pItem->InqStringQuality(NAME_STRING, "") == "Weeping Wand"
					&& (pItem->InqDIDQuality(SPELL_DID, 0) > 0 || pItem->InqFloatQuality(SLAYER_DAMAGE_BONUS_FLOAT, 0) != 1.4))
				{
					pItem->m_Qualities.RemoveDataID(SPELL_DID);
					pItem->m_Qualities.SetFloat(SLAYER_DAMAGE_BONUS_FLOAT, 1.4);
				}
			}



		}
	}

	pPlayer->SendText("GDLEnhanced " SERVER_VERSION_NUMBER_STRING " " SERVER_VERSION_STRING, LTT_DEFAULT);
	pPlayer->SendText("Maintained by the GDLE Development Team. Contact us at https://discord.gg/WzGX348", LTT_DEFAULT);
	pPlayer->SendText("Powered by GamesDeadLol. Not an official Asheron's Call server.", LTT_DEFAULT);

	/*
	if (*g_pConfig->WelcomeMessage() != 0)
	{
		pPlayer->SendText(g_pConfig->WelcomeMessage(), LTT_DEFAULT);
	}
	*/

	g_pWorld->CreateEntity(pPlayer);

	//temporarily add all enchantments back from the character's wielded items
	for (auto item : pPlayer->m_Wielded)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (pItem->m_Qualities._spell_book)
		{
			bool bShouldCast = true;

			std::string name;
			if (pItem->m_Qualities.InqString(CRAFTSMAN_NAME_STRING, name))
			{
				if (!name.empty() && name != pItem->InqStringQuality(NAME_STRING, ""))
				{
					bShouldCast = false;

					pPlayer->NotifyWeenieErrorWithString(WERROR_ACTIVATION_NOT_CRAFTSMAN, name.c_str());
				}
			}

			int difficulty;
			difficulty = 0;
			if (pItem->m_Qualities.InqInt(ITEM_DIFFICULTY_INT, difficulty, TRUE, FALSE))
			{
				DWORD skillLevel = 0;
				if (!pPlayer->m_Qualities.InqSkill(ARCANE_LORE_SKILL, skillLevel, FALSE) || (int)skillLevel < difficulty)
				{
					bShouldCast = false;

					pPlayer->NotifyWeenieError(WERROR_ACTIVATION_ARCANE_LORE_TOO_LOW);
				}
			}

			if (bShouldCast)
			{
				difficulty = 0;
				DWORD skillActivationTypeDID = 0;

				if (pItem->m_Qualities.InqInt(ITEM_SKILL_LEVEL_LIMIT_INT, difficulty, TRUE, FALSE) && pItem->m_Qualities.InqDataID(ITEM_SKILL_LIMIT_DID, skillActivationTypeDID))
				{
					STypeSkill skillActivationType = SkillTable::OldToNewSkill((STypeSkill)skillActivationTypeDID);

					DWORD skillLevel = 0;
					if (!pPlayer->m_Qualities.InqSkill(skillActivationType, skillLevel, FALSE) || (int)skillLevel < difficulty)
					{
						bShouldCast = false;

						pPlayer->NotifyWeenieErrorWithString(WERROR_ACTIVATION_SKILL_TOO_LOW, CachedSkillTable->GetSkillName(skillActivationType).c_str());
					}
				}
			}

			if (bShouldCast && pItem->InqIntQuality(ITEM_ALLEGIANCE_RANK_LIMIT_INT, 0) > pItem->InqIntQuality(ALLEGIANCE_RANK_INT, 0))
			{
				bShouldCast = false;
				pPlayer->NotifyInventoryFailedEvent(pItem->GetID(), WERROR_ACTIVATION_RANK_TOO_LOW);
			}

			if (bShouldCast)
			{
				int heritageRequirement = pItem->InqIntQuality(HERITAGE_GROUP_INT, -1);
				if (heritageRequirement != -1 && heritageRequirement != pItem->InqIntQuality(HERITAGE_GROUP_INT, 0))
				{
					bShouldCast = false;
					std::string heritageString = pItem->InqStringQuality(ITEM_HERITAGE_GROUP_RESTRICTION_STRING, "of the correct heritage");
					pPlayer->NotifyWeenieErrorWithString(WERROR_ACTIVATION_WRONG_RACE, heritageString.c_str());
				}
			}

			int currentMana = 0;
			if (bShouldCast && pItem->m_Qualities.InqInt(ITEM_CUR_MANA_INT, currentMana, TRUE, FALSE))
			{
				if (currentMana == 0)
				{
					bShouldCast = false;
					pPlayer->NotifyWeenieError(WERROR_ACTIVATION_NOT_ENOUGH_MANA);
				}
				else
					pItem->_nextManaUse = Timer::cur_time;
			}

			if (bShouldCast)
			{
				DWORD serial = 0;
				serial |= ((DWORD)pPlayer->GetEnchantmentSerialByteForMask(pItem->InqIntQuality(LOCATIONS_INT, 0, TRUE)) << (DWORD)0);
				serial |= ((DWORD)pPlayer->GetEnchantmentSerialByteForMask(pItem->InqIntQuality(CLOTHING_PRIORITY_INT, 0, TRUE)) << (DWORD)8);

				for (auto &spellPage : pItem->m_Qualities._spell_book->_spellbook)
				{
					pItem->MakeSpellcastingManager()->CastSpellEquipped(pPlayer->GetID(), spellPage.first, (WORD)serial);
				}
			}
		}
	}
	pPlayer->DebugValidate();

	return;
}

void CClientEvents::SendText(const char *szText, long lColor)
{
	m_pClient->SendNetMessage(ServerText(szText, lColor), PRIVATE_MSG, FALSE, TRUE);
}

void CClientEvents::Attack(DWORD target, DWORD height, float power)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (height <= 0 || height >= ATTACK_HEIGHT::NUM_ATTACK_HEIGHTS)
	{
		SERVER_WARN << "Bad melee attack height" << height << "sent by player"<< pPlayer->GetID();
		return;
	}

	if (power < 0.0f || power > 1.0f)
	{
		SERVER_WARN << "Bad melee attack power" << power << "sent by player" << pPlayer->GetID();
		return;
	}

	pPlayer->TryMeleeAttack(target, (ATTACK_HEIGHT) height, power);
}

void CClientEvents::MissileAttack(DWORD target, DWORD height, float power)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (height <= 0 || height >= ATTACK_HEIGHT::NUM_ATTACK_HEIGHTS)
	{
		SERVER_WARN << "Bad missile attack height" << height << "sent by player" << pPlayer->GetID();
		return;
	}

	if (power < 0.0f || power > 1.0f)
	{
		SERVER_WARN << "Bad missile attack power" << power << "sent by player" << pPlayer->GetID();
		return;
	}

	pPlayer->TryMissileAttack(target, (ATTACK_HEIGHT)height, power);
}

void CClientEvents::SendTellByGUID(const char* szText, DWORD dwGUID)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (strlen(szText) > 300)
		return;

	//should really check for invalid characters and such ;]

	while (szText[0] == ' ') //Skip leading spaces.
		szText++;
	if (szText[0] == '\0') //Make sure the text isn't blank
		return;

	/*
	if (dwGUID == pPlayer->GetID())
	{
		pPlayer->SendNetMessage(ServerText("You really need some new friends..", 1), PRIVATE_MSG, FALSE);
		return;
	}
	*/

	std::shared_ptr<CPlayerWeenie> pTarget;

	if (!(pTarget = g_pWorld->FindPlayer(dwGUID)))
		return;

	if (pTarget->GetID() != pPlayer->GetID())
	{
		char szResponse[300];
		_snprintf(szResponse, 300, "You tell %s, \"%s\"", pTarget->GetName().c_str(), szText);
		pPlayer->SendNetMessage(ServerText(szResponse, 4), PRIVATE_MSG, FALSE, TRUE);
	}

	pTarget->SendNetMessage(DirectChat(szText, pPlayer->GetName().c_str(), pPlayer->GetID(), pTarget->GetID(), 3), PRIVATE_MSG, TRUE);
}

void CClientEvents::SendTellByName(const char* szText, const char* szName)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (strlen(szName) > 300)
		return;
	if (strlen(szText) > 300)
		return;

	//should really check for invalid characters and such ;]

	while (szText[0] == ' ') //Skip leading spaces.
		szText++;
	if (szText[0] == '\0') //Make sure the text isn't blank
		return;

	std::shared_ptr<CPlayerWeenie> pTarget;

	if (!(pTarget = g_pWorld->FindPlayer(szName)))
		return;

	if (pTarget->GetID() != pPlayer->GetID())
	{
		char szResponse[300];
		_snprintf(szResponse, 300, "You tell %s, \"%s\"", pTarget->GetName().c_str(), szText);
		pPlayer->SendNetMessage(ServerText(szResponse, 4), PRIVATE_MSG, FALSE, TRUE);
	}

	pTarget->SendNetMessage(DirectChat(szText, pPlayer->GetName().c_str(), pPlayer->GetID(), pTarget->GetID(), 3), PRIVATE_MSG, TRUE);
}

void CClientEvents::ClientText(const char *szText)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (strlen(szText) > 500)
		return;

	//should really check for invalid characters and such ;]

	while (szText[0] == ' ') //Skip leading spaces.
		szText++;
	if (szText[0] == '\0') //Make sure the text isn't blank
		return;

	std::string filteredText = m_pClient->GetAccessLevel() >= SENTINEL_ACCESS ? szText : FilterBadChatCharacters(szText);
	szText = filteredText.c_str(); // hacky

	if (szText[0] == '!' || szText[0] == '@' || szText[0] == '/')
	{
		CommandBase::Execute((char *) (++szText), m_pClient);
	}
	else
	{
		if (CheckForChatSpam())
		{
			pPlayer->SpeakLocal(szText);
		}
	}
}

void CClientEvents::EmoteText(const char* szText)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (strlen(szText) > 300)
		return;

	//TODO: Check for invalid characters and such ;)

	while (szText[0] == ' ') //Skip leading spaces.
		szText++;
	if (szText[0] == '\0') //Make sure the text isn't blank
		return;

	pPlayer->EmoteLocal(szText);
}

void CClientEvents::ActionText(const char *text)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (strlen(text) > 300)
		return;
	
	while (text[0] == ' ') //Skip leading spaces.
		text++;
	if (text[0] == '\0') //Make sure the text isn't blank
		return;

	pPlayer->ActionLocal(text);
}

void CClientEvents::ChannelText(DWORD channel_id, const char *text)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (strlen(text) > 300)
		return;

	// TODO: check for invalid characters and such
	while (text[0] == ' ')
		text++;
	if (text[0] == '\0')
		return;

	switch (channel_id)
	{
	case Fellow_ChannelID:
		{
			std::string fellowName;
			if (!pPlayer->m_Qualities.InqString(FELLOWSHIP_STRING, fellowName))
				return;

			g_pFellowshipManager->Chat(fellowName, pPlayer->GetID(), text);
			CHAT_LOG << pPlayer->GetName().c_str() << "says (fellowship)," << text;
			break;
		}

	case Patron_ChannelID:
		g_pAllegianceManager->ChatPatron(pPlayer->GetID(), text);
		CHAT_LOG << pPlayer->GetName().c_str() << "says (patron)," << text;
		break;

	case Vassals_ChannelID:
		g_pAllegianceManager->ChatVassals(pPlayer->GetID(), text);
		CHAT_LOG << pPlayer->GetName().c_str() << "says (vassals)," << text;
		break;

	case Covassals_ChannelID:
		g_pAllegianceManager->ChatCovassals(pPlayer->GetID(), text);
		CHAT_LOG << pPlayer->GetName().c_str() << "says (covassals)," << text;
		break;

	case Monarch_ChannelID:
		g_pAllegianceManager->ChatMonarch(pPlayer->GetID(), text);
		CHAT_LOG << pPlayer->GetName().c_str() << "says (monarch)," << text;
		break;
	}
}

void CClientEvents::RequestHealthUpdate(DWORD dwGUID)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	std::shared_ptr<CWeenieObject> pEntity = g_pWorld->FindWithinPVS(pPlayer, dwGUID);

	if (pEntity)
	{
		if (std::shared_ptr<CMonsterWeenie> pMonster = pEntity->AsMonster())
		{
			pPlayer->SetLastHealthRequest(pEntity->GetID());

			m_pClient->SendNetMessage(HealthUpdate(pMonster), PRIVATE_MSG, TRUE, TRUE);
		}
	}
}

void CClientEvents::ChangeCombatStance(COMBAT_MODE mode)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	pPlayer->ChangeCombatMode(mode, true);
	// ActionComplete();
}

void CClientEvents::ExitPortal()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	pPlayer->ExitPortal();
}

void CClientEvents::Ping()
{
	// Pong!
	DWORD Pong = 0x1EA;
	m_pClient->SendNetMessage(&Pong, sizeof(Pong), PRIVATE_MSG, TRUE);
}

void CClientEvents::UseItemEx(DWORD dwSourceID, DWORD dwDestID)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	std::shared_ptr<CWeenieObject> pSource = g_pWorld->FindWithinPVS(pPlayer, dwSourceID);
	std::shared_ptr<CWeenieObject> pDest = g_pWorld->FindWithinPVS(pPlayer, dwDestID);

	if (pSource && pSource->AsCaster())
		pPlayer->ExecuteUseEvent(new CWandSpellUseEvent(dwSourceID, pDest ? dwDestID : dwSourceID));
	else if (pSource && pDest)
		pSource->UseWith(pPlayer, pDest);
	else
		ActionComplete();
}

void CClientEvents::UseObject(DWORD dwEID)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (pPlayer->IsBusyOrInAction())
	{
		ActionComplete(WERROR_ACTIONS_LOCKED);
		return;
	}

	std::shared_ptr<CWeenieObject> pTarget = g_pWorld->FindWithinPVS(pPlayer, dwEID);

	if (pTarget)
	{
		if (pTarget->AsContainer() && pTarget->AsContainer()->_openedById == pPlayer->GetID())
		{
			//we're closing a chest
			pTarget->AsContainer()->OnContainerClosed(pPlayer);
			ActionComplete();
			return;
		}

		int error = pTarget->UseChecked(pPlayer);

		if (error != WERROR_NONE)
			ActionComplete(error);
	}
	else
	{
		ActionComplete(WERROR_OBJECT_GONE);
	}
}

void CClientEvents::ActionComplete(int error)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	pPlayer->NotifyUseDone(error);
}

void CClientEvents::Identify(DWORD target_id)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (_next_allowed_identify > Timer::cur_time)
	{
		// do not allow to ID too fast
		return;
	}

	/*
	std::shared_ptr<CWeenieObject> pTarget = g_pWorld->FindWithinPVS(pPlayer, target_id);

	if (!pTarget)
	{
		// used to check for vendor items, temporary, should be changed
		pTarget = g_pWorld->FindObject(target_id);
	}
	*/

	std::shared_ptr<CWeenieObject> pTarget = g_pWorld->FindObject(target_id);

#ifndef PUBLIC_BUILD
	if (pTarget)
#else
	int vis = 0;
	if (pTarget && !pTarget->m_Qualities.InqBool(VISIBILITY_BOOL, vis))
#endif
	{
		pTarget->TryIdentify(pPlayer);
		pPlayer->SetLastAssessed(pTarget->GetID());
	}	

	_next_allowed_identify = Timer::cur_time + 0.5;
}

void CClientEvents::SpendAttributeXP(STypeAttribute key, DWORD exp)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	// TODO use attribute map
	if (key < 1 || key > 6)
		return;

	// TODO verify they are trying to spend the correct amount of XP

	__int64 unassignedExp = 0;
	pPlayer->m_Qualities.InqInt64(AVAILABLE_EXPERIENCE_INT64, unassignedExp);

	if ((unsigned __int64)unassignedExp < (unsigned __int64)exp)
	{
		// Not enough experience
		return;
	}

	Attribute attr;
	if (!pPlayer->m_Qualities.InqAttribute(key, attr))
	{
		// Doesn't have the attribute
		return;
	}

	const DWORD amountNeededForMaxXp = attr.GetXpNeededForMaxXp();

	exp = min(exp, amountNeededForMaxXp);

	if (exp > 0)
	{
		pPlayer->GiveAttributeXP(key, exp);
		pPlayer->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, (unsigned __int64)unassignedExp - (unsigned __int64)exp);

		pPlayer->NotifyAttributeStatUpdated(key);
		pPlayer->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
	}
}

void CClientEvents::SpendAttribute2ndXP(STypeAttribute2nd key, DWORD exp)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	// TODO use vital map
	if (key != 1 && key != 3 && key != 5)
		return;

	// TODO verify they are trying to spend the correct amount of XP

	__int64 unassignedExp = 0;
	pPlayer->m_Qualities.InqInt64(AVAILABLE_EXPERIENCE_INT64, unassignedExp);
	if ((unsigned __int64)unassignedExp < (unsigned __int64)exp)
	{
		// Not enough experience
		return;
	}

	SecondaryAttribute attr;
	if (!pPlayer->m_Qualities.InqAttribute2nd(key, attr))
	{
		// Doesn't have the secondary attribute
		return;
	}

	const DWORD amountNeededForMaxXp = attr.GetXpNeededForMaxXp();

	// If the exp is more than is needed to reach max, it is limited to the amount needed to reach max
	// This is done as the client may send more than the amount needed if it is desynced
	exp = min(exp, amountNeededForMaxXp);

	if (exp > 0)
	{
		pPlayer->GiveAttribute2ndXP(key, exp);
		pPlayer->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, (unsigned __int64)unassignedExp - (unsigned __int64)exp);
		pPlayer->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
	}
}

void CClientEvents::SpendSkillXP(STypeSkill key, DWORD exp)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	// TODO verify they are trying to spend the correct amount of XP

	__int64 unassignedExp = 0;
	pPlayer->m_Qualities.InqInt64(AVAILABLE_EXPERIENCE_INT64, unassignedExp);
	if ((unsigned __int64)unassignedExp < (unsigned __int64)exp)
	{
		// Not enough experience
		return;
	}

	Skill skill;
	if (!pPlayer->m_Qualities.InqSkill(key, skill) || skill._sac < TRAINED_SKILL_ADVANCEMENT_CLASS)
	{
		// Skill doesn't exist or isn't trained
		return;
	}

	const DWORD amountNeededForMaxXp = skill.GetXpNeededForMaxXp();

	// If the exp is more than is needed to reach max, it is limited to the amount needed to reach max
	// This is done as the client may send more than the amount needed if it is desynced
	exp = min(exp, amountNeededForMaxXp);


	if (exp > 0)
	{
		// Only give the skill exp if it's not maxed
		pPlayer->GiveSkillXP(key, exp);

		pPlayer->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, (unsigned __int64)unassignedExp - (unsigned __int64)exp);
		pPlayer->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
	}
}

void CClientEvents::SpendSkillCredits(STypeSkill key, DWORD credits)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	// TODO verify they are trying to spend the correct amount of XP

	DWORD unassignedCredits = 0;
	pPlayer->m_Qualities.InqInt(AVAILABLE_SKILL_CREDITS_INT, *(int *)&unassignedCredits);
	if (unassignedCredits < credits)
	{
		// Not enough experience
		return;
	}

	Skill skill;
	if (!pPlayer->m_Qualities.InqSkill(key, skill) || skill._sac >= TRAINED_SKILL_ADVANCEMENT_CLASS)
	{
		// Skill doesn't exist or already trained
		return;
	}

	DWORD costToRaise = pPlayer->GetCostToRaiseSkill(key);

	if (pPlayer->GetCostToRaiseSkill(key) != credits)
	{
		SERVER_WARN << pPlayer->GetName() << "- Credit cost to raise skill does not match what player is trying to spend.";
		return;
	}

	pPlayer->GiveSkillAdvancementClass(key, TRAINED_SKILL_ADVANCEMENT_CLASS);
	pPlayer->m_Qualities.SetSkillLevel(key, 5);
	pPlayer->NotifySkillStatUpdated(key);

	pPlayer->m_Qualities.SetInt(AVAILABLE_SKILL_CREDITS_INT, unassignedCredits - costToRaise);
	pPlayer->NotifyIntStatUpdated(AVAILABLE_SKILL_CREDITS_INT);
}

void CClientEvents::LifestoneRecall()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if ( pPlayer->CheckPKActivity())
	{
		pPlayer->SendText("You have been involved in Player Killer combat too recently!", LTT_MAGIC);
		return;
	}

	Position lifestone;
	if (pPlayer->m_Qualities.InqPosition(SANCTUARY_POSITION, lifestone) && lifestone.objcell_id)
	{
		if (!pPlayer->IsBusyOrInAction())
		{
			pPlayer->ExecuteUseEvent(new CLifestoneRecallUseEvent());
		}
	}
	else
	{
		m_pClient->SendNetMessage(ServerText("You are not bound to a Lifestone!", 7), PRIVATE_MSG);
	}
}

void CClientEvents::MarketplaceRecall()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (pPlayer->CheckPKActivity())
	{
		pPlayer->SendText("You have been involved in Player Killer combat too recently!", LTT_MAGIC);
		return;
	}

	if (!pPlayer->IsBusyOrInAction())
	{
		pPlayer->ExecuteUseEvent(new CMarketplaceRecallUseEvent());
	}
}

void CClientEvents::TryInscribeItem(DWORD object_id, const std::string &text)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	std::shared_ptr<CWeenieObject> weenie = pPlayer->FindContainedItem(object_id);

	if (!weenie)
	{
		return;
	}

	if (!weenie->IsInscribable())
	{
		return;
	}

	weenie->m_Qualities.SetString(INSCRIPTION_STRING, text.length() <= 800 ? text : text.substr(0, 800));
	weenie->m_Qualities.SetString(SCRIBE_NAME_STRING, pPlayer->GetName());
}

void CClientEvents::TryBuyItems(DWORD vendor_id, std::list<class ItemProfile *> &items)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	std::shared_ptr<CWeenieObject> weenie = g_pWorld->FindWithinPVS(pPlayer, vendor_id);

	if (!weenie)
	{
		ActionComplete(WERROR_NO_OBJECT);
		return;
	}

	int error = WERROR_NO_OBJECT;
	if (std::shared_ptr<CVendor> pVendor = weenie->AsVendor())
	{
		error = pVendor->TrySellItemsToPlayer(pPlayer, items);
		pVendor->SendVendorInventory(pPlayer);
	}

	ActionComplete(error);
}


void CClientEvents::TrySellItems(DWORD vendor_id, std::list<class ItemProfile *> &items)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	std::shared_ptr<CWeenieObject> weenie = g_pWorld->FindWithinPVS(pPlayer, vendor_id);

	if (!weenie)
	{
		ActionComplete(WERROR_NO_OBJECT);
		return;
	}

	int error = WERROR_NO_OBJECT;
	if (std::shared_ptr<CVendor> pVendor = weenie->AsVendor())
	{
		error = pVendor->TryBuyItemsFromPlayer(pPlayer, items);
		pVendor->SendVendorInventory(pPlayer);
	}

	ActionComplete(error);
}

void CClientEvents::TryFellowshipCreate(const std::string name, int shareXP)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (pPlayer->HasFellowship())
		return;

	int error = g_pFellowshipManager->Create(name, pPlayer->GetID(), shareXP);

	if (error)
		pPlayer->NotifyWeenieError(error);
}

void CClientEvents::TryFellowshipQuit(int disband)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	std::string fellowshipName;
	if (!pPlayer->m_Qualities.InqString(FELLOWSHIP_STRING, fellowshipName))
		return;

	int error = 0;
	if (disband)
		error = g_pFellowshipManager->Disband(fellowshipName, pPlayer->GetID());
	else
		error = g_pFellowshipManager->Quit(fellowshipName, pPlayer->GetID());

	if (error)
		pPlayer->NotifyWeenieError(error);
}

void CClientEvents::TryFellowshipDismiss(DWORD dismissed)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	std::string fellowshipName;
	if (!pPlayer->m_Qualities.InqString(FELLOWSHIP_STRING, fellowshipName))
		return;

	int error = g_pFellowshipManager->Dismiss(fellowshipName, pPlayer->GetID(), dismissed);

	if (error)
		pPlayer->NotifyWeenieError(error);
}

void CClientEvents::TryFellowshipRecruit(DWORD target)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	std::string fellowshipName;
	if (!pPlayer->m_Qualities.InqString(FELLOWSHIP_STRING, fellowshipName))
		return;

	int error = g_pFellowshipManager->Recruit(fellowshipName, pPlayer->GetID(), target);

	if (error)
		pPlayer->NotifyWeenieError(error);
}

void CClientEvents::TryFellowshipUpdate(int on)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	std::string fellowshipName;
	if (!pPlayer->m_Qualities.InqString(FELLOWSHIP_STRING, fellowshipName))
		return;

	int error = g_pFellowshipManager->RequestUpdates(fellowshipName, pPlayer->GetID(), on);

	if (error)
		pPlayer->NotifyWeenieError(error);
}

void CClientEvents::TryFellowshipAssignNewLeader(DWORD target)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	std::string fellowshipName;
	if (!pPlayer->m_Qualities.InqString(FELLOWSHIP_STRING, fellowshipName))
		return;

	int error = g_pFellowshipManager->AssignNewLeader(fellowshipName, pPlayer->GetID(), target);

	if (error)
		pPlayer->NotifyWeenieError(error);
}

void CClientEvents::TryFellowshipChangeOpenness(int open)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	std::string fellowshipName;
	if (!pPlayer->m_Qualities.InqString(FELLOWSHIP_STRING, fellowshipName))
		return;

	int error = g_pFellowshipManager->ChangeOpen(fellowshipName, pPlayer->GetID(), open);

	if (error)
		pPlayer->NotifyWeenieError(error);
}

void CClientEvents::SendAllegianceUpdate()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (!pPlayer)
		return;

	g_pAllegianceManager->SendAllegianceProfile(pPlayer);
}

void CClientEvents::SendAllegianceMOTD()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	AllegianceTreeNode *self = g_pAllegianceManager->GetTreeNode(pPlayer->GetID());
	if (!self)
		return;

	AllegianceInfo *info = g_pAllegianceManager->GetInfo(self->_monarchID);
	if (!info)
		return;

	pPlayer->SendText(csprintf("\"%s\" -- %s", info->_info.m_motd.c_str(), info->_info.m_motdSetBy.c_str()), LTT_DEFAULT);
}

void CClientEvents::SetRequestAllegianceUpdate(int on)
{
	m_bSendAllegianceUpdates = on;
}

void CClientEvents::TryBreakAllegiance(DWORD target)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	int error = g_pAllegianceManager->TryBreakAllegiance(pPlayer, target);
	pPlayer->NotifyWeenieError(error);

	if (error == WERROR_NONE)
	{
		SendAllegianceUpdate();
	}
}

void CClientEvents::TrySwearAllegiance(DWORD target)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	std::shared_ptr<CPlayerWeenie> targetWeenie = g_pWorld->FindPlayer(target);
	if (!targetWeenie)
	{
		pPlayer->NotifyWeenieError(WERROR_NO_OBJECT);
		return;
	}
	
	int error = g_pAllegianceManager->TrySwearAllegiance(pPlayer, targetWeenie);	
	pPlayer->NotifyWeenieError(error);

	if (error == WERROR_NONE)
	{
		SendAllegianceUpdate();
	}
}

void CClientEvents::AllegianceInfoRequest(const std::string &target)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	AllegianceTreeNode *myNode = g_pAllegianceManager->GetTreeNode(pPlayer->GetID());
	if (!myNode)
	{
		pPlayer->SendText("You are not in an allegiance.", LTT_DEFAULT);
		return;
	}

	AllegianceTreeNode *myMonarchNode = g_pAllegianceManager->GetTreeNode(myNode->_monarchID);
	if (!myMonarchNode)
	{
		pPlayer->SendText("There was an error processing your request.", LTT_DEFAULT);
		return;
	}

	AllegianceTreeNode *myTargetNode = myMonarchNode->FindCharByNameRecursivelySlow(target);
	if (!myTargetNode)
	{
		pPlayer->SendText("Could not find allegiance member.", LTT_DEFAULT);
		return;
	}

	unsigned int rank = 0;
	if (AllegianceProfile *profile = g_pAllegianceManager->CreateAllegianceProfile(myTargetNode->_charID, &rank))
	{
		BinaryWriter allegianceUpdate;
		allegianceUpdate.Write<DWORD>(0x27C);
		allegianceUpdate.Write<DWORD>(myTargetNode->_charID);
		profile->Pack(&allegianceUpdate);
		pPlayer->SendNetMessage(&allegianceUpdate, PRIVATE_MSG, TRUE, FALSE);

		delete profile;
	}
	else
	{
		pPlayer->SendText("Error retrieving allegiance member information.", LTT_DEFAULT);
	}
}

void CClientEvents::TrySetAllegianceMOTD(const std::string &text)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	AllegianceTreeNode *self = g_pAllegianceManager->GetTreeNode(pPlayer->GetID());
	if (!self)
		return;

	if (self->_charID != self->_monarchID)
	{
		pPlayer->SendText("Only the monarch can set the Message of the Day.", LTT_DEFAULT);
		return;
	}

	AllegianceInfo *info = g_pAllegianceManager->GetInfo(self->_monarchID);
	if (!info)
		return;

	info->_info.m_motd = text;
	info->_info.m_motdSetBy = pPlayer->GetName();
	pPlayer->SendText(csprintf("MOTD changed to: \"%s\" -- %s", info->_info.m_motd.c_str(), info->_info.m_motdSetBy.c_str()), LTT_DEFAULT);
}

void CClientEvents::AllegianceHometownRecall()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (pPlayer->CheckPKActivity())
	{
		pPlayer->SendText("You have been involved in Player Killer combat too recently!", LTT_MAGIC);
		return;
	}

	AllegianceTreeNode *allegianceNode = g_pAllegianceManager->GetTreeNode(pPlayer->GetID());

	if (!allegianceNode)
	{
		pPlayer->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	AllegianceInfo *allegianceInfo = g_pAllegianceManager->GetInfo(allegianceNode->_monarchID);

	if (allegianceInfo && allegianceInfo->_info.m_BindPoint.objcell_id)
	{
		if (!pPlayer->IsBusyOrInAction())
			pPlayer->ExecuteUseEvent(new CAllegianceHometownRecallUseEvent());
	}
	else
		pPlayer->NotifyWeenieError(WERROR_ALLEGIANCE_HOMETOWN_NOT_SET);
}

void CClientEvents::HouseBuy(DWORD slumlord, const PackableList<DWORD> &items)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (std::shared_ptr<CWeenieObject> slumlordObj = g_pWorld->FindObject(slumlord))
	{
		if (pPlayer->DistanceTo(slumlordObj, true) > 10.0)
			return;

		if (std::shared_ptr<CSlumLordWeenie> slumlord = slumlordObj->AsSlumLord())
		{
			slumlord->BuyHouse(pPlayer, items);
		}
	}
}

void CClientEvents::HouseRent(DWORD slumlord, const PackableList<DWORD> &items)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (std::shared_ptr<CWeenieObject> slumlordObj = g_pWorld->FindObject(slumlord))
	{
		if (pPlayer->DistanceTo(slumlordObj, true) > 10.0)
			return;

		if (std::shared_ptr<CSlumLordWeenie> slumlord = slumlordObj->AsSlumLord())
		{
			slumlord->RentHouse(pPlayer, items);
		}
	}
}

void CClientEvents::HouseAbandon()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData && houseData->_ownerId == pPlayer->GetID())
		{
			houseData->AbandonHouse();
			pPlayer->SendText("You've abandoned your house.", LTT_DEFAULT); //todo: made up message.
		}
		else
			pPlayer->SendText("That house does not belong to you.", LTT_DEFAULT); //todo: made up message.
	}
	else
		m_pClient->SendNetMessage(ServerText("You do not have a house!", 7), PRIVATE_MSG);
}

void CClientEvents::HouseRecall()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (pPlayer->CheckPKActivity())
	{
		pPlayer->SendText("You have been involved in Player Killer combat too recently!", LTT_MAGIC);
		return;
	}

	DWORD houseId = pPlayer->GetAccountHouseId();
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerAccount == pPlayer->GetClient()->GetAccountInfo().id)
		{
			if (!pPlayer->IsBusyOrInAction())
				pPlayer->ExecuteUseEvent(new CHouseRecallUseEvent());
		}
		else
			pPlayer->SendText("That house does not belong to you.", LTT_DEFAULT); //todo: made up message.
	}
	else
		m_pClient->SendNetMessage(ServerText("You do not have a house!", 7), PRIVATE_MSG);
}

void CClientEvents::HouseMansionRecall()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (pPlayer->CheckPKActivity())
	{
		pPlayer->SendText("You have been involved in Player Killer combat too recently!", LTT_MAGIC);
		return;
	}

	AllegianceTreeNode *allegianceNode = g_pAllegianceManager->GetTreeNode(pPlayer->GetID());

	if (!allegianceNode)
	{
		pPlayer->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	DWORD allegianceHouseId;

	std::shared_ptr<CWeenieObject> monarch = g_pWorld->FindObject(allegianceNode->_monarchID);
	if (!monarch)
	{
		monarch = CWeenieObject::Load(allegianceNode->_monarchID);
		if (!monarch)
			return;
		allegianceHouseId = monarch->InqDIDQuality(HOUSEID_DID, 0);
	}
	else
		allegianceHouseId = monarch->InqDIDQuality(HOUSEID_DID, 0);

	if (allegianceHouseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(allegianceHouseId);
		if (houseData && houseData->_ownerId == allegianceNode->_monarchID && (houseData->_houseType == 2 || houseData->_houseType == 3)) //2 = villa, 3 = mansion
		{
			if (!pPlayer->IsBusyOrInAction())
				pPlayer->ExecuteUseEvent(new CMansionRecallUseEvent());
			return;
		}
	}

	if(allegianceNode->_patronID)
		m_pClient->SendNetMessage(ServerText("Your monarch does not own a mansion or villa!", 7), PRIVATE_MSG);
	else
		m_pClient->SendNetMessage(ServerText("You do not own a mansion or villa!", 7), PRIVATE_MSG);
}

void CClientEvents::HouseRequestData()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
		g_pHouseManager->SendHouseData(pPlayer, houseId);
	else
	{
		//if we can't get the data send the "no house" packet
		BinaryWriter noHouseData;
		noHouseData.Write<DWORD>(0x0226);
		noHouseData.Write<DWORD>(0);
		pPlayer->SendNetMessage(&noHouseData, PRIVATE_MSG, TRUE, FALSE);
	}
}

void CClientEvents::HouseToggleHooks(bool newSetting)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerId == pPlayer->GetID())
		{
			houseData->SetHookVisibility(newSetting);
			if (newSetting)
				pPlayer->SendText("Your dwelling's hooks are now visible.", LTT_DEFAULT); //todo: made up message.
			else
				pPlayer->SendText("Your dwelling's hooks are now hidden.", LTT_DEFAULT); //todo: made up message.
		}
		else
			pPlayer->SendText("Only the character who owns the house may use this command.", LTT_DEFAULT);
	}
	else
		pPlayer->SendText("You do not own a house.", LTT_DEFAULT);
}

void CClientEvents::HouseRequestAccessList()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerId == pPlayer->GetID())
		{
			pPlayer->SendText("Access:", LTT_DEFAULT);
			pPlayer->SendText(csprintf("   Public: %s", houseData->_everyoneAccess ? "Allow" : "Deny"), LTT_DEFAULT);
			pPlayer->SendText(csprintf("   Allegiance: %s", houseData->_allegianceAccess ? "Allow" : "Deny"), LTT_DEFAULT);

			pPlayer->SendText("Storage:", LTT_DEFAULT);
			pPlayer->SendText(csprintf("   Public: %s", houseData->_everyoneStorageAccess ? "Allow" : "Deny"), LTT_DEFAULT);
			pPlayer->SendText(csprintf("   Allegiance: %s", houseData->_allegianceStorageAccess ? "Allow" : "Deny"), LTT_DEFAULT);

			if (houseData->_accessList.empty())
				pPlayer->SendText("Your dwelling's acess list is empty.", LTT_DEFAULT);
			else
			{
				pPlayer->SendText("Access List:", LTT_DEFAULT);
				std::list<DWORD>::iterator i = houseData->_accessList.begin();
				while (i != houseData->_accessList.end())
				{
					std::string name = g_pWorld->GetPlayerName(*(i), true);
					if (!name.empty())
					{
						pPlayer->SendText(csprintf("   %s", name.c_str()), LTT_DEFAULT);
						i++;
					}
					else
						i = houseData->_accessList.erase(i); //no longer exists.
				}
			}

			if (houseData->_storageAccessList.empty())
				pPlayer->SendText("Your dwelling's storage access list is empty.", LTT_DEFAULT);
			else
			{
				pPlayer->SendText("Storage Access list:", LTT_DEFAULT);
				std::list<DWORD>::iterator i = houseData->_storageAccessList.begin();
				while (i != houseData->_storageAccessList.end())
				{
					std::string name = g_pWorld->GetPlayerName(*(i), true);
					if (!name.empty())
					{
						pPlayer->SendText(csprintf("   %s", name.c_str()), LTT_DEFAULT);
						i++;
					}
					else
						i = houseData->_storageAccessList.erase(i); //no longer exists.
				}
			}
		}
		else
			pPlayer->SendText("Only the character who owns the house may use this command.", LTT_DEFAULT);
	}
	else
		pPlayer->SendText("You do not own a house.", LTT_DEFAULT);
}

void CClientEvents::HouseAddPersonToAccessList(std::string name)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	DWORD targetId = g_pWorld->GetPlayerId(name.c_str(), true);
	if (!target)
	{
		pPlayer->SendText("Can't find a player by that name.", LTT_DEFAULT); //todo: made up message.
		return;
	}

	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerId == pPlayer->GetID())
		{
			if (houseData->_accessList.size() > 128)
			{
				pPlayer->SendText("The access list is full", LTT_DEFAULT); //todo: made up message.
				return;
			}
			if (std::find(houseData->_accessList.begin(), houseData->_accessList.end(), targetId) != houseData->_accessList.end())
			{
				pPlayer->SendText(csprintf("%s is already in the access list", name.c_str()), LTT_DEFAULT); //todo: made up message.
			}
			else
			{
				pPlayer->SendText(csprintf("You add %s to your dwelling's access list", name.c_str()), LTT_DEFAULT); //todo: made up message.
				houseData->_accessList.push_back(targetId);
			}
		}
		else
			pPlayer->SendText("Only the character who owns the house may use this command.", LTT_DEFAULT);
	}
	else
		pPlayer->SendText("You do not own a house.", LTT_DEFAULT);
}

void CClientEvents::HouseRemovePersonFromAccessList(std::string name)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	DWORD targetId = g_pWorld->GetPlayerId(name.c_str(), true);
	if (!target)
	{
		pPlayer->SendText("Can't find a player by that name.", LTT_DEFAULT); //todo: made up message.
		return;
	}

	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerId == pPlayer->GetID())
		{
			auto iter = std::find(houseData->_accessList.begin(), houseData->_accessList.end(), targetId);
			if (iter != houseData->_accessList.end())
			{
				pPlayer->SendText(csprintf("You remove %s from your dwelling's access list", name.c_str()), LTT_DEFAULT); //todo: made up message.
				houseData->_accessList.erase(iter);
			}
			else
			{
				pPlayer->SendText(csprintf("%s is not in the access list", name.c_str()), LTT_DEFAULT); //todo: made up message.
			}
		}
		else
			pPlayer->SendText("Only the character who owns the house may use this command.", LTT_DEFAULT);
	}
	else
		pPlayer->SendText("You do not own a house.", LTT_DEFAULT);
}

void CClientEvents::HouseToggleOpenAccess(bool newSetting)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerId == pPlayer->GetID())
		{
			if (houseData->_everyoneAccess != newSetting)
			{
				houseData->_everyoneAccess = newSetting;
				if (newSetting)
					pPlayer->SendText("Your dwelling is now open to the public.", LTT_DEFAULT); //todo: made up message.
				else
					pPlayer->SendText("Your dwelling is now private.", LTT_DEFAULT); //todo: made up message.
			}
			else
			{
				if (newSetting)
					pPlayer->SendText("Your dwelling is already open to the public.", LTT_DEFAULT); //todo: made up message.
				else
					pPlayer->SendText("Your dwelling is already private.", LTT_DEFAULT); //todo: made up message.
			}
		}
		else
			pPlayer->SendText("Only the character who owns the house may use this command.", LTT_DEFAULT);
	}
	else
		pPlayer->SendText("You do not own a house.", LTT_DEFAULT);
}

void CClientEvents::HouseToggleOpenStorageAccess()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	//not sure how this worked? Is this a toggle? If not which command was used to disable it?
	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerId == pPlayer->GetID())
		{
			if (!houseData->_everyoneStorageAccess)
			{
				pPlayer->SendText("Your dwelling's storage is now open to the public.", LTT_DEFAULT); //todo: made up message.
				houseData->_everyoneStorageAccess = true;
			}
			else
			{
				pPlayer->SendText("Your dwelling's storage is now private.", LTT_DEFAULT); //todo: made up message.
				houseData->_everyoneStorageAccess = false;
			}
		}
		else
			pPlayer->SendText("Only the character who owns the house may use this command.", LTT_DEFAULT);
	}
	else
		pPlayer->SendText("You do not own a house.", LTT_DEFAULT);
}

void CClientEvents::HouseAddOrRemovePersonToStorageList(std::string name, bool isAdd)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	DWORD targetId = g_pWorld->GetPlayerId(name.c_str(), true);
	if (!target)
	{
		pPlayer->SendText("Can't find a player by that name.", LTT_DEFAULT); //todo: made up message.
		return;
	}

	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerId == pPlayer->GetID())
		{
			if (isAdd)
			{
				if (houseData->_accessList.size() > 128)
				{
					pPlayer->SendText("The storage access list is full", LTT_DEFAULT); //todo: made up message.
					return;
				}
				if (std::find(houseData->_storageAccessList.begin(), houseData->_storageAccessList.end(), targetId) != houseData->_storageAccessList.end())
				{
					pPlayer->SendText(csprintf("%s is already in the storage access list", name.c_str()), LTT_DEFAULT); //todo: made up message.
				}
				else
				{
					pPlayer->SendText(csprintf("You add %s to your dwelling's storage access list", name.c_str()), LTT_DEFAULT); //todo: made up message.
					houseData->_storageAccessList.push_back(targetId);
				}
			}
			else
			{
				auto iter = std::find(houseData->_storageAccessList.begin(), houseData->_storageAccessList.end(), targetId);
				if (iter != houseData->_storageAccessList.end())
				{
					pPlayer->SendText(csprintf("You remove %s from your dwelling's storage access list", name.c_str()), LTT_DEFAULT); //todo: made up message.
					houseData->_storageAccessList.erase(iter);
				}
				else
				{
					pPlayer->SendText(csprintf("%s is not in the storage access list", name.c_str()), LTT_DEFAULT); //todo: made up message.
				}
			}
		}
		else
			pPlayer->SendText("Only the character who owns the house may use this command.", LTT_DEFAULT);
	}
	else
		pPlayer->SendText("You do not own a house.", LTT_DEFAULT);
}

void CClientEvents::HouseAddOrRemoveAllegianceToAccessList(bool isAdd)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerId == pPlayer->GetID())
		{
			if (houseData->_allegianceAccess != isAdd)
			{
				houseData->_allegianceAccess = isAdd;
				if (isAdd)
					pPlayer->SendText("You have granted your monarchy access to your dwelling.", LTT_DEFAULT);
				else
					pPlayer->SendText("You have revoked access to your dwelling to your monarchy.", LTT_DEFAULT);
			}
			else
			{
				if (isAdd)
					pPlayer->SendText("The monarchy already has access to your dwelling.", LTT_DEFAULT);
				else
					pPlayer->SendText("The monarchy did not have access to your dwelling.", LTT_DEFAULT);
			}
		}
		else
			pPlayer->SendText("Only the character who owns the house may use this command.", LTT_DEFAULT);
	}
	else
		pPlayer->SendText("You do not own a house.", LTT_DEFAULT);
}

void CClientEvents::HouseAddOrRemoveAllegianceToStorageList(bool isAdd)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerId == pPlayer->GetID())
		{
			if (houseData->_allegianceStorageAccess != isAdd)
			{
				houseData->_allegianceStorageAccess = isAdd;
				if (isAdd)
					pPlayer->SendText("You have granted your monarchy access to your storage.", LTT_DEFAULT);
				else
					pPlayer->SendText("You have revoked storage access to your monarchy.", LTT_DEFAULT);
			}
			else
			{
				if (isAdd)
					pPlayer->SendText("The monarchy already has storage access in your dwelling.", LTT_DEFAULT);
				else
					pPlayer->SendText("The monarchy did not have storage access in your dwelling.", LTT_DEFAULT);
			}
		}
		else
			pPlayer->SendText("Only the character who owns the house may use this command.", LTT_DEFAULT);
	}
	else
		pPlayer->SendText("You do not own a house.", LTT_DEFAULT);
}

void CClientEvents::HouseClearAccessList()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerId == pPlayer->GetID())
		{
			houseData->_everyoneAccess = false;
			houseData->_allegianceAccess = false;
			if (houseData->_accessList.empty())
			{
				houseData->_accessList.clear();
				pPlayer->SendText("Your clear the dwelling's access list.", LTT_DEFAULT); //todo: made up message.
			}
			else
				pPlayer->SendText("There's no one in the dwelling's access list.", LTT_DEFAULT); //todo: made up message.
		}
		else
			pPlayer->SendText("Only the character who owns the house may use this command.", LTT_DEFAULT);
	}
	else
		pPlayer->SendText("You do not own a house.", LTT_DEFAULT);
}

void CClientEvents::HouseClearStorageAccess()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	DWORD houseId = pPlayer->InqDIDQuality(HOUSEID_DID, 0);
	if (houseId)
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerId == pPlayer->GetID())
		{
			houseData->_everyoneStorageAccess = false;
			houseData->_allegianceStorageAccess = false;
			if (houseData->_storageAccessList.empty())
			{
				houseData->_storageAccessList.clear();
				pPlayer->SendText("Your clear the dwelling's storage list.", LTT_DEFAULT); //todo: made up message.
			}
			else
				pPlayer->SendText("There's no one in the dwelling's storage list.", LTT_DEFAULT); //todo: made up message.
		}
		else
			pPlayer->SendText("Only the character who owns the house may use this command.", LTT_DEFAULT);
	}
	else
		pPlayer->SendText("You do not own a house.", LTT_DEFAULT);
}

void CClientEvents::NoLongerViewingContents(DWORD container_id)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	if (std::shared_ptr<CWeenieObject> remoteContainerObj = g_pWorld->FindObject(container_id))
	{
		if (std::shared_ptr<CContainerWeenie> remoteContainer = remoteContainerObj->AsContainer())
		{
			remoteContainer->HandleNoLongerViewing(pPlayer);
		}
	}
}

void CClientEvents::ChangePlayerOption(PlayerOptions option, bool value)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return;
	}

	auto changeCharOption = [&](DWORD optionBit)
	{
		pPlayer->_playerModule.options_ &= ~optionBit;
		if (value)
			pPlayer->_playerModule.options_ |= optionBit;
	};

	auto changeCharOption2 = [&](DWORD optionBit)
	{
		pPlayer->_playerModule.options2_ &= ~optionBit;
		if (value)
			pPlayer->_playerModule.options2_ |= optionBit;
	};

	switch (option)
	{
	case AutoRepeatAttack_PlayerOption:
		changeCharOption(AutoRepeatAttack_CharacterOption);
		break;

	case IgnoreAllegianceRequests_PlayerOption:
		changeCharOption(IgnoreAllegianceRequests_CharacterOption);
		break;

	case IgnoreFellowshipRequests_PlayerOption:
		changeCharOption(IgnoreFellowshipRequests_CharacterOption);
		break;

	case IgnoreTradeRequests_PlayerOption:
		changeCharOption(IgnoreAllegianceRequests_CharacterOption);
		break;

	case DisableMostWeatherEffects_PlayerOption:
		changeCharOption(DisableMostWeatherEffects_CharacterOption);
		break;
		 
	case PersistentAtDay_PlayerOption:
		changeCharOption2(PersistentAtDay_CharacterOptions2);
		break;

	case AllowGive_PlayerOption:
		changeCharOption(AllowGive_CharacterOption);
		break;

	case ViewCombatTarget_PlayerOption:
		changeCharOption(ViewCombatTarget_CharacterOption);
		break;

	case ShowTooltips_PlayerOption:
		changeCharOption(ShowTooltips_CharacterOption);
		break;

	case UseDeception_PlayerOption:
		changeCharOption(UseDeception_CharacterOption);
		break;

	case ToggleRun_PlayerOption:
		changeCharOption(ToggleRun_CharacterOption);
		break;

	case StayInChatMode_PlayerOption:
		changeCharOption(StayInChatMode_CharacterOption);
		break;

	case AdvancedCombatUI_PlayerOption:
		changeCharOption(AdvancedCombatUI_CharacterOption);
		break;

	case AutoTarget_PlayerOption:
		changeCharOption(AutoTarget_CharacterOption);
		break;

	case VividTargetingIndicator_PlayerOption:
		changeCharOption(VividTargetingIndicator_CharacterOption);
		break;

	case FellowshipShareXP_PlayerOption:
		changeCharOption(FellowshipShareXP_CharacterOption);
		break;

	case AcceptLootPermits_PlayerOption:
		changeCharOption(AcceptLootPermits_PlayerOption);
		break;

	case FellowshipShareLoot_PlayerOption:
		changeCharOption(FellowshipShareLoot_CharacterOption);
		break;

	case FellowshipAutoAcceptRequests_PlayerOption:
		changeCharOption(FellowshipAutoAcceptRequests_PlayerOption);
		break;

	case SideBySideVitals_PlayerOption:
		changeCharOption(SideBySideVitals_CharacterOption);
		break;

	case CoordinatesOnRadar_PlayerOption:
		changeCharOption(CoordinatesOnRadar_CharacterOption);
		break;

	case SpellDuration_PlayerOption:
		changeCharOption(SpellDuration_CharacterOption);
		break;

	case DisableHouseRestrictionEffects_PlayerOption:
		changeCharOption(DisableHouseRestrictionEffects_PlayerOption);
		break;

	case DragItemOnPlayerOpensSecureTrade_PlayerOption:
		changeCharOption(DragItemOnPlayerOpensSecureTrade_CharacterOption);
		break;

	case DisplayAllegianceLogonNotifications_PlayerOption:
		changeCharOption(DisplayAllegianceLogonNotifications_CharacterOption);
		break;

	case UseChargeAttack_PlayerOption:
		changeCharOption(UseChargeAttack_CharacterOption);
		break;

	case UseCraftSuccessDialog_PlayerOption:
		changeCharOption(UseCraftSuccessDialog_CharacterOption);
		break;

	case HearAllegianceChat_PlayerOption:
		changeCharOption(HearAllegianceChat_CharacterOption);

		if (value)
			SendText("Joined Allegiance Channel!", LTT_CHANNEL_SEND);
		else
			SendText("Left Allegiance Channel!", LTT_CHANNEL_SEND);
		break;

	case DisplayDateOfBirth_PlayerOption:
		changeCharOption2(DisplayDateOfBirth_CharacterOptions2);
		break;

	case DisplayAge_PlayerOption:
		changeCharOption2(DisplayAge_CharacterOptions2);
		break;

	case DisplayChessRank_PlayerOption:
		changeCharOption2(DisplayChessRank_CharacterOptions2);
		break;

	case DisplayFishingSkill_PlayerOption:
		changeCharOption2(DisplayFishingSkill_CharacterOptions2);
		break;

	case DisplayNumberDeaths_PlayerOption:
		changeCharOption2(DisplayNumberDeaths_CharacterOptions2);
		break;

	case DisplayTimeStamps_PlayerOption:
		changeCharOption2(TimeStamp_CharacterOptions2);
		break;

	case SalvageMultiple_PlayerOption:
		changeCharOption(SalvageMultiple_CharacterOptions2);
		break;

	case HearGeneralChat_PlayerOption:
		changeCharOption2(HearGeneralChat_CharacterOptions2);

		if (value)
			SendText("Joined General Channel!", LTT_CHANNEL_SEND);
		else
			SendText("Left General Channel!", LTT_CHANNEL_SEND);
		break;

	case HearTradeChat_PlayerOption:
		changeCharOption2(HearTradeChat_CharacterOptions2);

		if (value)
			SendText("Joined Trade Channel!", LTT_CHANNEL_SEND);
		else
			SendText("Left Trade Channel!", LTT_CHANNEL_SEND);

		break;

	case HearLFGChat_PlayerOption:
		changeCharOption2(HearLFGChat_CharacterOptions2);

		if (value)
			SendText("Joined LFG Channel!", LTT_CHANNEL_SEND);
		else
			SendText("Left LFG Channel!", LTT_CHANNEL_SEND);
		break;

	case HearRoleplayChat_PlayerOption:
		changeCharOption2(HearRoleplayChat_CharacterOptions2);

		if (value)
			SendText("Joined Roleplay Channel!", LTT_CHANNEL_SEND);
		else
			SendText("Left Roleplay Channel!", LTT_CHANNEL_SEND);
		break;

	case AppearOffline_PlayerOption:
		changeCharOption2(AppearOffline_CharacterOptions2);
		break;

	case DisplayNumberCharacterTitles_PlayerOption:
		changeCharOption2(DisplayNumberCharacterTitles_CharacterOptions2);
		break;

	case MainPackPreferred_PlayerOption:
		changeCharOption2(MainPackPreferred_CharacterOptions2);
		break;

	case LeadMissileTargets_PlayerOption:
		changeCharOption2(LeadMissileTargets_CharacterOptions2);
		break;

	case UseFastMissiles_PlayerOption:
		changeCharOption2(UseFastMissiles_CharacterOptions2);
		break;

	case FilterLanguage_PlayerOption:
		changeCharOption2(FilterLanguage_CharacterOptions2);
		break;

	case ConfirmVolatileRareUse_PlayerOption:
		changeCharOption2(ConfirmVolatileRareUse_CharacterOptions2);
		break;

	case HearSocietyChat_PlayerOption:
		changeCharOption2(HearSocietyChat_CharacterOptions2);
		break;

	case ShowHelm_PlayerOption:
		changeCharOption2(ShowHelm_CharacterOptions2);
		break;

	case DisableDistanceFog_PlayerOption:
		changeCharOption2(DisableDistanceFog_CharacterOptions2);
		break;

	case UseMouseTurning_PlayerOption:
		changeCharOption2(UseMouseTurning_CharacterOptions2);
		break;

	case ShowCloak_PlayerOption:
		changeCharOption2(ShowCloak_CharacterOptions2);
		break;

	case LockUI_PlayerOption:
		changeCharOption2(LockUI_CharacterOptions2);
		break;

	case TotalNumberOfPlayerOptions_PlayerOption:
		changeCharOption2(TotalNumberOfPlayerOptions_PlayerOption);
		break;
	}
}

bool CClientEvents::CheckForChatSpam()
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer)
	{
		return false;
	}

	if (_next_chat_allowed > Timer::cur_time)
	{
		return false;
	}

	if (_next_chat_interval < Timer::cur_time)
	{
		_next_chat_interval = Timer::cur_time + 8.0;
		_num_chat_sent = 0;
	}

	_num_chat_sent++;
	if (_num_chat_sent > 8)
	{
		_next_chat_allowed = Timer::cur_time + 10.0;
		pPlayer->SendText("You are sending too many messages and have been temporarily muted.", LTT_DEFAULT);
		return false;
	}

	return true;
}

// This is it!
void CClientEvents::ProcessEvent(BinaryReader *pReader)
{
	std::shared_ptr<CPlayerWeenie> pPlayer = m_pPlayer.lock();
	
	if (!pPlayer || !pReader)
	{
		return;
	}

	DWORD dwSequence = pReader->ReadDWORD();
	DWORD dwEvent = pReader->ReadDWORD();
	if (pReader->GetLastError()) return;

#ifdef _DEBUG
	DEBUG_DATA << "Processing event:" << dwEvent;
#endif

	switch (dwEvent)
	{
		case CHANGE_PLAYER_OPTION: // Change player option
			{
				DWORD option = pReader->ReadDWORD();
				DWORD value = pReader->ReadDWORD();
				if (pReader->GetLastError())
					break;

				ChangePlayerOption((PlayerOptions)option, value ? true : false);
				break;
			}
		case MELEE_ATTACK: // Melee Attack
			{
				DWORD dwTarget = pReader->ReadDWORD();
				DWORD dwHeight = pReader->ReadDWORD();
				float flPower = pReader->ReadFloat();
				if (pReader->GetLastError()) break;

				Attack(dwTarget, dwHeight, flPower);
				break;
			}
		case MISSILE_ATTACK: // Missile Attack
			{
				DWORD target = pReader->ReadDWORD();
				DWORD height = pReader->ReadDWORD();
				float power = pReader->ReadFloat();
				if (pReader->GetLastError()) break;

				MissileAttack(target, height, power);
				break;
			}
		case TEXT_CLIENT: //Client Text
			{
				char *szText = pReader->ReadString();
				if (pReader->GetLastError()) break;

				ClientText(szText);
				break;
			}
		case STORE_ITEM: //Store Item
			{
				DWORD dwItemID = pReader->ReadDWORD();
				DWORD dwContainer = pReader->ReadDWORD();
				DWORD dwSlot = pReader->ReadDWORD();
				if (pReader->GetLastError()) break;

				pPlayer->MoveItemToContainer(dwItemID, dwContainer, (char)dwSlot);
				break;
			}
		case EQUIP_ITEM: //Equip Item
			{
				DWORD dwItemID = pReader->ReadDWORD();
				DWORD dwCoverage = pReader->ReadDWORD();
				if (pReader->GetLastError()) break;

				pPlayer->MoveItemToWield(dwItemID, dwCoverage);
				break;
			}
		case DROP_ITEM: //Drop Item
			{
				DWORD dwItemID = pReader->ReadDWORD();
				if (pReader->GetLastError()) break;

				pPlayer->MoveItemTo3D(dwItemID);
				break;
			}
		case ALLEGIANCE_SWEAR: // Swear Allegiance request
			{
				DWORD target = pReader->Read<DWORD>();
				if (pReader->GetLastError()) break;

				TrySwearAllegiance(target);
				break;
			}
		case ALLEGIANCE_BREAK: // Break Allegiance request
			{
				DWORD target = pReader->Read<DWORD>();
				if (pReader->GetLastError())
					break;

				TryBreakAllegiance(target);
				break;
			}
		case ALLEGIANCE_SEND_UPDATES: // Allegiance Update request
			{
				int on = pReader->Read<int>();
				if (pReader->GetLastError()) break;

				SetRequestAllegianceUpdate(on);
				break;
			}
		case CONFIRMATION_RESPONSE: // confirmation response
		{
			DWORD confirmType = pReader->ReadDWORD();
			int context = pReader->ReadInt32();
			bool accepted = pReader->ReadInt32();

			switch (confirmType)
			{
			case 0x05: // crafting
				if (accepted)
				{
					pPlayer->UseEx(true);
				}
				break;
			}

			break;
		}
		case UST_SALVAGE_REQUEST: // ust salvage request
		{
			DWORD toolId = pReader->ReadDWORD();

			PackableList<DWORD> items;
			items.UnPack(pReader);

			if (pReader->GetLastError())
				break;

			if (items.size() <= 300) //just some sanity checking: 102 items in main pack + (24 * 7) items in subpacks = 270 items. 300 just to be safe.
				pPlayer->PerformSalvaging(toolId, items);
			break;
		}
		case SEND_TELL_BY_GUID: //Send Tell by GUID
			{
				char *text = pReader->ReadString();
				DWORD GUID = pReader->ReadDWORD();

				if (pReader->GetLastError())
					break;

				if (CheckForChatSpam())
				{
					std::string filteredText = FilterBadChatCharacters(text);
					SendTellByGUID(filteredText.c_str(), GUID);
				}

				break;
			}
		case USE_ITEM_EX: //Use Item Ex
			{
			DWORD dwSourceID = pReader->ReadDWORD();
			DWORD dwDestID = pReader->ReadDWORD();
			if (pReader->GetLastError()) break;
			UseItemEx(dwSourceID, dwDestID);
			break;
			}
		case USE_OBJECT: //Use Object
		{
			DWORD dwEID = pReader->ReadDWORD();
			if (pReader->GetLastError()) break;
			UseObject(dwEID);
			break;
		}
		case SPEND_XP_VITALS: // spend XP on vitals
			{
				DWORD dwAttribute2nd = pReader->ReadDWORD();
				DWORD dwXP = pReader->ReadDWORD();
				if (pReader->GetLastError()) break;

				SpendAttribute2ndXP((STypeAttribute2nd)dwAttribute2nd, dwXP);
				break;
			}
		case SPEND_XP_ATTRIBUTES: // spend XP on attributes
			{
				DWORD dwAttribute = pReader->ReadDWORD();
				DWORD dwXP = pReader->ReadDWORD();
				if (pReader->GetLastError()) break;

				SpendAttributeXP((STypeAttribute)dwAttribute, dwXP);
				break;
			}
		case SPEND_XP_SKILLS: // spend XP on skills
			{
				DWORD dwSkill = pReader->ReadDWORD();
				DWORD dwXP = pReader->ReadDWORD();
				if (pReader->GetLastError()) break;

				SpendSkillXP((STypeSkill)dwSkill, dwXP);
				break;
			}
		case SPEND_SKILL_CREDITS: // spend credits to train skill
			{
				DWORD dwSkill = pReader->ReadDWORD();
				DWORD dwCredits = pReader->ReadDWORD();
				if (pReader->GetLastError()) break;

				SpendSkillCredits((STypeSkill)dwSkill, dwCredits);
				break;
			}
		case CAST_UNTARGETED_SPELL: // cast untargeted spell
			{
				DWORD spell_id = pReader->ReadDWORD();
				if (pReader->GetLastError()) break;

				pPlayer->TryCastSpell(pPlayer->GetID() /*0*/, spell_id);
				break;
			}
		case CAST_TARGETED_SPELL: // cast targeted spell
			{
				DWORD target = pReader->ReadDWORD();
				DWORD spell_id = pReader->ReadDWORD();
				if (pReader->GetLastError()) break;

				pPlayer->TryCastSpell(target, spell_id);
				break;
			}
		case CHANGE_COMBAT_STANCE: // Evt_Combat__ChangeCombatMode_ID "Change Combat Mode"
			{
				DWORD mode = pReader->ReadDWORD();
				if (pReader->GetLastError()) break;

				ChangeCombatStance((COMBAT_MODE)mode);
				break;
			}
		case STACKABLE_MERGE: // Evt_Inventory__StackableMerge
			{
				DWORD merge_from_id = pReader->Read<DWORD>();
				DWORD merge_to_id = pReader->Read<DWORD>();
				DWORD amount = pReader->Read<DWORD>();

				if (pReader->GetLastError())
					break;

				pPlayer->MergeItem(merge_from_id, merge_to_id, amount);
				break;
			}
		case STACKABLE_SPLIT_TO_CONTAINER: // Evt_Inventory__StackableSplitToContainer
			{
				DWORD stack_id = pReader->Read<DWORD>();
				DWORD container_id = pReader->Read<DWORD>();
				DWORD place = pReader->Read<DWORD>();
				DWORD amount = pReader->Read<DWORD>();

				if (pReader->GetLastError())
					break;

				pPlayer->SplitItemToContainer(stack_id, container_id, place, amount);
				break;
			}
		case STACKABLE_SPLIT_TO_3D: // Evt_Inventory__StackableSplitTo3D
			{
				DWORD stack_id = pReader->Read<DWORD>();
				DWORD amount = pReader->Read<DWORD>();

				if (pReader->GetLastError())
					break;

				pPlayer->SplitItemto3D(stack_id, amount);
				break;
			}
		case STACKABLE_SPLIT_TO_WIELD: // Evt_Inventory__StackableSplitToWield
			{				
				DWORD stack_id = pReader->Read<DWORD>();
				DWORD loc = pReader->Read<DWORD>();
				DWORD amount = pReader->Read<DWORD>();

				if (pReader->GetLastError())
					break;

				pPlayer->SplitItemToWield(stack_id, loc, amount);
				break;
			}
		case SEND_TELL_BY_NAME: //Send Tell by Name
		{
			char* szText = pReader->ReadString();
			char* szName = pReader->ReadString();
			if (pReader->GetLastError()) break;

			if (CheckForChatSpam())
			{
				std::string filteredText = FilterBadChatCharacters(szText);
				SendTellByName(filteredText.c_str(), szName);
			}

			break;
		}
		case BUY_FROM_VENDOR: // Buy from Vendor
			{
				DWORD vendorID = pReader->Read<DWORD>();
				DWORD numItems = pReader->Read<DWORD>();
				if (numItems >= 300)
					break;

				bool error = false;
				std::list<ItemProfile *> items;
				
				for (DWORD i = 0; i < numItems; i++)
				{
					ItemProfile *item = new ItemProfile();
					error = item->UnPack(pReader);
					items.push_back(item);

					if (error || pReader->GetLastError())
						break;
				}

				if (!error && !pReader->GetLastError())
				{
					TryBuyItems(vendorID, items);
				}

				for (auto item : items)
				{
					delete item;
				}

				break;
			}
		case SELL_TO_VENDOR: // Sell to Vendor
			{
				DWORD vendorID = pReader->Read<DWORD>();
				DWORD numItems = pReader->Read<DWORD>();
				if (numItems >= 300)
					break;

				bool error = false;
				std::list<ItemProfile *> items;

				for (DWORD i = 0; i < numItems; i++)
				{
					ItemProfile *item = new ItemProfile();
					error = item->UnPack(pReader);
					items.push_back(item);

					if (error || pReader->GetLastError())
						break;
				}

				if (!error && !pReader->GetLastError())
				{
					TrySellItems(vendorID, items);
				}

				for (auto item : items)
				{
					delete item;
					
				}

				break;
			}
		case RECALL_LIFESTONE: // Lifestone Recall
		{
			LifestoneRecall();
			break;
		}
		case LOGIN_COMPLETE: // "Login Complete"
		{
			ExitPortal();
			break;
		}
		case FELLOW_CREATE: // "Create Fellowship"
		{
			std::string name = pReader->ReadString();
			int shareXP = pReader->Read<int>();

			if (pReader->GetLastError())
				break;

			TryFellowshipCreate(name, shareXP);
			break;
		}
		case FELLOW_QUIT: // "Quit Fellowship"
		{
			int disband = pReader->Read<int>();

			if (pReader->GetLastError())
				break;

			TryFellowshipQuit(disband);
			break;
		}
		case FELLOW_DISMISS: // "Fellowship Dismiss"
		{
			DWORD dismissed = pReader->Read<DWORD>();
			if (pReader->GetLastError()) break;

			TryFellowshipDismiss(dismissed);
			break;
		}
		case FELLOW_RECRUIT: // "Fellowship Recruit"
		{
			DWORD target = pReader->Read<DWORD>();

			if (pReader->GetLastError())
				break;

			TryFellowshipRecruit(target);
			break;
		}
		case FELLOW_UPDATE: // "Fellowship Update"
		{
			int on = pReader->Read<int>();

			if (pReader->GetLastError())
				break;

			TryFellowshipUpdate(on);
			break;
		}
		case PUT_OBJECT_IN_CONTAINER: // Put object in container
			{
				DWORD target_id = pReader->Read<DWORD>();
				DWORD object_id = pReader->Read<DWORD>();
				DWORD amount = pReader->Read<DWORD>();

				if (pReader->GetLastError())
					break;

				pPlayer->GiveItem(target_id, object_id, amount);
				break;
			}		
		case INSCRIBE: // "Inscribe"
			{
				DWORD target_id = pReader->Read<DWORD>();
				std::string msg = pReader->ReadString();

				if (pReader->GetLastError())
					break;

				TryInscribeItem(target_id, msg);
				break;
			}
		case IDENTIFY: // Identify
		{
			DWORD target_id = pReader->ReadDWORD();

			if (pReader->GetLastError())
				break;

			Identify(target_id);
			break;
		}
		case ADMIN_TELEPORT: // Advocate teleport (triggered by having an admin flag set, clicking the mini-map)
		{
			if (pPlayer->GetAccessLevel() < ADVOCATE_ACCESS)
				break;

			// Starts with string (was empty when I tested)
			pReader->ReadString();

			// Then position (target)
			Position position;
			position.UnPack(pReader);

			if (pReader->GetLastError())
				break;

			pPlayer->Movement_Teleport(position);
			break;
		}
		case TEXT_CHANNEL: // Channel Text
		{
			DWORD channel_id = pReader->ReadDWORD();
			char *msg = pReader->ReadString();

			if (pReader->GetLastError())
				break;

			if (CheckForChatSpam())
			{
				std::string filteredText = FilterBadChatCharacters(msg);
				ChannelText(channel_id, filteredText.c_str());
			}

			break;
		}
		case NO_LONGER_VIEWING_CONTAINER: // No longer viewing contents
			{
				DWORD container_id = pReader->Read<DWORD>();
				if (pReader->GetLastError())
					break;

				NoLongerViewingContents(container_id);
				break;
			}
		case ADD_ITEM_SHORTCUT: // Add item to shortcut bar
			{
				ShortCutData data;
				data.UnPack(pReader);
				if (pReader->GetLastError())
					break;

				pPlayer->_playerModule.AddShortCut(data);
				break;
			}
		case REMOVE_ITEM_SHORTCUT: // Add item to shortcut bar
			{
				int index = pReader->Read<int>();
				if (pReader->GetLastError())
					break;

				pPlayer->_playerModule.RemoveShortCut(index);
				break;
			}
		case TOGGLE_SHOW_HELM:
			{
				PlayerModule module;
				if (!module.UnPack(pReader) || pReader->GetLastError())
					break;

				SendText("Updating character configuration.", LTT_SYSTEM_EVENT);
				pPlayer->UpdateModuleFromClient(module);
				break;
			}
		case CANCEL_ATTACK: // Cancel attack
		{
			// TODO
			pPlayer->TryCancelAttack();
			break;
		}
		case HEALTH_UPDATE_REQUEST: // Request health update
		{
			DWORD target_id = pReader->ReadDWORD();

			if (pReader->GetLastError())
				break;

			RequestHealthUpdate(target_id);
			break;
		}
		case TEXT_INDIRECT: // Indirect Text (@me)
		{
			char *msg = pReader->ReadString();

			if (pReader->GetLastError())
				break;

			if (CheckForChatSpam())
			{
				std::string filteredText = FilterBadChatCharacters(msg);
				EmoteText(filteredText.c_str());
			}

			break;
		}
		case TEXT_EMOTE: // Emote Text (*laugh* sends 'laughs')
		{
			char *msg = pReader->ReadString();

			if (pReader->GetLastError())
				break;

			if (CheckForChatSpam())
			{
				std::string filteredText = FilterBadChatCharacters(msg);
				ActionText(filteredText.c_str());
			}

			break;
		}
		case ADD_TO_SPELLBAR: // Add item to spell bar
			{
				DWORD spellID = pReader->Read<DWORD>();
				int index = pReader->Read<int>();
				int spellBar = pReader->Read<int>();
				if (pReader->GetLastError())
					break;

				pPlayer->_playerModule.AddSpellFavorite(spellID, index, spellBar);
				break;
			}
		case REMOVE_FROM_SPELLBAR: // Remove item from spell bar
			{
				DWORD spellID = pReader->Read<DWORD>();
				int spellBar = pReader->Read<int>();
				if (pReader->GetLastError())
					break;

				pPlayer->_playerModule.RemoveSpellFavorite(spellID, spellBar);
				break;
			}
		case 0x01E9: // Ping
		{
			Ping();
			break;
		}
		case TRADE_OPEN: // Open Trade Negotiations
		{
			if (pPlayer->GetTradeManager())
			{
				//already trading
				return;
			}

			DWORD target = pReader->Read<DWORD>();
			if (pReader->GetLastError())
				return;

			std::shared_ptr<CWeenieObject> pOther = g_pWorld->FindWithinPVS(pPlayer, target);
			std::shared_ptr<CPlayerWeenie> pTarget;
			if (pOther)
				pTarget = pOther->AsPlayer();

			if (!pTarget)
			{
				// cannot open trade
				pPlayer->SendText("Unable to open trade.", LTT_ERROR);
			}
			else if (pTarget->_playerModule.options_ & 0x20000)
			{
				SendText((pTarget->GetName() + " has disabled trading.").c_str(), LTT_ERROR);
			}
			else if (pTarget->IsBusyOrInAction())
			{
				SendText((pTarget->GetName() + " is busy.").c_str(), LTT_ERROR);
			}
			else if (pTarget->GetTradeManager())
			{
				SendText((pTarget->GetName() + " is already trading with someone else!").c_str(), LTT_ERROR);
			}
			else if (pPlayer->DistanceTo(pOther, true) > 1)
			{
				SendText((pTarget->GetName() + " is too far away!").c_str(), LTT_ERROR);
			}
			else
			{
				std::shared_ptr<TradeManager> tm = std::shared_ptr<TradeManager>(new TradeManager(pPlayer, pTarget));

				pPlayer->SetTradeManager(tm);
				pTarget->SetTradeManager(tm);
			}
			break;
		}
		case TRADE_CLOSE: // Close Trade Negotiations
		{
			if (std::shared_ptr<TradeManager> tm = pPlayer->GetTradeManager())
			{
				tm->CloseTrade(pPlayer);
				return;
			}
			break;
		}
		case TRADE_ADD: // AddToTrade
		{
			if (std::shared_ptr<TradeManager> tm = pPlayer->GetTradeManager())
			{
				DWORD item = pReader->Read<DWORD>();

				tm->AddToTrade(pPlayer, item);
			}
			break;
		}
		case TRADE_ACCEPT: // Accept trade
		{
			if (std::shared_ptr<TradeManager> tm = pPlayer->GetTradeManager())
			{
				tm->AcceptTrade(pPlayer);
			}
			break;
		}
		case TRADE_DECLINE: // Decline trade
		{
			if (std::shared_ptr<TradeManager> tm = pPlayer->GetTradeManager())
			{
				tm->DeclineTrade(pPlayer);
			}
			break;
		}
		case TRADE_RESET: // Reset trade
		{
			if (std::shared_ptr<TradeManager> tm = pPlayer->GetTradeManager())
			{
				tm->ResetTrade(pPlayer);
			}
			break;
		}
		case HOUSE_BUY: //House_BuyHouse 
			{
				DWORD slumlord = pReader->Read<DWORD>();
			
				// TODO sanity check on the number of items here
				PackableList<DWORD> items;
				items.UnPack(pReader);

				if (pReader->GetLastError())
					break;

				HouseBuy(slumlord, items);
				break;
			}
		case HOUSE_ABANDON: //House_AbandonHouse 
		{
			HouseAbandon();
			break;
		}
		case HOUSE_OF_PLAYER_QUERY: //House_QueryHouse 
		{
			HouseRequestData();
			break;
		}
		case HOUSE_RENT: //House_RentHouse 
			{
				DWORD slumlord = pReader->Read<DWORD>();

				// TODO sanity check on the number of items here
				PackableList<DWORD> items;
				items.UnPack(pReader);

				if (pReader->GetLastError())
					break;

				HouseRent(slumlord, items);
				break;
			}
		case HOUSE_ADD_GUEST: //House_AddPermanentGuest 
		{
			std::string name = pReader->ReadString();

			if (pReader->GetLastError())
				break;

			HouseAddPersonToAccessList(name);
			break;
		}
		case HOUSE_REMOVE_GUEST: //House_RemovePermanentGuest
		{
			std::string name = pReader->ReadString();

			if (pReader->GetLastError())
				break;

			HouseRemovePersonFromAccessList(name);
			break;
		}
		case HOUSE_SET_OPEN_ACCESS: //House_SetOpenHouseStatus
		{
			int newSetting = pReader->Read<int>();

			if (pReader->GetLastError())
				break;

			HouseToggleOpenAccess(newSetting > 0);
			break;
		}
		case HOUSE_CHANGE_STORAGE_PERMISSIONS: //House_ChangeStoragePermission
		{
			std::string name = pReader->ReadString();
			int isAdd = pReader->Read<int>();

			if (pReader->GetLastError())
				break;

			HouseAddOrRemovePersonToStorageList(name, isAdd > 0);
			break;
		}
		case HOUSE_CLEAR_STORAGE_PERMISSIONS: //House_RemoveAllStoragePermission 
		{
			HouseClearStorageAccess();
			break;
		}
		case HOUSE_GUEST_LIST: //House_RequestFullGuestList
		{
			HouseRequestAccessList();
			break;
		}
		case ALLEGIANCE_MOTD: //Request allegiance MOTD
		{
			SendAllegianceMOTD();
			break;
		}
		case HOUSE_SET_OPEN_STORAGE_ACCESS: //House_AddAllStoragePermission
		{
			HouseToggleOpenStorageAccess();
			break;
		}
		case HOUSE_REMOVE_ALL_GUESTS: //House_RemoveAllPermanentGuests
		{
			HouseClearAccessList();
			break;
		}
		case RECALL_HOUSE: // House Recall
		{
			HouseRecall();
			break;
		}		
		case ITEM_MANA_REQUEST: // Request Item Mana
		{
			DWORD itemId = pReader->ReadDWORD();

			if (pReader->GetLastError())
				break;

			pPlayer->HandleItemManaRequest(itemId);
			break;
		}
		case HOUSE_SET_HOOKS_VISIBILITY: // House_SetHooksVisibility 
		{
			int newSetting = pReader->Read<int>();

			if (pReader->GetLastError())
				break;

			HouseToggleHooks(newSetting > 0);
			break;
		}
		case HOUSE_CHANGE_ALLEGIANCE_GUEST_PERMISSIONS: //House_ModifyAllegianceGuestPermission 
		{
			int newSetting = pReader->Read<int>();

			if (pReader->GetLastError())
				break;

			HouseAddOrRemoveAllegianceToAccessList(newSetting > 0);
			break;
		}
		case HOUSE_CHANGE_ALLEGIANCE_STORAGE_PERMISSIONS: //House_ModifyAllegianceStoragePermission
		{
			int newSetting = pReader->Read<int>();

			if (pReader->GetLastError())
				break;

			HouseAddOrRemoveAllegianceToStorageList(newSetting > 0);
			break;
		}
		case RECALL_HOUSE_MANSION: // House_TeleToMansion
		{
			HouseMansionRecall();
			break;
		}
		case DIE_COMMAND: // "/die" command
		{
			if (!pPlayer->IsDead() && !pPlayer->IsInPortalSpace() && !pPlayer->IsBusyOrInAction())
			{
				// this is a bad way of doing this...
				pPlayer->SetHealth(0, true);
				pPlayer->OnDeath(pPlayer->GetID());
			}

			break;
		}
		case ALLEGIANCE_INFO_REQUEST: // allegiance info request
			{
				std::string target = pReader->ReadString();
				if (target.empty() || pReader->GetLastError())
					break;

				AllegianceInfoRequest(target);
				break;
			}
		case SPELLBOOK_FILTERS: // "/die" command
			{
				DWORD filters = pReader->Read<DWORD>();
				if (pReader->GetLastError())
					break;

				pPlayer->_playerModule.spell_filters_ = filters;
				break;
			}
		case RECALL_MARKET: // Marketplace Recall
		{
			MarketplaceRecall();
			break;
		}
		case FELLOW_ASSIGN_NEW_LEADER: // "Fellowship Assign New Leader"
			{
				DWORD target_id = pReader->Read<DWORD>();

				if (pReader->GetLastError())
					break;

				TryFellowshipAssignNewLeader(target_id);
				break;
			}
		case FELLOW_CHANGE_OPENNESS: // "Fellowship Change Openness"
			{
				int open = pReader->Read<int>();

				if (pReader->GetLastError())
					break;

				TryFellowshipChangeOpenness(open);
				break;
			}
		case RECALL_ALLEGIANCE_HOMETOWN: //Allegiance_RecallAllegianceHometown (bindstone)
		{
			AllegianceHometownRecall();
			break;
		}
		case JUMP_MOVEMENT: // Jump Movement
		{
			float extent = pReader->Read<float>(); // extent

			Vector jumpVelocity;
			jumpVelocity.UnPack(pReader);

			Position position;
			position.UnPack(pReader);

			if (pReader->GetLastError())
				break;

			// CTransition *transition = pPlayer->transition(&pPlayer->m_Position, &position, 0);

			/*
			CTransition *transit = pPlayer->transition(&pPlayer->m_Position, &position, 0);
			if (transit)
			{
				pPlayer->SetPositionInternal(transit);
			}
			*/
			
			/*
			double dist = pPlayer->m_Position.distance(position);
			if (dist >= 5)
			{
				pPlayer->_force_position_timestamp++;
				pPlayer->Movement_UpdatePos();

				pPlayer->SendText(csprintf("Correcting position due to jump %f", dist), LTT_DEFAULT);
			}
			*/

			double dist = pPlayer->m_Position.distance(position);
			if (dist >= 10)
			{
				// Snap them back to their previous position
				pPlayer->_force_position_timestamp++;
			}
			else
			{
				pPlayer->SetPositionSimple(&position, TRUE);
				
				/*
				CTransition *transit = pPlayer->transition(&pPlayer->m_Position, &position, 0);
				if (transit)
				{
					pPlayer->SetPositionInternal(transit);
				}
				*/
			}

			// pPlayer->m_Position = position;

			/*
			long stamina = 0;
			pPlayer->get_minterp()->jump(extent, stamina);
			if (stamina > 0)
			{
				unsigned int curStamina = pPlayer->GetStamina();

				if (stamina > curStamina)
				{
					// should maybe change the extent? idk
					pPlayer->SetStamina(0);
				}
				else
					pPlayer->SetStamina(curStamina - stamina);
			}

			pPlayer->Animation_Jump(extent, jumpVelocity);
			pPlayer->m_bAnimUpdate = TRUE;
			*/
			long stamina = 0;
			if (pPlayer->JumpStaminaCost(extent, stamina) && stamina > 0)
			{
				unsigned int curStamina = pPlayer->GetStamina();

				if (stamina > curStamina)
				{
					// should maybe change the extent? idk
					pPlayer->SetStamina(0);
				}
				else
					pPlayer->SetStamina(curStamina - stamina);
			}
			
			pPlayer->transient_state &= ~((DWORD)TransientState::CONTACT_TS);
			pPlayer->transient_state &= ~((DWORD)WATER_CONTACT_TS);
			pPlayer->calc_acceleration();
			pPlayer->set_on_walkable(FALSE);
			pPlayer->set_local_velocity(jumpVelocity, 0);

			/*
			Vector localVel = pPlayer->get_local_physics_velocity();
			Vector vel = pPlayer->m_velocityVector;
			pPlayer->EmoteLocal(csprintf("Received jump with velocity %.1f %.1f %.1f (my lv: %.1f %.1f %.1f my v: %.1f %.1f %.1f)",
				jumpVelocity.x, jumpVelocity.y, jumpVelocity.z,
				localVel.x, localVel.y, localVel.z,
				vel.x, vel.y, vel.z));
				*/

			pPlayer->Movement_UpdateVector();

			break;
		}
		case MOVE_TO: // CM_Movement__Event_MoveToState (update vector movement?)
		{
			// TODO: Cancel attack
			
			MoveToStatePack moveToState;
			moveToState.UnPack(pReader);

			if (pReader->GetLastError())
			{
				SERVER_WARN << "Bad animation message!";
				SERVER_WARN << pReader->GetDataStart() << pReader->GetDataLen();
				break;
			}

			//if (is_newer_event_stamp(moveToState.server_control_timestamp, pPlayer->_server_control_timestamp))
			//{
				// LOG(Temp, Normal, "Old server control timestamp on 0xF61C. Ignoring.\n");
			//	break;
			//}

			if (is_newer_event_stamp(moveToState.teleport_timestamp, pPlayer->_teleport_timestamp))
			{
				SERVER_WARN << "Old teleport timestamp on 0xF61C. Ignoring.";
				break;
			}
			if (is_newer_event_stamp(moveToState.force_position_ts, pPlayer->_force_position_timestamp))
			{
				SERVER_WARN << "Old force position timestamp on 0xF61C. Ignoring.";
				break;
			}

			if (pPlayer->IsDead())
			{
				SERVER_WARN << "Dead players can't move. Ignoring.";
				break;
			}

			/*
			CTransition *transit = pPlayer->transition(&pPlayer->m_Position, &moveToState.position, 0);
			if (transit)
			{
				pPlayer->SetPositionInternal(transit);
			}
			*/

			/*
			double dist = pPlayer->m_Position.distance(moveToState.position);
			if (dist >= 5)
			{
				pPlayer->_force_position_timestamp++;
				pPlayer->Movement_UpdatePos();

				pPlayer->SendText(csprintf("Correcting position due to state %f", dist), LTT_DEFAULT);
			}
			*/

			/*
			bool bHasCell = pPlayer->cell ? true : false;
			pPlayer->SetPositionSimple(&moveToState.position, TRUE);
			if (!pPlayer->cell && bHasCell)
			{
				pPlayer->SendText("Damnet...", LTT_DEFAULT);
			}
			*/

			double dist = pPlayer->m_Position.distance(moveToState.position);
			if (dist >= 10)
			{
				// Snap them back to their previous position
				pPlayer->_force_position_timestamp++;
			}
			else
			{
				pPlayer->SetPositionSimple(&moveToState.position, TRUE);

				/*
				CTransition *transit = pPlayer->transition(&pPlayer->m_Position, &moveToState.position, 0);
				if (transit)
				{
					pPlayer->SetPositionInternal(transit);
				}
				*/
			}

			// pPlayer->m_Position = moveToState.position; // should interpolate to this, but oh well

			/*
			if (moveToState.contact)
			{
				pPlayer->transient_state |= ((DWORD)TransientState::CONTACT_TS);
			}
			else
			{
				pPlayer->transient_state &= ~((DWORD)TransientState::CONTACT_TS);
				pPlayer->transient_state &= ~((DWORD)WATER_CONTACT_TS);
			}
			pPlayer->calc_acceleration();
			pPlayer->set_on_walkable(moveToState.contact);

			pPlayer->get_minterp()->standing_longjump = moveToState.longjump_mode ? TRUE : FALSE;
			*/

			pPlayer->last_move_was_autonomous = true;
			pPlayer->cancel_moveto();

			if (!(moveToState.raw_motion_state.current_style & CM_Style) && moveToState.raw_motion_state.current_style)
			{
				SERVER_WARN << "Bad style received" << moveToState.raw_motion_state.current_style;
				break;
			}

			if (moveToState.raw_motion_state.forward_command & CM_Action)
			{
				SERVER_WARN << "Bad forward command received" << moveToState.raw_motion_state.forward_command;
				break;
			}

			if (moveToState.raw_motion_state.sidestep_command & CM_Action)
			{
				SERVER_WARN << "Bad sidestep command received" << moveToState.raw_motion_state.sidestep_command;
				break;
			}

			if (moveToState.raw_motion_state.turn_command & CM_Action)
			{
				SERVER_WARN << "Bad turn command received" << moveToState.raw_motion_state.turn_command;
				break;
			}

			CMotionInterp *minterp = pPlayer->get_minterp();
			minterp->raw_state = moveToState.raw_motion_state;
			minterp->apply_raw_movement(TRUE, minterp->motion_allows_jump(minterp->interpreted_state.forward_command != 0));

			WORD newestActionStamp = m_MoveActionStamp;

			for (const auto &actionNew : moveToState.raw_motion_state.actions)
			{
				if (pPlayer->get_minterp()->interpreted_state.GetNumActions() >= MAX_EMOTE_QUEUE)
					break;

				if (is_newer_event_stamp(newestActionStamp, actionNew.stamp))
				{
					DWORD commandID = GetCommandID(actionNew.action);

					if (!(commandID & CM_Action) || !(commandID & CM_ChatEmote))
					{
						SERVER_WARN << "Bad action received" << commandID;
						continue;
					}

					MovementParameters params;
					params.action_stamp = ++pPlayer->m_wAnimSequence;
					params.autonomous = 1;
					params.speed = actionNew.speed;
					pPlayer->get_minterp()->DoMotion(commandID, &params);

					// minterp->interpreted_state.AddAction(ActionNode(actionNew.action, actionNew.speed, ++pPlayer->m_wAnimSequence, TRUE));

					// newestActionStamp = actionNew.stamp;
					// pPlayer->Animation_PlayEmote(actionNew.action, actionNew.speed);
				}
			}

			m_MoveActionStamp = newestActionStamp;

			// pPlayer->Movement_UpdatePos();
			pPlayer->Animation_Update();
			// pPlayer->m_bAnimUpdate = TRUE;

			// pPlayer->Movement_UpdatePos();
			break;
		}
		case UPDATE_POSITION: // Update Exact Position
		{
			Position position;
			position.UnPack(pReader);

			WORD instance = pReader->ReadWORD();

			if (pReader->GetLastError())
				break;

			if (instance != pPlayer->_instance_timestamp)
			{
				SERVER_WARN << "Bad instance.";
				break;
			}

			WORD server_control_timestamp = pReader->ReadWORD();
			if (pReader->GetLastError())
				break;
			//if (is_newer_event_stamp(server_control_timestamp, pPlayer->_server_control_timestamp))
			//{
			//	LOG(Temp, Normal, "Old server control timestamp. Ignoring.\n");
			//	break;
			//}

			WORD teleport_timestamp = pReader->ReadWORD();
			if (pReader->GetLastError())
				break;
			if (is_newer_event_stamp(teleport_timestamp, pPlayer->_teleport_timestamp))
			{
				SERVER_WARN << "Old teleport timestamp. Ignoring.";
				break;
			}

			WORD force_position_ts = pReader->ReadWORD();
			if (pReader->GetLastError())
				break;
			if (is_newer_event_stamp(force_position_ts, pPlayer->_force_position_timestamp))
			{
				SERVER_WARN << "Old force position timestamp. Ignoring.";
				break;
			}

			BOOL bHasContact = pReader->ReadBYTE() ? TRUE : FALSE;
			if (pReader->GetLastError())
				break;
			
			double dist =pPlayer->m_Position.distance(position);
			if (dist >= 10)
			{
				// Snap them back to their previous position
				pPlayer->_force_position_timestamp++;
				// pPlayer->SendText(csprintf("Correcting position due to position update %f", dist), LTT_DEFAULT);
			}
			else
			{
				/*
				CTransition *transit = pPlayer->transition(&pPlayer->m_Position, &position, 0);
				if (transit)
				{
					pPlayer->SetPositionInternal(transit);
					*/
					/*
					double distFromClient = pPlayer->m_Position.distance(position);

					if (distFromClient >= 3.0)
					{
						pPlayer->_force_position_timestamp++;
					}
				}
				*/

				pPlayer->SetPositionSimple(&position, TRUE);

				/*
				if (!pPlayer->cell && bHasCell)
				{
					pPlayer->SendText("Damnet...", LTT_DEFAULT);
				}
				*/
				// pPlayer->m_Position = position; // should interpolate this, not set this directly, but oh well
			
			}

			if (bHasContact)
			{
				pPlayer->transient_state |= ((DWORD)TransientState::CONTACT_TS);
			}
			else
			{
				pPlayer->transient_state &= ~((DWORD)TransientState::CONTACT_TS);
				pPlayer->transient_state &= ~((DWORD)WATER_CONTACT_TS);
			}
			pPlayer->calc_acceleration();
			pPlayer->set_on_walkable(bHasContact);

			pPlayer->Movement_UpdatePos();
			break;
		}
		default:
		{
			//Unknown Event
#ifdef _DEBUG
			SERVER_WARN << "Unhandled client event" << dwEvent;
#endif
			// LOG_BYTES(Temp, Verbose, in->GetDataPtr(), in->GetDataEnd() - in->GetDataPtr() );
		}
	}
}








