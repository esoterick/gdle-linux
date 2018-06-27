
#include "StdAfx.h"
#include "Container.h"
#include "World.h"
#include "ObjectMsgs.h"
#include "ChatMsgs.h"
#include "Player.h"
#include "WeenieFactory.h"
#include "WorldLandBlock.h"
#include "Config.h"

CContainerWeenie::CContainerWeenie()
{
	for (DWORD i = 0; i < MAX_WIELDED_COMBAT; i++)
		m_WieldedCombat[i] = std::shared_ptr<CWeenieObject>();
}

CContainerWeenie::~CContainerWeenie()
{
	if (!g_pWorld)
	{
		return;
	}

	for (DWORD i = 0; i < MAX_WIELDED_COMBAT; i++)
		m_WieldedCombat[i] = std::shared_ptr<CWeenieObject>();

	for (auto item : m_Wielded)
	{
		g_pWorld->RemoveEntity(item.lock());
	}

	m_Wielded.clear();

	for (auto item : m_Items)
	{
		g_pWorld->RemoveEntity(item.lock());
	}

	m_Items.clear();

	for (auto container : m_Packs)
	{
		g_pWorld->RemoveEntity(container.lock());
	}

	m_Packs.clear();
}

void CContainerWeenie::ApplyQualityOverrides()
{
	CWeenieObject::ApplyQualityOverrides();

	if (m_Qualities.GetInt(ITEM_TYPE_INT, 0) & TYPE_CONTAINER)
	{
		if (GetItemsCapacity() < 0)
		{
			m_Qualities.SetInt(ITEMS_CAPACITY_INT, 120);
		}

		if (GetContainersCapacity() < 0)
		{
			m_Qualities.SetInt(CONTAINERS_CAPACITY_INT, 10);
		}
	}
}

void CContainerWeenie::PostSpawn()
{
	CWeenieObject::PostSpawn();

	m_bInitiallyLocked = IsLocked();
}

int CContainerWeenie::GetItemsCapacity()
{
	return InqIntQuality(ITEMS_CAPACITY_INT, 0);
}

int CContainerWeenie::GetContainersCapacity()
{
	return InqIntQuality(CONTAINERS_CAPACITY_INT, 0);
}

std::shared_ptr<CContainerWeenie> CContainerWeenie::FindContainer(DWORD container_id)
{
	if (GetID() == container_id)
		return AsContainer();

	for (auto pack : m_Packs)
	{
		std::shared_ptr<CWeenieObject> pPack = pack.lock();

		if (pPack && pPack->GetID() == container_id)
		{
			if (std::shared_ptr<CContainerWeenie> packContainer = pPack->AsContainer())
			{
				return packContainer;
			}
		}
	}

	if (std::shared_ptr<CWeenieObject> externalObject = g_pWorld->FindObject(container_id))
	{
		if (std::shared_ptr<CContainerWeenie> externalContainer = externalObject->AsContainer())
		{
			if (externalContainer->_openedById == GetTopLevelID())
			{
				return externalContainer;
			}
		}
	}

	return NULL;
}

std::shared_ptr<CWeenieObject> CContainerWeenie::GetWieldedCombat(COMBAT_USE combatUse)
{
	// The first entry is "Undef" so we omit that.
	int index = combatUse - 1;

	if (index < 0 || index >= MAX_WIELDED_COMBAT)
		return NULL;

	return m_WieldedCombat[index].lock();
}

void CContainerWeenie::SetWieldedCombat(std::shared_ptr<CWeenieObject> wielded, COMBAT_USE combatUse)
{
	// The first entry is "Undef" so we omit that.
	int index = combatUse - 1;

	if (index < 0 || index >= MAX_WIELDED_COMBAT)
		return;

	m_WieldedCombat[index] = wielded;
}

std::shared_ptr<CWeenieObject> CContainerWeenie::GetWieldedMelee()
{
	return GetWieldedCombat(COMBAT_USE_MELEE);
}

std::shared_ptr<CWeenieObject> CContainerWeenie::GetWieldedMissile()
{
	return GetWieldedCombat(COMBAT_USE_MISSILE);
}

std::shared_ptr<CWeenieObject> CContainerWeenie::GetWieldedAmmo()
{
	return GetWieldedCombat(COMBAT_USE_AMMO);
}

std::shared_ptr<CWeenieObject> CContainerWeenie::GetWieldedShield()
{
	return GetWieldedCombat(COMBAT_USE_SHIELD);
}

std::shared_ptr<CWeenieObject> CContainerWeenie::GetWieldedTwoHanded()
{
	return GetWieldedCombat(COMBAT_USE_TWO_HANDED);
}

std::shared_ptr<CWeenieObject> CContainerWeenie::GetWieldedCaster()
{
	for (auto item : m_Wielded)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (pItem && pItem->AsCaster())
			return pItem;
	}

	return NULL;
}

void CContainerWeenie::Container_GetWieldedByMask(std::list<std::shared_ptr<CWeenieObject> > &wielded, DWORD inv_loc_mask)
{
	for (auto item : m_Wielded)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (pItem && pItem->InqIntQuality(CURRENT_WIELDED_LOCATION_INT, 0, TRUE) & inv_loc_mask)
			wielded.push_back(pItem);
	}
}

std::shared_ptr<CWeenieObject> CContainerWeenie::GetWielded(INVENTORY_LOC slot)
{
	for (auto item : m_Wielded)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (pItem && pItem->InqIntQuality(CURRENT_WIELDED_LOCATION_INT, 0, TRUE) == slot)
			return pItem;
	}

	return NULL;
}

