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

	//if (pClient && pClient->GetAccessLevel() >= SENTINEL_ACCESS)
	//	SetRadarBlipColor(Sentinel_RadarBlipEnum);

	SetLoginPlayerQualities();

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
	if (pEntity->m_Qualities.InqBool(VISIBILITY_BOOL, vis) && !m_bAdminVision)
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

std::string CPlayerWeenie::RemoveLastAssessed()
{
	if (m_LastAssessed != 0)
	{
		CWeenieObject *pObject = g_pWorld->FindWithinPVS(this, m_LastAssessed);

		if (pObject != NULL && !pObject->AsPlayer() && !pObject->m_bDontClear) {
			std::string name = pObject->GetName();
			pObject->MarkForDestroy();
			m_LastAssessed = 0;
			return name;
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
	CMonsterWeenie::OnDeath(killer_id);

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
			CWeenieObject *modificationSource = NULL;
			switch (intMod._unk) //this is a guess
			{
			case 0:
				modificationSource = this;
				break;
			case 1:
				modificationSource = pTool;
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

			int value = pTarget->InqIntQuality(intMod._stat, 0, true);
			switch (intMod._operationType)
			{
			case 1: //=
				value = intMod._value;
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
	//Temporary as a way to fix existing characters
	if (m_Qualities._skillStatsTable)
	{
		for (PackableHashTableWithJson<STypeSkill, Skill>::iterator entry = m_Qualities._skillStatsTable->begin(); entry != m_Qualities._skillStatsTable->end(); entry++)
		{
			Skill skill = entry->second;
			if (skill._sac == SKILL_ADVANCEMENT_CLASS::SPECIALIZED_SKILL_ADVANCEMENT_CLASS)
				m_Qualities.SetSkillLevel(entry->first, 10);
			else if (skill._sac == SKILL_ADVANCEMENT_CLASS::TRAINED_SKILL_ADVANCEMENT_CLASS)
				m_Qualities.SetSkillLevel(entry->first, 5);
			else
				m_Qualities.SetSkillLevel(entry->first, 0);
		}
	}

	if (m_Qualities.GetIID(CONTAINER_IID, 0))
	{
		m_Qualities.RemoveInstanceID(CONTAINER_IID);
	}

	//set scale on Lugian and Empyrean characters and Setup DID on Olthoi
	if (m_Qualities.GetInt(HERITAGE_GROUP_INT, 1) == Lugian_HeritageGroup)
		m_Qualities.SetFloat(DEFAULT_SCALE_FLOAT, 1.3);

	if (m_Qualities.GetInt(HERITAGE_GROUP_INT, 1) == Empyrean_HeritageGroup)
		m_Qualities.SetFloat(DEFAULT_SCALE_FLOAT, 1.2);

	//End of temporary code

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
	_weenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	ExecuteUseAnimation(Motion_LifestoneRecall);
	g_pWorld->BroadcastLocal(_weenie->GetLandcell(), csprintf("%s is recalling to the lifestone.", _weenie->GetName().c_str()));
}

void CLifestoneRecallUseEvent::OnUseAnimSuccess(DWORD motion)
{
	_weenie->AdjustMana(_weenie->GetMana() * -0.5);
	_weenie->TeleportToLifestone();
	Done();
}

void CHouseRecallUseEvent::OnReadyToUse()
{
	_weenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	ExecuteUseAnimation(Motion_HouseRecall);
	g_pWorld->BroadcastLocal(_weenie->GetLandcell(), csprintf("%s is recalling home.", _weenie->GetName().c_str()));
}

void CHouseRecallUseEvent::OnUseAnimSuccess(DWORD motion)
{
	_weenie->TeleportToHouse();
	Done();
}

void CMansionRecallUseEvent::OnReadyToUse()
{
	_weenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	ExecuteUseAnimation(Motion_HouseRecall);
	g_pWorld->BroadcastLocal(_weenie->GetLandcell(), csprintf("%s is recalling to the Allegiance housing.", _weenie->GetName().c_str()));
}

void CMansionRecallUseEvent::OnUseAnimSuccess(DWORD motion)
{
	_weenie->TeleportToMansion();
	Done();
}

void CMarketplaceRecallUseEvent::OnReadyToUse()
{
	_weenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	ExecuteUseAnimation(Motion_MarketplaceRecall);
	g_pWorld->BroadcastLocal(_weenie->GetLandcell(), csprintf("%s is going to the Marketplace.", _weenie->GetName().c_str()));
}

void CMarketplaceRecallUseEvent::OnUseAnimSuccess(DWORD motion)
{
	if (_weenie->IsDead() || _weenie->IsInPortalSpace())
	{
		Cancel();
		return;
	}

	_weenie->Movement_Teleport(Position(0x016C01BC, Vector(49.11f, -31.22f, 0.005f), Quaternion(0.7009f, 0, 0, -0.7132f)));
	Done();
}

void CAllegianceHometownRecallUseEvent::OnReadyToUse()
{
	_weenie->ChangeCombatMode(NONCOMBAT_COMBAT_MODE, false);
	ExecuteUseAnimation(Motion_AllegianceHometownRecall);
	g_pWorld->BroadcastLocal(_weenie->GetLandcell(), csprintf("%s is going to the Allegiance hometown.", _weenie->GetName().c_str()));
}

void CAllegianceHometownRecallUseEvent::OnUseAnimSuccess(DWORD motion)
{
	_weenie->TeleportToAllegianceHometown();
	Done();
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
	//Foolproof tinks, use wcid to grab the operation. 100% chance is handled in Imbue code.
		case W_MATERIALRAREFOOLPROOFAQUAMARINE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4436); break;
		case W_MATERIALRAREFOOLPROOFBLACKGARNET_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4449); break;
		case W_MATERIALRAREFOOLPROOFBLACKOPAL_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3863); break;
		case W_MATERIALRAREFOOLPROOFEMERALD_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4450); break;
		case W_MATERIALRAREFOOLPROOFFIREOPAL_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3864); break;
		case W_MATERIALRAREFOOLPROOFIMPERIALTOPAZ_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4454); break;
		case W_MATERIALRAREFOOLPROOFJET_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4451); break;
		case W_MATERIALRAREFOOLPROOFPERIDOT_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4435); break;
		case W_MATERIALRAREFOOLPROOFREDGARNET_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4452); break;
		case W_MATERIALRAREFOOLPROOFSUNSTONE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(3865); break;
		case W_MATERIALRAREFOOLPROOFWHITESAPPHIRE_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4453); break;
		case W_MATERIALRAREFOOLPROOFYELLOWTOPAZ_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4434); break;
		case W_MATERIALRAREFOOLPROOFZIRCON_CLASS:
			op = g_pPortalDataEx->_craftTableData._operations.lookup(4433); break;

		default:
			return NULL;
	}


	return op;
}