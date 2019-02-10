#include "StdAfx.h"
#include "WeenieObject.h"
#include "PhysicsObj.h"
#include "Monster.h"
#include "Player.h"
#include "Client.h"
#include "ClientEvents.h"
#include "BinaryWriter.h"
#include "ObjectMsgs.h"
#include "Database.h"
#include "Database2.h"
#include "World.h"
#include "WorldLandBlock.h"
#include "ClientCommands.h"
#include "ChatMsgs.h"
#include "WeenieFactory.h"
#include "InferredPortalData.h"
#include "AllegianceManager.h"
#include "Config.h"
#include "SpellcastingManager.h"
#include "Corpse.h"
#include "House.h"
#include "easylogging++.h"
#include "Util.h"
#include "ChessManager.h"
#include "RandomRange.h"
#include "AugmentationDevice.h"


#include <chrono>
#include <algorithm>
#include <functional>


#define PLAYER_SAVE_INTERVAL 180.0
#define PLAYER_HEALTH_POLL_INTERVAL 5.0

DEFINE_PACK(SalvageResult)
{
	pWriter->Write<DWORD>(material);
	pWriter->Write<double>(workmanship);
	pWriter->Write<int>(units);
}

DEFINE_UNPACK(SalvageResult)
{
	UNFINISHED();

	return false;
}

CPlayerWeenie::CPlayerWeenie(CClient *pClient, DWORD dwGUID, WORD instance_ts)
{
	m_bDontClear = true;
	m_pClient = pClient;
	SetID(dwGUID);

	_instance_timestamp = instance_ts;

	m_Qualities.SetInt(CREATION_TIMESTAMP_INT, (int)time(NULL));
	m_Qualities.SetFloat(CREATION_TIMESTAMP_FLOAT, Timer::cur_time);

	//clear portal use timestamps on login. 
	m_Qualities.SetFloat(LAST_PORTAL_TELEPORT_TIMESTAMP_FLOAT, 0);
	m_Qualities.SetFloat(LAST_TELEPORT_START_TIMESTAMP_FLOAT, 0);

	//if (pClient && pClient->GetAccessLevel() >= SENTINEL_ACCESS)
	//	SetRadarBlipColor(Sentinel_RadarBlipEnum);

	//SetLoginPlayerQualities();

	m_Qualities.SetInt(PHYSICS_STATE_INT, PhysicsState::HIDDEN_PS | PhysicsState::IGNORE_COLLISIONS_PS | PhysicsState::EDGE_SLIDE_PS | PhysicsState::GRAVITY_PS);

	// Human Female by Default

	// Used by physics object
	SetSetupID(0x2000001); // 0x0200004E);
	SetMotionTableID(0x9000001);
	SetSoundTableID(0x20000001); // 0x20000002);
	SetPETableID(0x34000004);
	SetScale(1.0f);

	// Moon location... loc_t( 0xEFEA0001, 0, 0, 0 );
	// SetInitialPosition(Position(0x9722003A, Vector(168.354004f, 24.618000f, 102.005005f), Quaternion(-0.922790f, 0.000000, 0.000000, -0.385302f)));

	m_LastAssessed = 0;

	m_NextSave = Timer::cur_time + PLAYER_SAVE_INTERVAL;
}

CPlayerWeenie::~CPlayerWeenie()
{
	LeaveFellowship();
	
	if (m_pTradeManager)
	{
		m_pTradeManager->CloseTrade(this);
		m_pTradeManager = NULL;
	}

	CClientEvents *pEvents;
	if (m_pClient && (pEvents = m_pClient->GetEvents()))
	{
		pEvents->DetachPlayer();
	}
}

void CPlayerWeenie::DetachClient()
{
	m_pClient = NULL;
}

void CPlayerWeenie::SendNetMessage(void *_data, DWORD _len, WORD _group, BOOL _event)
{
	if (m_pClient)
		m_pClient->SendNetMessage(_data, _len, _group, _event);
}

void CPlayerWeenie::SendNetMessage(BinaryWriter *_food, WORD _group, BOOL _event, BOOL del)
{
	if (m_pClient)
		m_pClient->SendNetMessage(_food, _group, _event, del);
}

void CPlayerWeenie::AddSpellByID(DWORD id)
{
	BinaryWriter AddSpellToSpellbook;
	AddSpellToSpellbook.Write<DWORD>(0x02C1);
	AddSpellToSpellbook.Write<DWORD>(id);
	AddSpellToSpellbook.Write<DWORD>(0x0);
	SendNetMessage(AddSpellToSpellbook.GetData(), AddSpellToSpellbook.GetSize(), EVENT_MSG, true);
}

bool CPlayerWeenie::IsAdvocate()
{
	return GetAccessLevel() >= ADVOCATE_ACCESS;
}

bool CPlayerWeenie::IsSentinel()
{
	return GetAccessLevel() >= SENTINEL_ACCESS;
}

bool CPlayerWeenie::IsAdmin()
{
	return GetAccessLevel() >= ADMIN_ACCESS;
}

int CPlayerWeenie::GetAccessLevel()
{
	if (!m_pClient)
		return BASIC_ACCESS;

	return m_pClient->GetAccessLevel();
}

void CPlayerWeenie::BeginLogout()
{
	if (IsLoggingOut())
		return;

	_beginLogoutTime = max(Timer::cur_time, (double)m_iPKActivity);
	if (IsPK()) _beginLogoutTime += 15.0;
	_logoutTime = _beginLogoutTime + 5.0;

	ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	LeaveFellowship();

	if (m_pTradeManager)
	{
		m_pTradeManager->CloseTrade(this);
		m_pTradeManager = NULL;
	}

	sChessManager->Quit(this);
	
	StopCompletely(0);
}

void CPlayerWeenie::OnLogout()
{
	DoForcedMotion(Motion_LogOut);
	Save();
}

void CPlayerWeenie::Tick()
{
	CMonsterWeenie::Tick();

	if (IsDead() && (!m_bReviveAfterAnim || get_minterp()->interpreted_state.forward_command != Motion_Dead))
	{
		m_bReviveAfterAnim = false;

		//if (IsDead())
		Revive();
	}

	double pkTimestamp;
	if (m_Qualities.InqFloat(PK_TIMESTAMP_FLOAT, pkTimestamp, TRUE))
	{
		if (pkTimestamp <= Timer::cur_time)
		{
			m_Qualities.SetInt(PLAYER_KILLER_STATUS_INT, PKStatusEnum::PK_PKStatus);
			NotifyIntStatUpdated(PLAYER_KILLER_STATUS_INT, false);

			m_Qualities.RemoveFloat(PK_TIMESTAMP_FLOAT);
			NotifyFloatStatUpdated(PK_TIMESTAMP_FLOAT);

			SendText("The power of Bael'Zharon flows through you, you are once more a player killer.", LTT_MAGIC);
		}
	}
	
	if (m_pClient)
	{
		if (m_NextSave <= Timer::cur_time)
		{
			if (!IsDead()) // && !IsBusyOrInAction())
			{
				Save();
				m_NextSave = Timer::cur_time + PLAYER_SAVE_INTERVAL;
			}
		}
	}

	if (m_fNextMakeAwareCacheFlush <= Timer::cur_time)
	{
		FlushMadeAwareof();
		m_fNextMakeAwareCacheFlush = Timer::cur_time + 25.0;
	}

	if (IsLoggingOut() && _beginLogoutTime <= Timer::cur_time)
	{
		OnLogout();

		_beginLogoutTime = Timer::cur_time + 999999;
	}
	if (IsLoggingOut() && _logoutTime <= Timer::cur_time)
	{
		// time to logout
		if (m_pClient && m_pClient->GetEvents())
		{
			m_pClient->GetEvents()->OnLogoutCompleted();
		}

		MarkForDestroy();
		_logoutTime = DBL_MAX;
	}

	if (IsRecalling() && _recallTime <= Timer::cur_time)
	{
		_recallTime = -1.0;
		Movement_Teleport(_recallPos, false);
	}

	if (m_pTradeManager && m_fNextTradeCheck <= Timer::cur_time)
	{
		m_pTradeManager->CheckDistance();
		m_fNextTradeCheck = Timer::cur_time + 2;
	}

	if (m_NextHealthUpdate <= Timer::cur_time)
	{
		CWeenieObject *pTarget = g_pWorld->FindWithinPVS(this, m_LastHealthRequest);
		if (pTarget)
		{
			SendNetMessage(HealthUpdate((CMonsterWeenie *)pTarget), PRIVATE_MSG, TRUE, TRUE);

			m_NextHealthUpdate = Timer::cur_time + PLAYER_HEALTH_POLL_INTERVAL;
		}
		else
		{
			RemoveLastHealthRequest();
		}

	}

	if (m_Qualities.GetBool(UNDER_LIFESTONE_PROTECTION_BOOL, 0) && m_Qualities.GetFloat(LIFESTONE_PROTECTION_TIMESTAMP_FLOAT, -1) <= Timer::cur_time)
	{
		m_Qualities.SetBool(UNDER_LIFESTONE_PROTECTION_BOOL, 0);
		m_Qualities.SetFloat(LIFESTONE_PROTECTION_TIMESTAMP_FLOAT, 0);
		SendText("You're no longer protected by the Lifestone's magic!", LTT_MAGIC);
	}

	if (HasPortalUseCooldown() && InqFloatQuality(LAST_PORTAL_TELEPORT_TIMESTAMP_FLOAT, 0) <= Timer::cur_time)
	{
		m_Qualities.SetFloat(LAST_PORTAL_TELEPORT_TIMESTAMP_FLOAT, 0);
	}

	if (HasTeleportUseCooldown() && InqFloatQuality(LAST_TELEPORT_START_TIMESTAMP_FLOAT, 0) <= Timer::cur_time)
	{
		m_Qualities.SetFloat(LAST_TELEPORT_START_TIMESTAMP_FLOAT, 0);
		m_Qualities.SetFloat(LAST_PORTAL_TELEPORT_TIMESTAMP_FLOAT, Timer::cur_time + 3); //set timeout for next portal use now that we have materialized (for portals).
	}
	
	if (_deathTimer > 0)
	{
		if (_deathTimer <= Timer::cur_time)
		{
			SetHealth(0, true);
			OnDeath(GetID());
			_deathTimer = -1.0;
			_dieTextTimer = -1.0;
			_dieTextCounter = 0;
		}
		else if (_dieTextTimer > 0 && _dieTextTimer <= Timer::cur_time)
		{
			switch (_dieTextCounter)
			{
			case 0: SpeakLocal("I feel faint..."); break;
			case 1: SpeakLocal("My sight is growing dim..."); break;
			case 2: SpeakLocal("My life is flashing before my eyes..."); break;
			case 3: SpeakLocal("I see a light"); break;
			case 4: SpeakLocal("Oh cruel, cruel world!"); break;
			default: break;
			}

			_dieTextTimer = Timer::cur_time + 2.0;
			_dieTextCounter++;
		}
	}
}


bool CPlayerWeenie::IsBusy()
{
	if (IsRecalling() || IsLoggingOut() || CWeenieObject::IsBusy())
		return true;

	return false;
}

const double AWARENESS_TIMEOUT = 25.0;

bool CPlayerWeenie::AlreadyMadeAwareOf(DWORD object_id)
{
	std::unordered_map<DWORD, double>::iterator entry = _objMadeAwareOf.find(object_id);

	if (entry != _objMadeAwareOf.end())
	{
		if ((entry->second + AWARENESS_TIMEOUT) > Timer::cur_time)
			return true;
	}

	return false;
}

void CPlayerWeenie::SetMadeAwareOf(DWORD object_id)
{
	_objMadeAwareOf[object_id] = Timer::cur_time;
}

void CPlayerWeenie::FlushMadeAwareof()
{
	for (std::unordered_map<DWORD, double>::iterator i = _objMadeAwareOf.begin(); i != _objMadeAwareOf.end();)
	{
		if ((i->second + AWARENESS_TIMEOUT) <= Timer::cur_time)
			i = _objMadeAwareOf.erase(i);
		else
			i++;
	}
}

void CPlayerWeenie::MakeAware(CWeenieObject *pEntity, bool bForceUpdate)
{
#ifndef PUBLIC_BUILD
	int vis;

	// Admins should always be aware of themselves. Allows logging in while invisible.
	if (pEntity != this && pEntity->m_Qualities.InqBool(VISIBILITY_BOOL, vis) && !m_bAdminVision)
		return;
#else
	int vis;
	if (pEntity->m_Qualities.InqBool(VISIBILITY_BOOL, vis))
		return;
#endif

	if (!bForceUpdate && AlreadyMadeAwareOf(pEntity->GetID()))
		return;

	SetMadeAwareOf(pEntity->GetID());

	BinaryWriter *CM = pEntity->CreateMessage();

	if (CM)
	{
		// LOG(Temp, Normal, "Sending object %X in cell %X\n", pEntity->GetID(), pEntity->m_Position.objcell_id);
		SendNetMessage(CM, OBJECT_MSG);
	}

	if (pEntity->children && pEntity->children->num_objects)
	{
		for (DWORD i = 0; i < pEntity->children->num_objects; i++)
		{
			if (CPhysicsObj *pChild = pEntity->children->objects.array_data[i])
			{
				if (CWeenieObject *pChildWeenie = pChild->weenie_obj)
				{
					if (BinaryWriter *CM = pChildWeenie->CreateMessage())
					{
						SendNetMessage(CM, OBJECT_MSG);
					}
				}
			}
		}
	}

	if (pEntity == this)
	{
		// make aware of inventory too
		for (auto item : m_Wielded)
		{
			MakeAware(item);
		}

		for (auto item : m_Items)
		{
			MakeAware(item);
		}

		for (auto item : m_Packs)
		{
			MakeAware(item);

			if (CContainerWeenie *container = item->AsContainer())
			{
				container->MakeAwareViewContent(this);
			}
		}
	}
}

void CPlayerWeenie::LoginCharacter(void)
{
	DWORD SC[2];

	SC[0] = 0xF746;
	SC[1] = GetID();
	SendNetMessage(SC, sizeof(SC), 10);

	BinaryWriter *LC = ::LoginCharacter(this);
	SendNetMessage(LC->GetData(), LC->GetSize(), PRIVATE_MSG, TRUE);
	delete LC;
}

void CPlayerWeenie::ExitPortal()
{
	if (_isFirstPortalInSession)
	{
		TryToUnloadAllegianceXP(true);
		_isFirstPortalInSession = false;
	}

	if (_phys_obj)
		_phys_obj->ExitPortal();
}

void CPlayerWeenie::SetLastHealthRequest(DWORD guid)
{
	m_LastHealthRequest = guid;

	m_NextHealthUpdate = Timer::cur_time + PLAYER_HEALTH_POLL_INTERVAL;
}

void CPlayerWeenie::RemoveLastHealthRequest()
{
	m_LastHealthRequest = 0;

	// nothing targeted so we don't want to send messages
	m_NextHealthUpdate = Timer::cur_time + 86400.0;
}

void CPlayerWeenie::RefreshTargetHealth()
{
	m_NextHealthUpdate = 0;
}

void CPlayerWeenie::SetLastAssessed(DWORD guid)
{
	m_LastAssessed = guid;
}

std::string CPlayerWeenie::RemoveLastAssessed(bool forced)
{
	if (m_LastAssessed != 0)
	{
		CWeenieObject *pObject = g_pWorld->FindWithinPVS(this, m_LastAssessed);

		if (pObject != NULL && !pObject->AsPlayer()) {

			if (forced || !pObject->m_bDontClear)
			{
					std::string name = pObject->GetName();
					pObject->MarkForDestroy();
					m_LastAssessed = 0;
					return name;
			}
		}
	}

	return "";
}

void CPlayerWeenie::OnDeathAnimComplete()
{
	CMonsterWeenie::OnDeathAnimComplete();

	if (m_bReviveAfterAnim)
	{
		m_bReviveAfterAnim = false;

		if (!g_pConfig->HardcoreMode())
		{
			//clear damage sources on death
			m_aDamageSources.clear();
			m_highestDamageSource = 0;
			m_totalDamageTaken = 0;


			//if (IsDead())
			Revive();
		}
	}
}

void CPlayerWeenie::UpdateVitaePool(DWORD pool)
{
	m_Qualities.SetInt(VITAE_CP_POOL_INT, pool);
	NotifyIntStatUpdated(VITAE_CP_POOL_INT, true);
}

void CPlayerWeenie::ReduceVitae(float amount)
{
	int level = InqIntQuality(LEVEL_INT, 1);
	double maxVitaePenalty;
	if (level < 15)
		maxVitaePenalty = min(max((level - 1) * 3, 1) / 100.0, 0.4);
	else
		maxVitaePenalty = 0.4;
	double minVitaeEnergy = 1.0 - maxVitaePenalty;

	Enchantment enchant;
	if (m_Qualities.InqVitae(&enchant))
		enchant._smod.val -= amount;
	else
	{
		enchant._id = Vitae_SpellID;
		enchant.m_SpellSetID = 0; // ???
		enchant._spell_category = 204; // Vitae_SpellCategory;
		enchant._power_level = 30;
		enchant._start_time = 0;
		enchant._duration = -1.0;
		enchant._caster = 0;
		enchant._degrade_modifier = 0;
		enchant._degrade_limit = 1;
		enchant._last_time_degraded = 0;
		enchant._smod.type = SecondAtt_EnchantmentType | Skill_EnchantmentType | MultipleStat_EnchantmentType | Multiplicative_EnchantmentType | Additive_Degrade_EnchantmentType | Vitae_EnchantmentType;
		enchant._smod.key = 0;
		enchant._smod.val = 1.0f - amount;
	}

	if (enchant._smod.val < minVitaeEnergy)
		enchant._smod.val = minVitaeEnergy;
	if (enchant._smod.val > 1.0f)
		enchant._smod.val = 1.0f;

	m_Qualities.UpdateEnchantment(&enchant);
	UpdateVitaeEnchantment();

	CheckVitalRanges();
}

void CPlayerWeenie::UpdateVitaeEnchantment()
{
	Enchantment enchant;
	if (m_Qualities.InqVitae(&enchant))
	{
		if (enchant._smod.val < 1.0f)
		{
			NotifyEnchantmentUpdated(&enchant);
		}
		else
		{
			PackableListWithJson<DWORD> expired;
			expired.push_back(666);

			if (m_Qualities._enchantment_reg)
			{
				m_Qualities._enchantment_reg->RemoveEnchantments(&expired);
			}

			BinaryWriter expireMessage;
			expireMessage.Write<DWORD>(0x2C5);
			expired.Pack(&expireMessage);
			SendNetMessage(&expireMessage, PRIVATE_MSG, TRUE, FALSE);
			EmitSound(Sound_SpellExpire, 1.0f, true);
		}
	}
}

void CPlayerWeenie::OnGivenXP(long long amount, bool allegianceXP)
{
	if (m_Qualities.GetVitaeValue() < 1.0 && !allegianceXP)
	{
		DWORD64 vitae_pool = InqIntQuality(VITAE_CP_POOL_INT, 0) + min(amount, 1000000000ll);
		float new_vitae = 1.0;
		bool has_new_vitae = VitaeSystem::DetermineNewVitaeLevel(m_Qualities.GetVitaeValue(), InqIntQuality(DEATH_LEVEL_INT, 1), &vitae_pool, &new_vitae);

		UpdateVitaePool(vitae_pool);

		if (has_new_vitae)
		{
			if (new_vitae < 1.0f)
			{
				SendText("Your experience has reduced your Vitae penalty!", LTT_MAGIC);
			}

			Enchantment enchant;
			if (m_Qualities.InqVitae(&enchant))
			{
				enchant._smod.val = new_vitae;
				m_Qualities.UpdateEnchantment(&enchant);
			}

			UpdateVitaeEnchantment();

			CheckVitalRanges();
		}
	}
	else
	{
		// there should be no vitae...
		UpdateVitaeEnchantment();
	}
}

void addItemsToDropLists(PhysObjVector items, std::vector<CWeenieObject *> &removeList, std::vector<CWeenieObject *> &alwaysDropList, std::vector<CWeenieObject *> &allValidItems)
{
	for (auto item : items)
	{
		if (item->m_Qualities.id == W_COINSTACK_CLASS)
			continue;
		else if (item->IsDestroyedOnDeath())
			removeList.push_back(item);
		else if (item->IsDroppedOnDeath())
			alwaysDropList.push_back(item);
		else if (!item->IsBonded())
			allValidItems.push_back(item);
	}
}

//TODO Add nonwielded handling for level > 11 && level < 35
void CPlayerWeenie::CalculateAndDropDeathItems(CCorpseWeenie *pCorpse, DWORD killer_id)
{
	if (!pCorpse)
		return;

	if (IsBonded()) //we're using the bonded property on the player to determine if they should or not drop items on death.
		return;

	int level = InqIntQuality(LEVEL_INT, 1);
	CWeenieObject *pKiller = g_pWorld->FindObject(killer_id);
	int amountOfItemsToDrop = 0;
	int augDropLess = InqIntQuality(AUGMENTATION_LESS_DEATH_ITEM_LOSS_INT, 0); // Take Death Item Augs into Consideration
	if (level <= 10)
	{
		amountOfItemsToDrop = 0;
	}
	else if (level <= 20)
	{
		amountOfItemsToDrop = 1;
	}
	else
	{
		amountOfItemsToDrop = floor(level / 20) + Random::GenInt(0, 2);
	}

	// either we can't find a killer (killed self?) or the killer is not a player
	if (!pKiller || !pKiller->_IsPlayer())
		amountOfItemsToDrop = max(0, (amountOfItemsToDrop - (augDropLess * 5)));

	pCorpse->_begin_destroy_at = Timer::cur_time + max((60.0 * 5 * level), 60.0 * 60); //override corpse decay time to 5 minutes per level with a minimum of 1 hour.
	pCorpse->_shouldSave = true;
	pCorpse->m_bDontClear = true;

	DWORD coinConsumed = 0;
	if (level > 5)
	{
		coinConsumed = ConsumeCoin(RecalculateCoinAmount(W_COINSTACK_CLASS) / 2, W_COINSTACK_CLASS);
		pCorpse->SpawnInContainer(W_COINSTACK_CLASS, coinConsumed);
	}

	std::vector<CWeenieObject *> removeList;
	std::vector<CWeenieObject *> alwaysDropList;
	std::vector<CWeenieObject *> allValidItems;

	addItemsToDropLists(m_Wielded, removeList, alwaysDropList, allValidItems);
	addItemsToDropLists(m_Items, removeList, alwaysDropList, allValidItems);

	for (auto packAsWeenie : m_Packs)
	{
		CContainerWeenie *pack = packAsWeenie->AsContainer();
		if (pack)
		{
			addItemsToDropLists(pack->m_Items, removeList, alwaysDropList, allValidItems);
		}
	}

	for (auto item : removeList)
		item->Remove();

	for (auto item : alwaysDropList)
		FinishMoveItemToContainer(item, pCorpse, 0, true, true);


	// This section calculates the effective value for each item (halved for each previous dupe)
	std::multimap <int, CWeenieObject *> itemValueList;
	std::map<std::string, int> itemNameList;
	std::string objName;
	for (auto item : allValidItems)
	{
		// if this item is stackable
		if (item->m_Qualities.GetInt(MAX_STACK_SIZE_INT, 0))
		{
			int stack_sz = item->InqIntQuality(STACK_SIZE_INT, 0);
			int value = item->InqIntQuality(STACK_UNIT_VALUE_INT, 0);

			objName = item->InqStringQuality(NAME_STRING, objName);

			// initialise to 0 (this will silently fail if it already exists)
			itemNameList.insert(std::pair<std::string, int>(objName, 0));

			// reduce value by half for all previous items with same name
			value /= 1 << min(15, itemNameList[objName]); // 15 to prevent overflow

			itemNameList[objName] += stack_sz;

			for (int i = 0; i < stack_sz; i++)
			{
				itemValueList.insert(std::pair<int, CWeenieObject *>(value, item));

				// reduce value by half again
				value /= 2;
			}
		}
		else // not a stack
		{
			int value = item->GetValue();

			// from item->getName - if it has a material type then it's a randomly generated item and we don't want to add it to the dupe list
			if (item->InqIntQuality(MATERIAL_TYPE_INT, 0))
			{
				objName = item->InqStringQuality(NAME_STRING, objName);

				// initialise to 0 (this will silently fail if it already exists)
				itemNameList.insert(std::pair<std::string, int>(objName, 0));

				// reduce value by half for all previous items with same name
				value /= 1 << itemNameList[objName];

				itemNameList[objName]++;
			}

			itemValueList.insert(std::pair<int, CWeenieObject *>(value, item));
		}
	}

	// cap the amount of items to drop to the number of items we can drop so the message properly ends with " and "
	amountOfItemsToDrop = min(amountOfItemsToDrop, (int)itemValueList.size());
	int itemsLost = 0;

	std::string itemsLostText;
	if (coinConsumed)
	{
		itemsLostText = csprintf("You've lost %s Pyreal%s", FormatNumberString(coinConsumed).c_str(), coinConsumed > 1 ? "s" : "");

		// increment itemsLost so the next item starts with " and your " or ", your"
		itemsLost++;
		amountOfItemsToDrop++;
	}
	else
	{
		itemsLostText = "You've lost ";
	}

	// START item dropping BY VALUE
	for (auto iter = itemValueList.rbegin(); iter != itemValueList.rend(); ++iter)
	{
		if (itemsLost >= amountOfItemsToDrop)
		{
			break;
		}

		if (iter->second->InqIntQuality(STACK_SIZE_INT, 1) > 1)
		{
			iter->second->DecrementStackNum();
			pCorpse->SpawnCloneInContainer(iter->second, 1);
		}
		else
		{
			FinishMoveItemToContainer(iter->second, pCorpse, 0, true, true);
		}

		itemsLost++;

		if (amountOfItemsToDrop > 1 && itemsLost == amountOfItemsToDrop)
			itemsLostText.append(" and your ");
		else if (itemsLost > 1)
			itemsLostText.append(", your ");
		else
			itemsLostText.append("your ");

		itemsLostText.append(iter->second->GetName());
	}

	itemsLostText.append("!");

	DEATH_LOG << InqStringQuality(NAME_STRING, "") << "-" << itemsLostText;
	if (coinConsumed || itemsLost)
		SendText(itemsLostText.c_str(), LTT_DEFAULT);

	if (_pendingCorpse)
	{
		//make the player corpse visible.
		_pendingCorpse->m_Qualities.RemoveBool(VISIBILITY_BOOL);
		_pendingCorpse->NotifyBoolStatUpdated(VISIBILITY_BOOL, false);
		_pendingCorpse->NotifyObjectCreated(false);
		_pendingCorpse->Save();
		_pendingCorpse = NULL;
	}
}