void CContainerWeenie::ReleaseContainedItemRecursive(std::shared_ptr<CWeenieObject> item)
{
	if (!item)
		return;

	for (DWORD i = 0; i < MAX_WIELDED_COMBAT; i++)
	{
		if (item == m_WieldedCombat[i].lock())
			m_WieldedCombat[i] = std::shared_ptr<CWeenieObject>();
	}

	for (std::vector<std::weak_ptr<CWeenieObject> >::iterator equipmentIterator = m_Wielded.begin(); equipmentIterator != m_Wielded.end();)
	{
		if (equipmentIterator->lock() != item)
		{
			equipmentIterator++;
			continue;
		}

		equipmentIterator = m_Wielded.erase(equipmentIterator);
	}

	for (std::vector<std::weak_ptr<CWeenieObject> >::iterator itemIterator = m_Items.begin(); itemIterator != m_Items.end();)
	{
		if (itemIterator->lock() != item)
		{
			itemIterator++;
			continue;
		}
		
		itemIterator = m_Items.erase(itemIterator);
	}

	for (std::vector<std::weak_ptr<CWeenieObject> >::iterator packIterator = m_Packs.begin(); packIterator != m_Packs.end();)
	{
		std::shared_ptr<CWeenieObject> pack = packIterator->lock();

		if (pack != item)
		{
			pack->ReleaseContainedItemRecursive(item);
			packIterator++;
			continue;
		}

		packIterator = m_Packs.erase(packIterator);
	}

	if (item->GetContainerID() == GetID())
	{
		item->m_Qualities.SetInstanceID(CONTAINER_IID, 0);
	}

	if (item->GetWielderID() == GetID())
	{
		item->m_Qualities.SetInstanceID(WIELDER_IID, 0);
	}

	item->RecacheHasOwner();
}

BOOL CContainerWeenie::Container_CanEquip(std::shared_ptr<CWeenieObject> item, DWORD location)
{
	if (!item)
		return FALSE;

	if (!item->IsValidWieldLocation(location))
		return FALSE;

	for (auto wielded : m_Wielded)
	{
		std::shared_ptr<CWeenieObject> pWielded = wielded.lock();

		if (pWielded == item)
			return TRUE;

		if (pWielded && !pWielded->CanEquipWith(item, location))
			return FALSE;
	}

	return TRUE;
}

void CContainerWeenie::Container_EquipItem(DWORD dwCell, std::shared_ptr<CWeenieObject> item, DWORD inv_loc, DWORD child_location, DWORD placement)
{
	if (int combatUse = item->InqIntQuality(COMBAT_USE_INT, 0, TRUE))
		SetWieldedCombat(item, (COMBAT_USE)combatUse);

	bool bAlreadyEquipped = false;
	for (auto entry : m_Wielded)
	{
		std::shared_ptr<CWeenieObject> pWielded = entry.lock();

		if (pWielded == item)
		{
			bAlreadyEquipped = true;
			break;
		}
	}
	if (!bAlreadyEquipped)
	{
		m_Wielded.push_back(item);
	}

	if (child_location && placement)
	{
		item->m_Qualities.SetInt(PARENT_LOCATION_INT, child_location);
		item->set_parent(AsWeenie(), child_location);
		item->SetPlacementFrame(placement, FALSE);

		if (m_bWorldIsAware)
		{
			if (std::shared_ptr<CWeenieObject> owner = GetWorldTopLevelOwner())
			{
				if (owner->GetBlock())
				{
					owner->GetBlock()->ExchangePVS(item, 0);
				}
			}

			/*
			BinaryWriter *writer = item->CreateMessage();
			g_pWorld->BroadcastPVS(dwCell, writer->GetData(), writer->GetSize(), OBJECT_MSG, 0, FALSE);
			delete writer;
			*/

			BinaryWriter Blah;
			Blah.Write<DWORD>(0xF749);
			Blah.Write<DWORD>(GetID());
			Blah.Write<DWORD>(item->GetID());
			Blah.Write<DWORD>(child_location);
			Blah.Write<DWORD>(placement);
			Blah.Write<WORD>(GetPhysicsObj()->_instance_timestamp);
			Blah.Write<WORD>(++item->_position_timestamp);
			g_pWorld->BroadcastPVS(dwCell, Blah.GetData(), Blah.GetSize());
		}
	}
	else
	{
		if (m_bWorldIsAware)
		{
			item->_position_timestamp++;

			BinaryWriter Blah;
			Blah.Write<DWORD>(0xF74A);
			Blah.Write<DWORD>(item->GetID());
			Blah.Write<WORD>(item->_instance_timestamp);
			Blah.Write<WORD>(item->_position_timestamp);
			g_pWorld->BroadcastPVS(dwCell, Blah.GetData(), Blah.GetSize());
		}
	}
}

std::shared_ptr<CWeenieObject> CContainerWeenie::FindContainedItem(DWORD object_id)
{
	for (auto item : m_Wielded)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (pItem && pItem->GetID() == object_id)
			return pItem;
	}

	for (auto item : m_Items)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (pItem && pItem->GetID() == object_id)
			return pItem;
	}

	for (auto item : m_Packs)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (!pItem)
		{
			continue;
		}

		if (pItem->GetID() == object_id)
			return pItem;
		
		if (auto subitem = pItem->FindContainedItem(object_id))
			return subitem;
	}

	return NULL;
}

DWORD CContainerWeenie::Container_GetNumFreeMainPackSlots()
{
	return (DWORD) max(0, GetItemsCapacity() - (signed)m_Items.size());
}

BOOL CContainerWeenie::Container_CanStore(std::shared_ptr<CWeenieObject> pItem)
{
	return Container_CanStore(pItem, pItem->RequiresPackSlot());
}

BOOL CContainerWeenie::IsItemsCapacityFull()
{
	int capacity = GetItemsCapacity();

	if (capacity >= 0)
	{
		if (m_Items.size() < capacity)
			return TRUE;

		return FALSE;
	}

	return TRUE;
}

BOOL CContainerWeenie::IsContainersCapacityFull()
{
	int capacity = GetContainersCapacity();

	if (capacity >= 0)
	{
		if (m_Packs.size() < capacity)
			return TRUE;

		return FALSE;
	}

	return TRUE;
}

