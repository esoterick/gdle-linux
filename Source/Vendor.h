
#pragma once

#include "Monster.h"
#include "VendorProfile.h"

class CVendorItem
{
public:
	CVendorItem();
	~CVendorItem();

	std::shared_ptr<CWeenieObject> weenie = NULL;
	int amount = -1;
};

class CVendor : public CMonsterWeenie
{
public:
	CVendor();
	virtual ~CVendor() override;

	virtual class std::shared_ptr<CVendor> AsVendor() { return std::dynamic_pointer_cast<CVendor>(m_spThis.lock()); }

	virtual int DoUseResponse(std::shared_ptr<CWeenieObject> player) override;

	void DoVendorEmote(int type, DWORD target_id);

	virtual void PreSpawnCreate() override;

	void SendVendorInventory(std::shared_ptr<CWeenieObject> other);

	void ResetItems();
	void GenerateItems();
	void GenerateAllItems();
	void ValidateItems();
	void AddVendorItem(DWORD wcid, int ptid, float shade, int amount);
	void AddVendorItem(DWORD wcid, int amount);
	void AddVendorItemByAllMatchingNames(const char *name);
	std::shared_ptr<CVendorItem> FindVendorItem(DWORD item_id);
	int TrySellItemsToPlayer(std::shared_ptr<CPlayerWeenie> buyer, const std::list<class ItemProfile *> &desiredItems);
	int TryBuyItemsFromPlayer(std::shared_ptr<CPlayerWeenie> seller, const std::list<ItemProfile *> &desiredItems);

	VendorProfile profile;
	std::list<std::shared_ptr<CVendorItem>> m_Items;
};

class CAvatarVendor : public CVendor
{
public:
	CAvatarVendor() { }

	virtual void PreSpawnCreate() override;
};