void CPlayerWeenie::OnDeath(DWORD killer_id)
{
	_recallTime = -1.0; // cancel any portal recalls

	m_bReviveAfterAnim = true;
	m_bChangingStance = false;
	CMonsterWeenie::OnDeath(killer_id);

	m_bChangingStance = false;

	m_Qualities.SetFloat(DEATH_TIMESTAMP_FLOAT, Timer::cur_time);
	NotifyFloatStatUpdated(DEATH_TIMESTAMP_FLOAT);

	m_Qualities.SetInt(DEATH_LEVEL_INT, m_Qualities.GetInt(LEVEL_INT, 1));
	NotifyIntStatUpdated(DEATH_LEVEL_INT, true);

	if ((m_Position.objcell_id & 0xFFFF) < 0x100) //outdoors
	{
		m_Qualities.SetPosition(LAST_OUTSIDE_DEATH_POSITION, m_Position);
		NotifyPositionStatUpdated(LAST_OUTSIDE_DEATH_POSITION, true);
	}

	UpdateVitaePool(0);
	ReduceVitae(0.05f);
	UpdateVitaeEnchantment();
	ClearPKActivity();

	bool isPkKill = false;

	if (killer_id != GetID())
	{
		if (CWeenieObject *pKiller = g_pWorld->FindObject(killer_id))
		{
			if (IsPK() && pKiller->_IsPlayer())
			{
				m_Qualities.SetFloat(PK_TIMESTAMP_FLOAT, Timer::cur_time + g_pConfig->PKRespiteTime());
				m_Qualities.SetInt(PLAYER_KILLER_STATUS_INT, PKStatusEnum::NPK_PKStatus);
				NotifyIntStatUpdated(PLAYER_KILLER_STATUS_INT, false);
				isPkKill = true;
				NotifyWeenieError(WERROR_PK_SWITCH_RESPITE);
				SendText("Bael'Zharon has granted you respite after your moment of weakness. You are temporarily no longer a player killer.", LTT_MAGIC);
			}
		}
	}

	// create corpse but make it invisible.
	_pendingCorpse = CreateCorpse(false);

	if (_pendingCorpse)
	{
		if (isPkKill)
		{
			_pendingCorpse->m_Qualities.SetBool(PK_KILLER_BOOL, 1); // flag the corpse as one made by a PK
			isPkKill = false;
		}

		CalculateAndDropDeathItems(_pendingCorpse, killer_id);
	}

	if (g_pConfig->HardcoreMode())
	{
		OnDeathAnimComplete();
	}
}

void CPlayerWeenie::NotifyAttackDone(int error)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x1A7);
	msg.Write<int>(error); // no error
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyCommenceAttack()
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x1B8);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::OnMotionDone(DWORD motion, BOOL success)
{
	CMonsterWeenie::OnMotionDone(motion, success);

	m_bChangingStance = false;

	//if (IsAttackMotion(motion) && success)
	//{
	//	NotifyAttackDone();

	//	/*
	//	if (ShouldRepeatAttacks() && m_LastAttackTarget)
	//	{
	//		NotifyCommenceAttack();

	//		m_bChargingAttack = true;
	//		m_ChargingAttackHeight = m_LastAttackHeight;
	//		m_ChargingAttackTarget = m_LastAttackTarget;
	//		m_ChargingAttackPower = m_LastAttackPower;
	//		m_fChargeAttackStartTime = Timer::cur_time;
	//	}
	//	*/
	//}
}

void CPlayerWeenie::OnRegen(STypeAttribute2nd currentAttrib, int newAmount)
{
	CMonsterWeenie::OnRegen(currentAttrib, newAmount);

	NotifyAttribute2ndStatUpdated(currentAttrib);
}

void CPlayerWeenie::NotifyAttackerEvent(const char *name, unsigned int dmgType, float healthPercent, unsigned int health, unsigned int crit, unsigned int attackConditions)
{
	// when the player deals damage
	BinaryWriter msg;
	msg.Write<DWORD>(0x1B1);
	msg.WriteString(name);
	msg.Write<DWORD>(dmgType);
	msg.Write<double>(healthPercent);
	msg.Write<DWORD>(health);
	msg.Write<DWORD>(crit);
	msg.Write<DWORD64>(attackConditions);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);

	// Update health of monster ASAP
	RefreshTargetHealth();
}

void CPlayerWeenie::NotifyDefenderEvent(const char *name, unsigned int dmgType, float healthPercent, unsigned int health, BODY_PART_ENUM hitPart, unsigned int crit, unsigned int attackConditions)
{
	// when the player receives damage
	BinaryWriter msg;
	msg.Write<DWORD>(0x1B2);
	msg.WriteString(name);
	msg.Write<DWORD>(dmgType);
	msg.Write<double>(healthPercent);
	msg.Write<DWORD>(health);
	msg.Write<int>(hitPart);
	msg.Write<DWORD>(crit);
	msg.Write<DWORD>(attackConditions);
	msg.Write<DWORD>(0); // probably DWORD align
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyKillerEvent(const char *text)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x1AD);
	msg.WriteString(text);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyVictimEvent(const char *text)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x1AC);
	msg.WriteString(text);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyUseDone(int error)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x1C7);
	msg.Write<int>(error);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyWeenieError(int error)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x28A);
	msg.Write<int>(error);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyWeenieErrorWithString(int error, const char *text)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x28B);
	msg.Write<int>(error);
	msg.WriteString(text);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::NotifyInventoryFailedEvent(DWORD object_id, int error)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0xA0);
	msg.Write<DWORD>(object_id);
	msg.Write<int>(error);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

bool CPlayerWeenie::ImmuneToDamage(CWeenieObject *other)
{
	if (this != other)
	{
		if (other && other->AsPlayer())
		{
			if (IsPK() && other->IsPK())
			{
			}
			else if (IsPKLite() && other->IsPKLite())
			{
			}
			else
			{
				PKStatusEnum selfStatus = (PKStatusEnum)InqIntQuality(PLAYER_KILLER_STATUS_INT, PKStatusEnum::Undef_PKStatus);
				PKStatusEnum otherStatus = (PKStatusEnum)other->InqIntQuality(PLAYER_KILLER_STATUS_INT, PKStatusEnum::Undef_PKStatus);

				if (selfStatus == PKStatusEnum::Baelzharon_PKStatus || otherStatus == PKStatusEnum::Baelzharon_PKStatus)
				{
				}
				else
					return true;
			}
		}
	}

	return CMonsterWeenie::ImmuneToDamage(other);
}

bool CPlayerWeenie::IsDead()
{
	return CMonsterWeenie::IsDead();
}

DWORD CPlayerWeenie::OnReceiveInventoryItem(CWeenieObject *source, CWeenieObject *item, DWORD desired_slot)
{
	item->ReleaseFromAnyWeenieParent(false, true);
	item->SetWieldedLocation(INVENTORY_LOC::NONE_LOC);

	item->SetWeenieContainer(GetID());
	item->ReleaseFromBlock();

	DWORD result = Container_InsertInventoryItem(source->GetLandcell(), item, desired_slot);

	if (AsPlayer() && item->IsCurrency(item->m_Qualities.id))
		RecalculateCoinAmount(item->m_Qualities.id);

	return result;
}

/*
void CPlayerWeenie::HandleKilledEvent(CWeenieObject *victim, DAMAGE_TYPE damageType)
{
	switch (damageType)
	{
	case BLUDGEON_DAMAGE_TYPE:
		{
			switch (Random::GenInt(0, 7))
			{
			case 0:
				{
					NotifyKillerEvent(csprintf("You flatten %s's body with the force of your assault!", pTarget->GetName().c_str()));
					break;
				}
			case 1:
				{
					NotifyKillerEvent(csprintf("You beat %s to a lifeless pulp!", pTarget->GetName().c_str()));
					break;
				}
			case 2:
				{
					NotifyKillerEvent(csprintf("You smite %s mightily!", pTarget->GetName().c_str()));
					break;
				}
			case 3:
				{
					NotifyKillerEvent(csprintf("You knock %s into next Morningthaw!", pTarget->GetName().c_str()));
					break;
				}
			case 4:
				{
					NotifyKillerEvent(csprintf("%s is utterly destroyed by your attack!", pTarget->GetName().c_str()));
					break;
				}
			case 5:
				{
					NotifyKillerEvent(csprintf("%s catches your attack, with dire consequences!", pTarget->GetName().c_str()));
					break;
				}
			case 6:
				{
					NotifyKillerEvent(csprintf("The deadly force of your attack is so strong that %s's ancestors feel it!", pTarget->GetName().c_str()));
					break;
				}
			case 7:
				{
					NotifyKillerEvent(csprintf("The thunder of crushing %s is followed by the deafening silence of death!", pTarget->GetName().c_str()));
					break;
				}
			}
		}
	}
}
*/

/*
void CPlayerWeenie::NotifyVictimEvent(CWeenieObject *killer, DAMAGE_TYPE damageType)
{

}

void CPlayerWeenie::OnDealtDamage(CWeenieObject *attacker, DAMAGE_TYPE damageType, unsigned int damage)
{
	if (!pTarget->IsDead())
	{
		NotifyAttackerEvent(pTarget->GetName().c_str(), 4, damageDone / (double)(pTarget->GetMaxHealth()), damageDone, 0, 0);
	}
	else
	{
		switch (Random::GenInt(0, 7))
		{
		case 0:
			{
				NotifyKillerEvent(csprintf("You flatten %s's body with the force of your assault!", pTarget->GetName().c_str()));
				break;
			}
		case 1:
			{
				NotifyKillerEvent(csprintf("You beat %s to a lifeless pulp!", pTarget->GetName().c_str()));
				break;
			}
		case 2:
			{
				NotifyKillerEvent(csprintf("You smite %s mightily!", pTarget->GetName().c_str()));
				break;
			}
		case 3:
			{
				NotifyKillerEvent(csprintf("You knock %s into next Morningthaw!", pTarget->GetName().c_str()));
				break;
			}
		case 4:
			{
				NotifyKillerEvent(csprintf("%s is utterly destroyed by your attack!", pTarget->GetName().c_str()));
				break;
			}
		case 5:
			{
				NotifyKillerEvent(csprintf("%s catches your attack, with dire consequences!", pTarget->GetName().c_str()));
				break;
			}
		case 6:
			{
				NotifyKillerEvent(csprintf("The deadly force of your attack is so strong that %s's ancestors feel it!", pTarget->GetName().c_str()));
				break;
			}
		case 7:
			{
				NotifyKillerEvent(csprintf("The thunder of crushing %s is followed by the deafening silence of death!", pTarget->GetName().c_str()));
				break;
			}
		}
	}
}
*/

void CPlayerWeenie::PreSpawnCreate()
{
}

struct CompareManaNeeds //: public std::function<CWeenieObject*, CWeenieObject*, bool>
{
	bool operator()(CWeenieObject* left, CWeenieObject* right)
	{
		// comparator for making a min-heap based on remaining mana
		return ((left->InqIntQuality(ITEM_MAX_MANA_INT, 0, TRUE) - left->InqIntQuality(ITEM_CUR_MANA_INT, -1, TRUE))
			> (right->InqIntQuality(ITEM_MAX_MANA_INT, 0, TRUE) - right->InqIntQuality(ITEM_CUR_MANA_INT, -1, TRUE)));
	}
};

int CPlayerWeenie::UseEx(CWeenieObject *pTool, CWeenieObject *pTarget)
{
	// Save these for later as we may have to send a confirmation
	m_pCraftingTool = pTool;
	m_pCraftingTarget = pTarget;

	return UseEx(false);
}
int CPlayerWeenie::UseEx(bool bConfirmed)
{
	// Load the saved crafting targets
	CWeenieObject *pTool = m_pCraftingTool;
	CWeenieObject *pTarget = m_pCraftingTarget;

	if (pTool == NULL || pTarget == NULL)
	{
		// no queued crafting op
		return WERROR_NONE;
	}

	if (pTool->m_Qualities.m_WeenieType == AugmentationDevice_WeenieType)
		pTool->AsAugmentationDevice()->UseEx(this, bConfirmed);

	int toolType = pTool->InqIntQuality(ITEM_TYPE_INT, 0);

	switch (toolType)
	{
	case TYPE_MANASTONE:
	{
		pTool->UseWith(this, pTarget);
		break;
	}
	default:
	{
		if (AsPlayer()->IsInPortalSpace())
			return WERROR_ACTIONS_LOCKED;

		CCraftOperation *op = g_pPortalDataEx->GetCraftOperation(pTool->m_Qualities.id, pTarget->m_Qualities.id);
		if (!op)
		{
			//Try using our get alternative operation function for Rare Dyes, Infinite Leather, etc.
			op = TryGetAlternativeOperation(pTarget, pTool, op);

			if (!op)
			{
				//Try inversing the combination.
				op = g_pPortalDataEx->GetCraftOperation(pTarget->m_Qualities.id, pTool->m_Qualities.id);
					
				if (!op)
					return WERROR_NONE;

				//swap things around
				CWeenieObject *swapHelper = pTool;
				pTool = pTarget;
				pTarget = swapHelper;
			}
		}

		if (pTool->IsWielded() || pTarget->IsWielded())
			return WERROR_CRAFT_ALL_OBJECTS_NOT_FROZEN;

		if (get_minterp()->interpreted_state.current_style != Motion_NonCombat)
			return WERROR_CRAFT_NOT_IN_PEACE_MODE;

		//if (GetWielded(WEAPON_LOC) != NULL || GetWielded(SHIELD_LOC) != NULL)
		//	return WERROR_HANDS_NOT_FREE;

		int requiredHealth = 0;
		int requiredStamina = 0;
		int requiredMana = 0;
		for (int i = 0; i < 4; i++)
		{
			if (op->_mods[i]._RequiresHealth)
				requiredHealth += -op->_mods[i]._ModifyHealth;
			if (op->_mods[i]._RequiresStamina)
				requiredStamina += -op->_mods[i]._ModifyStamina;
			if (op->_mods[i]._RequiresMana)
				requiredMana += -op->_mods[i]._ModifyMana;
		}

		if (GetHealth() < requiredHealth)
		{
			SendText("You don't have enough health to do that.", LTT_CRAFT);
			return WERROR_CRAFT_FAILED_REQUIREMENTS;
		}

		if (requiredStamina != 0)
		{
			if (GetStamina() < requiredStamina)
			{
				//SendText("You don't have enough stamina to do that.", LTT_CRAFT);
				return WERROR_STAMINA_TOO_LOW;
			}
		}
		//else if (GetStamina() < 5) //I can't find a source but I'm pretty sure use actions always consumed some amount of stamina.
		//{
		//	//SendText("You don't have enough stamina to do that.", LTT_CRAFT);
		//	return WERROR_STAMINA_TOO_LOW;
		//}

		if (GetMana() < requiredMana)
		{
			//SendText("You don't have enough mana to do that.", LTT_CRAFT);
			return WERROR_MAGIC_INSUFFICIENT_MANA;
		}

		//unk: always 0
		//op->_mod unknown7 - known values: true, false - true on dying items? = Recalculate Icon?
		//op->_mod unknown9 - known values: 1 in stamping failures, attaching fetish of the dark idol
		//op->_mod unknown10 - always 0

		if (!CheckUseRequirements(0, op, pTool, pTarget))
			return WERROR_CRAFT_FAILED_REQUIREMENTS;
		if (!CheckUseRequirements(1, op, pTool, pTarget))
			return WERROR_CRAFT_FAILED_REQUIREMENTS;
		if (!CheckUseRequirements(2, op, pTool, pTarget))
			return WERROR_CRAFT_FAILED_REQUIREMENTS;

		int targetStackSize = pTarget->GetStackOrStructureNum();
		int toolStackSize = pTool->GetStackOrStructureNum();

		if (targetStackSize < op->_successConsumeTargetAmount || toolStackSize < op->_successConsumeToolAmount ||
			targetStackSize < op->_failureConsumeTargetAmount || toolStackSize < op->_failureConsumeToolAmount)
			return WERROR_CRAFT_DONT_CONTAIN_EVERYTHING;

		DWORD skillLevel = 0;
		if (op->_skill != 0)
		{
			SKILL_ADVANCEMENT_CLASS sac;
			m_Qualities.InqSkillAdvancementClass((STypeSkill)op->_skill, sac);

			if (sac < TRAINED_SKILL_ADVANCEMENT_CLASS)
				return WERROR_CRAFT_NOT_HAVE_SKILL;

			InqSkill(op->_skill, skillLevel, FALSE);
		}

		double successChance;
		switch (op->_SkillCheckFormulaType)
		{
		case 0:
			if (skillLevel == 0)
			{
				// success = true
				successChance = 1.0;
			}
			else
			{
				successChance = GetSkillChance(skillLevel, op->_difficulty);
			}
			break;
		case 1: //tinkers
		case 2: //imbues
		{
			double toolWorkmanship = pTool->InqIntQuality(ITEM_WORKMANSHIP_INT, 0);
			double itemWorkmanship = pTarget->InqIntQuality(ITEM_WORKMANSHIP_INT, 0);
			toolWorkmanship /= (double)pTool->InqIntQuality(NUM_ITEMS_IN_MATERIAL_INT, 1);
			int amountOfTimesTinkered = pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0);

			if (amountOfTimesTinkered > 9)  // Don't allow 10 tinked items to have any more tinkers/imbues (Ivory & Leather don't use this case)
			{
				return WERROR_NONE;   
			}

			int salvageMod;
			
			if (op->_SkillCheckFormulaType == 1)
			{
				salvageMod = GetMaterialMod(pTool->InqIntQuality(MATERIAL_TYPE_INT, 0));
			}
			else
			{
				salvageMod = 20; // All imbue materials have mod of 20
			}

			int multiple = 1;
			double aDifficulty[10] = {1, 1.1, 1.3, 1.6, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5}; // Attempt difficulty numbers from Endy's Tinker Calc 2.3.2
			double difficulty = aDifficulty[amountOfTimesTinkered];

			if (toolWorkmanship >= itemWorkmanship)
			{
				multiple = 2;
			}

			successChance = GetSkillChance(skillLevel, ((int)floor(((5 * salvageMod) + (2 * itemWorkmanship * salvageMod) - (toolWorkmanship * multiple * salvageMod / 5)) * difficulty))); //Formulas from Endy's Tinkering Calculator

			if (op->_SkillCheckFormulaType == 2) // imbue
			{
				successChance /= 3;

				successChance = min(successChance, 0.33);
				
				if (m_Qualities.GetInt(AUGMENTATION_BONUS_IMBUE_CHANCE_INT, 0))
					successChance += 0.05;
			}

			switch (pTool->m_Qualities.id)
			{
			case W_MATERIALRAREFOOLPROOFAQUAMARINE_CLASS:
			case W_MATERIALRAREFOOLPROOFBLACKGARNET_CLASS:
			case W_MATERIALRAREFOOLPROOFBLACKOPAL_CLASS:
			case W_MATERIALRAREFOOLPROOFEMERALD_CLASS:
			case W_MATERIALRAREFOOLPROOFFIREOPAL_CLASS:
			case W_MATERIALRAREFOOLPROOFIMPERIALTOPAZ_CLASS:
			case W_MATERIALRAREFOOLPROOFJET_CLASS:
			case W_MATERIALRAREFOOLPROOFPERIDOT_CLASS:
			case W_MATERIALRAREFOOLPROOFREDGARNET_CLASS:
			case W_MATERIALRAREFOOLPROOFSUNSTONE_CLASS:
			case W_MATERIALRAREFOOLPROOFWHITESAPPHIRE_CLASS:
			case W_MATERIALRAREFOOLPROOFYELLOWTOPAZ_CLASS:
			case W_MATERIALRAREFOOLPROOFZIRCON_CLASS:
				successChance = 1.0;
			}


			break;
		}
		default:
			SendText(csprintf("Unsupported skill check formula type: %u.", op->_SkillCheckFormulaType), LTT_CRAFT);
			return WERROR_CRAFT_DONT_CONTAIN_EVERYTHING;
		}

		// 0x80000000 : Use Crafting Chance of Success Dialog
		if (_playerModule.options_ & 0x80000000 && !bConfirmed )
		{
			std::ostringstream sstrMessage;
			sstrMessage.precision(3);
			sstrMessage << "You have a " << successChance*100 << "% chance of using " << pTool->GetName() << " on " << pTarget->GetName() << ".";


			BinaryWriter confirmCrafting;
			confirmCrafting.Write<DWORD>(0x274);	// Message Type
			confirmCrafting.Write<DWORD>(0x05);		// Confirm type (craft)
			confirmCrafting.Write<int>(0);			// Sequence number??
			confirmCrafting.WriteString(sstrMessage.str());

			SendNetMessage(&confirmCrafting, PRIVATE_MSG, TRUE, FALSE);

			return WERROR_NONE;
		}

		double successRoll = Random::RollDice(0.0, 1.0);

		if (successRoll <= successChance)
		{
			// Results!
			// Create item(s)
			CWeenieObject *newItem = g_pWeenieFactory->CreateWeenieByClassID(op->_successWcid, NULL, false);
			if (op->_successAmount > 1)
				newItem->SetStackSize(op->_successAmount);

			// if we can't put the item in inventory
			if (op->_successWcid != 0 && !SpawnInContainer(newItem, op->_successAmount))
			{
				SendText("Unable to create result!", LTT_ERROR);
				// Return before we delete the tool/target
				// TODO: is this to do with pack space? should check before the roll if so
				return WERROR_NONE;
			}

			if (op->_successMessage != "")
				SendText(op->_successMessage.c_str(), LTT_CRAFT);
			
			// Broadcast messages for tinkering
			switch (op->_SkillCheckFormulaType)
			{
			case 1: //tinkers
			case 2: //imbues
			{
				double toolWorkmanship = pTool->InqIntQuality(ITEM_WORKMANSHIP_INT, 0);
				double itemWorkmanship = pTarget->InqIntQuality(ITEM_WORKMANSHIP_INT, 0);
				if (pTool->InqIntQuality(ITEM_TYPE_INT, 0) == ITEM_TYPE::TYPE_TINKERING_MATERIAL)
					toolWorkmanship /= (double)pTool->InqIntQuality(NUM_ITEMS_IN_MATERIAL_INT, 1);
				int amountOfTimesTinkered = pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0);


				std::string text = csprintf("%s successfully applies the %s (workmanship %.2f) to the %s.", GetName().c_str(), pTool->GetName().c_str(), toolWorkmanship, pTarget->GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(GetLandcell(), text);
				}

				IMBUE_LOG << "P:" << InqStringQuality(NAME_STRING, "") << " SL:" << skillLevel << " T:" << pTarget->InqStringQuality(NAME_STRING, "") << " TW:" << itemWorkmanship << " TT:" << amountOfTimesTinkered <<
					" M:" << pTool->InqStringQuality(NAME_STRING, "") << " MW:" << toolWorkmanship << " %:" << successChance << " Roll:" << successRoll;

				break;
			}
			}

			PerformUseModifications(0, op, pTool, pTarget, newItem);
			PerformUseModifications(1, op, pTool, pTarget, newItem);
			PerformUseModifications(2, op, pTool, pTarget, newItem);
			PerformUseModifications(3, op, pTool, pTarget, newItem);

			if (op->_successConsumeTargetChance == 1.0 || Random::RollDice(0.0, 1.0) <= op->_successConsumeTargetChance)
			{
				pTarget->DecrementStackNum(op->_successConsumeTargetAmount);
				if (!op->_successConsumeTargetMessage.empty())
					SendText(op->_successConsumeTargetMessage.c_str(), LTT_CRAFT);
			}
			if (op->_successConsumeToolChance == 1.0 || Random::RollDice(0.0, 1.0) <= op->_successConsumeToolChance)
			{
				pTool->DecrementStackNum(op->_successConsumeToolAmount);
				if (!op->_successConsumeToolMessage.empty())
					SendText(op->_successConsumeToolMessage.c_str(), LTT_CRAFT);
			}
		}
		else // Not successful
		{
			// Results!
			// Create item(s)
			CWeenieObject *newItem = g_pWeenieFactory->CreateWeenieByClassID(op->_failWcid, NULL, false);
			if (op->_failAmount > 1)
				newItem->SetStackSize(op->_failAmount);

			// if we can't put the item in inventory
			if (op->_failWcid != 0 && !SpawnInContainer(newItem, op->_failAmount))
			{
				SendText("Unable to create result!", LTT_ERROR);
				// Return before we delete the tool/target
				// TODO: is this to do with pack space? should check before the roll if so
				return WERROR_NONE;
			}

			if (op->_failMessage != "")
				SendText(op->_failMessage.c_str(), LTT_CRAFT);

			// Broadcast messages for tinkering
			switch (op->_SkillCheckFormulaType)
			{
			case 1: //tinkers
			case 2: //imbues
			{
				double toolWorkmanship = pTool->InqIntQuality(ITEM_WORKMANSHIP_INT, 0);
				double itemWorkmanship = pTarget->InqIntQuality(ITEM_WORKMANSHIP_INT, 0);
				if (pTool->InqIntQuality(ITEM_TYPE_INT, 0) == ITEM_TYPE::TYPE_TINKERING_MATERIAL)
					toolWorkmanship /= (double)pTool->InqIntQuality(NUM_ITEMS_IN_MATERIAL_INT, 1);
				int amountOfTimesTinkered = pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0);


				std::string text = csprintf("%s fails to apply the %s (workmanship %.2f) to the %s. The target is destroyed.", GetName().c_str(), pTool->GetName().c_str(), toolWorkmanship, pTarget->GetName().c_str());
				if (!text.empty())
				{
					g_pWorld->BroadcastLocal(GetLandcell(), text);
				}

				IMBUE_LOG << "P:" << InqStringQuality(NAME_STRING, "") << " SL:" << skillLevel << " T:" << pTarget->InqStringQuality(NAME_STRING, "") << " TW:" << itemWorkmanship << " TT:" << amountOfTimesTinkered <<
					" M:" << pTool->InqStringQuality(NAME_STRING, "") << " MW:" << toolWorkmanship << " %:" << successChance << " Roll:" << successRoll;

				break;
			}
			}

			PerformUseModifications(4, op, pTool, pTarget, newItem);
			PerformUseModifications(5, op, pTool, pTarget, newItem);
			PerformUseModifications(6, op, pTool, pTarget, newItem);
			PerformUseModifications(7, op, pTool, pTarget, newItem);

			if (op->_failureConsumeTargetChance == 1.0 || Random::RollDice(0.0, 1.0) <= op->_failureConsumeTargetChance)
			{
				pTarget->DecrementStackNum(op->_failureConsumeTargetAmount);
				if (!op->_failureConsumeTargetMessage.empty())
					SendText(op->_failureConsumeTargetMessage.c_str(), LTT_CRAFT);
			}
			if (op->_failureConsumeToolChance == 1.0 || Random::RollDice(0.0, 1.0) <= op->_failureConsumeToolChance)
			{
				pTool->DecrementStackNum(op->_failureConsumeToolAmount);
				if (!op->_failureConsumeToolMessage.empty())
					SendText(op->_failureConsumeToolMessage.c_str(), LTT_CRAFT);
			}

			// Your craft attempt fails.
			NotifyWeenieError(0x432);
		}

		// We don't need these anymore
		m_pCraftingTool = NULL;
		m_pCraftingTarget = NULL;

		RecalculateEncumbrance();
		break;
	}
	}
	return WERROR_NONE;
}