BOOL CContainerWeenie::Container_CanStore(std::shared_ptr<CWeenieObject> pItem, bool bPackSlot)
{
	// TODO handle: pItem->InqBoolQuality(REQUIRES_BACKPACK_SLOT_BOOL, FALSE)

	if (bPackSlot)
	{
		if (!pItem->RequiresPackSlot())
			return FALSE;
		
		int capacity = GetContainersCapacity();

		if (capacity >= 0)
		{
			if (m_Packs.size() < capacity)
				return TRUE;

			for (auto container : m_Packs)
			{
				if (container.lock() == pItem)
					return TRUE;
			}

			return FALSE;
		}
		else
		{
			if (InqBoolQuality(AI_ACCEPT_EVERYTHING_BOOL, FALSE))
				return TRUE;

			// check emote item acceptance here

			return FALSE;
		}
	}
	else
	{
		if (pItem->RequiresPackSlot())
			return FALSE;

		int capacity = GetItemsCapacity();

		if (capacity >= 0)
		{
			if (m_Items.size() < capacity)
				return TRUE;

			for (auto container : m_Items)
			{
				if (container.lock() == pItem)
					return TRUE;
			}

			return FALSE;
		}
		else
		{
			if (InqBoolQuality(AI_ACCEPT_EVERYTHING_BOOL, FALSE))
				return TRUE;

			// check emote item acceptance here
			if (m_Qualities._emote_table)
			{
				PackableList<EmoteSet> *emoteCategory = m_Qualities._emote_table->_emote_table.lookup(Give_EmoteCategory);

				if (emoteCategory)
				{
					for (auto &emoteSet : *emoteCategory)
					{
						if (emoteSet.classID == pItem->m_Qualities.id)
						{
							return TRUE;
						}
					}
				}
			}

			return FALSE;
		}
	}
}

void CContainerWeenie::Container_DeleteItem(DWORD item_id)
{
	std::shared_ptr<CWeenieObject> item = FindContainedItem(item_id);
	if (!item)
		return;

	bool bWielded = item->IsWielded() ? true : false;

	// take it out of whatever slot it is in
	ReleaseContainedItemRecursive(item);

	item->SetWeenieContainer(0);
	item->SetWielderID(0);
	item->SetWieldedLocation(INVENTORY_LOC::NONE_LOC);
	
	if (bWielded && item->AsClothing())
	{
		UpdateModel();
	}

	DWORD RemoveObject[3];
	RemoveObject[0] = 0xF747;
	RemoveObject[1] = item->GetID();
	RemoveObject[2] = item->_instance_timestamp;
	g_pWorld->BroadcastPVS(AsWeenie(), RemoveObject, sizeof(RemoveObject));

	g_pWorld->RemoveEntity(item);
}

DWORD CContainerWeenie::Container_InsertInventoryItem(DWORD dwCell, std::shared_ptr<CWeenieObject> item, DWORD slot)
{
	// You should check if the inventory is full before calling this.
	if (!item->RequiresPackSlot())
	{
		if (slot > (DWORD) m_Items.size())
			slot = (DWORD) m_Items.size();

		m_Items.insert(m_Items.begin() + slot, item);
	}
	else
	{
		if (slot > (DWORD) m_Packs.size())
			slot = (DWORD) m_Packs.size();

		m_Packs.insert(m_Packs.begin() + slot, item);
	}

	if (dwCell && m_bWorldIsAware)
	{
		item->_position_timestamp++;

		BinaryWriter Blah;
		Blah.Write<DWORD>(0xF74A);
		Blah.Write<DWORD>(item->GetID());
		Blah.Write<WORD>(item->_instance_timestamp);
		Blah.Write<WORD>(item->_position_timestamp);
		g_pWorld->BroadcastPVS(dwCell, Blah.GetData(), Blah.GetSize());
	}

	item->m_Qualities.SetInt(PARENT_LOCATION_INT, 0);
	item->unset_parent();
	item->leave_world();

	RecalculateEncumbrance();

	return slot;
}

bool CContainerWeenie::SpawnTreasureInContainer(eTreasureCategory category, int tier, int workmanship)
{
	std::shared_ptr<CWeenieObject> treasure = g_pTreasureFactory->GenerateTreasure(tier, category);

	if (!treasure)
		return false;

	if (workmanship > 0)
	{
		workmanship = min(max(workmanship, 1), 10);
		treasure->m_Qualities.SetInt(ITEM_WORKMANSHIP_INT, workmanship);
	}
	else if (workmanship == 0)
		treasure->m_Qualities.RemoveInt(ITEM_WORKMANSHIP_INT);

	return SpawnInContainer(treasure);
}

bool CContainerWeenie::SpawnInContainer(DWORD wcid, int amount, int ptid, float shade, bool sendEnvent)
{
	if (amount < 1)
		return false;

	std::shared_ptr<CWeenieObject> item = g_pWeenieFactory->CreateWeenieByClassID(wcid, NULL, false);

	if (!item)
		return false;

	if (ptid)
		item->m_Qualities.SetInt(PALETTE_TEMPLATE_INT, ptid);

	if (shade > 0.0)
		item->m_Qualities.SetFloat(SHADE_FLOAT, shade);

	int maxStackSize = item->m_Qualities.GetInt(MAX_STACK_SIZE_INT, 1);
	
	if (maxStackSize > 1)
	{
		//If we're stackable, first let's try stacking to existing items.
		for (auto possibleMatch : m_Items)
		{
			std::shared_ptr<CWeenieObject> pPossibleMatch = possibleMatch.lock();

			if (item->m_Qualities.id != pPossibleMatch->m_Qualities.id)
				continue;
	
			if (ptid != 0 && pPossibleMatch->InqIntQuality(PALETTE_TEMPLATE_INT, 0) != ptid)
				continue;
	
			if (shade >= 0.0 && pPossibleMatch->InqFloatQuality(SHADE_FLOAT, 0) != shade)
				continue;
	
			int possibleMatchStackSize = pPossibleMatch->InqIntQuality(STACK_SIZE_INT, 1);
			if (possibleMatchStackSize < maxStackSize)
			{
				//we have room.
				int roomFor = maxStackSize - possibleMatchStackSize;
				if (roomFor >= amount)
				{
					//room for everything!
					pPossibleMatch->SetStackSize(possibleMatchStackSize + amount);
					return true;
				}
				else
				{
					//room for some.
					amount -= roomFor;
					pPossibleMatch->SetStackSize(maxStackSize);
				}
			}
		}
	}

	//We're done stacking and we still have enough for a new item.

	DWORD totalSlotsRequired = 0;
	if (maxStackSize < 1)
		maxStackSize = 1;
	totalSlotsRequired = amount / maxStackSize;

	if (Container_GetNumFreeMainPackSlots() < totalSlotsRequired)
		return false;

	if (amount > 1)
	{
		int maxStackSize = item->m_Qualities.GetInt(MAX_STACK_SIZE_INT, 1);
		if (amount <= maxStackSize)
			item->SetStackSize(amount);
		else
		{
			int amountOfStacks = amount / maxStackSize;
			int restStackSize = amount % maxStackSize;
			for (int i = 0; i < amountOfStacks; i++)
				SpawnInContainer(wcid, maxStackSize, ptid, shade);
			if (restStackSize > 0)
				item->SetStackSize(restStackSize);
			else
			{
				return true;
			}
		}
	}

	SpawnInContainer(item, sendEnvent);
	return true;
}

