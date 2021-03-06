
#pragma once

#include "Monster.h"
#include "VendorProfile.h"

class CVendorItem
{
public:
	CVendorItem();
	~CVendorItem();

	CWeenieObject *weenie = NULL;
	int amount = -1;
};

class CVendor : public CMonsterWeenie
{
public:
	CVendor();
	virtual ~CVendor() override;
	virtual void Tick() override;

	virtual class CVendor *AsVendor() { return this; }

	virtual int DoUseResponse(CWeenieObject *player) override;

	void DoVendorEmote(int type, DWORD target_id);

	virtual void PreSpawnCreate() override;

	void SendVendorInventory(CWeenieObject *other);

	void ResetItems();
	void GenerateItems();
	void GenerateAllItems();
	void ValidateItems();
	void AddVendorItem(DWORD wcid, int ptid, float shade, int amount);
	void AddVendorItem(DWORD wcid, int amount);
	void AddVendorItemByAllMatchingNames(const char *name);
	CVendorItem *FindVendorItem(DWORD item_id);
	int TrySellItemsToPlayer(CPlayerWeenie *buyer, const std::list<class ItemProfile *> &desiredItems);
	int TryBuyItemsFromPlayer(CPlayerWeenie *seller, const std::list<ItemProfile *> &desiredItems);
	const std::list<ItemProfile *> GetFilteredItems(std::list<ItemProfile *> desiredItems, CPlayerWeenie *seller);

	VendorProfile profile;
	std::list<CVendorItem *> m_Items;

protected:
	void CheckRange();
	std::set<DWORD> m_ActiveBuyers;
	double m_VendorCycleTime = 0.0;
};

class CAvatarVendor : public CVendor
{
public:
	CAvatarVendor() { }

	virtual void PreSpawnCreate() override;
};