int CPlayerWeenie::GetMaterialMod(int materialInt)
{
	switch (materialInt)
	{
	case Gold_MaterialType:
	case Oak_MaterialType:
	{
		return 10;
	}
	case Ebony_MaterialType:
	case Teak_MaterialType:
	case Steel_MaterialType :
	case Satin_MaterialType:
	case Porcelain_MaterialType:
	case Mahogany_MaterialType:
	case Iron_MaterialType:
	case Green_Garnet_MaterialType:
	{
		return 12;
	}
	case Alabaster_MaterialType:
	case Brass_MaterialType:
	case Armoredillo_Hide_MaterialType:
	case Wool_MaterialType:
	case Velvet_MaterialType:
	case Reed_Shark_Hide_MaterialType:
	case Pine_MaterialType:
	case Opal_MaterialType:
	case Marble_MaterialType:
	case Linen_MaterialType:
	case Granite_MaterialType:
	case Ceramic_MaterialType:
	case Bronze_MaterialType:
	case Moonstone_MaterialType:
	{
		return 11;
	}
	case Bloodstone_MaterialType:
	case Rose_Quartz_MaterialType:
	case Red_Jade_MaterialType:
	case Malachite_MaterialType:
	case Lavender_Jade_MaterialType:
	case Hematite_MaterialType:
	case Citrine_MaterialType:
	case Carnelian_MaterialType:
	{
		return 25;
	}
	default:
		return 20; // Imbue material
	}
}

std::string CPlayerWeenie::ToUpperCase(string tName)
{
	transform(tName.begin(), tName.end(), tName.begin(), ::toupper);
	return tName;
}