bool CContainerWeenie::SpawnCloneInContainer(std::shared_ptr<CWeenieObject> itemToClone, int amount, bool sendEnvent)
{
	std::shared_ptr<CWeenieObject> item = g_pWeenieFactory->CloneWeenie(itemToClone);

	if (!item)
		return false;

	DWORD totalSlotsRequired = 0;
	int maxStackSize = item->InqIntQuality(MAX_STACK_SIZE_INT, 1);
	if (maxStackSize < 1)
		maxStackSize = 1;
	totalSlotsRequired = amount / maxStackSize;

	if (Container_GetNumFreeMainPackSlots() < totalSlotsRequired)
		return false;

	if (amount > 1)
	{
		int maxStackSize = item->m_Qualities.GetInt(MAX_STACK_SIZE_INT, 1);
		if (amount <= maxStackSize)
			item->SetStackSize(amount);
		else
		{
			int amountOfStacks = amount / maxStackSize;
			int restStackSize = amount % maxStackSize;
			for (int i = 0; i < amountOfStacks; i++)
				SpawnCloneInContainer(itemToClone, maxStackSize, sendEnvent);
			if (restStackSize > 0)
				item->SetStackSize(restStackSize);
			else
			{
				return true;
			}
		}
	}
	else if(item->InqIntQuality(STACK_SIZE_INT, 1) > 1)
		item->SetStackSize(1);

	if (!SpawnInContainer(item, sendEnvent, false))
	{
		std::shared_ptr<CWeenieObject> owner = this->GetWorldTopLevelOwner();
		if (owner)
		{
			item->SetInitialPosition(owner->m_Position);

			if (!g_pWorld->CreateEntity(item))
			{
				return true;
			}

			item->_timeToRot = Timer::cur_time + 300.0;
			item->_beganRot = false;
			item->m_Qualities.SetFloat(TIME_TO_ROT_FLOAT, item->_timeToRot);
		}
		else
		{
			return true;
		}
	}

	return true;
}

bool CContainerWeenie::SpawnInContainer(std::shared_ptr<CWeenieObject> item, bool sendEnvent, bool deleteItemOnFailure)
{
	item->SetID(g_pWorld->GenerateGUID(eDynamicGUID));
	if (!Container_CanStore(item))
	{
		if(sendEnvent)
			NotifyInventoryFailedEvent(item->GetID(), WERROR_GIVE_NOT_ALLOWED);

		return false;
	}

	if (!g_pWorld->CreateEntity(item))
	{
		if (sendEnvent)
			NotifyInventoryFailedEvent(item->GetID(), WERROR_GIVE_NOT_ALLOWED);

		return false;
	}

	if (sendEnvent)
	{
		SendNetMessage(InventoryMove(item->GetID(), GetID(), 0, item->RequiresPackSlot() ? 1 : 0), PRIVATE_MSG, TRUE);
		if (item->AsContainer())
			item->AsContainer()->MakeAwareViewContent(AsWeenie());
		MakeAware(item, true);

		if (_openedById != 0)
		{
			std::shared_ptr<CWeenieObject> openedBy = g_pWorld->FindObject(_openedById);

			if (openedBy)
			{
				openedBy->SendNetMessage(InventoryMove(item->GetID(), GetID(), 0, item->RequiresPackSlot() ? 1 : 0), PRIVATE_MSG, TRUE);
				if (item->AsContainer())
					item->AsContainer()->MakeAwareViewContent(AsWeenie());
				openedBy->MakeAware(item, true);
			}
		}
	}

	OnReceiveInventoryItem(AsWeenie(), item, 0);
	return true;
}

DWORD CContainerWeenie::OnReceiveInventoryItem(std::shared_ptr<CWeenieObject> source, std::shared_ptr<CWeenieObject> item, DWORD desired_slot)
{
	if (source != AsWeenie())
	{
		// By default, if we receive things just delete them... creatures can override this
		g_pWorld->RemoveEntity(item);
		return 0;
	}
	else
	{
		// Unless the source is ourselves, that means the item is spawning.
		item->ReleaseFromAnyWeenieParent(false, true);
		item->SetWieldedLocation(INVENTORY_LOC::NONE_LOC);

		item->SetWeenieContainer(GetID());
		item->ReleaseFromBlock();

		return Container_InsertInventoryItem(0, item, desired_slot);
	}
}

std::shared_ptr<CWeenieObject> CContainerWeenie::FindContained(DWORD object_id)
{
	return FindContainedItem(object_id);
}

void CContainerWeenie::InitPhysicsObj()
{
	CWeenieObject::InitPhysicsObj();

	if (!_phys_obj.lock())
		return;
	
	for (auto item : m_Wielded)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (!pItem)
		{
			continue;
		}

#ifdef _DEBUG
		assert(item.lock()->GetWielderID() == GetID());
#endif

		int parentLocation = pItem->InqIntQuality(PARENT_LOCATION_INT, PARENT_ENUM::PARENT_NONE);
		if (parentLocation != PARENT_ENUM::PARENT_NONE)
		{
			pItem->set_parent(AsWeenie(), parentLocation);

			int placement = pItem->InqIntQuality(PLACEMENT_POSITION_INT, 0);
			assert(placement);

			pItem->SetPlacementFrame(placement, FALSE);
		}
	}

#ifdef _DEBUG
	for (auto item : m_Items)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();
		if (!pItem)
		{
			continue;
		}

		assert(pItem->InqIntQuality(PARENT_LOCATION_INT, PARENT_ENUM::PARENT_NONE) == PARENT_ENUM::PARENT_NONE);
	}

	for (auto pack : m_Packs)
	{
		std::shared_ptr<CWeenieObject> pPack = pack.lock();

		if (!pPack)
		{
			continue;
		}

		assert(pPack->InqIntQuality(PARENT_LOCATION_INT, PARENT_ENUM::PARENT_NONE) == PARENT_ENUM::PARENT_NONE);
	}
#endif
}

