
#pragma once

#include "WeenieObject.h"
#include "Portal.h"
#include "Chest.h"

class CHouseData: PackObj
{
public:
	CHouseData() { };
	~CHouseData() { };
	DECLARE_PACKABLE()

	void ClearOwnershipData();
	void SetHookVisibility(bool newSetting);
	void AbandonHouse();
	void Save();

	DWORD _slumLordId = 0;
	Position _position;
	DWORD _houseType = 0;
	DWORD _ownerId = 0;
	DWORD _ownerAccount = 0;
	DWORD _purchaseTimestamp = 0;
	DWORD _currentMaintenancePeriod = 0;
	HousePaymentList _buy;
	HousePaymentList _rent;
	PackableList<DWORD> _accessList;
	PackableList<DWORD> _storageAccessList;
	bool _allegianceAccess = false;
	bool _allegianceStorageAccess = false;
	bool _everyoneAccess = false;
	bool _everyoneStorageAccess = false;
	bool _hooksVisible = true;

	//dynamic fields
	DWORD _houseId = 0;
	std::set<DWORD> _hookList;
};

class CHouseManager
{
public:
	CHouseManager();
	~CHouseManager();

	void Load();
	void Save();

	DWORD _currentHouseMaintenancePeriod = 0;
	DWORD _nextHouseMaintenancePeriod = 0;
	bool _freeHouseMaintenancePeriod = false;

	CHouseData *GetHouseData(DWORD houseId);
	void SaveHouseData(DWORD houseId);
	void SendHouseData(std::shared_ptr<CPlayerWeenie> player, DWORD houseId);

private:
	PackableHashTable<DWORD, CHouseData> _houseDataMap;
};

class CHouseWeenie : public CWeenieObject
{
public:
	CHouseWeenie();

	virtual class std::shared_ptr<CHouseWeenie> AsHouse() { return std::static_pointer_cast<CHouseWeenie>(GetPointer()); }

	virtual void EnsureLink(std::shared_ptr<CWeenieObject> source) override;

	virtual bool ShouldSave() override { return true; }
	
	virtual bool HasAccess(std::shared_ptr<CPlayerWeenie> requester);
	virtual bool HasStorageAccess(std::shared_ptr<CPlayerWeenie> requester);

	CHouseData *GetHouseData();
	std::string GetHouseOwnerName();
	DWORD GetHouseOwner();
	DWORD GetHouseDID();
	int GetHouseType();
	std::shared_ptr<CSlumLordWeenie> GetSlumLord();

	//std::set<DWORD> _hookList;
	//DWORD _currentMaintenancePeriod;
	//HousePaymentList _rent;
	//PackableList<DWORD> _accessList;
	//PackableList<DWORD> _storageAccessList;
	//bool _allegianceAccess = false;
	//bool _allegianceStorageAccess = false;
	//bool _everyoneAccess = false;
	//bool _everyoneStorageAccess = false;
};

class CSlumLordWeenie : public CWeenieObject
{
public:
	CSlumLordWeenie();

	virtual void Tick() override;

	virtual class std::shared_ptr<CSlumLordWeenie> AsSlumLord() { return std::static_pointer_cast<CSlumLordWeenie>(GetPointer()); }

	std::shared_ptr<CHouseWeenie> GetHouse();
	void GetHouseProfile(HouseProfile &prof);

	virtual int DoUseResponse(std::shared_ptr<CWeenieObject> other) override;

	void BuyHouse(std::shared_ptr<CPlayerWeenie> player, const PackableList<DWORD> &items);
	void RentHouse(std::shared_ptr<CPlayerWeenie> player, const PackableList<DWORD> &items);
	void CheckRentPeriod();

	bool _initialized = false;
	double _nextSave = -1.0;
};

class CHookWeenie : public CContainerWeenie
{
public:
	CHookWeenie();

	virtual class std::shared_ptr<CHookWeenie> AsHook() { return std::static_pointer_cast<CHookWeenie>(GetPointer()); }

	virtual void Tick() override;

	virtual bool ShouldSave() override { return true; }
	virtual void SaveEx(class CWeenieSave &save) override;
	virtual void LoadEx(class CWeenieSave &save) override;

	int DoUseResponse(std::shared_ptr<CWeenieObject> other) override;
	void Identify(std::shared_ptr<CWeenieObject> other, DWORD overrideId = 0) override;

	virtual DWORD Container_InsertInventoryItem(DWORD dwCell, std::shared_ptr<CWeenieObject> pItem, DWORD slot) override;
	virtual void ReleaseContainedItemRecursive(std::shared_ptr<CWeenieObject> item) override;
	void UpdateHookedObject(std::shared_ptr<CWeenieObject> hookedItem = NULL, bool sendUpdate = true);
	void ClearHookedObject(bool sendUpdate = true);
	void SetHookVisibility(bool newSetting);

	std::shared_ptr<CHouseWeenie> GetHouse();
	CHouseData *GetHouseData();

	bool _initialized = false;
	double _nextInitCheck = -1.0;
};

class CDeedWeenie : public CWeenieObject
{
public:
	CDeedWeenie();

	virtual class std::shared_ptr<CDeedWeenie> AsDeed() { return std::static_pointer_cast<CDeedWeenie>(GetPointer()); }

	class std::shared_ptr<CHouseWeenie> GetHouse();
};

class CBootSpotWeenie : public CWeenieObject
{
public:
	CBootSpotWeenie();

	virtual class std::shared_ptr<CBootSpotWeenie> AsBootSpot() { return std::static_pointer_cast<CBootSpotWeenie>(GetPointer()); }

	class std::shared_ptr<CHouseWeenie> GetHouse();
};

class CHousePortalWeenie : public CPortal
{
public:
	CHousePortalWeenie();

	virtual class std::shared_ptr<CHousePortalWeenie> AsHousePortal() { return std::static_pointer_cast<CHousePortalWeenie>(GetPointer()); }
	virtual void ApplyQualityOverrides() override;

	class std::shared_ptr<CHouseWeenie> GetHouse();

	virtual int Use(std::shared_ptr<CPlayerWeenie> other) override;

	virtual bool GetDestination(Position &position) override;
};

class CStorageWeenie : public CChestWeenie
{
public:
	CStorageWeenie();

	virtual class std::shared_ptr<CStorageWeenie> AsStorage() { return std::static_pointer_cast<CStorageWeenie>(GetPointer()); }
	virtual bool ShouldSave() override { return true; }

	int DoUseResponse(std::shared_ptr<CWeenieObject> other) override;

	class std::shared_ptr<CHouseWeenie> GetHouse();
};