bool CPlayerWeenie::CheckUseRequirements(int index, CCraftOperation *op, CWeenieObject *pTool, CWeenieObject *pTarget)
{
	CWeenieObject *requirementTarget;
	switch (index)
	{
	case 0:
		requirementTarget = pTarget;
		break;
	case 1:
		requirementTarget = pTool;
		break;
	case 2:
		requirementTarget = this;
		break;
	default:
#ifdef _DEBUG
		assert(false);
#endif
		return false; //should never happen
	}

	if (!op->_requirements[index]._intRequirement.empty())
	{
		for each (TYPERequirement<STypeInt, int> intRequirement in op->_requirements[index]._intRequirement)
		{
			int value = 0;
			bool exists = requirementTarget->m_Qualities.InqInt(intRequirement._stat, value, true, true);

			switch (intRequirement._operationType)
			{
			case 0: //> - unconfirmed
				if (value > intRequirement._value)
				{
					SendText(intRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 1: //<=
				if (value <= intRequirement._value)
				{
					SendText(intRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 2: //<
				if (value < intRequirement._value)
				{
					SendText(intRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 3: //>=
				if (value >= intRequirement._value)
				{
					SendText(intRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 4: //!=
				if (value != intRequirement._value)
				{
					SendText(intRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 5: //doesnt exist or !=
				if (!exists || value != intRequirement._value)
				{
					SendText(intRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 6: //exists and ==
				if (exists && value == intRequirement._value)
				{
					SendText(intRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 7: //doesnt exist
				if (!exists)
				{
					SendText(intRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 8: //exists and != (or maybe just exists)
				if (exists && value != intRequirement._value)
				{
					SendText(intRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break;
			}
		}
	}

	if (!op->_requirements[index]._boolRequirement.empty())
	{
		for each (TYPERequirement<STypeBool, BOOL> boolRequirement in op->_requirements[index]._boolRequirement)
		{
			int value = 0;
			bool exists = requirementTarget->m_Qualities.InqBool(boolRequirement._stat, value);

			switch (boolRequirement._operationType)
			{
			case 0: //> - unconfirmed
				if (value > boolRequirement._value)
				{
					SendText(boolRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 1: //<=
				if (value <= boolRequirement._value)
				{
					SendText(boolRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 2: //<
				if (value < boolRequirement._value)
				{
					SendText(boolRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 3: //>=
				if (value >= boolRequirement._value)
				{
					SendText(boolRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 4: //!=
				if (value != boolRequirement._value)
				{
					SendText(boolRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 5: //doesnt exist or !=
				if (!exists || value != boolRequirement._value)
				{
					SendText(boolRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 6: //exists and ==
				if (exists && value == boolRequirement._value)
				{
					SendText(boolRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 7: //doesnt exist
				if (!exists)
				{
					SendText(boolRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 8: //exists and != (or maybe just exists)
				if (exists && value != boolRequirement._value)
				{
					SendText(boolRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break;
			}
		}
	}

	if (!op->_requirements[index]._floatRequirement.empty())
	{
		for each (TYPERequirement<STypeFloat, double> floatRequirement in op->_requirements[index]._floatRequirement)
		{
			double value = 0;
			bool exists = requirementTarget->m_Qualities.InqFloat(floatRequirement._stat, value, true);

			switch (floatRequirement._operationType)
			{
			case 0: //> - unconfirmed
				if (value > floatRequirement._value)
				{
					SendText(floatRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 1: //<=
				if (value <= floatRequirement._value)
				{
					SendText(floatRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 2: //<
				if (value < floatRequirement._value)
				{
					SendText(floatRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 3: //>=
				if (value >= floatRequirement._value)
				{
					SendText(floatRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 4: //!=
				if (value != floatRequirement._value)
				{
					SendText(floatRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 5: //doesnt exist or !=
				if (!exists || value != floatRequirement._value)
				{
					SendText(floatRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 6: //exists and ==
				if (exists && value == floatRequirement._value)
				{
					SendText(floatRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 7: //doesnt exist
				if (!exists)
				{
					SendText(floatRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 8: //exists and != (or maybe just exists)
				if (exists && value != floatRequirement._value)
				{
					SendText(floatRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break;
			}
		}
	}

	if (!op->_requirements[index]._stringRequirement.empty())
	{
		for each (TYPERequirement<STypeString, std::string> stringRequirement in op->_requirements[index]._stringRequirement)
		{
			std::string value = "";
			bool exists = requirementTarget->m_Qualities.InqString(stringRequirement._stat, value);

			switch (stringRequirement._operationType)
			{
			case 0: //> - unconfirmed
				if (value > stringRequirement._value)
				{
					SendText(stringRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 1: //<=
				if (value <= stringRequirement._value)
				{
					SendText(stringRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 2: //<
				if (value < stringRequirement._value)
				{
					SendText(stringRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 3: //>=
				if (value >= stringRequirement._value)
				{
					SendText(stringRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 4: //!=
				if (value != stringRequirement._value)
				{
					SendText(stringRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 5: //doesnt exist or !=
				if (!exists || value != stringRequirement._value)
				{
					SendText(stringRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 6: //exists and ==
				if (exists && value == stringRequirement._value)
				{
					SendText(stringRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 7: //doesnt exist
				if (!exists)
				{
					SendText(stringRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 8: //exists and != (or maybe just exists)
				if (exists && value != stringRequirement._value)
				{
					SendText(stringRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break;
			}
		}
	}

	if (!op->_requirements[index]._didRequirement.empty())
	{
		for each (TYPERequirement<STypeDID, DWORD> dIDRequirement in op->_requirements[index]._didRequirement)
		{
			DWORD value = 0;


			bool exists = requirementTarget->m_Qualities.InqDataID(dIDRequirement._stat, value);

			switch (dIDRequirement._operationType)
			{
			case 0: //> - unconfirmed
				if (value > dIDRequirement._value)
				{
					SendText(dIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 1: //<=
				if (value <= dIDRequirement._value)
				{
					SendText(dIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 2: //<
				if (value < dIDRequirement._value)
				{
					SendText(dIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 3: //>=
				if (value >= dIDRequirement._value)
				{
					SendText(dIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 4: //!=
				if (value != dIDRequirement._value)
				{
					SendText(dIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 5: //doesnt exist or !=
				if (!exists || value != dIDRequirement._value)
				{
					SendText(dIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 6: //exists and ==
				if (exists && value == dIDRequirement._value)
				{
					SendText(dIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 7: //doesnt exist
				if (!exists)
				{
					SendText(dIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 8: //exists and != (or maybe just exists)
				if (exists && value != dIDRequirement._value)
				{
					SendText(dIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break;
			}
		}
	}

	if (!op->_requirements[index]._iidRequirement.empty())
	{
		for each (TYPERequirement<STypeIID, DWORD> iIDRequirement in op->_requirements[index]._iidRequirement)
		{
			DWORD value = 0;
			bool exists = requirementTarget->m_Qualities.InqInstanceID(iIDRequirement._stat, value);

			switch (iIDRequirement._operationType)
			{
			case 0: //> - unconfirmed
				if (value > iIDRequirement._value)
				{
					SendText(iIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 1: //<=
				if (value <= iIDRequirement._value)
				{
					SendText(iIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 2: //<
				if (value < iIDRequirement._value)
				{
					SendText(iIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 3: //>=
				if (value >= iIDRequirement._value)
				{
					SendText(iIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 4: //!=
				if (value != iIDRequirement._value)
				{
					SendText(iIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 5: //doesnt exist or !=
				if (!exists || value != iIDRequirement._value)
				{
					SendText(iIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 6: //exists and ==
				if (exists && value == iIDRequirement._value)
				{
					SendText(iIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 7: //doesnt exist
				if (!exists)
				{
					SendText(iIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			case 8: //exists and != (or maybe just exists)
				if (exists && value != iIDRequirement._value)
				{
					SendText(iIDRequirement._message.c_str(), LTT_CRAFT);
					return false;
				}
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break;
			}
		}
	}

	return true;
}

void CPlayerWeenie::PerformUseModificationScript(DWORD scriptId, CCraftOperation *op, CWeenieObject *pTool, CWeenieObject *pTarget, CWeenieObject *pCreatedItem)
{
	double currentVar = pTarget->InqFloatQuality(DAMAGE_VARIANCE_FLOAT, 0, TRUE);
	switch (scriptId)
	{
	case 0x3800000f: //flag stamp failure
		//not sure what is supposed to be done here.
		break;
	case 0x38000011: //steel
		pTarget->m_Qualities.SetInt(ARMOR_LEVEL_INT, pTarget->InqIntQuality(ARMOR_LEVEL_INT, 0, TRUE) + 20);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000012: //armoredillo hide
		pTarget->m_Qualities.SetFloat(ARMOR_MOD_VS_ACID_FLOAT, min(pTarget->InqFloatQuality(ARMOR_MOD_VS_ACID_FLOAT, 0, TRUE) + 0.4, 2.0));
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000013: //marble
		pTarget->m_Qualities.SetFloat(ARMOR_MOD_VS_BLUDGEON_FLOAT, min(pTarget->InqFloatQuality(ARMOR_MOD_VS_BLUDGEON_FLOAT, 0, TRUE) + 0.2, 2.0));
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000014: //wool
		pTarget->m_Qualities.SetFloat(ARMOR_MOD_VS_COLD_FLOAT, min(pTarget->InqFloatQuality(ARMOR_MOD_VS_COLD_FLOAT, 0, TRUE) + 0.4, 2.0));
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000015: //reedshark hide
		pTarget->m_Qualities.SetFloat(ARMOR_MOD_VS_ELECTRIC_FLOAT, min(pTarget->InqFloatQuality(ARMOR_MOD_VS_ELECTRIC_FLOAT, 0, TRUE) + 0.4, 2.0));
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000016: //ceramic
		pTarget->m_Qualities.SetFloat(ARMOR_MOD_VS_FIRE_FLOAT, min(pTarget->InqFloatQuality(ARMOR_MOD_VS_FIRE_FLOAT, 0, TRUE) + 0.4, 2.0));
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000017: //alabaster
		pTarget->m_Qualities.SetFloat(ARMOR_MOD_VS_PIERCE_FLOAT, min(pTarget->InqFloatQuality(ARMOR_MOD_VS_PIERCE_FLOAT, 0, TRUE) + 0.2, 2.0));
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000018: //bronze
		pTarget->m_Qualities.SetFloat(ARMOR_MOD_VS_SLASH_FLOAT, min(pTarget->InqFloatQuality(ARMOR_MOD_VS_SLASH_FLOAT, 0, TRUE) + 0.2, 2.0));
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000019: //linen
	{
		double newEncumbrance = pTarget->InqIntQuality(ENCUMB_VAL_INT, 1, TRUE);
		newEncumbrance = max(newEncumbrance * 0.85, 1.0);
		pTarget->m_Qualities.SetInt(ENCUMB_VAL_INT, (int)round(newEncumbrance));
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	}
	case 0x3800001a: //iron
		pTarget->m_Qualities.SetInt(DAMAGE_INT, pTarget->InqIntQuality(DAMAGE_INT, 0, TRUE) + 1);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800001b: //mahogany
		pTarget->m_Qualities.SetFloat(DAMAGE_MOD_FLOAT, pTarget->InqFloatQuality(DAMAGE_MOD_FLOAT, 0, TRUE) + 0.04);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800001c: //granite
		pTarget->m_Qualities.SetFloat(DAMAGE_VARIANCE_FLOAT, max(currentVar - (currentVar / 5), 0.0));
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800001d: //oak
		pTarget->m_Qualities.SetInt(WEAPON_TIME_INT, max(pTarget->InqIntQuality(WEAPON_TIME_INT, 0, TRUE) - 50, 0));
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800001e: //pine
		pTarget->m_Qualities.SetInt(VALUE_INT, max(pTarget->InqIntQuality(VALUE_INT, 0, TRUE) * 0.75, 1.0));
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800001f: //gold
		pTarget->m_Qualities.SetInt(VALUE_INT, pTarget->InqIntQuality(VALUE_INT, 0, TRUE) * 1.25);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000020: //brass
		pTarget->m_Qualities.SetFloat(WEAPON_DEFENSE_FLOAT, pTarget->InqFloatQuality(WEAPON_DEFENSE_FLOAT, 0, TRUE) + 0.01);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000021: //velvet
		pTarget->m_Qualities.SetFloat(WEAPON_OFFENSE_FLOAT, pTarget->InqFloatQuality(WEAPON_OFFENSE_FLOAT, 0, TRUE) + 0.01);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000023: //black opal
		pTarget->AddImbueEffect(ImbuedEffectType::CriticalStrike_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000024: //fire opal
		pTarget->AddImbueEffect(ImbuedEffectType::CripplingBlow_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000025: //sunstone
		pTarget->AddImbueEffect(ImbuedEffectType::ArmorRending_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800002e: //opal
		pTarget->m_Qualities.SetFloat(MANA_CONVERSION_MOD_FLOAT, pTarget->InqFloatQuality(MANA_CONVERSION_MOD_FLOAT, 0, TRUE) + 0.01);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800002f: //moonstone
		pTarget->m_Qualities.SetInt(ITEM_MAX_MANA_INT, max(pTarget->InqIntQuality(ITEM_MAX_MANA_INT, 0, TRUE) + 500, 0));
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000034: //silver
	{
		STypeSkill oldRequirement = (STypeSkill)pTarget->InqIntQuality(WIELD_SKILLTYPE_INT, 0);
		if (oldRequirement == MELEE_DEFENSE_SKILL)
		{
			pTarget->m_Qualities.SetInt(WIELD_SKILLTYPE_INT, MISSILE_DEFENSE_SKILL);
			pTarget->m_Qualities.SetDataID(ITEM_SKILL_LIMIT_DID, MISSILE_DEFENSE_SKILL);
			int oldValue = pTarget->InqIntQuality(WIELD_DIFFICULTY_INT, 0);
			int newValue;
			switch (oldValue)
			{
			case 200: newValue = 160; break;
			case 250: newValue = 205; break;
			case 300: newValue = 245; break;
			case 325: newValue = 270; break;
			case 350: newValue = 290; break;
			case 370: newValue = 305; break;
			case 400: newValue = 330; break;
			default:
				newValue = oldValue * 0.8; //todo: figure out the exact formula for this conversion.
				break;
			}

			pTarget->m_Qualities.SetInt(WIELD_DIFFICULTY_INT, newValue);
			pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		}
		break;
	}
	case 0x38000035: //copper
	{
		STypeSkill oldRequirement = (STypeSkill)pTarget->InqIntQuality(WIELD_SKILLTYPE_INT, 0);
		if (oldRequirement == MISSILE_DEFENSE_SKILL)
		{
			pTarget->m_Qualities.SetInt(WIELD_SKILLTYPE_INT, MELEE_DEFENSE_SKILL);
			pTarget->m_Qualities.SetDataID(ITEM_SKILL_LIMIT_DID, MELEE_DEFENSE_SKILL);
			int oldValue = pTarget->InqIntQuality(WIELD_DIFFICULTY_INT, 0);
			int newValue;
			switch (oldValue)
			{
			case 160: newValue = 200; break;
			case 205: newValue = 250; break;
			case 245: newValue = 300; break;
			case 270: newValue = 325; break;
			case 290: newValue = 350; break;
			case 305: newValue = 370; break;
			case 330: newValue = 400; break;
			default:
				newValue = oldValue * 1.25; //todo: figure out the exact formula for this conversion.
				break;
			}

			pTarget->m_Qualities.SetInt(WIELD_DIFFICULTY_INT, newValue);
			pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		}
		break;
	}
	case 0x38000036: //silk
	{
		int allegianceRank;
		if (pTarget->m_Qualities.InqInt(ITEM_ALLEGIANCE_RANK_LIMIT_INT, allegianceRank, TRUE))
		{
			pTarget->m_Qualities.RemoveInt(ITEM_ALLEGIANCE_RANK_LIMIT_INT);
			int spellCraft = pTarget->InqIntQuality(ITEM_SPELLCRAFT_INT, 0, TRUE);
			pTarget->m_Qualities.SetInt(ITEM_DIFFICULTY_INT, pTarget->InqIntQuality(ITEM_DIFFICULTY_INT, 0, TRUE) + spellCraft);
			pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		}
		break;
	}
	case 0x38000037: //zircon
		pTarget->AddImbueEffect(ImbuedEffectType::MagicDefense_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000038: //peridot
		pTarget->AddImbueEffect(ImbuedEffectType::MeleeDefense_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000039: //yellow topaz
		pTarget->AddImbueEffect(ImbuedEffectType::MissileDefense_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800003a: //emerald
		pTarget->AddImbueEffect(ImbuedEffectType::AcidRending_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800003b: //white sapphire
		pTarget->AddImbueEffect(ImbuedEffectType::BludgeonRending_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800003c: //aquamarine
		pTarget->AddImbueEffect(ImbuedEffectType::ColdRending_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800003d: //jet
		pTarget->AddImbueEffect(ImbuedEffectType::ElectricRending_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800003e: //red garnet
		pTarget->AddImbueEffect(ImbuedEffectType::FireRending_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x3800003f: //black garnet
		pTarget->AddImbueEffect(ImbuedEffectType::PierceRending_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000040: //imperial topaz
		pTarget->AddImbueEffect(ImbuedEffectType::SlashRending_ImbuedEffectType);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000041: //azurite, malachite, citrine, hematite, lavender jade, red jade, carnelian, lapis lazuli, agate, rose quartz, smokey quartz, bloodstone
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000042: //ebony, porcelain, teak
	{
		HeritageGroup heritageRequirement = (HeritageGroup)pTarget->InqIntQuality(HERITAGE_GROUP_INT, 0, TRUE);

		if (heritageRequirement != Invalid_HeritageGroup)
		{
			HeritageGroup heritageTarget;
			switch (pTool->InqIntQuality(MATERIAL_TYPE_INT, 0, TRUE))
			{
			case MaterialType::Ebony_MaterialType:
				heritageTarget = Gharundim_HeritageGroup;
				break;
			case MaterialType::Porcelain_MaterialType:
				heritageTarget = Sho_HeritageGroup;
				break;
			case MaterialType::Teak_MaterialType:
				heritageTarget = Aluvian_HeritageGroup;
				break;
			default:
				return;
			}

			pTarget->m_Qualities.SetInt(HERITAGE_GROUP_INT, heritageTarget);
			switch (heritageTarget)
			{
			case Aluvian_HeritageGroup:
				pTarget->m_Qualities.SetString(ITEM_HERITAGE_GROUP_RESTRICTION_STRING, "Aluvian");
				break;
			case Gharundim_HeritageGroup:
				pTarget->m_Qualities.SetString(ITEM_HERITAGE_GROUP_RESTRICTION_STRING, "Gharu'ndim");
				break;
			case Sho_HeritageGroup:
				pTarget->m_Qualities.SetString(ITEM_HERITAGE_GROUP_RESTRICTION_STRING, "Sho");
				break;
			}
			pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		}
		break;
	}
	case 0x38000043: //leather
		pTarget->m_Qualities.SetBool(RETAINED_BOOL, 1);
		break;
	case 0x38000044: //green garnet
		pTarget->m_Qualities.SetFloat(ELEMENTAL_DAMAGE_MOD_FLOAT, pTarget->InqFloatQuality(ELEMENTAL_DAMAGE_MOD_FLOAT, 0, TRUE) + 0.01);
		pTarget->m_Qualities.SetInt(NUM_TIMES_TINKERED_INT, pTarget->InqIntQuality(NUM_TIMES_TINKERED_INT, 0, TRUE) + 1);
		break;
	case 0x38000046: //fetish of the dark idol
		pTarget->AddImbueEffect(ImbuedEffectType::IgnoreSomeMagicProjectileDamage_ImbuedEffectType);
		break;
	default:
		SendText(csprintf("Tried to run unsupported modification script: %u.", scriptId), LTT_CRAFT);
		break;
	}
}

void CPlayerWeenie::PerformUseModifications(int index, CCraftOperation *op, CWeenieObject *pTool, CWeenieObject *pTarget, CWeenieObject *pCreatedItem)
{
	if (op->_mods[index]._modificationScriptId)
		PerformUseModificationScript(op->_mods[index]._modificationScriptId, op, pTool, pTarget, pCreatedItem);

	if (op->_mods[index]._ModifyHealth)
		AdjustHealth(op->_mods[index]._ModifyHealth);
	if (op->_mods[index]._ModifyStamina)
		AdjustStamina(op->_mods[index]._ModifyStamina);
	if (op->_mods[index]._ModifyMana)
		AdjustMana(op->_mods[index]._ModifyMana);

	if (!op->_mods[index]._intMod.empty())
	{
		for each (TYPEMod<STypeInt, int> intMod in op->_mods[index]._intMod)
		{
			bool applyToCreatedItem = false;
			bool applyToTool = false;
			bool removeFromTarget = false;
			CWeenieObject *modificationSource = NULL;
			switch (intMod._unk) //this is a guess
			{
			case 0:
				modificationSource = this;
				break;
			case 1:
				modificationSource = pTool;
				break;
			case 2:
				modificationSource = pTool;
				applyToTool = true;
				break;
			case 60:
				//dying armor entries have a second entry to armor reduction that has -30 armor and _unk value of 60
				//not sure what to do with that so we skip it.
				continue;
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break; //should never happen
			}

			int value = applyToTool ? pTool->InqIntQuality(intMod._stat, 0, true): pTarget->InqIntQuality(intMod._stat, 0, true);
			switch (intMod._operationType)
			{
			case 1: //=
				value = intMod._value;
				if (value == 0)
					removeFromTarget = true;
				break;
			case 2: //+
				value += intMod._value;
				if (value < 0)
					value = 0;
				break;
			case 3: //copy value from modificationSource to target
				if (modificationSource)
					value = modificationSource->InqIntQuality(intMod._stat, 0);
				break;
			case 4: //copy value from modificationSource to created item
				applyToCreatedItem = true;
				if (modificationSource)
					value = modificationSource->InqIntQuality(intMod._stat, 0);
				break;
			case 7: //add spell
			{
				CSpellTable * pSpellTable = MagicSystem::GetSpellTable();
				if (!intMod._stat || !pSpellTable || !pSpellTable->GetSpellBase(intMod._stat))
					break;
				pTarget->m_Qualities.AddSpell(intMod._stat);
				break;
			}
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break;
			}

			if (pCreatedItem && applyToCreatedItem)
			{
				pCreatedItem->m_Qualities.SetInt(intMod._stat, value);
				pCreatedItem->NotifyIntStatUpdated(intMod._stat, false);
			}
			if (removeFromTarget)
			{
				pTarget->m_Qualities.RemoveInt(intMod._stat);
			}
			if (applyToTool)
			{
				pTool->m_Qualities.SetInt(intMod._stat, value);
				pTool->NotifyIntStatUpdated(intMod._stat, false);
			}
			else
			{
				pTarget->m_Qualities.SetInt(intMod._stat, value);
				pTarget->NotifyIntStatUpdated(intMod._stat, false);
			}
		}
	}

	if (!op->_mods[index]._boolMod.empty())
	{
		for each (TYPEMod<STypeBool, BOOL> boolMod in op->_mods[index]._boolMod)
		{
			bool applyToCreatedItem = false;
			CWeenieObject *modificationSource = NULL;
			switch (boolMod._unk) //this is a guess
			{
			case 0:
				modificationSource = this;
				break;
			case 1:
				modificationSource = pTool;
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break; //should never happen
			}

			int value = pTarget->InqBoolQuality(boolMod._stat, 0);
			switch (boolMod._operationType)
			{
			case 1: //=
				value = boolMod._value;
				break;
			case 2: //+
				value += boolMod._value;
				break;
			case 3: //copy value from modificationSource to target
				if (modificationSource)
					value = modificationSource->InqBoolQuality(boolMod._stat, 0);
				break;
			case 4: //copy value from modificationSource to created item
				applyToCreatedItem = true;
				if (modificationSource)
					value = modificationSource->InqBoolQuality(boolMod._stat, 0);
				break;
			case 7: //add spell
#ifdef _DEBUG
				assert(false);
#endif
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break;
			}

			if (pCreatedItem && applyToCreatedItem)
			{
				pCreatedItem->m_Qualities.SetBool(boolMod._stat, value);
				pCreatedItem->NotifyBoolStatUpdated(boolMod._stat, false);
			}
			else
			{
				pTarget->m_Qualities.SetBool(boolMod._stat, value);
				pTarget->NotifyBoolStatUpdated(boolMod._stat, false);
			}
		}
	}

	if (!op->_mods[index]._floatMod.empty())
	{
		for each (TYPEMod<STypeFloat, double> floatMod in op->_mods[index]._floatMod)
		{
			bool applyToCreatedItem = false;
			CWeenieObject *modificationSource = NULL;
			switch (floatMod._unk) //this is a guess
			{
			case 0:
				modificationSource = this;
				break;
			case 1:
				modificationSource = pTool;
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break; //should never happen
			}

			double value = pTarget->InqFloatQuality(floatMod._stat, 0, true);
			switch (floatMod._operationType)
			{
			case 1: //=
				value = floatMod._value;
				break;
			case 2: //+
				value += floatMod._value;
				if (value < 0.0)
					value = 0.0;
				break;
			case 3: //copy value from modificationSource to target
				if (modificationSource)
					value = modificationSource->InqFloatQuality(floatMod._stat, 0);
				break;
			case 4: //copy value from modificationSource to created item
				applyToCreatedItem = true;
				if (modificationSource)
					value = modificationSource->InqFloatQuality(floatMod._stat, 0);
				break;
			case 7: //add spell
#ifdef _DEBUG
				assert(false);
#endif
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break;
			}

			if (pCreatedItem && applyToCreatedItem)
			{
				pCreatedItem->m_Qualities.SetFloat(floatMod._stat, value);
				pCreatedItem->NotifyFloatStatUpdated(floatMod._stat, false);
			}
			else
			{
				pTarget->m_Qualities.SetFloat(floatMod._stat, value);
				pTarget->NotifyFloatStatUpdated(floatMod._stat, false);
			}
		}
	}

	if (!op->_mods[index]._stringMod.empty())
	{
		for each (TYPEMod<STypeString, std::string> stringMod in op->_mods[index]._stringMod)
		{
			bool applyToCreatedItem = false;
			bool removeFromTarget = false;
			CWeenieObject *modificationSource = NULL;
			switch (stringMod._unk) //this is a guess
			{
			case 0:
				modificationSource = this;
				break;
			case 1:
				modificationSource = pTool;
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break; //should never happen
			}

			std::string value = pTarget->InqStringQuality(stringMod._stat, "");
			switch (stringMod._operationType)
			{
			case 1: //=
				value = stringMod._value;
				if (value == "")
					removeFromTarget = true;
				break;
			case 2: //+
				value += stringMod._value;
				break;
			case 3: //copy value from modificationSource to target
				if (modificationSource)
				{
					switch (stringMod._stat)
					{
					case TINKER_NAME_STRING:
					case IMBUER_NAME_STRING:
					case CRAFTSMAN_NAME_STRING:
						value = this->InqStringQuality(NAME_STRING, "");
						break;
					default:
						value = modificationSource->InqStringQuality(stringMod._stat, "");
						break;
					}
				}
				break;
			case 4: //copy value from modificationSource to created item
				applyToCreatedItem = true;
				if (modificationSource)
				{
					switch (stringMod._stat)
					{
					case TINKER_NAME_STRING:
					case IMBUER_NAME_STRING:
					case CRAFTSMAN_NAME_STRING:
						value = this->InqStringQuality(NAME_STRING, "");
						break;
					default:
						value = modificationSource->InqStringQuality(stringMod._stat, "");
						break;
					}
				}
				break;
			case 7: //add spell
#ifdef _DEBUG
				assert(false);
#endif
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break;
			}

			if (pCreatedItem && applyToCreatedItem)
			{
				pCreatedItem->m_Qualities.SetString(stringMod._stat, value);
				pCreatedItem->NotifyStringStatUpdated(stringMod._stat, false);
			}
			if (removeFromTarget)
			{
				pTarget->m_Qualities.RemoveString(stringMod._stat);
			}
			else
			{
				pTarget->m_Qualities.SetString(stringMod._stat, value);
				pTarget->NotifyStringStatUpdated(stringMod._stat, false);
			}
		}
	}

	if (!op->_mods[index]._didMod.empty())
	{
		for each (TYPEMod<STypeDID, DWORD> didMod in op->_mods[index]._didMod)
		{
			bool applyToCreatedItem = false;
			bool removeFromTarget = false;
			CWeenieObject *modificationSource = NULL;
			switch (didMod._unk) //this is a guess
			{
			case 0:
				modificationSource = this;
				break;
			case 1:
				modificationSource = pTool;
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break; //should never happen
			}

			DWORD value = pTarget->InqDIDQuality(didMod._stat, 0);
			switch (didMod._operationType)
			{
			case 1: //=
				value = didMod._value;
				if (value == 0)
					value = didMod._value;
				break;
			case 2: //+
				value += didMod._value;
				break;
			case 3: //copy value from modificationSource to target
				if (modificationSource)
					value = modificationSource->InqDIDQuality(didMod._stat, 0);
				break;
			case 4: //copy value from modificationSource to created item
				applyToCreatedItem = true;
				if (modificationSource)
					value = modificationSource->InqDIDQuality(didMod._stat, 0);
				break;
			case 7: //add spell
#ifdef _DEBUG
				assert(false);
#endif
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break;
			}

			if (pCreatedItem && applyToCreatedItem)
			{
				pCreatedItem->m_Qualities.SetDataID(didMod._stat, value);
				pCreatedItem->NotifyDIDStatUpdated(didMod._stat, false);
			}
			if (removeFromTarget)
			{
				pTarget->m_Qualities.RemoveDataID(didMod._stat);
			}
			else
			{
				pTarget->m_Qualities.SetDataID(didMod._stat, value);
				pTarget->NotifyDIDStatUpdated(didMod._stat, false);
			}
		}
	}

	if (!op->_mods[index]._iidMod.empty())
	{
		for each (TYPEMod<STypeIID, DWORD> iidMod in op->_mods[index]._iidMod)
		{
			bool applyToCreatedItem = false;
			CWeenieObject *modificationSource = NULL;
			switch (iidMod._unk) //this is a guess
			{
			case 0:
				modificationSource = this;
				break;
			case 1:
				modificationSource = pTool;
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break; //should never happen
			}

			DWORD value = pTarget->InqIIDQuality(iidMod._stat, 0);
			switch (iidMod._operationType)
			{
			case 1: //=
				value = iidMod._value;
				break;
			case 2: //+
				value += iidMod._value;
				break;
			case 3: //copy value from modificationSource to target
				if (modificationSource)
				{
					switch (iidMod._stat)
					{
					case ALLOWED_WIELDER_IID:
					case ALLOWED_ACTIVATOR_IID:
						value = this->GetID();
						break;
					default:
						value = modificationSource->InqIIDQuality(iidMod._stat, 0);
						break;
					}
				}
				break;
			case 4: //copy value from modificationSource to created item
				applyToCreatedItem = true;
				if (modificationSource)
				{
					switch (iidMod._stat)
					{
					case ALLOWED_WIELDER_IID:
					case ALLOWED_ACTIVATOR_IID:
						value = this->GetID();
						break;
					default:
						value = modificationSource->InqIIDQuality(iidMod._stat, 0);
						break;
					}
				}
				break;
			case 7: //add spell
#ifdef _DEBUG
				assert(false);
#endif
				break;
			default:
#ifdef _DEBUG
				assert(false);
#endif
				break;
			}

			if (pCreatedItem && applyToCreatedItem)
			{
				pCreatedItem->m_Qualities.SetInstanceID(iidMod._stat, value);
				pCreatedItem->NotifyIIDStatUpdated(iidMod._stat, false);
			}
			else
			{
				pTarget->m_Qualities.SetInstanceID(iidMod._stat, value);
				pTarget->NotifyIIDStatUpdated(iidMod._stat, false);
			}
		}
	}

	if (op->_mods[index]._unknown7) //this is a guess
	{
		//update icon
		DWORD clothing_table_id = pTarget->InqDIDQuality(CLOTHINGBASE_DID, 0);
		DWORD palette_template_key = pTarget->InqIntQuality(PALETTE_TEMPLATE_INT, 0);

		if (clothing_table_id && palette_template_key)
		{
			if (!pTarget->InqBoolQuality(IGNORE_CLO_ICONS_BOOL, FALSE))
			{
				if (ClothingTable *clothingTable = ClothingTable::Get(clothing_table_id))
				{
					if (const CloPaletteTemplate *pPaletteTemplate = clothingTable->_paletteTemplatesHash.lookup(palette_template_key))
					{
						if (pPaletteTemplate->iconID)
						{
							pTarget->m_Qualities.SetDataID(ICON_DID, pPaletteTemplate->iconID);
							pTarget->NotifyDIDStatUpdated(ICON_DID, false);
						}
					}
					ClothingTable::Release(clothingTable);
				}
			}
		}
	}
}

DWORD CPlayerWeenie::MaterialToSalvageBagId(MaterialType material)
{
	switch (material)
	{
	case Ceramic_MaterialType:
		return W_MATERIALCERAMIC_CLASS;
	case Porcelain_MaterialType:
		return W_MATERIALPORCELAIN_CLASS;
	case Linen_MaterialType:
		return W_MATERIALLINEN_CLASS;
	case Satin_MaterialType:
		return W_MATERIALSATIN_CLASS;
	case Silk_MaterialType:
		return W_MATERIALSILK_CLASS;
	case Velvet_MaterialType:
		return W_MATERIALVELVET_CLASS;
	case Wool_MaterialType:
		return W_MATERIALWOOL_CLASS;
	case Agate_MaterialType:
		return W_MATERIALAGATE_CLASS;
	case Amber_MaterialType:
		return W_MATERIALAMBER_CLASS;
	case Amethyst_MaterialType:
		return W_MATERIALAMETHYST_CLASS;
	case Aquamarine_MaterialType:
		return W_MATERIALAQUAMARINE_CLASS;
	case Azurite_MaterialType:
		return W_MATERIALAZURITE_CLASS;
	case Black_Garnet_MaterialType:
		return W_MATERIALBLACKGARNET_CLASS;
	case Black_Opal_MaterialType:
		return W_MATERIALBLACKOPAL_CLASS;
	case Bloodstone_MaterialType:
		return W_MATERIALBLOODSTONE_CLASS;
	case Carnelian_MaterialType:
		return W_MATERIALCARNELIAN_CLASS;
	case Citrine_MaterialType:
		return W_MATERIALCITRINE_CLASS;
	case Diamond_MaterialType:
		return W_MATERIALDIAMOND_CLASS;
	case Emerald_MaterialType:
		return W_MATERIALEMERALD_CLASS;
	case Fire_Opal_MaterialType:
		return W_MATERIALFIREOPAL_CLASS;
	case Green_Garnet_MaterialType:
		return W_MATERIALGREENGARNET_CLASS;
	case Green_Jade_MaterialType:
		return W_MATERIALGREENJADE_CLASS;
	case Hematite_MaterialType:
		return W_MATERIALHEMATITE_CLASS;
	case Imperial_Topaz_MaterialType:
		return W_MATERIALIMPERIALTOPAZ_CLASS;
	case Jet_MaterialType:
		return W_MATERIALJET_CLASS;
	case Lapis_Lazuli_MaterialType:
		return W_MATERIALLAPISLAZULI_CLASS;
	case Lavender_Jade_MaterialType:
		return W_MATERIALLAVENDERJADE_CLASS;
	case Malachite_MaterialType:
		return W_MATERIALMALACHITE_CLASS;
	case Moonstone_MaterialType:
		return W_MATERIALMOONSTONE_CLASS;
	case Onyx_MaterialType:
		return W_MATERIALONYX_CLASS;
	case Opal_MaterialType:
		return W_MATERIALOPAL_CLASS;
	case Peridot_MaterialType:
		return W_MATERIALPERIDOT_CLASS;
	case Red_Garnet_MaterialType:
		return W_MATERIALREDGARNET_CLASS;
	case Red_Jade_MaterialType:
		return W_MATERIALREDJADE_CLASS;
	case Rose_Quartz_MaterialType:
		return W_MATERIALROSEQUARTZ_CLASS;
	case Ruby_MaterialType:
		return W_MATERIALRUBY_CLASS;
	case Sapphire_MaterialType:
		return W_MATERIALSAPPHIRE_CLASS;
	case Smoky_Quartz_MaterialType:
		return W_MATERIALSMOKYQUARTZ_CLASS;
	case Sunstone_MaterialType:
		return W_MATERIALSUNSTONE_CLASS;
	case Tiger_Eye_MaterialType:
		return W_MATERIALTIGEREYE_CLASS;
	case Tourmaline_MaterialType:
		return W_MATERIALTOURMALINE_CLASS;
	case Turquoise_MaterialType:
		return W_MATERIALTURQUOISE_CLASS;
	case White_Jade_MaterialType:
		return W_MATERIALWHITEJADE_CLASS;
	case White_Quartz_MaterialType:
		return W_MATERIALWHITEQUARTZ_CLASS;
	case White_Sapphire_MaterialType:
		return W_MATERIALWHITESAPPHIRE_CLASS;
	case Yellow_Garnet_MaterialType:
		return W_MATERIALYELLOWGARNET_CLASS;
	case Yellow_Topaz_MaterialType:
		return W_MATERIALYELLOWTOPAZ_CLASS;
	case Zircon_MaterialType:
		return W_MATERIALZIRCON_CLASS;
	case Ivory_MaterialType:
		return W_MATERIALIVORY_CLASS;
	case Leather_MaterialType:
		return W_MATERIALLEATHER_CLASS;
	case Armoredillo_Hide_MaterialType:
		return W_MATERIALARMOREDILLOHIDE_CLASS;
	case Gromnie_Hide_MaterialType:
		return W_MATERIALGROMNIEHIDE_CLASS;
	case Reed_Shark_Hide_MaterialType:
		return W_MATERIALREEDSHARKHIDE_CLASS;
	case Brass_MaterialType:
		return W_MATERIALBRASS_CLASS;
	case Bronze_MaterialType:
		return W_MATERIALBRONZE_CLASS;
	case Copper_MaterialType:
		return W_MATERIALCOPPER_CLASS;
	case Gold_MaterialType:
		return W_MATERIALGOLD_CLASS;
	case Iron_MaterialType:
		return W_MATERIALIRON_CLASS;
	case Pyreal_MaterialType:
		return W_MATERIALPYREAL_CLASS;
	case Silver_MaterialType:
		return W_MATERIALSILVER_CLASS;
	case Steel_MaterialType:
		return W_MATERIALSTEEL_CLASS;
	case Alabaster_MaterialType:
		return W_MATERIALALABASTER_CLASS;
	case Granite_MaterialType:
		return W_MATERIALGRANITE_CLASS;
	case Marble_MaterialType:
		return W_MATERIALMARBLE_CLASS;
	case Obsidian_MaterialType:
		return W_MATERIALOBSIDIAN_CLASS;
	case Sandstone_MaterialType:
		return W_MATERIALSANDSTONE_CLASS;
	case Serpentine_MaterialType:
		return W_MATERIALSERPENTINE_CLASS;
	case Ebony_MaterialType:
		return W_MATERIALEBONY_CLASS;
	case Mahogany_MaterialType:
		return W_MATERIALMAHOGANY_CLASS;
	case Oak_MaterialType:
		return W_MATERIALOAK_CLASS;
	case Pine_MaterialType:
		return W_MATERIALPINE_CLASS;
	case Teak_MaterialType:
		return W_MATERIALTEAK_CLASS;
	default:
		return 0;
	}
}

struct SalvageInfo
{
	int amount = 0;

	int totalValue = 0;
	int totalWorkmanship = 0;

	//int itemsSalvagedCount = 0;
	//Discrete/Continous vars into the structure of salvageMap
	int itemsSalvagedCountCont = 0;
	int itemsSalvagedCountDiscrete = 0;
};

void CPlayerWeenie::PerformSalvaging(DWORD toolId, PackableList<DWORD> items)
{
	if (items.empty())
		return;

	CWeenieObject *pTool = g_pWorld->FindObject(toolId);

	if (!pTool)
		return;

	if (pTool->GetWorldTopLevelOwner() != this)
	{
		NotifyWeenieError(WERROR_SALVAGE_DONT_OWN_TOOL);
		return;
	}

	//SALVAGING SKILL DETERMINES SALVAGE AMOUNT
	DWORD salvagingSkillValue;
	InqSkill(STypeSkill::SALVAGING_SKILL, salvagingSkillValue, false);

	DWORD highestTinkeringSkillValue;
	DWORD tinkeringSkills[4];
	InqSkill(STypeSkill::ARMOR_APPRAISAL_SKILL, tinkeringSkills[0], false);
	InqSkill(STypeSkill::WEAPON_APPRAISAL_SKILL, tinkeringSkills[1], false);
	InqSkill(STypeSkill::MAGIC_ITEM_APPRAISAL_SKILL, tinkeringSkills[2], false);
	InqSkill(STypeSkill::ITEM_APPRAISAL_SKILL, tinkeringSkills[3], false);

	highestTinkeringSkillValue = max(max(max(tinkeringSkills[0], tinkeringSkills[1]), tinkeringSkills[2]), tinkeringSkills[3]);

	int numAugs = max(0, min(4, InqIntQuality(AUGMENTATION_BONUS_SALVAGE_INT, 0)));

	std::map<MaterialType, SalvageInfo> salvageMap;
	std::list<CWeenieObject *> itemsToDestroy;

	//Lists for the success message
	PackableList<SalvageResult> salvageResults;
	PackableList<DWORD> notSalvagable;

	for (auto itemId : items)
	{
		CWeenieObject *pItem = g_pWorld->FindObject(itemId);

		if (!pItem)
		{
			NotifyWeenieError(WERROR_SALVAGE_INVALID_LOOT_LIST);
			return;
		}

		if (pItem->GetWorldTopLevelOwner() != this)
		{
			NotifyWeenieError(WERROR_SALVAGE_DONT_OWN_LOOT);
			notSalvagable.push_back(pItem->GetID());
			continue;
		}

		bool isRetained = pItem->InqBoolQuality(RETAINED_BOOL, FALSE);
		ITEM_TYPE itemType = (ITEM_TYPE)pItem->InqIntQuality(ITEM_TYPE_INT, 0, TRUE);
		MaterialType material = (MaterialType)pItem->InqIntQuality(MATERIAL_TYPE_INT, 0, TRUE);
		int workmanship = pItem->InqIntQuality(ITEM_WORKMANSHIP_INT, 0, TRUE);
		int itemValue = pItem->InqIntQuality(VALUE_INT, 0, TRUE);

		if (workmanship < 1 || material == MaterialType::Undef_MaterialType || isRetained)
		{
			NotifyWeenieError(WERROR_SALVAGE_NOT_SUITABLE);
			notSalvagable.push_back(pItem->GetID());
			continue;
		}

		itemsToDestroy.push_back(pItem);
		/*keep Discrete and Continous .itemsSalvaged
		Discrete for immediate calculations, Continous for reference sake and foolish number porn*/
		if (itemType == ITEM_TYPE::TYPE_TINKERING_MATERIAL)
		{
			salvageMap[material].amount += pItem->InqIntQuality(STRUCTURE_INT, 1);
			salvageMap[material].totalValue += itemValue;
			salvageMap[material].itemsSalvagedCountCont += pItem->InqIntQuality(NUM_ITEMS_IN_MATERIAL_INT, 1);
			//salvageMap[material].itemsSalvagedCountDiscrete++;
		}
		else
		{
			int salvagingAmount = CalculateSalvageAmount(salvagingSkillValue, workmanship, numAugs);

			// tinkering can at best return the workmanship as the amount
			int tinkeringAmount = min(CalculateSalvageAmount(highestTinkeringSkillValue, workmanship, 0), workmanship);

			// We choose the one that gives best results
			int salvageAmount = max(salvagingAmount, tinkeringAmount);

			// formula taken from http://asheron.wikia.com/wiki/Salvaging/Value_Pre2013
			int salvageValue = itemValue * ( salvagingSkillValue / 387.0 ) *(1 + numAugs * 0.25);
			salvageMap[material].totalValue += salvageValue;
			salvageMap[material].amount += salvageAmount;
			salvageMap[material].itemsSalvagedCountCont++;
		}
		salvageMap[material].totalWorkmanship += workmanship;
	}

	if (itemsToDestroy.empty())
		return;

	// Check if we have enough pack space first!
	int numBags = 0;
	for (auto salvageEntry : salvageMap)
	{
		SalvageInfo salvageInfo = salvageEntry.second;
		numBags += ceil(salvageInfo.amount / 100.0);
	}
	if (numBags > Container_GetNumFreeMainPackSlots())
	{
		SendText("Not enough pack space!", LTT_ERROR);
		return;
	}


	for (auto item : itemsToDestroy)
		item->Remove();

	for (auto salvageEntry : salvageMap)
	{
		MaterialType material = salvageEntry.first;
		SalvageInfo salvageInfo = salvageEntry.second;
		int valuePerUnit = salvageInfo.totalValue / salvageInfo.amount;

		double workmanship = salvageInfo.totalWorkmanship/(double)salvageInfo.itemsSalvagedCountCont;
		int fullBagItems = ceil(salvageInfo.itemsSalvagedCountCont / (salvageInfo.amount / 100.0));

		int remainingAmount = salvageInfo.amount;
		int remainingItems = salvageInfo.itemsSalvagedCountCont;

		while (remainingAmount > 0)
		{
			int amount = min(remainingAmount, 100);
			int numItems = min(max(1, remainingItems), fullBagItems);
			SpawnSalvageBagInContainer(material, amount, floor(numItems * workmanship), min(valuePerUnit * amount, 75000), numItems);

			SalvageResult salvageResult;
			salvageResult.material = material;
			salvageResult.units = amount;
			salvageResult.workmanship = floor(numItems * workmanship)/(double)numItems;
			salvageResults.push_back(salvageResult);

			remainingAmount -= 100;
			remainingItems -= fullBagItems;
		}
	}

	BinaryWriter salvageMsg;
	salvageMsg.Write<DWORD>(0x02B4);
	salvageMsg.Write<DWORD>(0x28);		// SkillID: Salvaging
	notSalvagable.Pack(&salvageMsg);	// PackableList<ObjectID>
	salvageResults.Pack(&salvageMsg);	// PackableList<SalvageResult>
	salvageMsg.Write<int>(0);			// int: Aug bonus - not implemented?

	SendNetMessage(&salvageMsg, PRIVATE_MSG, TRUE, FALSE);
}


// See http://web.archive.org/web/20170130213649/http://www.thejackcat.com/AC/Shopping/Crafts/Salvage_old.htm
// and http://web.archive.org/web/20170130194012/http://www.thejackcat.com/AC/Shopping/Crafts/Salvage.htm
int CPlayerWeenie::CalculateSalvageAmount(int salvagingSkill, int workmanship, int numAugs)
{
	return 1 + floor( salvagingSkill/195.0 * workmanship * (1 + 0.25*numAugs) );
}

bool CPlayerWeenie::SpawnSalvageBagInContainer(MaterialType material, int amount, int workmanship, int value, int numItems)
{
	int salvageWcid = MaterialToSalvageBagId(material);
	if (salvageWcid == 0)
		return false;

	CWeenieObject *weenie = g_pWeenieFactory->CreateWeenieByClassID(salvageWcid, NULL, false);
	weenie->m_Qualities.SetString(NAME_STRING, csprintf("Salvage (%s)", FormatNumberString(amount).c_str())); //modern client prepends the salvage type automatically so we need to adapt to this.

	if (!weenie)
		return false;

	weenie->m_Qualities.SetInt(ITEM_WORKMANSHIP_INT, workmanship);
	weenie->m_Qualities.SetInt(VALUE_INT, value);
	weenie->m_Qualities.SetInt(STRUCTURE_INT, amount);
	weenie->m_Qualities.SetInt(NUM_ITEMS_IN_MATERIAL_INT, numItems);
	//weenie->m_Qualities.SetFloat(SalvageWorkmanship, )

	//Which one of these is Vtank missing to make it not calculate the real workmanship
	//none. it has them all. where is SalvageWorkmanship double being calculated

	//NumberItemsSalvagedFrom = 170, // 0x000000AA
	//SalvageWorkmanship = 167772169, // 0x0A000009
	//	Divide SalvageWorkmanship by NumberItemsSalvagedFrom

	return SpawnInContainer(weenie);
}

void CPlayerWeenie::SetLoginPlayerQualities()
{
	if (m_Qualities.GetIID(CONTAINER_IID, 0))
	{
		m_Qualities.RemoveInstanceID(CONTAINER_IID);
	}

	DWORD patronID = 0;
	m_Qualities.InqInstanceID(PATRON_IID, patronID);

	if (g_pDBIO->GetCharacterInfo(GetID()).account_id == g_pDBIO->GetCharacterInfo(patronID).account_id)
	{
		g_pAllegianceManager->TryBreakAllegiance(this, patronID);
	}

	if (g_pConfig->FixOldChars())
	{

		//Refund Credits for those who lost them at Asheron's Castle.
		AdjustSkillCredits(GetExpectedSkillCredits(), GetTotalSkillCredits(), true);

		int heritage = InqIntQuality(HERITAGE_GROUP_INT, 1);

		// fix scale and other misc things for different heritage groups
		switch (heritage)
		{
		case Lugian_HeritageGroup:
		{
			if (InqStringQuality(SEX_STRING, "") == "Male")
				m_Qualities.SetFloat(DEFAULT_SCALE_FLOAT, 1.3);
			else
				m_Qualities.SetFloat(DEFAULT_SCALE_FLOAT, 1.2);
		}
		break;
		case Empyrean_HeritageGroup:
		{
			if (InqStringQuality(SEX_STRING, "") == "Male")
				m_Qualities.SetFloat(DEFAULT_SCALE_FLOAT, 1.2);
			else
				m_Qualities.SetFloat(DEFAULT_SCALE_FLOAT, 1.15);
		}
		break;
		case Gearknight_HeritageGroup:
		{
			if (InqStringQuality(SEX_STRING, "") == "Male")
				m_Qualities.SetFloat(DEFAULT_SCALE_FLOAT, 1.2);
			else
				m_Qualities.SetFloat(DEFAULT_SCALE_FLOAT, 1.1);

			ObjDesc gk;
			GetObjDesc(gk);

			m_Qualities.SetDataID(HEAD_OBJECT_DID, gk.lastAPChange->part_id);
		}
		break;
		case Tumerok_HeritageGroup:
		{
			if (InqStringQuality(SEX_STRING, "") == "Male")
				m_Qualities.SetFloat(DEFAULT_SCALE_FLOAT, 1.1);
		}
		break;
		case Olthoi_HeritageGroup:
		case OlthoiAcid_HeritageGroup:
		{
			Position sanc = Position(g_pConfig->OlthoiStartPosition());

			m_Qualities.SetPosition(SANCTUARY_POSITION, sanc);
			m_Qualities.SetBool((STypeBool)LOGIN_AT_LIFESTONE_BOOL, 1);
			m_Qualities.SetInt(PLAYER_KILLER_STATUS_INT, PK_PKStatus); // Olthoi are always Red

			if (m_Qualities.GetInt(HERITAGE_GROUP_INT, 1) == Olthoi_HeritageGroup)
			{
				m_Qualities.SetFloat(DEFAULT_SCALE_FLOAT, 0.9);
				m_Qualities.SetDataID(HEAD_OBJECT_DID, 0x010045f0);
			}
			else
			{
				m_Qualities.SetFloat(DEFAULT_SCALE_FLOAT, 0.6);
				m_Qualities.SetDataID(HEAD_OBJECT_DID, 0x01004616);
			}
		}
		break;
		default:
			break;
		}

		// add racial augmentations
		switch (heritage)
		{
		case Shadowbound_HeritageGroup:
		case Penumbraen_HeritageGroup:
			m_Qualities.SetInt(AUGMENTATION_CRITICAL_EXPERTISE_INT, 1); break;
		case Gearknight_HeritageGroup:
			m_Qualities.SetInt(AUGMENTATION_DAMAGE_REDUCTION_INT, 1); break;
		case Tumerok_HeritageGroup:
			m_Qualities.SetInt(AUGMENTATION_CRITICAL_POWER_INT, 1); break;
		case Lugian_HeritageGroup:
		{
			if (!m_Qualities.GetInt(AUGMENTATION_INCREASED_CARRYING_CAPACITY_INT, 0))
				m_Qualities.SetInt(AUGMENTATION_INCREASED_CARRYING_CAPACITY_INT, 1); 

			break;
		}
		case Empyrean_HeritageGroup:
			m_Qualities.SetInt(AUGMENTATION_INFUSED_LIFE_MAGIC_INT, 1); break;
		case Undead_HeritageGroup:
			m_Qualities.SetInt(AUGMENTATION_CRITICAL_DEFENSE_INT, 1); break;
		case Olthoi_HeritageGroup:
		case OlthoiAcid_HeritageGroup:
			break; // none
		default: // sho, aluv, gharu, viamont
			m_Qualities.SetInt(AUGMENTATION_JACK_OF_ALL_TRADES_INT, 1); break;
		}
	}

	//End of temporary code

	// Generate a random number of seconds between 1 second and 2 months of seconds and write int for the Real Time Rare drop system.
	if (g_pConfig->RealTimeRares() && !m_Qualities.GetInt(RARES_LOGIN_TIMESTAMP_INT, 0))
	{
		int seconds = getRandomNumber(5184000);

		time_t t = chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		t += seconds;

		m_Qualities.SetInt(RARES_LOGIN_TIMESTAMP_INT, t);
	}

	g_pAllegianceManager->SetWeenieAllegianceQualities(this);
	auto loginTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
	m_Qualities.SetFloat(LOGIN_TIMESTAMP_FLOAT, loginTime);
	m_Qualities.SetInt(LOGIN_TIMESTAMP_INT, loginTime);

	// Position startPos = Position(0xDB75003B, Vector(186.000000f, 65.000000f, 36.088333f), Quaternion(1.000000, 0.000000, 0.000000, 0.000000));
	// Position startPos = Position(0xA9B4001F, Vector(87.750603f, 147.722321f, 66.005005f), Quaternion(0.011819f, 0.000000, 0.000000, -0.999930f));

	// Your location is: 0xC98C0028 [113.665604 190.259003 22.004999] -0.707107 0.000000 0.000000 -0.707107 Rithwic
	// Position startPos = Position(0xC98C0028, Vector(113.665604f, 190.259003f, 22.004999f), Quaternion(-0.707107f, 0.000000, 0.000000, -0.707107f));
	// SetInitialPosition(startPos);
	// m_Qualities.SetPosition(SANCTUARY_POSITION, startPos);

	extern bool g_bStartOverride;
	extern Position g_StartPosition;

	if (g_bStartOverride)
	{
		SetInitialPosition(g_StartPosition);
		m_Qualities.SetPosition(SANCTUARY_POSITION, g_StartPosition);
	}

	Position m_initLocPosition;
	if (m_Qualities.InqPosition(LOCATION_POSITION, m_initLocPosition) && m_initLocPosition.objcell_id) //governs whether or not to log player in at lifestone. NOTE: In the absence of a "location position" a player is already logged in at sanc or instantiation position.
	{
		if (InqBoolQuality(LOGIN_AT_LIFESTONE_BOOL, FALSE))
		{
			SetSanctuaryAsLogin();
			m_Qualities.SetBool((STypeBool)LOGIN_AT_LIFESTONE_BOOL, 0);
		}

		else //If a player's location position matches the listed landblocks in restrictedlandblocks.json or login at lifestone is configured to true their login position will be changed (one time) to their sanctuary position.
		{
			DWORD LogoutLandBlock = (BLOCK_WORD(m_initLocPosition.objcell_id) * 65536);
			auto NoLogLandBlocks = g_pPortalDataEx->GetRestrictedLandblocks();
			if ((NoLogLandBlocks.find(LogoutLandBlock) != NoLogLandBlocks.end()) || g_pConfig->LoginAtLS())
			{
				SetSanctuaryAsLogin();
			}
		}
	}


	// should never be in a fellowship when logging in, but let's be sure
	m_Qualities.RemoveString(FELLOWSHIP_STRING);

	m_Qualities.SetInt(ARMOR_LEVEL_INT, 0);
	m_Qualities.SetInt(PHYSICS_STATE_INT, PhysicsState::HIDDEN_PS | PhysicsState::IGNORE_COLLISIONS_PS | PhysicsState::EDGE_SLIDE_PS | PhysicsState::GRAVITY_PS);
	m_Qualities.SetBool(ROT_PROOF_BOOL, FALSE);
	m_Qualities.RemoveInt(RADARBLIP_COLOR_INT);

	if (atoi(g_pConfig->GetValue("player_killer_only", "0")) != 0)
	{
		double pkTimestamp;
		if (!m_Qualities.InqFloat(PK_TIMESTAMP_FLOAT, pkTimestamp, TRUE))
		{
			m_Qualities.SetInt(PLAYER_KILLER_STATUS_INT, PK_PKStatus);
		}
	}

	if (IsAdvocate())
	{
		m_Qualities.SetBool(IS_ADVOCATE_BOOL, TRUE);
		m_Qualities.SetBool(ADVOCATE_STATE_BOOL, TRUE);
	}
	else
	{
		m_Qualities.RemoveBool(IS_ADVOCATE_BOOL);
		m_Qualities.RemoveBool(ADVOCATE_STATE_BOOL);
	}

	if (IsSentinel())
	{
		m_Qualities.SetBool(IS_SENTINEL_BOOL, TRUE);
	}
	else
	{
		m_Qualities.RemoveBool(IS_SENTINEL_BOOL);
	}

	if (IsAdmin())
	{
		m_Qualities.SetBool(IS_ADMIN_BOOL, TRUE);
		m_Qualities.SetBool(IS_ARCH_BOOL, TRUE);

		m_Qualities.SetBool(SPELL_COMPONENTS_REQUIRED_BOOL, FALSE);
		m_Qualities.SetInt(BONDED_INT, 1); //do not drop items on death
	}
	else
	{
		m_Qualities.RemoveBool(IS_ADMIN_BOOL);
		m_Qualities.RemoveBool(IS_ARCH_BOOL);

		m_Qualities.SetBool(SPELL_COMPONENTS_REQUIRED_BOOL, TRUE);
		m_Qualities.SetInt(BONDED_INT, 0); //drop items on death
	}

	for (auto wielded : m_Wielded)
	{
		if (wielded->InqIntQuality(ITEM_CUR_MANA_INT, 0, true) > 0)
		{
			double manaRate = 0.0f;
			if (wielded->m_Qualities.InqFloat(MANA_RATE_FLOAT, manaRate, TRUE) && manaRate != 0.0)
				wielded->_nextManaUse = Timer::cur_time + (-manaRate * 1000);
		}
	}

	if (!InqBoolQuality(FIRST_ENTER_WORLD_DONE_BOOL, FALSE))
	{
		for (auto item : m_Items)
		{
			if (item->m_Qualities.id == W_TUTORIALBOOK_CLASS)
			{
				item->Use(this);
				break;
			}
		}
		m_Qualities.SetBool(FIRST_ENTER_WORLD_DONE_BOOL, TRUE);
	}

	//check if we still own our house.	
	if (DWORD houseId = InqDIDQuality(HOUSEID_DID, 0))
	{
		CHouseData *houseData = g_pHouseManager->GetHouseData(houseId);
		if (houseData->_ownerId != GetID())
			m_Qualities.SetDataID(HOUSEID_DID, 0);
	}
}

void CPlayerWeenie::SetSanctuaryAsLogin()
{
	Position m_StartPosition;
	if (m_Qualities.InqPosition(SANCTUARY_POSITION, m_StartPosition) && m_StartPosition.objcell_id)
	{
		m_Qualities.SetPosition(LOCATION_POSITION, m_StartPosition);
		SendNetMessage(ServerText("The currents of portal space cannot return you from whence you came. Your previous location forbids login.", LTT_DEFAULT), PRIVATE_MSG, TRUE);
	}
}

std::list<CharacterSquelch_t> CPlayerWeenie::GetSquelches()
{
	return squelches;
}

void CPlayerWeenie::LoadSquelches()
{
	squelches = g_pDBIO->GetCharacterSquelch(GetID());
}

bool CPlayerWeenie::IsPlayerSquelched(const DWORD dwGUID, bool checkAccount)
{
	for (auto sq : squelches)
	{
		if (sq.squelched_id == dwGUID)
			return TRUE;
		if (g_pDBIO->GetPlayerAccountId(dwGUID) == sq.account_id)
			return TRUE;
	}
	return FALSE;
}

void CPlayerWeenie::HandleItemManaRequest(DWORD itemId)
{
	if (!itemId)
		return;

	CWeenieObject *item = FindValidNearbyItem(itemId, 60);

	if (!item)
		return;

	m_pClient->SendNetMessage(ItemManaUpdate(item), PRIVATE_MSG, TRUE, TRUE);
}

void CPlayerWeenie::UpdateModuleFromClient(PlayerModule &module)
{
	_playerModule.options_ = module.options_;
	_playerModule.options2_ = module.options2_;
	_playerModule.spell_filters_ = module.spell_filters_;

	for (DWORD i = 0; i < 8; i++)
		_playerModule.favorite_spells_[i] = module.favorite_spells_[i];

	CloneMemberPointerData<ShortCutManager>(_playerModule.shortcuts_, module.shortcuts_);
	CloneMemberPointerData<PackableHashTable<DWORD, long>>(_playerModule.desired_comps_, module.desired_comps_);
	CloneMemberPointerData<GenericQualitiesData>(_playerModule.m_pPlayerOptionsData, module.m_pPlayerOptionsData);

	UpdateModel();
}

void CPlayerWeenie::LoadEx(CWeenieSave &save)
{
	CContainerWeenie::LoadEx(save);

	if (save._playerModule)
		_playerModule = *save._playerModule;

	if (save._questTable)
		_questTable = *save._questTable;

	CheckVitalRanges();

}

void CPlayerWeenie::LoadTitles()
{
	std::string titlesFromDb = g_pDBIO->LoadCharacterTitles(GetID());
	auto tj = json::parse(titlesFromDb);
	charTitles = tj["titles"].get<std::vector<int>>();

	m_Qualities.SetInt(NUM_CHARACTER_TITLES_INT, charTitles.size());
}

void CPlayerWeenie::SendTitles()
{
	if (charTitles.size() > 0)
	{
		BinaryWriter msg;
		msg.Write<DWORD>(0x29);
		msg.Write<DWORD>(1);
		msg.Write<DWORD>(m_Qualities.GetInt(CHARACTER_TITLE_ID_INT, 0));
		msg.Write<DWORD>(charTitles.size());

		for (auto item : charTitles)
		{
			msg.Write<DWORD>(item);
		}

		SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
	}
	else
		AddTitle(1);
}

void CPlayerWeenie::SetTitle(int titleId)
{
	for (auto &title : charTitles) {

		if (title == titleId)
		{
			m_Qualities.SetInt(CHARACTER_TITLE_ID_INT, titleId);
			m_Qualities.SetString(TITLE_STRING, GetTitleString());
			NotifyNewTitle(titleId, true);
			return;
		}
	}

	NotifyWeenieError(WERROR_NONE);
}

void CPlayerWeenie::AddTitle(int titleid)
{
	if (titleid < 1 || titleid > 894)
		return;

	for (auto const& title : charTitles) {

		if (title == titleid)
		{
			NotifyWeenieError(WERROR_NONE);
			return;
		}
	}

	charTitles.push_back(titleid);

	if(m_Qualities.GetInt(CHARACTER_TITLE_ID_INT, 0) == 0)
		NotifyNewTitle(titleid, true);
	else
		NotifyNewTitle(titleid, false);
	
	SendText(csprintf("You have been awarded the title of %s.", GetTitleStringById(titleid).c_str()), LTT_DEFAULT);
	SendTextToOverlay("You have been granted a new title.");
	SaveTitles();
}

void CPlayerWeenie::NotifyNewTitle(int titleId, bool set)
{
	BinaryWriter msg;
	msg.Write<DWORD>(0x2B);
	msg.Write<DWORD>(titleId);
	if (set)
		msg.Write<DWORD>(1);
	else
		msg.Write<DWORD>(0);
	SendNetMessage(&msg, PRIVATE_MSG, TRUE, FALSE);
}

void CPlayerWeenie::SaveTitles()
{
	json title;
	title["titles"] = charTitles;

	g_pDBIO->SaveCharacterTitles(GetID(), title.dump());
}

void CPlayerWeenie::SaveEx(CWeenieSave &save)
{
	DebugValidate();

	CContainerWeenie::SaveEx(save);

	SafeDelete(save._playerModule);
	save._playerModule = new PlayerModule;
	*save._playerModule = _playerModule;

	SafeDelete(save._questTable);
	save._questTable = new QuestTable;
	*save._questTable = _questTable;
}

bool CPlayerWeenie::ShowHelm()
{
	if (_playerModule.options2_ & ShowHelm_CharacterOptions2)
		return true;

	return false;
}

bool CPlayerWeenie::InqQuest(const char *questName)
{
	return _questTable.InqQuest(questName);
}

int CPlayerWeenie::InqTimeUntilOkayToComplete(const char *questName)
{
	return _questTable.InqTimeUntilOkayToComplete(questName);
}

unsigned int CPlayerWeenie::InqQuestSolves(const char *questName)
{
	return _questTable.InqQuestSolves(questName);
}

bool CPlayerWeenie::UpdateQuest(const char *questName)
{
	return _questTable.UpdateQuest(questName);
}

void CPlayerWeenie::StampQuest(const char *questName)
{
	_questTable.StampQuest(questName);
}

void CPlayerWeenie::IncrementQuest(const char *questName)
{
	_questTable.IncrementQuest(questName);
}

void CPlayerWeenie::DecrementQuest(const char *questName)
{
	_questTable.DecrementQuest(questName);
}

void CPlayerWeenie::EraseQuest(const char *questName)
{
	_questTable.RemoveQuest(questName);
}

void CPlayerWeenie::SetQuestCompletions(const char *questName, int numCompletions)
{
	_questTable.SetQuestCompletions(questName, numCompletions);
}

std::string CPlayerWeenie::Ktref(const char *questName)
{
	return _questTable.Ktref(questName);
}

unsigned int CPlayerWeenie::InqQuestMax(const char *questName)
{
	return _questTable.InqQuestMax(questName);
}

CWandSpellUseEvent::CWandSpellUseEvent(DWORD wandId, DWORD targetId)
{
	_wandId = wandId;
	_targetId = targetId;
}

void CWandSpellUseEvent::OnReadyToUse()
{
	if (_manager->_next_allowed_use > Timer::cur_time)
	{	
		Cancel();
		return;
	}

	CWeenieObject *wand = g_pWorld->FindObject(_wandId);
	if (!wand)
	{
		Cancel();
		return;
	}

	_spellId = wand->InqDIDQuality(SPELL_DID, 0);
	if (!_spellId)
	{
		Cancel();
		return;
	}

	STypeSkill skill = (STypeSkill)wand->InqDIDQuality(ITEM_SKILL_LIMIT_DID, 0);
	DWORD skillValue;
	if (skill != 0 && _weenie->InqSkill(skill, skillValue, false))
	{
		if (skillValue < wand->InqIntQuality(ITEM_SKILL_LEVEL_LIMIT_INT, 0))
		{
			SkillTable *pSkillTable = SkillSystem::GetSkillTable();
			const SkillBase *pSkillBase = pSkillTable->GetSkillBase(skill);
			if (pSkillBase != NULL)
				_weenie->NotifyWeenieErrorWithString(WERROR_ACTIVATION_SKILL_TOO_LOW, pSkillBase->_name.c_str());
			Cancel();
			return;
		}
	}

	DWORD motion = wand->InqDIDQuality(USE_USER_ANIMATION_DID, 0);

	int manaCost = wand->InqIntQuality(ITEM_MANA_COST_INT, 0);
	if (manaCost > 0)
	{
		CSpellTable * pSpellTable = MagicSystem::GetSpellTable();
		if (!pSpellTable)
		{
			Cancel();
			return;
		}

		const CSpellBase *spell = pSpellTable->GetSpellBase(_spellId);
		if (!spell)
		{
			Cancel();
			return;
		}

		DWORD manaConvSkill = _weenie->GetEffectiveManaConversionSkill();
		int spellCraft = wand->InqIntQuality(ITEM_SPELLCRAFT_INT, 0);
		manaCost = GetManaCost(spellCraft, spell->_power, manaCost, manaConvSkill);

		int itemCurrentMana = wand->InqIntQuality(ITEM_CUR_MANA_INT, 0);
		if (manaCost > itemCurrentMana)
		{
			Cancel(WERROR_ACTIVATION_NOT_ENOUGH_MANA);
			return;
		}

		_newManaValue = itemCurrentMana - manaCost;
	}

	_weenie->MakeSpellcastingManager()->m_bCasting = true;
	_weenie->m_SpellcastingManager->m_SpellCastData.caster_id = _wandId;

	if (motion)
		ExecuteUseAnimation(motion);
	else
		OnUseAnimSuccess(motion);
}

void CWandSpellUseEvent::OnUseAnimSuccess(DWORD motion)
{
	if (_newManaValue != -1)
	{
		CWeenieObject *wand = g_pWorld->FindObject(_wandId);
		if (!wand)
		{
			Cancel();
			return;
		}
		wand->m_Qualities.SetInt(ITEM_CUR_MANA_INT, _newManaValue);
		wand->NotifyIntStatUpdated(ITEM_CUR_MANA_INT, false);
	}

	_weenie->MakeSpellcastingManager()->CastSpellInstant(_targetId, _spellId);
	_weenie->DoForcedStopCompletely();
	_manager->_next_allowed_use = Timer::cur_time + 2.0;
	Done();
}

void CWandSpellUseEvent::Cancel(DWORD error)
{
	_weenie->MakeSpellcastingManager()->m_bCasting = false;

	CUseEventData::Cancel(error);
}

void CWandSpellUseEvent::Done(DWORD error)
{
	_weenie->MakeSpellcastingManager()->m_bCasting = false;

	CUseEventData::Done(error);
}


void CLifestoneRecallUseEvent::OnReadyToUse()
{
	SetupRecall();
	_timeout = Timer::cur_time + 15.0;
	_weenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	ExecuteUseAnimation(Motion_LifestoneRecall);
	g_pWorld->BroadcastLocal(_weenie->GetLandcell(), csprintf("%s is recalling to the lifestone.", _weenie->GetName().c_str()));
}

void CLifestoneRecallUseEvent::OnUseAnimSuccess(DWORD motion)
{
	if (_weenie->IsDead() || _weenie->IsInPortalSpace() || _weenie->get_minterp()->interpreted_state.forward_command != Motion_Ready)
	{
		Cancel(WERROR_INTERRUPTED);
		return;
	}

	if (InMoveRange())
	{
		_weenie->AdjustMana(_weenie->GetMana() * -0.5);
		_weenie->TeleportToLifestone();
		Done();
	}
	else
		Cancel(WERROR_MOVED_TOO_FAR);
}

void CHouseRecallUseEvent::OnReadyToUse()
{
	SetupRecall();
	_timeout = Timer::cur_time + 15.0;
	_weenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	ExecuteUseAnimation(Motion_HouseRecall);
	g_pWorld->BroadcastLocal(_weenie->GetLandcell(), csprintf("%s is recalling home.", _weenie->GetName().c_str()));
}

void CHouseRecallUseEvent::OnUseAnimSuccess(DWORD motion)
{
	if (_weenie->IsDead() || _weenie->IsInPortalSpace() || _weenie->get_minterp()->interpreted_state.forward_command != Motion_Ready)
	{
		Cancel(WERROR_INTERRUPTED);
		return;
	}

	if (InMoveRange())
	{
		_weenie->TeleportToHouse();
		Done();
	}
	else
		Cancel(WERROR_MOVED_TOO_FAR);
}

void CMansionRecallUseEvent::OnReadyToUse()
{
	SetupRecall();
	_timeout = Timer::cur_time + 15.0;
	_weenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	ExecuteUseAnimation(Motion_HouseRecall);
	g_pWorld->BroadcastLocal(_weenie->GetLandcell(), csprintf("%s is recalling to the Allegiance housing.", _weenie->GetName().c_str()));
}

void CMansionRecallUseEvent::OnUseAnimSuccess(DWORD motion)
{
	if (_weenie->IsDead() || _weenie->IsInPortalSpace() || _weenie->get_minterp()->interpreted_state.forward_command != Motion_Ready)
	{
		Cancel(WERROR_INTERRUPTED);
		return;
	}

	if (InMoveRange())
	{
		_weenie->TeleportToMansion();
		Done();
	}
	else
		Cancel(WERROR_MOVED_TOO_FAR);
}

void CMarketplaceRecallUseEvent::OnReadyToUse()
{
	SetupRecall();
	_timeout = Timer::cur_time + 18.0;
	_weenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	ExecuteUseAnimation(Motion_MarketplaceRecall);
	g_pWorld->BroadcastLocal(_weenie->GetLandcell(), csprintf("%s is going to the Marketplace.", _weenie->GetName().c_str()));
}

void CMarketplaceRecallUseEvent::OnUseAnimSuccess(DWORD motion)
{
	if (_weenie->IsDead() || _weenie->IsInPortalSpace() || _weenie->get_minterp()->interpreted_state.forward_command != Motion_Ready)
	{
		Cancel(WERROR_INTERRUPTED);
		return;
	}

	if (InMoveRange())
	{
		_weenie->Movement_Teleport(Position(0x016C01BC, Vector(49.11f, -31.22f, 0.005f), Quaternion(0.7009f, 0, 0, -0.7132f)));
		Done();
	}
	else
		Cancel(WERROR_MOVED_TOO_FAR);
}

void CPKArenaUseEvent::OnReadyToUse()
{
	SetupRecall();
	_timeout = Timer::cur_time + 18.0;
	_weenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	ExecuteUseAnimation(Motion_PKArenaRecall);
	g_pWorld->BroadcastLocal(_weenie->GetLandcell(), csprintf("%s is going to the PK Arena.", _weenie->GetName().c_str()));
}

void CPKArenaUseEvent::OnUseAnimSuccess(DWORD motion)
{
	if (_weenie->IsDead() || _weenie->IsInPortalSpace() || _weenie->get_minterp()->interpreted_state.forward_command != Motion_Ready)
	{
		Cancel(WERROR_INTERRUPTED);
		return;
	}

	Position *positions = new Position[5];
	positions[0] = (Position(0x00660117, Vector(30, -50, 0.005f), Quaternion(1, 0, 0, 0)));
	positions[1] = (Position(0x00660106, Vector(10, 0, 0.005f), Quaternion(0.321023f, 0, 0, -0.947071f)));
	positions[2] = (Position(0x00660103, Vector(0, -30, 0.005f), Quaternion(0.714424f, 0, 0, -0.699713f)));
	positions[3] = (Position(0x0066011E, Vector(50, 0, 0.005f), Quaternion(-0.276474f, 0, 0, -0.961021f)));
	positions[4] = (Position(0x00660127, Vector(60, -30, 0.005f), Quaternion(0.731689f, 0, 0, 0.681639f)));

	int randomLoc = getRandomNumber(0, 4);

	if (InMoveRange())
	{
		_weenie->Movement_Teleport(positions[randomLoc]);
		Done();
	}
	else
		Cancel(WERROR_MOVED_TOO_FAR);
}

void CPKLArenaUseEvent::OnReadyToUse()
{
	SetupRecall();
	_timeout = Timer::cur_time + 18.0;
	_weenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	ExecuteUseAnimation(Motion_PKArenaRecall);
	g_pWorld->BroadcastLocal(_weenie->GetLandcell(), csprintf("%s is going to the PKL Arena.", _weenie->GetName().c_str()));
}

void CPKLArenaUseEvent::OnUseAnimSuccess(DWORD motion)
{
	if (_weenie->IsDead() || _weenie->IsInPortalSpace() || _weenie->get_minterp()->interpreted_state.forward_command != Motion_Ready)
	{
		Cancel(WERROR_INTERRUPTED);
		return;
	}

	Position *positions = new Position[5];
	positions[0] = (Position(0x00670117, Vector(30, -50, 0.005f), Quaternion(1, 0, 0, 0)));
	positions[1] = (Position(0x00670106, Vector(10, 0, 0.005f), Quaternion(0.321023f, 0, 0, -0.947071f)));
	positions[2] = (Position(0x00670103, Vector(0, -30, 0.005f), Quaternion(0.714424f, 0, 0, -0.699713f)));
	positions[3] = (Position(0x0067011e, Vector(50, 0, 0.005f), Quaternion(-0.276474f, 0, 0, -0.961021f)));
	positions[4] = (Position(0x00670127, Vector(60, -30, 0.005f), Quaternion(0.731689f, 0, 0, 0.681639f)));

	int randomLoc = getRandomNumber(0, 4);

	if (InMoveRange())
	{
		_weenie->Movement_Teleport(positions[randomLoc]);
		Done();
	}
	else
		Cancel(WERROR_MOVED_TOO_FAR);
}

void CAllegianceHometownRecallUseEvent::OnReadyToUse()
{
	SetupRecall();
	_timeout = Timer::cur_time + 18.0;
	_weenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	ExecuteUseAnimation(Motion_AllegianceHometownRecall);
	g_pWorld->BroadcastLocal(_weenie->GetLandcell(), csprintf("%s is going to the Allegiance hometown.", _weenie->GetName().c_str()));
}

void CAllegianceHometownRecallUseEvent::OnUseAnimSuccess(DWORD motion)
{
	if (_weenie->IsDead() || _weenie->IsInPortalSpace() || _weenie->get_minterp()->interpreted_state.forward_command != Motion_Ready)
	{
		Cancel(WERROR_INTERRUPTED);
		return;
	}

	if (InMoveRange())
	{
		_weenie->TeleportToAllegianceHometown();
		Done();
	}
	else
		Cancel(WERROR_MOVED_TOO_FAR);
}

void CPlayerWeenie::BeginRecall(const Position &targetPos)
{
	EmitEffect(PS_Hide, 1.0f);

	_recallTime = Timer::cur_time + 2.0;
	_recallPos = targetPos;
}

void CPlayerWeenie::OnTeleported()
{
	CWeenieObject::OnTeleported();
	_recallTime = -1.0; // cancel any teleport
}

DWORD CPlayerWeenie::GetAccountHouseId()
{
	if (GetClient())
	{
		for (auto &character : GetClient()->GetCharacters())
		{
			if (character.weenie_id == id)
			{
				if (DWORD houseID = InqDIDQuality(HOUSEID_DID, 0))
					return houseID;
			}
			else
			{
				CWeenieObject *otherCharacter = CWeenieObject::Load(character.weenie_id);

				if (!otherCharacter)
				{
					WINLOG(Temp, Normal, "Failed to Load Character id: %u", character.weenie_id, " in GetAccountHouseID()");
					SERVER_ERROR << "Failed to Load Character id: %u" << character.weenie_id << " in GetAccountHouseID()";
				}
				else
				{
					if (DWORD houseID = otherCharacter->InqDIDQuality(HOUSEID_DID, 0))
					{
						delete otherCharacter;
						return houseID;
					}
					delete otherCharacter;
				}
			}
		}
	}

	return 0;
}

TradeManager* CPlayerWeenie::GetTradeManager()
{
	return m_pTradeManager;
}

void CPlayerWeenie::SetTradeManager(TradeManager * tradeManager)
{
	m_pTradeManager = tradeManager;
}

void CPlayerWeenie::ReleaseContainedItemRecursive(CWeenieObject *item)
{
	CContainerWeenie::ReleaseContainedItemRecursive(item);
}

void CPlayerWeenie::ChangeCombatMode(COMBAT_MODE mode, bool playerRequested)
{
	CMonsterWeenie::ChangeCombatMode(mode, playerRequested);

	if (m_pTradeManager && mode != NONCOMBAT_COMBAT_MODE)
	{
		m_pTradeManager->CloseTrade(this, 2); // EnteredCombat
	}
}

void CPlayerWeenie::AddCorpsePermission(CPlayerWeenie * target)
{
	if (GetID() != target->GetID()) // can't add yourself to permissions
	{
		if (!m_umCorpsePermissions.empty())
		{
			for (auto it : m_umCorpsePermissions)
			{
				if (it.first == target->GetID())
				{
					SendText(csprintf("%s is already permitted to loot your corpse!", target->GetName().c_str()), LTT_DEFAULT);
					return;
				}
			}
		}
		m_umCorpsePermissions.emplace(target->GetID(), static_cast<int>(Timer::cur_time)); // add them to our permissions list
		SendText(csprintf("%s has been permitted to loot your corpse.", target->GetName().c_str()), LTT_DEFAULT);
		target->SendText(csprintf("You have been permitted to loot the corpse of %s.", GetName().c_str()), LTT_DEFAULT);
		target->m_umConsentList.emplace(GetID(), static_cast<int>(Timer::cur_time));
	}
}

void CPlayerWeenie::RemoveCorpsePermission(CPlayerWeenie * target)
{
	if (GetID() != target->GetID()) // can't remove yourself from permissions
	{
		if (!m_umCorpsePermissions.empty())
		{
			for (auto it : m_umCorpsePermissions)
			{
				if (it.first == target->GetID())
				{
					m_umCorpsePermissions.erase(it.first); // remove them from our permissions list
					SendText(csprintf("%s is no longer permitted to loot your corpse.", target->GetName().c_str()), LTT_DEFAULT);
					target->SendText(csprintf("You are no longer permitted to loot the corpse of %s.", GetName().c_str()), LTT_DEFAULT);
					target->m_umConsentList.erase(GetID());
					return;
				}
			}
			SendText(csprintf("%s isn't permitted to loot your corpse!", target->GetName().c_str()), LTT_DEFAULT);
		}
		else
		{
			SendText("Nobody is currently permitted to loot your corpse.", LTT_DEFAULT);
		}
	}
}

void CPlayerWeenie::UpdateCorpsePermissions() // Check whether any permissions have expired
{
	if (!m_umCorpsePermissions.empty())
	{
		for (auto it : m_umCorpsePermissions)
		{
			CPlayerWeenie *cp = g_pWorld->FindPlayer(it.first);
			if (it.second < static_cast<int>(Timer::cur_time - 3600) || !cp) // If an hour has gone by or the player isn't online any more revoke permissions
			{
				RemoveCorpsePermission(cp);
			}
		}
	}
}

bool CPlayerWeenie::HasPermission(CPlayerWeenie * target)
{
	if (!m_umCorpsePermissions.empty())
	{
		for (auto it : m_umCorpsePermissions)
		{
			if (it.first == target->GetID())
				return true;
		}
	}
	return false;
}

void CPlayerWeenie::ClearPermissions()
{
	if (!m_umCorpsePermissions.empty())
	{
		m_umCorpsePermissions.clear();
	}
}

void CPlayerWeenie::RemoveConsent(CPlayerWeenie * target)
{
	if (!m_umConsentList.empty())
	{
		for (auto it : m_umConsentList)
		{
			if (it.first == target->GetID())
			{
				CPlayerWeenie *cp = g_pWorld->FindPlayer(it.first);
				m_umConsentList.erase(it.first); // remove from our consent list
				SendText(csprintf("You have removed the corpse looting permission of %s.", target->GetName().c_str()), LTT_DEFAULT);
				cp->m_umCorpsePermissions.erase(GetID());
				cp->SendText(csprintf("%s is no longer permitted to loot your corpse.", GetName().c_str()), LTT_DEFAULT);
				return;
			}
		}
		SendText(csprintf("You don't have corpse looting permission from %s.", target->GetName().c_str()), LTT_DEFAULT);
	}
}

void CPlayerWeenie::DisplayConsent()
{
	if (!m_umConsentList.empty())
	{
		SendText("You have corpse looting permission from:", LTT_DEFAULT);
		for (auto it : m_umConsentList)
		{
			CPlayerWeenie *cp = g_pWorld->FindPlayer(it.first);
			SendText(cp->GetName().c_str(), LTT_DEFAULT);
		}
	}
	else
	{
		SendText("You do not have permission to loot anyone's corpse.", LTT_DEFAULT);
	}
}

void CPlayerWeenie::ClearConsent(bool onLogout)
{
	if (!m_umConsentList.empty())
	{
		for (auto it : m_umConsentList)
		{
			CPlayerWeenie *cp = g_pWorld->FindPlayer(it.first);
			cp->m_umCorpsePermissions.erase(GetID());
			SendText(csprintf("You are no longer permitted to loot the corpse of %s.", cp->GetName().c_str()), LTT_DEFAULT);
			cp->SendText(csprintf("%s is no longer permitted to loot your corpse.", GetName().c_str()), LTT_DEFAULT);
		}
		m_umConsentList.clear();
		if (!onLogout)
			SendText("You have cleared your consent list. Players will have to permit you again to allow you access to their corpse.", LTT_DEFAULT);
	}
	else
	{
		if (!onLogout)
			SendText("You don't have any corpse consent to clear!", LTT_DEFAULT);
	}
}

void CPlayerWeenie::UpdatePKActivity()
{
	m_iPKActivity = Timer::cur_time + 20;

	//Set LAST_PK_ATTACK_TIMESTAMP_FLOAT for use in CACQualities::JumpStaminaCost as m_iPKActivity is not available.
	m_Qualities.SetFloat(LAST_PK_ATTACK_TIMESTAMP_FLOAT, (double) m_iPKActivity);
}

CCraftOperation *CPlayerWeenie::TryGetAlternativeOperation(CWeenieObject *target, CWeenieObject *tool, CCraftOperation *op)
{
	int coverage = target->m_Qualities.GetInt(LOCATIONS_INT, 0);

	switch (tool->m_Qualities.id)
	{
	case W_DYERAREETERNALFOOLPROOFBLUE_CLASS:
	case W_DYERAREETERNALFOOLPROOFBLACK_CLASS:
	case W_DYERAREETERNALFOOLPROOFBOTCHED_CLASS:
	case W_DYERAREETERNALFOOLPROOFDARKGREEN_CLASS:
	case W_DYERAREETERNALFOOLPROOFDARKRED_CLASS:
	case W_DYERAREETERNALFOOLPROOFDARKYELLOW_CLASS:
	case W_DYERAREETERNALFOOLPROOFLIGHTBLUE_CLASS:
	case W_DYERAREETERNALFOOLPROOFLIGHTGREEN_CLASS:
	case W_DYERAREETERNALFOOLPROOFPURPLE_CLASS:
	case W_DYERAREETERNALFOOLPROOFSILVER_CLASS:
	{
		//Get base dye recipe, set fail to false, etc.

		//Check if the item is armor/clothing and is Dyeable.
		if (target->m_Qualities.m_WeenieType != 2 || !target->m_Qualities.GetBool(DYABLE_BOOL, 0))
			return NULL;

		//Grab dye recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3844);
		op->_difficulty = 0;
		op->_failAmount = 0;
		op->_failureConsumeToolAmount = 0;
		op->_failureConsumeToolChance = 0;
		op->_successAmount = 0;
		op->_successConsumeToolAmount = 0;
		op->_successConsumeToolChance = 0;
		op->_successWcid = 0;
		op->_failWcid = 0;

		break;
	}
	case W_MATERIALRAREETERNALIVORY_CLASS:
	{
		//Ivory Stuff here, grab ivory recipe, set fail to false, etc.

		//Check if the item is Ivoryable
		if (!target->m_Qualities.GetBool(IVORYABLE_BOOL, 0))
			return NULL;

		//Grab ivory recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3977);
		op->_difficulty = 0;
		op->_failAmount = 0;
		op->_failureConsumeToolAmount = 0;
		op->_failureConsumeToolChance = 0;
		op->_successAmount = 0;
		op->_successConsumeToolAmount = 0;
		op->_successConsumeToolChance = 0;
		op->_successWcid = 0;
		op->_failWcid = 0;

		break;
	}
	case W_MATERIALRAREETERNALLEATHER_CLASS:
	{
		//Leather here, grab leather recipe, set fail to false, etc.

		//Check if the item is already retained or if it is not sellable.
		if (target->m_Qualities.GetBool(RETAINED_BOOL, 0) || !target->m_Qualities.GetBool(IS_SELLABLE_BOOL, 1))
			return NULL;

		//Grab leather recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(4426);

		op->_difficulty = 0;
		op->_failAmount = 0;
		op->_failureConsumeToolAmount = 0;
		op->_failureConsumeToolChance = 0;
		op->_successAmount = 0;
		op->_successConsumeToolAmount = 0;
		op->_successConsumeToolChance = 0;
		op->_successWcid = 0;
		op->_failWcid = 0;

		break;
	}
	case W_MATERIALIVORY_CLASS:
	{
		//Check if the item is Ivoryable
		if (!target->m_Qualities.GetBool(IVORYABLE_BOOL, 0))
			return NULL;

		//Grab ivory recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3977);
		break;
	}
	case W_MATERIALLEATHER_CLASS:
	{
		//Check if the item is already retained or if it is not sellable.
		if (target->m_Qualities.GetBool(RETAINED_BOOL, 0) || !target->m_Qualities.GetBool(IS_SELLABLE_BOOL, 1))
			return NULL;

		//Grab leather recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(4426);
		break;
	}
	case W_MATERIALGOLD_CLASS:
	{
		//Check if the item has value and workmanship.
		if (!target->m_Qualities.GetInt(VALUE_INT, 0) || !target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0))
			return NULL;

		//Grab gold recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3851);
		break;
	}
	case W_MATERIALLINEN_CLASS:
	{
		//Check if the item has burden and workmanship.
		if (!target->m_Qualities.GetInt(ENCUMB_VAL_INT, 0) || !target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0))
			return NULL;

		//Grab linen recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3854);
		break;
	}
	case W_MATERIALMOONSTONE_CLASS:
	{
		//Check if the item has mana and workmanship.
		if (!target->m_Qualities.GetInt(ITEM_MAX_MANA_INT, 0) || !target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0))
			return NULL;

		//Grab moonstone recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3978);
		break;
	}
	case W_MATERIALPINE_CLASS:
	{
		//Check if the item has value and workmanship.
		if (!target->m_Qualities.GetInt(VALUE_INT, 0) || !target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0))
			return NULL;

		//Grab pine recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3858);
		break;
	}
	case W_MATERIALIRON_CLASS:
	case W_MATERIALGRANITE_CLASS:
	case W_MATERIALVELVET_CLASS:
	{
		//Check if the item is a melee weapon and has workmanship.
		if (target->m_Qualities.m_WeenieType != MeleeWeapon_WeenieType || !target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0))
			return NULL;

		//Grab correct recipe to use as a base.
		switch (tool->m_Qualities.id)
		{
		case W_MATERIALIRON_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3853); break;
		case W_MATERIALGRANITE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3852); break;
		case W_MATERIALVELVET_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3861); break;
		}

		break;

	}
	case W_MATERIALMAHOGANY_CLASS:
	{
		//Check if the item is a missile weapon and has workmanship.
		if (target->m_Qualities.m_WeenieType != MissileLauncher_WeenieType || !target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0))
			return NULL;

		//Grab mahog recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3855);
		break;
	}
	case W_MATERIALOAK_CLASS:
	{
		//Check if the item is a missile weapon and has workmanship.
		if (target->m_Qualities.m_WeenieType != MissileLauncher_WeenieType || !target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0))
			return NULL;

		//Grab oak recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3857);
		break;
	}
	case W_MATERIALOPAL_CLASS:
	{
		//Check if the item is a caster and has workmanship.
		if (target->m_Qualities.m_WeenieType != Caster_WeenieType || !target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0))
			return NULL;

		//Grab opal recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3979);
		break;
	}
	case W_MATERIALGREENGARNET_CLASS:
	{
		//Check if the item is a caster and has workmanship.
		if (target->m_Qualities.m_WeenieType != Caster_WeenieType || !target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0))
			return NULL;

		//Grab green garnet recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(5202);
		break;
	}
	case W_MATERIALBRASS_CLASS:
	{
		//Check if the item has workmanship.
		if (!target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0))
			return NULL;

		//Grab brass recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3848);
		break;
	}
	case W_MATERIALROSEQUARTZ_CLASS:
	case W_MATERIALREDJADE_CLASS:
	case W_MATERIALMALACHITE_CLASS:
	case W_MATERIALLAVENDERJADE_CLASS:
	case W_MATERIALHEMATITE_CLASS:
	case W_MATERIALBLOODSTONE_CLASS:
	case W_MATERIALAZURITE_CLASS:
	case W_MATERIALAGATE_CLASS:
	case W_MATERIALSMOKYQUARTZ_CLASS:
	case W_MATERIALCITRINE_CLASS:
	case W_MATERIALCARNELIAN_CLASS:
	{
		//Check if the item is a generic weenietype, jewelry item type, and has workmanship.
		if (target->m_Qualities.m_WeenieType != Generic_WeenieType || !target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0) || target->m_Qualities.GetInt(ITEM_TYPE_INT, 0) != TYPE_JEWELRY)
			return NULL;

		//Grab correct recipe to use as a base.
		switch (tool->m_Qualities.id)
		{
		case W_MATERIALROSEQUARTZ_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4446); break;
		case W_MATERIALREDJADE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4442); break;
		case W_MATERIALMALACHITE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4438); break;
		case W_MATERIALLAVENDERJADE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4441); break;
		case W_MATERIALHEMATITE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4440); break;
		case W_MATERIALBLOODSTONE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4448); break;
		case W_MATERIALAZURITE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4437); break;
		case W_MATERIALAGATE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4445); break;
		case W_MATERIALSMOKYQUARTZ_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4447); break;
		case W_MATERIALCITRINE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4439); break;
		case W_MATERIALCARNELIAN_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4443); break;
		}

		break;

	}
	case W_MATERIALSTEEL_CLASS:
	case W_MATERIALALABASTER_CLASS:
	case W_MATERIALBRONZE_CLASS:
	case W_MATERIALMARBLE_CLASS:
	case W_MATERIALARMOREDILLOHIDE_CLASS:
	case W_MATERIALCERAMIC_CLASS:
	case W_MATERIALWOOL_CLASS:
	case W_MATERIALREEDSHARKHIDE_CLASS:
	case W_MATERIALSILVER_CLASS:
	case W_MATERIALCOPPER_CLASS:
	{
		//Armor and shields
		//Check if the item is armor item type, has AL, and has workmanship.
		if (target->m_Qualities.GetInt(ITEM_TYPE_INT, 0) != TYPE_ARMOR || !target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0) || !target->m_Qualities.GetInt(ARMOR_LEVEL_INT, 0))
			return NULL;

		//Grab correct recipe to use as a base.
		switch (tool->m_Qualities.id)
		{
		case W_MATERIALSTEEL_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3860); break;
		case W_MATERIALALABASTER_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3846); break;
		case W_MATERIALBRONZE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3849); break;
		case W_MATERIALMARBLE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3856); break;
		case W_MATERIALARMOREDILLOHIDE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3847); break;
		case W_MATERIALCERAMIC_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3850); break;
		case W_MATERIALWOOL_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3862); break;
		case W_MATERIALREEDSHARKHIDE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3859); break;
		case W_MATERIALSILVER_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4427); break;
		case W_MATERIALCOPPER_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4428); break;
		}

		break;
	}
	case W_MATERIALPERIDOT_CLASS:
	case W_MATERIALYELLOWTOPAZ_CLASS:
	case W_MATERIALZIRCON_CLASS:
	case W_MATERIALRAREFOOLPROOFPERIDOT_CLASS:
	case W_MATERIALRAREFOOLPROOFYELLOWTOPAZ_CLASS:
	case W_MATERIALRAREFOOLPROOFZIRCON_CLASS:
	{
		//Armor only
		//Check if the item is clothing weenietype, armor item type, has AL, and has workmanship.
		if (target->m_Qualities.m_WeenieType != Clothing_WeenieType || target->m_Qualities.GetInt(ITEM_TYPE_INT, 0) != TYPE_ARMOR || !target->m_Qualities.GetInt(ITEM_WORKMANSHIP_INT, 0) || !target->m_Qualities.GetInt(ARMOR_LEVEL_INT, 0))
			return NULL;

		//Grab correct recipe to use as a base.
		switch (tool->m_Qualities.id)
		{
		case W_MATERIALPERIDOT_CLASS:
		case W_MATERIALRAREFOOLPROOFPERIDOT_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4435); break;
		case W_MATERIALYELLOWTOPAZ_CLASS:
		case W_MATERIALRAREFOOLPROOFYELLOWTOPAZ_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4434); break;
		case W_MATERIALZIRCON_CLASS:
		case W_MATERIALRAREFOOLPROOFZIRCON_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4433); break;
		}

		break;

	}
	case W_POTDYEDARKGREEN_CLASS:
	case W_POTDYEDARKRED_CLASS:
	case W_POTDYEDARKYELLOW_CLASS:
	case W_POTDYEWINTERBLUE_CLASS:
	case W_POTDYEWINTERGREEN_CLASS:
	case W_POTDYEWINTERSILVER_CLASS:
	case W_POTDYESPRINGBLACK_CLASS:
	case W_POTDYESPRINGBLUE_CLASS:
	case W_POTDYESPRINGPURPLE_CLASS:
	{
		//Check if the item is armor/clothing and is Dyeable.
		if (target->m_Qualities.m_WeenieType != Clothing_WeenieType || !target->m_Qualities.GetBool(DYABLE_BOOL, 0))
			return NULL;

		//Grab dye recipe to use as a base.
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3844);
		break;
	}
	//Regular and Foolproof imbues, use wcid to grab the operation. 100% chance is handled in Imbue code for foolproof.
	case W_MATERIALRAREFOOLPROOFAQUAMARINE_CLASS:
	case W_MATERIALAQUAMARINE_CLASS:
		op = g_pPortalDataEx->_craftTableData._operations.lookup(4436); break;
	case W_MATERIALRAREFOOLPROOFBLACKGARNET_CLASS:
	case W_MATERIALBLACKGARNET_CLASS:
		op = g_pPortalDataEx->_craftTableData._operations.lookup(4449); break;
	case W_MATERIALRAREFOOLPROOFBLACKOPAL_CLASS:
	case W_MATERIALBLACKOPAL_CLASS:
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3863); break;
	case W_MATERIALRAREFOOLPROOFEMERALD_CLASS:
	case W_MATERIALEMERALD_CLASS:
		op = g_pPortalDataEx->_craftTableData._operations.lookup(4450); break;
	case W_MATERIALRAREFOOLPROOFFIREOPAL_CLASS:
	case W_MATERIALFIREOPAL_CLASS:
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3864); break;
	case W_MATERIALRAREFOOLPROOFIMPERIALTOPAZ_CLASS:
	case W_MATERIALIMPERIALTOPAZ_CLASS:
		op = g_pPortalDataEx->_craftTableData._operations.lookup(4454); break;
	case W_MATERIALRAREFOOLPROOFJET_CLASS:
	case W_MATERIALJET_CLASS:
		op = g_pPortalDataEx->_craftTableData._operations.lookup(4451); break;
	case W_MATERIALRAREFOOLPROOFREDGARNET_CLASS:
	case W_MATERIALREDGARNET_CLASS:
		op = g_pPortalDataEx->_craftTableData._operations.lookup(4452); break;
	case W_MATERIALRAREFOOLPROOFSUNSTONE_CLASS:
	case W_MATERIALSUNSTONE_CLASS:
		op = g_pPortalDataEx->_craftTableData._operations.lookup(3865); break;
	case W_MATERIALRAREFOOLPROOFWHITESAPPHIRE_CLASS:
	case W_MATERIALWHITESAPPHIRE_CLASS:
		op = g_pPortalDataEx->_craftTableData._operations.lookup(4453); break;

	case 45683: // Left-hand tether
		op = g_pPortalDataEx->_craftTableData._operations.lookup(6798); break;
	case 45684: // Left-hand tether remover
		op = g_pPortalDataEx->_craftTableData._operations.lookup(6799); break;

	case 42979: // Core Plating Integrator
		op = g_pPortalDataEx->_craftTableData._operations.lookup(6800);
		
		if (op)
		{
			for (auto it = op->_mods[0]._stringMod.begin(); it != op->_mods[0]._stringMod.end(); it++)
			{
				if (it._Ptr->_Myval._stat64i32 == GEAR_PLATING_NAME_STRING)
				{
					switch (coverage)
					{
					case INVENTORY_LOC::HEAD_WEAR_LOC:
						it._Ptr->_Myval._value = "Core Helm Plating"; break;
					case INVENTORY_LOC::HAND_WEAR_LOC:
						it._Ptr->_Myval._value = "Core Gauntlet Plating"; break;
					case INVENTORY_LOC::FOOT_WEAR_LOC:
						it._Ptr->_Myval._value = "Core Solleret Plating"; break;
					case INVENTORY_LOC::UPPER_ARM_ARMOR_LOC:
						it._Ptr->_Myval._value = "Core Pauldron Plating"; break;
					case INVENTORY_LOC::LOWER_ARM_ARMOR_LOC:
						it._Ptr->_Myval._value = "Core Bracer Plating"; break;
					case INVENTORY_LOC::LOWER_LEG_ARMOR_LOC:
						it._Ptr->_Myval._value = "Core Greaves Plating"; break;
					case INVENTORY_LOC::ABDOMEN_ARMOR_LOC:
						it._Ptr->_Myval._value = "Core Girth Plating"; break;
					case INVENTORY_LOC::CHEST_ARMOR_LOC:
						it._Ptr->_Myval._value = "Core Chest Plating"; break;
					case INVENTORY_LOC::UPPER_LEG_ARMOR_LOC:
						it._Ptr->_Myval._value = "Core Tasset Plating"; break;
					case 0x0E: // Tunic - Abdomen, Chest, Upper Arms
					case 0x1E: // Shirt - Abdomen, Chest, Upper/Lower Arms
						it._Ptr->_Myval._value = "Core Upper Body Underplating"; break;
					case 0xC4: // Pants - Upper/Lower Legs
						it._Ptr->_Myval._value = "Core Lower Body Underplating"; break;
					case 0xDE: // Raiment - Abdomen, Upper/Lower Arms, Upper/Lower Legs
						it._Ptr->_Myval._value = "Core Raiment Underplating"; break;
					case 0x600: // Cuirass - Chest, Abdomen
						it._Ptr->_Myval._value = "Core Cuirass Plating"; break;
					case 0xE00: // Chainmail Shirt - Chest, Upper Arms, Abdomen
					case 0xA00: // Jaleh's Chain Shirt - Chest, Upper Arms
						it._Ptr->_Myval._value = "Core Shirt Plating"; break;
					case 0x1A00: // Amuli Coat - Chest, Upper/Lower Arms
						it._Ptr->_Myval._value = "Core Coat Plating"; break;
					case 0x1E00: // Hauberk - Chest, Upper/Lower Arms, Abdomen
						it._Ptr->_Myval._value = "Core Hauberk Plating"; break;
					case 0x6000: // Celdon Leggings - Upper/Lower Leg
					case 0x6400: // Amuli Leggings - Abdomen, Upper/Lower Leg
						it._Ptr->_Myval._value = "Core Leg Plating"; break;
					case 0x7F00: // Faran Robe - Chest, Abdomen, Upper/Lower Arms, Upper/Lower Legs, Feet
					case 0x7F01: // Faran Robe with Hood - Head + Robe
					case 0x7A00: // Swamp Lord's War Paint - Robe - Feet
						it._Ptr->_Myval._value = "Core Body Plating"; break;
					case 0x7F20: // Guise - All parts - Head
					case 0x7F21: // Full Guise - All parts
						it._Ptr->_Myval._value = "Core Guise Plating"; break;
					default:
						if (coverage < 0x100) // Clothing
							it._Ptr->_Myval._value = "Core Underplating";
						else
							it._Ptr->_Myval._value = "Core Armor Plating"; break;
					}
				}
			}
		}
		break;

	case 43022: // Core Plating Deintegrator
		op = g_pPortalDataEx->_craftTableData._operations.lookup(6801); break;
	default:
		return NULL;
	}

	return op;
}

DWORD CPlayerWeenie::GetTotalSkillCredits(bool removeCreditQuests) //Total current credits on player in all skills.
{
	int totalCredits = InqIntQuality(AVAILABLE_SKILL_CREDITS_INT, 0, TRUE);

	for (int i = 1; i < 55; ++i) //54 usable skills
	{
		STypeSkill skillName = (STypeSkill)i;

		switch (skillName)
		{
		case UNDEF_SKILL:
		case AXE_SKILL:
		case BOW_SKILL:
		case CROSSBOW_SKILL:
		case DAGGER_SKILL:
		case MACE_SKILL:
		case SLING_SKILL:
		case SPEAR_SKILL:
		case STAFF_SKILL:
		case SWORD_SKILL:
		case THROWN_WEAPON_SKILL:
		case UNARMED_COMBAT_SKILL:
			continue;
		}

		SkillTable *pSkillTable = SkillSystem::GetSkillTable();
		const SkillBase *pSkillBase = pSkillTable->GetSkillBase(skillName);
		if (pSkillBase != NULL)
		{
			Skill skill;
			if (!m_Qualities.InqSkill(skillName, skill))
				continue;

			if (skillName == SALVAGING_SKILL) //No credit skill. Ignore.
				continue;

			//no spec cost for tinker skills.
			if ((skill._sac == TRAINED_SKILL_ADVANCEMENT_CLASS || skill._sac == SPECIALIZED_SKILL_ADVANCEMENT_CLASS) &&
				(skillName == WEAPON_APPRAISAL_SKILL ||
					skillName == ARMOR_APPRAISAL_SKILL ||
					skillName == MAGIC_ITEM_APPRAISAL_SKILL ||
					skillName == ITEM_APPRAISAL_SKILL))
			{
				totalCredits += pSkillBase->_trained_cost;
				continue;
			}

			if (skill._sac == TRAINED_SKILL_ADVANCEMENT_CLASS && skillName == ARCANE_LORE_SKILL) //ignore the 4 points to train arcane lore. We start with it and it cannot be untrained.
				continue;

			//these are base skills. Can't be untrianed/have no training cost. Lore needs an extra catch due to the errant training cost.
			if (skill._sac == SPECIALIZED_SKILL_ADVANCEMENT_CLASS &&
				(pSkillBase->_trained_cost <= 0 || skillName == ARCANE_LORE_SKILL))
				//skills of this nature -> ARCANE_LORE_SKILL, RUN_SKILL, JUMP_SKILL, MAGIC_DEFENSE_SKILL, LOYALTY_SKILL
			{
				totalCredits += pSkillBase->_specialized_cost - pSkillBase->_trained_cost;
				continue;
			}

			//all the rest
			if (skill._sac == TRAINED_SKILL_ADVANCEMENT_CLASS)
				totalCredits += pSkillBase->_trained_cost;

			if (skill._sac == SPECIALIZED_SKILL_ADVANCEMENT_CLASS)
				totalCredits += pSkillBase->_specialized_cost;
		}
	}
	if (removeCreditQuests) //defaults to including these.
	{
		if (InqQuest("arantahkill1")) //Aun Ralirea
			totalCredits -= 1;

		if (InqQuest("ChasingOswaldDone")) //Oswald
			totalCredits -= 1;

		//todo add Luminance Skill Credit Checks (2 credits).
	}
	return totalCredits;
}

DWORD CPlayerWeenie::GetExpectedSkillCredits(bool countCreditQuests)
{
	DWORD expectedCredits = 52; //Base starting credits. 102 credits is the MAX # credits attainable.
	if (countCreditQuests) //default true
	{
		if (InqQuest("arantahkill1")) //Aun Ralirea
			expectedCredits += 1;

		if (InqQuest("ChasingOswaldDone")) //Oswald
			expectedCredits += 1;

		//todo add Luminance Skill Credit Checks (2 credits).
	}

	int currentLevel = InqIntQuality(LEVEL_INT, 0, TRUE);
	currentLevel = min(currentLevel, 275);
	for (int x = 1; x < currentLevel + 1; x++)
	{
		expectedCredits += ExperienceSystem::GetCreditsForLevel(x);
	}
	
	return expectedCredits;
}

void CPlayerWeenie::CancelLifestoneProtection()
{
	if (m_Qualities.GetBool(UNDER_LIFESTONE_PROTECTION_BOOL, 0))
	{
		m_Qualities.SetBool(UNDER_LIFESTONE_PROTECTION_BOOL, 0);
		m_Qualities.SetFloat(LIFESTONE_PROTECTION_TIMESTAMP_FLOAT, 0);
		SendText("Your actions have dispelled the Lifestone's magic!", LTT_MAGIC);
	}
}

std::string CPlayerWeenie::GetTitleStringById(int titleId)
{
	switch (titleId)
	{
	case 1: return "Adventurer"; break;
	case 2: return "Archer"; break;
	case 3: return "Blademaster"; break;
	case 4: return "Enchanter"; break;
	case 5: return "Life Mage"; break;
	case 6: return "Sorcerer"; break;
	case 7: return "Vagabond"; break;
	case 8: return "Warrior"; break;
	case 9: return "Bow Hunter"; break;
	case 10: return "Life Caster"; break;
	case 11: return "Soldier"; break;
	case 12: return "Swashbuckler"; break;
	case 13: return "War Mage"; break;
	case 14: return "Wayfarer"; break;
	case 15: return "Abhorrent Warrior"; break;
	case 16: return "Alchemist"; break;
	case 17: return "Annihilator"; break;
	case 18: return "Apothecary"; break;
	case 19: return "Arctic Adventurer"; break;
	case 20: return "Arctic Mattekar Annihilator"; break;
	case 21: return "Artifex"; break;
	case 22: return "Axe Warrior"; break;
	case 23: return "Ballisteer"; break;
	case 24: return "Bane of the Remoran"; break;
	case 25: return "Blood Shreth Butcher"; break;
	case 26: return "Bloodletter"; break;
	case 27: return "Broodu Killer"; break;
	case 28: return "Browerk Killer"; break;
	case 29: return "Bug Butcher"; break;
	case 30: return "Bugstomper"; break;
	case 31: return "Bunny Master"; break;
	case 32: return "Carenzi Slayer"; break;
	case 33: return "Chain Breaker"; break;
	case 34: return "Champion of Dereth"; break;
	case 35: return "Champion of Sanamar"; break;
	case 36: return "Champion of Silyun"; break;
	case 37: return "Chef"; break;
	case 38: return "Creature Adept"; break;
	case 39: return "Dagger Fighter"; break;
	case 40: return "Deadeye"; break;
	case 41: return "Deathcap Defeater"; break;
	case 42: return "Defender of Dereth"; break;
	case 43: return "Deliverer"; break;
	case 44: return "Diplomat"; break;
	case 45: return "Dire Mattekar Dispatcher"; break;
	case 46: return "Duelist"; break;
	case 47: return "Ebon Gromnie Eradicator"; break;
	case 48: return "Engorged Scourge"; break;
	case 49: return "Evoker"; break;
	case 50: return "Explorer"; break;
	case 51: return "Exterminator"; break;
	case 52: return "Fisherman"; break;
	case 53: return "Fletcher"; break;
	case 54: return "Floeshark Flogger"; break;
	case 55: return "Friend of Sanamar"; break;
	case 56: return "Friend of Silyun"; break;
	case 57: return "Gaerlan Slayer"; break;
	case 58: return "Gardener Weeder"; break;
	case 59: return "Glenden Wood Knight"; break;
	case 60: return "Glenden Wood Militia"; break;
	case 61: return "Golden God"; break;
	case 62: return "Grave Robber"; break;
	case 63: return "Guardian of Dereth"; break;
	case 64: return "Gumshoe"; break;
	case 65: return "Guppy Master"; break;
	case 66: return "Heart Ripper"; break;
	case 67: return "Hero of Dereth"; break;
	case 68: return "Hero of Sanamar"; break;
	case 69: return "Hero of Silyun"; break;
	case 70: return "Honorary Snowman"; break;
	case 71: return "Honorary Windreave"; break;
	case 72: return "Hunter"; break;
	case 73: return "Impaler"; break;
	case 74: return "Insatiable Slayer"; break;
	case 75: return "Invaders Bane"; break;
	case 76: return "Iron Chef"; break;
	case 77: return "Iron Spined Chittick Immolator"; break;
	case 78: return "Item Adept"; break;
	case 79: return "Keeper of Dereth"; break;
	case 80: return "Keerik Killer"; break;
	case 81: return "Kingslayer"; break;
	case 82: return "Kiree Killer"; break;
	case 83: return "Knath Andras Assassinator"; break;
	case 84: return "Lakeman"; break;
	case 85: return "Life Adept"; break;
	case 86: return "Lightbringer"; break;
	case 87: return "Mace Warrior"; break;
	case 88: return "Man At Arms Thrasher"; break;
	case 89: return "Master Fletcher"; break;
	case 90: return "Master of Slaughter"; break;
	case 91: return "Master of Staves"; break;
	case 92: return "Miner"; break;
	case 93: return "Moarsman Defiler"; break;
	case 94: return "Morale Smasher"; break;
	case 95: return "Mosswart Worshipper Whipper"; break;
	case 96: return "Mottled Carenzi Mauler"; break;
	case 97: return "Mud Slinger"; break;
	case 98: return "Naughty Skeleton Snuffer"; break;
	case 99: return "Olthoi Ripper Reducer"; break;
	case 100: return "Paragon of Death"; break;
	case 101: return "Philanthropist"; break;
	case 102: return "Plate Armoredillo Punisher"; break;
	case 103: return "Platinum Prowler"; break;
	case 104: return "Polar Ursuin Pounder"; break;
	case 105: return "Polardillo Pummeler"; break;
	case 106: return "Pond Scum"; break;
	case 107: return "Pro Fisherman"; break;
	case 108: return "Projectilist"; break;
	case 109: return "Pugilist"; break;
	case 110: return "Queenslayer"; break;
	case 111: return "Ravenous Killer"; break;
	case 112: return "Red Fury"; break;
	case 113: return "Reeshan Killer"; break;
	case 114: return "Rehir Killer"; break;
	case 115: return "Repugnant Eater Ripper"; break;
	case 116: return "Resistance Fighter"; break;
	case 117: return "Resistance Recruit"; break;
	case 118: return "Servant of The Deep"; break;
	case 119: return "Sezzherei Slayer"; break;
	case 120: return "Shadow Stalker"; break;
	case 121: return "Shallows Shark Nemesis"; break;
	case 122: return "Sharpshooter"; break;
	case 123: return "Shellfish Hater"; break;
	case 124: return "Shield of Glenden"; break;
	case 125: return "Shredder"; break;
	case 126: return "Silver Serf"; break;
	case 127: return "Siraluun Slasher"; break;
	case 128: return "Skipper"; break;
	case 129: return "Skullcrusher"; break;
	case 130: return "Skullsplitter"; break;
	case 131: return "Slicer"; break;
	case 132: return "Sniper"; break;
	case 133: return "Soldier Slaughterer"; break;
	case 134: return "Spear Warrior"; break;
	case 135: return "Spring Cleaner"; break;
	case 136: return "Staff Warrior"; break;
	case 137: return "Stonebreaker"; break;
	case 138: return "Sureshot"; break;
	case 139: return "Swordfighter"; break;
	case 140: return "Theurgist"; break;
	case 141: return "Thrungus Reaper"; break;
	case 142: return "Timberman"; break;
	case 143: return "Tracker"; break;
	case 144: return "Trapper"; break;
	case 145: return "Tukora Lieutenant Trouncer"; break;
	case 146: return "Unarmed Brawler"; break;
	case 147: return "Violator Grievver Vetoer"; break;
	case 148: return "Voracious Flayer"; break;
	case 149: return "Voracious Hunter"; break;
	case 150: return "War Adept"; break;
	case 151: return "Warden of Dereth"; break;
	case 152: return "Warlock"; break;
	case 153: return "Warlord of Dereth"; break;
	case 154: return "Warrior Vanquisher"; break;
	case 155: return "Wicked Skeleton Walloper"; break;
	case 156: return "Windreave Stalker"; break;
	case 157: return "Worker Obliterator"; break;
	case 158: return "Dryreach Militia"; break;
	case 159: return "Honorary Shadow Hunter"; break;
	case 160: return "Perforated Knight"; break;
	case 162: return "Pest Control"; break;
	case 163: return "Rat Reaper"; break;
	case 164: return "Nymph Maniac"; break;
	case 165: return "SimiuS"; break;
	case 166: return "Second Place Lore Quiz Night"; break;
	case 167: return "AC Veteran"; break;
	case 168: return "Aint afraid of no ghost"; break;
	case 169: return "Aint afraid of no ghost two"; break;
	case 170: return "Aint afraid of no ghost three"; break;
	case 171: return "Ankle Biter"; break;
	case 172: return "Annoying Furry Talking Animal"; break;
	case 173: return "Archer Assassin"; break;
	case 174: return "Archmage"; break;
	case 175: return "Asherons Call God"; break;
	case 176: return "Avatar of Kain"; break;
	case 177: return "Azure Assailant"; break;
	case 178: return "Baby Bunny Master"; break;
	case 179: return "Baron Oddity"; break;
	case 180: return "Beginning of the End"; break;
	case 181: return "Best Melee in DT"; break;
	case 182: return "Big Block"; break;
	case 183: return "Big Rocx"; break;
	case 184: return "Blood"; break;
	case 185: return "Bounty Hunter"; break;
	case 186: return "British Tootsie"; break;
	case 187: return "Bug Bait"; break;
	case 188: return "Bunny Killer"; break;
	case 189: return "Carebear Stare"; break;
	case 190: return "Certified Ganksta"; break;
	case 191: return "Chicken Nugget"; break;
	case 192: return "Chicken Select"; break;
	case 193: return "Chimaeras Champion"; break;
	case 194: return "Co Monarch of TheChosen"; break;
	case 195: return "Containment Unit"; break;
	case 196: return "Coolest Title Ever"; break;
	case 197: return "Corrosive Soul"; break;
	case 198: return "Cow Killer"; break;
	case 199: return "Cowboy"; break;
	case 200: return "Crystal Lord King"; break;
	case 201: return "Cuddly Kitten"; break;
	case 202: return "Curmudgeons Friend"; break;
	case 203: return "Cursed Adventurer"; break;
	case 204: return "Dagger Master"; break;
	case 205: return "Dark Sword Lady"; break;
	case 206: return "Darktides Finest"; break;
	case 207: return "Darktides Most Wanted"; break;
	case 208: return "Dead Meat"; break;
	case 209: return "Deadly Eggburt"; break;
	case 210: return "Death by Default"; break;
	case 211: return "Death by Pumpkin"; break;
	case 212: return "Death by Sappho"; break;
	case 213: return "Death Specialist"; break;
	case 214: return "Defender of Mr  P"; break;
	case 215: return "Dereths Gladiator"; break;
	case 216: return "Dereths Master Dodger"; break;
	case 217: return "Dev Evader"; break;
	case 218: return "Dev Killer"; break;
	case 219: return "Dev Slayer"; break;
	case 220: return "Double Fashion King"; break;
	case 221: return "Double Golem Killer"; break;
	case 222: return "Double Survival Champion"; break;
	case 223: return "Dude of DOOM"; break;
	case 224: return "Ecto Cooler"; break;
	case 225: return "Ecto Cooler two"; break;
	case 226: return "Envoy Slayer"; break;
	case 227: return "Escapee Killer"; break;
	case 228: return "Evil Toothfairy"; break;
	case 229: return "Fancy Title"; break;
	case 230: return "Fashion King"; break;
	case 231: return "Fashion Penguin King"; break;
	case 232: return "Fashion Queen"; break;
	case 233: return "Fashion Victim"; break;
	case 234: return "Fearless Fighter"; break;
	case 235: return "Flag Boy"; break;
	case 236: return "Frelorn Slayer"; break;
	case 237: return "Friend to Mr  P"; break;
	case 238: return "Gangsta Style"; break;
	case 239: return "Gangster"; break;
	case 240: return "Ghostbuster"; break;
	case 241: return "Gimp"; break;
	case 242: return "Gimp Goddess"; break;
	case 243: return "Gimpy Mage of Might"; break;
	case 244: return "Gladiator Champion"; break;
	case 245: return "Glowing Archer"; break;
	case 246: return "God of the Swordsman"; break;
	case 247: return "Golem Killer"; break;
	case 248: return "Golem Slayer"; break;
	case 249: return "Great Pookie Slayer"; break;
	case 250: return "Guardian Angel"; break;
	case 251: return "Guardian of Arwic"; break;
	case 252: return "Hamster Herder"; break;
	case 253: return "Hamster Vitae"; break;
	case 254: return "Have Bunnies Will Travel"; break;
	case 255: return "Have Trike Will Babble"; break;
	case 256: return "Hide and Seek Specialist"; break;
	case 257: return "Hollywood Bud"; break;
	case 258: return "Holtburg Survival Champion"; break;
	case 259: return "Holtburgs Survivor Champion"; break;
	case 260: return "Honorary Pink Eep"; break;
	case 261: return "Hungry Hungry Hippo"; break;
	case 262: return "Husband of Lynnie"; break;
	case 263: return "Husbands Healer"; break;
	case 264: return "I am Big PIMPIN"; break;
	case 265: return "I liked dying"; break;
	case 266: return "Jessica Heart"; break;
	case 267: return "King of all that is Pumpkin"; break;
	case 268: return "King of Caul"; break;
	case 269: return "King of Darktide"; break;
	case 270: return "King of Pierce Diggys"; break;
	case 271: return "Knight Rider"; break;
	case 272: return "Lady of the Four Towers"; break;
	case 273: return "Lange Gang"; break;
	case 274: return "Level Master"; break;
	case 275: return "Leviathan"; break;
	case 276: return "Leviathan Slayer"; break;
	case 277: return "Libe Events Manager"; break;
	case 278: return "Life Preserver"; break;
	case 279: return "Live Event Coordinator"; break;
	case 280: return "Live Events Manager"; break;
	case 281: return "Live Op Coordinator"; break;
	case 282: return "Lore Second Place"; break;
	case 283: return "Lore Second Place Quiz Night"; break;
	case 284: return "Lore Champion   Quiz Night"; break;
	case 285: return "Lore Champion Quiz Night"; break;
	case 286: return "Lore Master"; break;
	case 287: return "Lore Master Quiz Night"; break;
	case 288: return "Lore Master  Quiz Night"; break;
	case 289: return "Lore Master Second Place"; break;
	case 290: return "Lore Master of Staves"; break;
	case 291: return "Lots of Vitae"; break;
	case 292: return "Lucky Lady"; break;
	case 293: return "Mad Cow Slayer"; break;
	case 294: return "Mages Assitant"; break;
	case 295: return "Master Tinkerer"; break;
	case 296: return "Mayor of Ayan Baqur"; break;
	case 297: return "Minty Fresh"; break;
	case 298: return "Misunderstood Pengy"; break;
	case 299: return "Mom"; break;
	case 300: return "Mommy Pengy"; break;
	case 301: return "Mosswart Ally"; break;
	case 302: return "Mr Nice Guy"; break;
	case 303: return "Murderer"; break;
	case 304: return "Murderer two"; break;
	case 305: return "Newby Title"; break;
	case 306: return "Nice Blue Lady"; break;
	case 307: return "Number one draft pick"; break;
	case 308: return "Old School"; break;
	case 309: return "Paradox Slayer"; break;
	case 310: return "Patron Saint of the Lifestone"; break;
	case 311: return "Peacebear"; break;
	case 312: return "Penguin Killer"; break;
	case 313: return "Pig Farmer"; break;
	case 314: return "Pink Bunny Killer"; break;
	case 315: return "Pink Bunny Master"; break;
	case 316: return "Pink Goddess"; break;
	case 317: return "Pink Lady"; break;
	case 318: return "PK Queen"; break;
	case 319: return "Playa Killa King"; break;
	case 320: return "Pokemon Slayer"; break;
	case 321: return "Pretty Pumpkin"; break;
	case 322: return "Prisoner"; break;
	case 323: return "Protector of the Cheese"; break;
	case 324: return "Proton Pack"; break;
	case 325: return "Pumpkin Slayer"; break;
	case 326: return "Pumpkin Stalker"; break;
	case 327: return "Purple Pengy"; break;
	case 328: return "Pwner of Newbs"; break;
	case 329: return "Queen of Brats"; break;
	case 330: return "Queens Crafter"; break;
	case 331: return "Queens Crafter two"; break;
	case 332: return "Queens Pack Rat"; break;
	case 333: return "Queens Packrat"; break;
	case 334: return "Queens Tinker"; break;
	case 335: return "Rawr"; break;
	case 336: return "Raynes Mage Pet"; break;
	case 337: return "Reggae Renegade"; break;
	case 338: return "Ring Bearer"; break;
	case 339: return "Ring ring ring Bananaphone"; break;
	case 340: return "Road Kill"; break;
	case 341: return "Rolling Death Destroyer"; break;
	case 342: return "Royal Chef"; break;
	case 343: return "Royal Cook"; break;
	case 344: return "Rugged"; break;
	case 345: return "Sanitation Engineer"; break;
	case 346: return "Scarecrow Slayer"; break;
	case 347: return "Scary Pumkpkin Dominator"; break;
	case 348: return "Scary Pumpkin Slayer"; break;
	case 349: return "Sentinal"; break;
	case 350: return "Sentry"; break;
	case 351: return "Shadow Child"; break;
	case 352: return "Shadow Hunter"; break;
	case 353: return "Shadow Slayer"; break;
	case 354: return "Slayer of the Great Pookie"; break;
	case 355: return "Slimer"; break;
	case 356: return "Snazzy Dresser"; break;
	case 357: return "Soul Man"; break;
	case 358: return "Spooky Title"; break;
	case 359: return "Staff Masta"; break;
	case 360: return "Staypuffed Mallow Girl"; break;
	case 361: return "Super Gimped Adventurer"; break;
	case 362: return "Supercalifragilisticexpialidocious"; break;
	case 363: return "Superman"; break;
	case 364: return "Survivor Champion"; break;
	case 365: return "Sword Scholar"; break;
	case 366: return "Teh Newb"; break;
	case 367: return "Teh Saint"; break;
	case 368: return "Testing Guy"; break;
	case 369: return "The Bait"; break;
	case 370: return "The Chosen King"; break;
	case 371: return "The Come Back Kid"; break;
	case 372: return "The Forgotten"; break;
	case 373: return "The Gimped"; break;
	case 374: return "The Great and Powerful"; break;
	case 375: return "The Half Fox"; break;
	case 376: return "The Noob"; break;
	case 377: return "The Purple Fez"; break;
	case 378: return "The Pwnerer"; break;
	case 379: return "The Unknown Pk"; break;
	case 380: return "Thunder Chicken Slayer"; break;
	case 381: return "TM Killer"; break;
	case 382: return "Tough Guy"; break;
	case 383: return "Tradesman of Auberean"; break;
	case 384: return "Treasure Hunter"; break;
	case 385: return "Tremendous Monouga Master"; break;
	case 386: return "Triple Golem Killer"; break;
	case 387: return "Turbine Slayer"; break;
	case 388: return "Twins of Terror"; break;
	case 389: return "Uberest Noob on Frostfell"; break;
	case 390: return "Underworld Thugg"; break;
	case 391: return "Viamontian Slayer"; break;
	case 392: return "Vitae King"; break;
	case 393: return "Vitae Vixen"; break;
	case 394: return "Vixen of Vitae"; break;
	case 395: return "Wabbit Bait"; break;
	case 396: return "Wabbit Killer"; break;
	case 397: return "Wandering Fool"; break;
	case 398: return "Wardens Assistant"; break;
	case 399: return "Wardens Thug"; break;
	case 400: return "Wimp"; break;
	case 401: return "Wolfpack Crafter"; break;
	case 402: return "Wonder Woman"; break;
	case 403: return "Ya got served"; break;
	case 404: return "GIMP caps"; break;
	case 405: return "Wardens Assistant two"; break;
	case 406: return "Supreme Bunny Master"; break;
	case 407: return "Guardian of the Dark"; break;
	case 408: return "Aprils Fool"; break;
	case 409: return "Ulgrims Eep"; break;
	case 410: return "Mukkir Masher"; break;
	case 411: return "Harbingers Bane"; break;
	case 412: return "Master of the Elements"; break;
	case 413: return "Killcranes Ally"; break;
	case 414: return "Apprentice Shadow Hunter"; break;
	case 415: return "Squire of the Golden Flame"; break;
	case 416: return "Knight of the Golden Flame"; break;
	case 417: return "Knight of the Whispering Blade"; break;
	case 418: return "The Black Spear"; break;
	case 420: return "Carraidas Avenger"; break;
	case 421: return "Paragon of the Whispering Blade"; break;
	case 422: return "Paragon of the Rossu Morta"; break;
	case 423: return "Chosen of Varicci"; break;
	case 424: return "Chosen of Elysa"; break;
	case 425: return "Small Mukkir Squasher"; break;
	case 426: return "Blood Warrior"; break;
	case 427: return "Strategist"; break;
	case 428: return "War Beast"; break;
	case 429: return "Champion of Daemal"; break;
	case 430: return "Hero of the Golden Flame"; break;
	case 431: return "Slayer of the Black Spear"; break;
	case 433: return "Obviously Bored"; break;
	case 434: return "Zefir Zapper"; break;
	case 435: return "Tusker Blight"; break;
	case 436: return "Elemental Eradicator"; break;
	case 437: return "Expert Banished Hunter"; break;
	case 438: return "Expert Fallen Hunter"; break;
	case 439: return "Elite Head Hunter"; break;
	case 440: return "Friend of the Frostglaive"; break;
	case 441: return "Squire of New Viamont"; break;
	case 442: return "Reanimator"; break;
	case 443: return "Zombie Incursion Survivor"; break;
	case 444: return "Ulgrims Drinking Buddy"; break;
	case 445: return "Master of the Porcelain Altar"; break;
	case 446: return "Master of the Mystical Mug"; break;
	case 447: return "Colosseum Champion"; break;
	case 448: return "Titan"; break;
	case 449: return "Colossus"; break;
	case 450: return "Master Gladiator"; break;
	case 451: return "Fearless"; break;
	case 452: return "Lord of the Arena"; break;
	case 453: return "Ring Master"; break;
	case 454: return "Pit Fighter"; break;
	case 455: return "Myrmidon"; break;
	case 456: return "Hostile Combatant"; break;
	case 457: return "Survivalist"; break;
	case 458: return "Warrior of the Seventh Circle"; break;
	case 459: return "Gladiator"; break;
	case 460: return "Contender"; break;
	case 461: return "Drudge Dread"; break;
	case 462: return "Arena Custodian"; break;
	case 463: return "Arena Rat"; break;
	case 464: return "Scrapper"; break;
	case 465: return "Champion Ring III"; break;
	case 466: return "Champion Ring V"; break;
	case 467: return "Champion Ring VI"; break;
	case 468: return "Champion Ring VII"; break;
	case 469: return "Champion Ring XII"; break;
	case 470: return "Master Champion Ring X"; break;
	case 471: return "Ruuk Ally"; break;
	case 472: return "Seeker of Asheron"; break;
	case 473: return "Seeker of Torgluuk"; break;
	case 474: return "Portal Ritualist"; break;
	case 475: return "Ranger Ruuk"; break;
	case 476: return "Basher Slasher"; break;
	case 477: return "Guruk Hunter"; break;
	case 478: return "Dead Eye for the Colossi"; break;
	case 479: return "Fiendish Huntsman"; break;
	case 480: return "Monster Killer"; break;
	case 481: return "Bosh Bosh Kibosh"; break;
	case 482: return "Mushroom Collector"; break;
	case 483: return "Fungal Farmer"; break;
	case 484: return "Mushroom Man"; break;
	case 485: return "Fungi"; break;
	case 486: return "Mushroom King"; break;
	case 487: return "Thrungamandius"; break;
	case 488: return "Initiate of the Blade"; break;
	case 489: return "Torgluuks Liberator"; break;
	case 490: return "Burun Liberator"; break;
	case 491: return "Asherons Savior"; break;
	case 492: return "Asherons Defender"; break;
	case 494: return "Fuzzy Bunny Slayer"; break;
	case 495: return "Moar Hunter"; break;
	case 496: return "Spiketooth Slayer"; break;
	case 497: return "Tower Guardian Architect"; break;
	case 498: return "Artisan of the Flame and Light"; break;
	case 499: return "Guardian of the Tower"; break;
	case 500: return "Cragstone Militia"; break;
	case 501: return "Cragstone Knight"; break;
	case 502: return "Guardian of Cragstone"; break;
	case 503: return "Knight of the Windmill"; break;
	case 504: return "Hero Of House Mhoire"; break;
	case 505: return "Lady Tairlas Champion"; break;
	case 506: return "Royal Investigator"; break;
	case 507: return "Wanga Fighter"; break;
	case 508: return "Hoogans Hero"; break;
	case 509: return "Cragstone Firefighter"; break;
	case 511: return "Stalker Stalker"; break;
	case 512: return "Ravenous"; break;
	case 513: return "Altered Hunter"; break;
	case 514: return "Augmented Hunter"; break;
	case 515: return "Drudge Doom"; break;
	case 516: return "Defender of Kryst"; break;
	case 517: return "Paradox touched Queenslayer"; break;
	case 518: return "Amelias Caretaker"; break;
	case 519: return "Prodigal Tusker Slayer"; break;
	case 520: return "Mudmouths Bane"; break;
	case 521: return "Oolutangas Bane"; break;
	case 522: return "Prodigal Shadow Slayer"; break;
	case 523: return "Prodigal Harbinger Slayer"; break;
	case 524: return "Blood Seeker"; break;
	case 525: return "Storm Rider"; break;
	case 526: return "Meddler"; break;
	case 527: return "Vile Betrayer"; break;
	case 528: return "Spy"; break;
	case 529: return "Slithis Slayer"; break;
	case 530: return "Chosen of the Rift"; break;
	case 531: return "Rift Shifter"; break;
	case 532: return "Rift Walker"; break;
	case 533: return "Reef Builder"; break;
	case 534: return "Reef Breaker"; break;
	case 535: return "Crypt Rat"; break;
	case 536: return "Grave Stalker"; break;
	case 537: return "Bone Breaker"; break;
	case 538: return "Crypt Creeper"; break;
	case 539: return "Tomb Raider"; break;
	case 540: return "Follower of the Vine"; break;
	case 541: return "Harvester"; break;
	case 542: return "Harvester Harvester"; break;
	case 543: return "Harvester Harvester Harvester"; break;
	case 544: return "Pumpkin Throne Usurper"; break;
	case 545: return "Death Knight"; break;
	case 546: return "NullTitle"; break;
	case 547: return "Tracker Guardian"; break;
	case 548: return "Dojiro Sangis Savior"; break;
	case 549: return "Master of the Hunt"; break;
	case 550: return "Initiate of the Hunt"; break;
	case 551: return "Apprentice of the Hunt"; break;
	case 552: return "Disciple of the Hunt"; break;
	case 553: return "Seeker of the Hunt"; break;
	case 554: return "Champion of the Hunt"; break;
	case 555: return "Guardian of Linvak Tukal"; break;
	case 556: return "Unwitting Participant"; break;
	case 557: return "Friend of Rheaga"; break;
	case 558: return "Slayer of Rheaga"; break;
	case 559: return "Protector of the Past"; break;
	case 560: return "Warrior of the Past"; break;
	case 561: return "Anthropologist"; break;
	case 562: return "Gold Farmer"; break;
	case 563: return "Third Eye Blinder"; break;
	case 564: return "Dire Drudge Decapitator"; break;
	case 565: return "Renegade Hunter"; break;
	case 566: return "Banderling Bully"; break;
	case 567: return "Xenophobic"; break;
	case 568: return "Shadow Sunderer"; break;
	case 569: return "Dire Huntsman"; break;
	case 570: return "Game Warden"; break;
	case 571: return "Honorary Rea of the Aun"; break;
	case 572: return "Hope of the Past"; break;
	case 573: return "Beacon of Hope"; break;
	case 574: return "Aerbaxs Bane"; break;
	case 575: return "Aerbax Slayer"; break;
	case 576: return "Jesters Emancipator"; break;
	case 577: return "Jesters Fool"; break;
	case 578: return "Jesters Accomplice"; break;
	case 579: return "Honorary Burun Scout"; break;
	case 580: return "Marble Wrangler"; break;
	case 581: return "Secret Keeper"; break;
	case 582: return "Jesters Little Helper"; break;
	case 583: return "Hard To Kill"; break;
	case 584: return "Expendable"; break;
	case 585: return "Novice of the Sea"; break;
	case 586: return "Acolyte of the Sea"; break;
	case 587: return "Minister of the Sea"; break;
	case 588: return "Defender of the Sea"; break;
	case 589: return "Champion of the Sea"; break;
	case 590: return "Master of the Sea"; break;
	case 591: return "DefeaterOfTheBlight"; break;
	case 592: return "Champion of the Aerlinthe Node"; break;
	case 593: return "Champion of the Amun Node"; break;
	case 594: return "Champion of the Esper Node"; break;
	case 595: return "Champion of the Halaetan Node"; break;
	case 596: return "Champion of the Linvak Node"; break;
	case 597: return "Champion of the Obsidian Node"; break;
	case 599: return "Ulgrims Happy Hundredth"; break;
	case 600: return "Plucker of Eyes"; break;
	case 601: return "Warden of the Burning Wood"; break;
	case 602: return "Tough Tough"; break;
	case 603: return "Archaeologist"; break;
	case 604: return "Hero of Woe"; break;
	case 605: return "Defender of Silyun"; break;
	case 606: return "Memorial Champion"; break;
	case 607: return "Thane of Colier"; break;
	case 608: return "Guardian of the Keep"; break;
	case 609: return "Warrior of the Crater Lake"; break;
	case 611: return "Arwician"; break;
	case 612: return "Desert Warrior"; break;
	case 613: return "Mayoi Protector"; break;
	case 614: return "Stone Collector"; break;
	case 615: return "Rock Star"; break;
	case 616: return "Rock Hound"; break;
	case 617: return "Warrior of Woe"; break;
	case 618: return "Has way too much time"; break;
	case 619: return "Champion Immemorial"; break;
	case 620: return "Colier Miner"; break;
	case 621: return "Legionnaire"; break;
	case 622: return "Volcanologist"; break;
	case 623: return "Hilltop Defender"; break;
	case 624: return "Arwic Noble"; break;
	case 625: return "Zharalim"; break;
	case 626: return "Samurai"; break;
	case 627: return "Guardian in the Patriarch Raids"; break;
	case 628: return "Warden in the Patriarch Raids"; break;
	case 629: return "Hero of the Patriarch Raids"; break;
	case 630: return "Marked by the Patriarchs"; break;
	case 637: return "Cathedrals Champion"; break;
	case 638: return "Asherons Militia"; break;
	case 639: return "Minik Ras Bane"; break;
	case 640: return "Kazyk Ris Bane"; break;
	case 641: return "Nivinizks Bane"; break;
	case 642: return "Minik Ras Nemesis"; break;
	case 643: return "Kazyk Ris Nemesis"; break;
	case 644: return "Nivinizks Nemesis"; break;
	case 645: return "Aubereans Sentinel"; break;
	case 646: return "Dereths Elite"; break;
	case 647: return "Defender of the Deru"; break;
	case 648: return "Bane of the Blessed"; break;
	case 649: return "Negotiator"; break;
	case 650: return "Bane of the Blight"; break;
	case 651: return "Gifted"; break;
	case 652: return "Seasonal Seeker"; break;
	case 653: return "Amateur Explorer"; break;
	case 654: return "Experienced Explorer"; break;
	case 655: return "Adept Explorer"; break;
	case 656: return "Master Explorer"; break;
	case 657: return "Elite Explorer"; break;
	case 658: return "Mountain Climber"; break;
	case 659: return "Daredevil"; break;
	case 660: return "Sky Diver"; break;
	case 661: return "Spelunker"; break;
	case 662: return "Party Goer"; break;
	case 663: return "Crater Crasher"; break;
	case 664: return "Tactical Aid"; break;
	case 665: return "True Tactician"; break;
	case 666: return "Tactical Fantastical"; break;
	case 667: return "Fire of the Tanada"; break;
	case 668: return "Crystalline Adventurer"; break;
	case 669: return "Wisp Whipper"; break;
	case 670: return "Burning Soul"; break;
	case 671: return "Frozen Fighter"; break;
	case 672: return "Dedicated"; break;
	case 673: return "Assassin"; break;
	case 674: return "Death Dealer"; break;
	case 675: return "Bridge Jumper"; break;
	case 676: return "Sure Step"; break;
	case 677: return "Player Slayer"; break;
	case 678: return "Bathed in Blood"; break;
	case 679: return "I Am Darktide"; break;
	case 680: return "Darkness in the Light"; break;
	case 681: return "Beginnings End"; break;
	case 682: return "Nexus Crawler"; break;
	case 683: return "Selfless Soul"; break;
	case 684: return "Timeless Adventurer"; break;
	case 685: return "Virindi Informer"; break;
	case 686: return "No Cage Could Hold Me"; break;
	case 687: return "All for One"; break;
	case 688: return "One for All"; break;
	case 689: return "Jack O All Trades"; break;
	case 690: return "Knight of the Northeast Tower"; break;
	case 691: return "Knight of the Northwest Tower"; break;
	case 692: return "Knight of the Southeast Tower"; break;
	case 693: return "Knight of the Southwest Tower"; break;
	case 694: return "Knight of the Mhoire Throne"; break;
	case 695: return "Historian of the Mhoire Throne"; break;
	case 696: return "Champion of House Mhoire"; break;
	case 697: return "Swordbearer of House Mhoire"; break;
	case 698: return "Archmage of House Mhoire"; break;
	case 699: return "Seeker of Castle Mhoire"; break;
	case 700: return "Steward of Castle Mhoire"; break;
	case 701: return "Golden Gear Crafter"; break;
	case 702: return "Ally of the Gold Primus"; break;
	case 703: return "Gear Knight Assassin"; break;
	case 704: return "Gear Knight Recruiter"; break;
	case 705: return "Gear Knight Emissary"; break;
	case 706: return "Menhir Seeker"; break;
	case 707: return "Mana Field Finder"; break;
	case 708: return "Assistants Assistant"; break;
	case 709: return "Arcanum Adventurer"; break;
	case 710: return "Gear Knight Defender"; break;
	case 711: return "Ripper"; break;
	case 712: return "Acid Spitter"; break;
	case 713: return "Bloodstone Hunter"; break;
	case 714: return "Guiding Light"; break;
	case 715: return "Clouded Soul"; break;
	case 716: return "Undercover of Darkness"; break;
	case 717: return "Brought to Light"; break;
	case 719: return "Illuminating the Shadows"; break;
	case 720: return "Exploring Archaeologist"; break;
	case 721: return "Contract Killer"; break;
	case 722: return "Shadow Puppet"; break;
	case 723: return "Hopebringer"; break;
	case 724: return "In the Dark"; break;
	case 726: return "Searching Shadows"; break;
	case 727: return "Shadow Soldier"; break;
	case 728: return "Bright Knight"; break;
	case 729: return "Merciful Killer"; break;
	case 730: return "Soul Siphon"; break;
	case 731: return "The TouTou Terror"; break;
	case 732: return "Double Agent"; break;
	case 733: return "Duleing with the Dark"; break;
	case 734: return "Creature of Chaos"; break;
	case 735: return "Conspirator"; break;
	case 736: return "Hero of the Night"; break;
	case 737: return "Master of the Twisted Word"; break;
	case 738: return "The Nightmare Lord"; break;
	case 739: return "The Nightmare Mage"; break;
	case 740: return "The Twisted Weaver"; break;
	case 741: return "Lord of Dark Dreams"; break;
	case 743: return "The Dark Dreamwalker"; break;
	case 745: return "The Dreamslayer"; break;
	case 746: return "The Nightmare Stalker"; break;
	case 748: return "The Thought Spiral"; break;
	case 751: return "Master of the Oubliette"; break;
	case 753: return "The Restless"; break;
	case 754: return "Fiery Spirit"; break;
	case 755: return "Icy Veins"; break;
	case 756: return "Shocking Disposition"; break;
	case 757: return "Acidic Soul"; break;
	case 758: return "Grounded Morals"; break;
	case 759: return "Darkened Heart"; break;
	case 760: return "The True Emperor"; break;
	case 761: return "Bearer of Darkness"; break;
	case 763: return "Healer Ritualist"; break;
	case 764: return "Vanquisher of the Titan"; break;
	case 765: return "Derethian Newbie"; break;
	case 766: return "Novice Wanderer"; break;
	case 767: return "Notable Citizen"; break;
	case 768: return "Adept Adventurer"; break;
	case 769: return "Intrepid Explorer"; break;
	case 770: return "Supreme Soldier"; break;
	case 771: return "Epic Warrior"; break;
	case 772: return "Paragon of New Aluvia"; break;
	case 773: return "An Auberean Legend"; break;
	case 774: return "Heretic"; break;
	case 775: return "Captain"; break;
	case 776: return "Pet Savior"; break;
	case 777: return "Avalanche Avoider"; break;
	case 778: return "Liberator of Uziz"; break;
	case 779: return "Out of Tune"; break;
	case 780: return "Hive Queenslayer"; break;
	case 781: return "Eviscerator Decimator"; break;
	case 782: return "Ultimate Warrior"; break;
	case 783: return "Stone Cold Killer"; break;
	case 784: return "Queller of Rage"; break;
	case 785: return "Ender of Torment"; break;
	case 786: return "Controller of Emotions"; break;
	case 787: return "Rynthid Ravager"; break;
	case 788: return "Lothus Servitor"; break;
	case 789: return "Ally of the Lothus"; break;
	case 790: return "Ally of the Council"; break;
	case 791: return "Emotional Wreck"; break;
	case 792: return "Minion Marauder"; break;
	case 793: return "Rynthid Wrecker"; break;
	case 794: return "Mender of the Rift"; break;
	case 795: return "Rage Quitter"; break;
	case 796: return "Sorcerer Slayer"; break;
	case 797: return "Night Owl"; break;
	case 798: return "Purifier"; break;
	case 799: return "Fan Of Asheron"; break;
	case 800: return "Asherons 1"; break;
	case 801: return "Shutterbug"; break;
	case 802: return "The Focused"; break;
	case 803: return "Dereth Recruiter"; break;
	case 804: return "Fowl Follower"; break;
	case 805: return "Follower of Asheron"; break;
	case 806: return "Follows Directions"; break;
	case 807: return "Cant Follow Directions"; break;
	case 808: return "Likes Asheron"; break;
	case 809: return "Likes Getting Titles"; break;
	case 810: return "Likes Where This Is Going"; break;
	case 811: return "Likes Ulgrim"; break;
	case 812: return "Likable"; break;
	case 813: return "Captivating"; break;
	case 814: return "Master of Ceremony"; break;
	case 815: return "Road Warrior"; break;
	case 816: return "Thriller"; break;
	case 817: return "Haunts Your Dreams"; break;
	case 818: return "Asherons Collect Caller"; break;
	case 819: return "The Inquisitive"; break;
	case 820: return "Portal Stormer"; break;
	case 821: return "Bringer of Pain"; break;
	case 822: return "Harbinger of Madness"; break;
	case 823: return "Marked By Luck"; break;
	case 824: return "Marked By Fate"; break;
	case 825: return "Marked By Fame"; break;
	case 826: return "Marked By Glamour"; break;
	case 827: return "Marked By Bacon"; break;
	case 828: return "Puzzlemaster"; break;
	case 829: return "Riddle Ravager"; break;
	case 830: return "The Cool Kids Club"; break;
	case 831: return "Epic Win"; break;
	case 832: return "Epic Fail"; break;
	case 833: return "Always Picked First"; break;
	case 834: return "Always Picked Last"; break;
	case 835: return "Forever Alone"; break;
	case 836: return "99 Problems Olthoi Ate 1"; break;
	case 837: return "Hungry for Moar"; break;
	case 838: return "I Can Haz Title Nao"; break;
	case 839: return "Snowbunny"; break;
	case 840: return "Cold As Ice"; break;
	case 841: return "Penguin Plunge"; break;
	case 842: return "Flower Sniffer"; break;
	case 843: return "Springs Into Action"; break;
	case 844: return "Heats Things Up"; break;
	case 845: return "Cruel Summer"; break;
	case 846: return "Hot in the City"; break;
	case 847: return "Runs Through Sprinklers"; break;
	case 848: return "The Knowledgeable"; break;
	case 849: return "Well Trained"; break;
	case 850: return "Pumpkin Lord"; break;
	case 851: return "Haunted"; break;
	case 852: return "Possessed"; break;
	case 853: return "Doomsayer"; break;
	case 854: return "Accursed"; break;
	case 855: return "Cold Turkey"; break;
	case 856: return "Jive Turkey"; break;
	case 857: return "Present Protector"; break;
	case 858: return "Holly Jolly Helper"; break;
	case 859: return "Loss Prevention"; break;
	case 860: return "Christmas Courier"; break;
	case 861: return "Holiday Hero"; break;
	case 862: return "Crown Of The Deru"; break;
	case 863: return "Viridian Dreamer"; break;
	case 864: return "Viridian Lord"; break;
	case 865: return "Viridian Knight"; break;
	case 866: return "Viridian Stalker"; break;
	case 867: return "Viridian Slayer"; break;
	case 868: return "Viridian Purifier"; break;
	case 869: return "Knight Of Thorns"; break;
	case 870: return "Hunter Of Thorns"; break;
	case 871: return "Mage Of Thorns"; break;
	case 872: return "Thornstalker"; break;
	case 873: return "Champion Of The Viridian Tree"; break;
	case 874: return "Corruptor Of The Root"; break;
	case 875: return "Killer Among Shadows"; break;
	case 876: return "Servant Of The Vile"; break;
	case 877: return "Knight Of Corrupted Amber"; break;
	case 878: return "Gauntlet Champion"; break;
	case 879: return "Bloodthirsty"; break;
	case 880: return "Gauntlet Gladiator"; break;
	case 881: return "Wily Warrior"; break;
	case 882: return "Soldier Of Fortune"; break;
	case 883: return "Society Savage"; break;
	case 884: return "Coin Collector"; break;
	case 885: return "Brutal Barbarian"; break;
	case 886: return "Idolized"; break;
	case 887: return "Master Of Puppets"; break;
	case 888: return "Titan Slayer"; break;
	case 889: return "Awakened"; break;
	case 890: return "Enlightened"; break;
	case 891: return "Illuminated"; break;
	case 892: return "Transcended"; break;
	case 893: return "Cosmic Conscious"; break;
	case 894: return "Last Man Standing"; break;

	default:
		return "Invalid";
		break;
	}
}