void CContainerWeenie::SaveEx(class CWeenieSave &save)
{
	CWeenieObject::SaveEx(save);

	for (auto item : m_Wielded)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();
		if (!pItem)
		{
			continue;
		}

		save._equipment.push_back(pItem->GetID());
		pItem->Save();
	}

	for (auto item : m_Items)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();
		if (!pItem)
		{
			continue;
		}

		save._inventory.push_back(pItem->GetID());
		pItem->Save();
	}

	for (auto item : m_Packs)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();
		if (!pItem)
		{
			continue;
		}

		save._packs.push_back(pItem->GetID());
		pItem->Save();
	}
}

void CContainerWeenie::LoadEx(class CWeenieSave &save)
{
	CWeenieObject::LoadEx(save);

	for (auto item : save._equipment)
	{
		std::shared_ptr<CWeenieObject> weenie = CWeenieObject::Load(item);

		if (weenie)
		{
			if (weenie->RequiresPackSlot())
			{
				continue;
			}

			assert(weenie->IsWielded());
			assert(!weenie->IsContained());

			// make sure it has the right settings (shouldn't be necessary)
			weenie->SetWielderID(GetID());
			weenie->SetWeenieContainer(0);
			weenie->m_Qualities.RemovePosition(INSTANTIATION_POSITION);
			weenie->m_Qualities.RemovePosition(LOCATION_POSITION);

			if (g_pWorld->CreateEntity(weenie, false))
			{
				m_Wielded.push_back(weenie);

				if (int combatUse = weenie->InqIntQuality(COMBAT_USE_INT, 0, TRUE))
					SetWieldedCombat(weenie, (COMBAT_USE)combatUse);

				assert(weenie->IsWielded());
				assert(!weenie->IsContained());
			}
			else
			{
				// remove any enchantments associated with this item that we failed to wield...
				if (m_Qualities._enchantment_reg)
				{
					PackableListWithJson<DWORD> spells_to_remove;

					if (m_Qualities._enchantment_reg->_add_list)
					{
						for (const auto &entry : *m_Qualities._enchantment_reg->_add_list)
						{
							if (entry._caster == item)
							{
								spells_to_remove.push_back(entry._id);
							}
						}
					}

					if (m_Qualities._enchantment_reg->_mult_list)
					{
						for (const auto &entry : *m_Qualities._enchantment_reg->_mult_list)
						{
							if (entry._caster == item)
							{
								spells_to_remove.push_back(entry._id);
							}
						}
					}

					if (m_Qualities._enchantment_reg->_cooldown_list)
					{
						for (const auto &entry : *m_Qualities._enchantment_reg->_cooldown_list)
						{
							if (entry._caster == item)
							{
								spells_to_remove.push_back(entry._id);
							}
						}
					}

					if (!spells_to_remove.empty())
					{
						m_Qualities._enchantment_reg->RemoveEnchantments(&spells_to_remove);
					}
				}
			}
		}
	}

	for (auto item : save._inventory)
	{
		std::shared_ptr<CWeenieObject> weenie = CWeenieObject::Load(item);

		if (weenie)
		{
			DWORD correct_container_iid = weenie->m_Qualities.GetIID(CONTAINER_IID, 0);

			if (weenie->RequiresPackSlot() || (correct_container_iid && correct_container_iid != GetID()))
			{
				continue;
			}

			assert(!weenie->IsWielded());
			assert(weenie->IsContained());
			assert(!weenie->InqIntQuality(PARENT_LOCATION_INT, 0));

			// make sure it has the right settings (shouldn't be necessary)
			weenie->SetWielderID(0);
			weenie->m_Qualities.SetInt(PARENT_LOCATION_INT, 0);
			weenie->SetWeenieContainer(GetID());
			weenie->m_Qualities.RemovePosition(INSTANTIATION_POSITION);
			weenie->m_Qualities.RemovePosition(LOCATION_POSITION);

			if (g_pWorld->CreateEntity(weenie, false))
			{
				m_Items.push_back(weenie);

				assert(!weenie->IsWielded());
				assert(weenie->IsContained());
			}
		}
	}

	for (auto item : save._packs)
	{
		std::shared_ptr<CWeenieObject> weenie = CWeenieObject::Load(item);

		if (weenie)
		{
			DWORD correct_container_iid = weenie->m_Qualities.GetIID(CONTAINER_IID, 0);

			if (!weenie->RequiresPackSlot() || (correct_container_iid && correct_container_iid != GetID()))
			{
				continue;
			}

			assert(!weenie->IsWielded());
			assert(weenie->IsContained());
			assert(!weenie->InqIntQuality(PARENT_LOCATION_INT, 0));

			// make sure it has the right settings (shouldn't be necessary)
			weenie->SetWielderID(0);
			weenie->m_Qualities.SetInt(PARENT_LOCATION_INT, 0);
			weenie->SetWeenieContainer(GetID());
			weenie->m_Qualities.RemovePosition(INSTANTIATION_POSITION);
			weenie->m_Qualities.RemovePosition(LOCATION_POSITION);

			if (g_pWorld->CreateEntity(weenie, false))
			{
				m_Packs.push_back(weenie);

				assert(!weenie->IsWielded());
				assert(weenie->IsContained());
			}
		}
	}
}

void CContainerWeenie::MakeAwareViewContent(std::shared_ptr<CWeenieObject> other)
{
	//start from the bottom of the tree so the packs have their fill bar correctly populated.
	for (auto pack : m_Packs)
	{
		std::shared_ptr<CWeenieObject> pPack = pack.lock();
		if (!pPack)
		{
			continue;
		}

		if (pPack->AsContainer())
			pPack->AsContainer()->MakeAwareViewContent(other);
		other->MakeAware(pPack, true);
	}

	for (auto item : m_Items)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();
		if (!pItem)
		{
			continue;
		}

		other->MakeAware(pItem, true);
	}

	PackableList<ContentProfile> inventoryList;
	for (auto item : m_Items)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();
		if (!pItem)
		{
			continue;
		}

		ContentProfile prof;
		prof.m_iid = pItem->GetID();
		prof.m_uContainerProperties = 0;
		inventoryList.push_back(prof);
	}
	for (auto pack : m_Packs)
	{
		std::shared_ptr<CWeenieObject> pPack = pack.lock();
		if (!pPack)
		{
			continue;
		}

		ContentProfile prof;
		prof.m_iid = pPack->GetID();
		prof.m_uContainerProperties = 1; //todo: what about foci? Do they need a different value here?
		inventoryList.push_back(prof);
	}

	BinaryWriter viewContent;
	viewContent.Write<DWORD>(0x196);
	viewContent.Write<DWORD>(GetID());
	inventoryList.Pack(&viewContent);
	other->SendNetMessage(&viewContent, PRIVATE_MSG, TRUE, FALSE);
}

