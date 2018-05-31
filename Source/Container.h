
#pragma once

#include "WeenieObject.h"

#define MAX_WIELDED_COMBAT 5

class CContainerWeenie : public CWeenieObject
{
public:
	CContainerWeenie();
	virtual ~CContainerWeenie() override;

	virtual class std::shared_ptr<CContainerWeenie> AsContainer() { return GetPointer<CContainerWeenie>(); }

	virtual bool IsAttunedOrContainsAttuned() override;

	virtual void ApplyQualityOverrides() override;
	virtual void ResetToInitialState() override;
	virtual void PostSpawn() override;

	virtual void InventoryTick() override;
	virtual void Tick() override;
	virtual void DebugValidate() override;

	virtual bool RequiresPackSlot() override { return true; }

	virtual void SaveEx(class CWeenieSave &save) override;
	virtual void LoadEx(class CWeenieSave &save) override;

	virtual void RecalculateEncumbrance() override;

	virtual DWORD RecalculateCoinAmount() override;
	virtual DWORD RecalculateAltCoinAmount(int currencyid) override;
	virtual DWORD ConsumeCoin(int amountToConsume) override;
	virtual DWORD ConsumeAltCoin(int amountToConsume, int currencyid) override;

	bool IsGroundContainer();
	bool IsInOpenRange(std::shared_ptr<CWeenieObject> other);

	virtual int DoUseResponse(std::shared_ptr<CWeenieObject> other);

	virtual void OnContainerOpened(std::shared_ptr<CWeenieObject> other);
	virtual void OnContainerClosed(std::shared_ptr<CWeenieObject> requestedBy = NULL);
	virtual DWORD OnReceiveInventoryItem(std::shared_ptr<CWeenieObject> source, std::shared_ptr<CWeenieObject> item, DWORD desired_slot);

	virtual void NotifyGeneratedPickedUp(std::shared_ptr<CWeenieObject> weenie) override;

	int GetItemsCapacity();
	int GetContainersCapacity();

	void MakeAwareViewContent(std::shared_ptr<CWeenieObject> other);

	void UnloadContainer();

	std::shared_ptr<CContainerWeenie> FindContainer(DWORD container_id);
	virtual std::shared_ptr<CWeenieObject> FindContainedItem(DWORD object_id) override;

	virtual std::shared_ptr<CWeenieObject> GetWieldedCombat(COMBAT_USE combatUse) override;
	void SetWieldedCombat(std::shared_ptr<CWeenieObject> wielded, COMBAT_USE combatUse);

	std::shared_ptr<CWeenieObject> GetWieldedMelee();
	std::shared_ptr<CWeenieObject> GetWieldedMissile();
	std::shared_ptr<CWeenieObject> GetWieldedAmmo();
	std::shared_ptr<CWeenieObject> GetWieldedShield();
	std::shared_ptr<CWeenieObject> GetWieldedTwoHanded();
	virtual std::shared_ptr<CWeenieObject> GetWieldedCaster() override;

	void Container_GetWieldedByMask(std::list<std::shared_ptr<CWeenieObject> > &wielded, DWORD inv_loc_mask);
	std::shared_ptr<CWeenieObject> GetWielded(INVENTORY_LOC slot) override;

	BOOL Container_CanEquip(std::shared_ptr<CWeenieObject> pItem, DWORD dwCoverage);
	BOOL Container_CanStore(std::shared_ptr<CWeenieObject> pItem, bool bPackSlot);
	BOOL Container_CanStore(std::shared_ptr<CWeenieObject> pItem);

	BOOL IsItemsCapacityFull();
	BOOL IsContainersCapacityFull();

	void Container_EquipItem(DWORD dwCell, std::shared_ptr<CWeenieObject> pItem, DWORD dwCoverage, DWORD child_location, DWORD placement);
	void Container_DeleteItem(DWORD object_id);
	virtual DWORD Container_InsertInventoryItem(DWORD dwCell, std::shared_ptr<CWeenieObject> pItem, DWORD slot);

	DWORD Container_GetNumFreeMainPackSlots();

	virtual void ReleaseContainedItemRecursive(std::shared_ptr<CWeenieObject> item) override;

	bool SpawnTreasureInContainer(eTreasureCategory category, int tier, int workmanship = -1);
	bool SpawnInContainer(DWORD wcid, int amount = 1, int ptid = 0, float shade = 0, bool sendEnvent = true);
	bool SpawnCloneInContainer(std::shared_ptr<CWeenieObject> itemToClone, int amount, bool sendEnvent = true);
	bool SpawnInContainer(std::shared_ptr<CWeenieObject> item, bool sendEnvent = true, bool deleteItemOnFailure = true);

	std::shared_ptr<CWeenieObject> FindContained(DWORD object_id);

	virtual void InitPhysicsObj() override;

	void CheckToClose();

	virtual int CheckOpenContainer(std::shared_ptr<CWeenieObject> other);

	void HandleNoLongerViewing(std::shared_ptr<CWeenieObject> other);

	virtual bool HasContainerContents() override;

	void AdjustToNewCombatMode();

	std::shared_ptr<CWeenieObject> m_WieldedCombat[MAX_WIELDED_COMBAT];
	std::vector<std::shared_ptr<CWeenieObject> > m_Wielded;
	std::vector<std::shared_ptr<CWeenieObject> > m_Items;
	std::vector<std::shared_ptr<CWeenieObject> > m_Packs;

	// For opening/closing containers
	double _nextCheckToClose = 0.0;
	bool _failedPreviousCheckToClose = false;
	DWORD _openedById = 0;

	bool m_bInitiallyLocked = false;

	double _nextInventoryTick = 0.0;
};