bool CContainerWeenie::IsGroundContainer()
{
	if (HasOwner())
		return false;

	if (!InValidCell())
		return false;

	return true;
}

bool CContainerWeenie::IsInOpenRange(std::shared_ptr<CWeenieObject> other)
{
	if (!IsGroundContainer())
		return false;

	if (DistanceTo(other, true) >= InqFloatQuality(USE_RADIUS_FLOAT, 1.0))
		return false;

	return true;
}

void CContainerWeenie::OnContainerOpened(std::shared_ptr<CWeenieObject> other)
{
	if (other && other->_lastOpenedRemoteContainerId && other->_lastOpenedRemoteContainerId != GetID())
	{
		if (std::shared_ptr<CWeenieObject> otherContainerObj = g_pWorld->FindObject(other->_lastOpenedRemoteContainerId))
		{
			if (std::shared_ptr<CContainerWeenie> otherContainer = otherContainerObj->AsContainer())
			{
				if (otherContainer->_openedById == other->GetID())
					otherContainer->OnContainerClosed();
			}
		}
	}

	MakeAwareViewContent(other);
	_openedById = other->GetID();
	other->_lastOpenedRemoteContainerId = GetID();
	_failedPreviousCheckToClose = false;
}

void CContainerWeenie::OnContainerClosed(std::shared_ptr<CWeenieObject> requestedBy)
{
	if (requestedBy)
	{
		BinaryWriter closeContent;
		closeContent.Write<DWORD>(0x52);
		closeContent.Write<DWORD>(GetID());
		requestedBy->SendNetMessage(&closeContent, PRIVATE_MSG, TRUE, FALSE);
	}

	_openedById = 0;

	if (InqStringQuality(QUEST_STRING, "") != "")
		ResetToInitialState(); //quest chests reset instantly
	else if (_nextReset < 0)
	{
		if (double resetInterval = InqFloatQuality(RESET_INTERVAL_FLOAT, 0))
			_nextReset = Timer::cur_time + (resetInterval * g_pConfig->RespawnTimeMultiplier());
		else if (double regenInterval = InqFloatQuality(REGENERATION_INTERVAL_FLOAT, 0)) //if we don't have a reset interval, fall back to regen interval
			_nextReset = Timer::cur_time + (regenInterval * g_pConfig->RespawnTimeMultiplier());
	}
}

void CContainerWeenie::NotifyGeneratedPickedUp(std::shared_ptr<CWeenieObject> weenie)
{
	//container contents do not regenerate individually. Instead once the container is closed we start a reset timer.
	weenie->m_Qualities.RemoveInstanceID(GENERATOR_IID);
}

void CContainerWeenie::ResetToInitialState()
{
	if (_openedById)
		OnContainerClosed();

	m_Qualities.RemoveInstanceID(OWNER_IID);

	SetLocked(m_bInitiallyLocked ? TRUE : FALSE);

	while (!m_Wielded.empty())
	{
		std::shared_ptr<CWeenieObject> pItem = m_Wielded.begin()->lock();

		if (!pItem)
		{
			m_Wielded.erase(m_Wielded.begin());
			continue;
		}

		Container_DeleteItem(pItem->GetID());
	}
	while (!m_Items.empty())
	{
		std::shared_ptr<CWeenieObject> pItem = m_Items.begin()->lock();

		if (!pItem)
		{
			m_Items.erase(m_Items.begin());
			continue;
		}

		Container_DeleteItem(pItem->GetID());
	}
	while (!m_Packs.empty())
	{
		std::shared_ptr<CWeenieObject> pItem = m_Packs.begin()->lock();

		if (!pItem)
		{
			m_Packs.erase(m_Packs.begin());
			continue;
		}

		Container_DeleteItem(pItem->GetID());
	}


	if (m_Qualities._generator_table)
	{
		if (m_Qualities._generator_registry || m_Qualities._generator_queue)
		{
			for (auto &entry : m_Qualities._generator_table->_profile_list)
			{
				if (entry.whereCreate & Contain_RegenLocationType)
				{
					if (m_Qualities._generator_registry)
					{
						for (PackableHashTable<unsigned long, GeneratorRegistryNode>::iterator i = m_Qualities._generator_registry->_registry.begin(); i != m_Qualities._generator_registry->_registry.end();)
						{
							if (entry.slot == i->second.slot)
								i = m_Qualities._generator_registry->_registry.erase(i);
							else
								i++;
						}
					}

					if (m_Qualities._generator_queue)
					{
						for (auto i = m_Qualities._generator_queue->_queue.begin(); i != m_Qualities._generator_queue->_queue.end();)
						{
							if (entry.slot == i->slot)
								i = m_Qualities._generator_queue->_queue.erase(i);
							else
								i++;
						}
					}
				}
			}
		}
		else
		{
			if(m_Qualities._generator_registry)
				m_Qualities._generator_registry->_registry.clear();
			if (m_Qualities._generator_queue)
				m_Qualities._generator_queue->_queue.clear();
		}
	}

	InitCreateGenerator();
}

int CContainerWeenie::DoUseResponse(std::shared_ptr<CWeenieObject> other)
{
	if (IsBusyOrInAction())
		return WERROR_NONE;

	if (!(GetItemType() & ITEM_TYPE::TYPE_CONTAINER))
		return WERROR_NONE;

	if (!IsGroundContainer())
		return WERROR_NONE;

	if (!IsInOpenRange(other))
		return WERROR_NONE;

	if (IsLocked())
	{
		EmitSound(Sound_OpenFailDueToLock, 1.0f);
		return WERROR_NONE;
	}

	int openError = CheckOpenContainer(other);

	if (openError)
		return WERROR_NONE;

	if (_openedById)
	{
		if (_openedById == other->GetID())
		{
			//this is actually a close container request.
			OnContainerClosed(other);
			return WERROR_NONE;
		}
		else
			return WERROR_CHEST_ALREADY_OPEN;
	}

	std::string questString;
	if (m_Qualities.InqString(QUEST_STRING, questString) && !questString.empty())
	{
		if (DWORD owner = InqIIDQuality(OWNER_IID, 0))
		{
			if (owner != other->GetID())
			{
				other->SendText(csprintf("This chest is claimed by the person who lockpicked it."), LTT_DEFAULT);
				return WERROR_NONE;
			}
		}
		else
		{
			if (other->InqQuest(questString.c_str()))
			{
				int timeTilOkay = other->InqTimeUntilOkayToComplete(questString.c_str());

				if (timeTilOkay > 0)
				{
					int secs = timeTilOkay % 60;
					timeTilOkay /= 60;

					int mins = timeTilOkay % 60;
					timeTilOkay /= 60;

					int hours = timeTilOkay % 24;
					timeTilOkay /= 24;

					int days = timeTilOkay;

					other->SendText(csprintf("You cannot open this for another %dd %dh %dm %ds.", days, hours, mins, secs), LTT_DEFAULT);
				}

				return WERROR_CHEST_USED_TOO_RECENTLY;
			}

			other->StampQuest(questString.c_str());
		}
	}

	OnContainerOpened(other);

	return WERROR_NONE;
}

void CContainerWeenie::InventoryTick()
{
	CWeenieObject::InventoryTick();

	auto wielded = m_Wielded.begin();
	while (wielded != m_Wielded.end())
	{
		std::shared_ptr<CWeenieObject> pItem = wielded->lock();

		if (!pItem)
		{
			m_Wielded.erase(wielded);
		}
		else
		{
			pItem->WieldedTick();

#ifdef _DEBUG
			pItem->DebugValidate();
#endif
		}

		wielded++;
	}

	auto item = m_Items.begin();
	while (item != m_Items.end())
	{
		std::shared_ptr<CWeenieObject> pItem = item->lock();

		if (!pItem)
		{
			m_Items.erase(wielded);
		}
		else
		{
			pItem->InventoryTick();

#ifdef _DEBUG
			pItem->DebugValidate();
#endif
		}
		item++;
	}

	auto pack = m_Packs.begin();
	while (pack != m_Packs.end())
	{
		std::shared_ptr<CWeenieObject> pItem = pack->lock();

		if (!pItem)
		{
			m_Packs.erase(wielded);
		}
		else
		{
			pItem->InventoryTick();

#ifdef _DEBUG
			pItem->DebugValidate();
#endif
		}

		pack++;
	}
}

void CContainerWeenie::Tick()
{
	CWeenieObject::Tick();

	if (_openedById)
	{
		CheckToClose();
	}

	if (Timer::cur_time < _nextInventoryTick)
	{
		return;
	}

	// TODO mwnciau removed a lot of code here and replaced it with this. If problems occur...
	InventoryTick();

	_nextInventoryTick = Timer::cur_time + Random::GenFloat(0.4, 0.6);
}

void CContainerWeenie::CheckToClose()
{
	if (_nextCheckToClose > Timer::cur_time)
	{
		return;
	}

	_nextCheckToClose = Timer::cur_time + 1.0;
	
	if (std::shared_ptr<CWeenieObject> other = g_pWorld->FindObject(_openedById))
	{
		if (other->IsDead() || !IsInOpenRange(other))
		{
			if (_failedPreviousCheckToClose)
				OnContainerClosed();
			else
			{
				_failedPreviousCheckToClose = true; //give the client a moment to send the chest close message or we might interpret it as a open request(both use the use packet)
				_nextCheckToClose = Timer::cur_time + 2.0;
			}
		}
		else
			_failedPreviousCheckToClose = false;
	}
	else
	{
		OnContainerClosed();
	}
}

int CContainerWeenie::CheckOpenContainer(std::shared_ptr<CWeenieObject> other)
{
	if (_openedById)
	{
		if (_openedById == other->GetID())
		{
			return WERROR_NONE;
		}
		
		return WERROR_CHEST_ALREADY_OPEN;
	}

	return WERROR_NONE;
}

void CContainerWeenie::HandleNoLongerViewing(std::shared_ptr<CWeenieObject> other)
{
	if (!_openedById || _openedById != other->GetID())
		return;

	OnContainerClosed();
}

void CContainerWeenie::DebugValidate()
{
	CWeenieObject::DebugValidate();

#ifdef _DEBUG
	assert(!GetWielderID());
	
	for (auto wielded : m_Wielded)
	{
		assert(wielded.lock()->GetWielderID() == GetID());
		wielded.lock()->DebugValidate();
	}

	for (auto item : m_Items)
	{
		assert(item.lock()->GetContainerID() == GetID());
		item.lock()->DebugValidate();
	}

	for (auto pack : m_Packs)
	{
		assert(pack.lock()->GetContainerID() == GetID());
		pack.lock()->DebugValidate();
	}
#endif
}

DWORD CContainerWeenie::RecalculateCoinAmount()
{
	int coinAmount = 0;
	for (auto item : m_Items)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (!pItem)
		{
			continue;
		}

		if (pItem->m_Qualities.id == W_COINSTACK_CLASS)
			coinAmount += pItem->InqIntQuality(STACK_SIZE_INT, 1, true);
	}

	for (auto pack : m_Packs)
	{
		std::shared_ptr<CWeenieObject> pItem = pack.lock();

		if (!pItem)
		{
			continue;
		}
		coinAmount += pItem->RecalculateCoinAmount();
	}

	m_Qualities.SetInt(COIN_VALUE_INT, coinAmount);
	NotifyIntStatUpdated(COIN_VALUE_INT);

	return coinAmount;
}

DWORD CContainerWeenie::RecalculateAltCoinAmount(int currencyid)
{
	int coinAmount = 0;
	for (auto item : m_Items)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (!pItem)
		{
			continue;
		}

		if (pItem->m_Qualities.id == currencyid)
			coinAmount += pItem->InqIntQuality(STACK_SIZE_INT, 1, true);
	}

	for (auto pack : m_Packs)
	{
		std::shared_ptr<CWeenieObject> pItem = pack.lock();

		if (!pItem)
		{
			continue;
		}

		coinAmount += pItem->RecalculateAltCoinAmount(currencyid);
	}

	return coinAmount;
}

DWORD CContainerWeenie::ConsumeCoin(int amountToConsume)
{
	if (amountToConsume < 1)
		return 0;

	if (AsPlayer()) //we don't need to recalculate this if we're a subcontainer
	{
		if (RecalculateCoinAmount() < amountToConsume) //force recalculate our coin amount and check so we don't even try to consume if we don't have enough.
			return 0;
	}

	std::list<std::shared_ptr<CWeenieObject> > removeList;

	DWORD amountConsumed = 0;
	for (auto item : m_Items)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (!pItem)
		{
			continue;
		}

		if (pItem->m_Qualities.id == W_COINSTACK_CLASS)
		{
			int stackSize = pItem->InqIntQuality(STACK_SIZE_INT, 1, true);
			if (stackSize <= amountToConsume)
			{
				removeList.push_back(pItem);
				amountToConsume -= stackSize;
				amountConsumed += stackSize;
			}
			else
			{
				pItem->SetStackSize(stackSize - amountToConsume);
				amountConsumed += amountToConsume;
				break;
			}
		}
	}

	for (auto item : removeList)
		item->Remove();

	if (amountToConsume > 0)
	{
		for (auto pack : m_Packs)
		{
			std::shared_ptr<CWeenieObject> pItem = pack.lock();

			if (!pItem)
			{
				continue;
			}

			DWORD amountFromPack = pItem->ConsumeCoin(amountToConsume);
			amountToConsume -= amountFromPack;
			amountConsumed += amountFromPack;

			if (amountToConsume <= 0)
				break;
		}
	}

	if(AsPlayer())
		RecalculateCoinAmount();
	return amountConsumed;
}

DWORD CContainerWeenie::ConsumeAltCoin(int amountToConsume, int currencyid)
{
	if (amountToConsume < 1)
		return 0;

	if (AsPlayer()) //we don't need to recalculate this if we're a subcontainer
	{
		if (RecalculateAltCoinAmount(currencyid) < amountToConsume) //force recalculate our coin amount and check so we don't even try to consume if we don't have enough.
			return 0;
	}

	std::list<std::shared_ptr<CWeenieObject> > removeList;

	DWORD amountConsumed = 0;
	for (auto item : m_Items)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (!pItem)
		{
			continue;
		}

		if (pItem->m_Qualities.id == currencyid)
		{
			int stackSize = pItem->InqIntQuality(STACK_SIZE_INT, 1, true);
			if (stackSize <= amountToConsume)
			{
				removeList.push_back(pItem);
				amountToConsume -= stackSize;
				amountConsumed += stackSize;
			}
			else
			{
				pItem->SetStackSize(stackSize - amountToConsume);
				amountConsumed += amountToConsume;
				break;
			}
		}
	}

	for (auto item : removeList)
		item->Remove();

	if (amountToConsume > 0)
	{
		for (auto pack : m_Packs)
		{
			std::shared_ptr<CWeenieObject> pItem = pack.lock();

			if (!pItem)
			{
				continue;
			}

			DWORD amountFromPack = pItem->ConsumeAltCoin(amountToConsume, currencyid);
			amountToConsume -= amountFromPack;
			amountConsumed += amountFromPack;

			if (amountToConsume <= 0)
				break;
		}
	}

	if (AsPlayer())
		RecalculateAltCoinAmount(currencyid);
	return amountConsumed;
}

void CContainerWeenie::RecalculateEncumbrance()
{
	int oldValue = InqIntQuality(ENCUMB_VAL_INT, 0);

	int newValue = 0;
	for (auto wielded : m_Wielded)
	{
		std::shared_ptr<CWeenieObject> pItem = wielded.lock();

		if (!pItem)
		{
			continue;
		}

		newValue += pItem->InqIntQuality(ENCUMB_VAL_INT, 0);
	}

	for (auto item : m_Items)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (!pItem)
		{
			continue;
		}

		newValue += pItem->InqIntQuality(ENCUMB_VAL_INT, 0);
	}

	for (auto pack : m_Packs)
	{
		std::shared_ptr<CWeenieObject> pItem = pack.lock();

		if (!pItem)
		{
			continue;
		}

		pItem->RecalculateEncumbrance();
		newValue += pItem->InqIntQuality(ENCUMB_VAL_INT, 0);
	}

	if (oldValue != newValue)
	{
		m_Qualities.SetInt(ENCUMB_VAL_INT, newValue);
		NotifyIntStatUpdated(ENCUMB_VAL_INT, true);
	}
}

bool CContainerWeenie::IsAttunedOrContainsAttuned()
{
	for (auto wielded : m_Wielded)
	{
		std::shared_ptr<CWeenieObject> pItem = wielded.lock();

		if (!pItem)
		{
			continue;
		}

		if (pItem->IsAttunedOrContainsAttuned())
			return true;
	}

	for (auto item : m_Items)
	{
		std::shared_ptr<CWeenieObject> pItem = item.lock();

		if (!pItem)
		{
			continue;
		}

		if (pItem->IsAttunedOrContainsAttuned())
			return true;
	}

	for (auto pack : m_Packs)
	{
		std::shared_ptr<CWeenieObject> pItem = pack.lock();

		if (!pItem)
		{
			continue;
		}

		if (pItem->IsAttunedOrContainsAttuned())
			return true;
	}

	return CWeenieObject::IsAttunedOrContainsAttuned();
}

bool CContainerWeenie::HasContainerContents()
{
	if (!m_Wielded.empty() || !m_Items.empty() || !m_Packs.empty())
		return true;

	return CWeenieObject::HasContainerContents();
}

void CContainerWeenie::AdjustToNewCombatMode()
{
	std::list<std::shared_ptr<CWeenieObject> > wielded;
	Container_GetWieldedByMask(wielded, WEAPON_LOC | HELD_LOC);

	COMBAT_MODE newCombatMode = COMBAT_MODE::UNDEF_COMBAT_MODE;

	for (auto item : wielded)
	{
		newCombatMode = item->GetEquippedCombatMode();
		if (newCombatMode != COMBAT_MODE::UNDEF_COMBAT_MODE)
			break;
	}

	if (newCombatMode == COMBAT_MODE::UNDEF_COMBAT_MODE)
		newCombatMode = COMBAT_MODE::MELEE_COMBAT_MODE;

	ChangeCombatMode(newCombatMode, false);
}