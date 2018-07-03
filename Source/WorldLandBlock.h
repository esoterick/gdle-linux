
#pragma once

#include "World.h"
#include "WeenieObject.h"
#include "Monster.h"
#include "Player.h"
#include "easylogging++.h"

#ifdef _DEBUG
#define DELETE_ENTITY(x) DEBUG_DATA << "Delete Entity" << pEntity->id << "@" << __FUNCTION__; delete pEntity
#else
#define DELETE_ENTITY(x) delete pEntity
#endif

enum LandblockDormancyStatus
{
	DoNotGoDormant,
	WaitToGoDormant,
	Dormant
};

class CWorldLandBlock
{
public:
	CWorldLandBlock(CWorld *pWorld, WORD wHeader);
	~CWorldLandBlock();
	
	void ClearOldDatabaseEntries();

	void Init();

	WORD GetHeader();
	BOOL Think();

	void ClearSpawns();

	void Insert(std::shared_ptr<CWeenieObject> pEntity, WORD wOld = 0, BOOL bNew = FALSE, bool bMakeAware = true);
	void Destroy(std::shared_ptr<CWeenieObject> pEntity, bool bDoRelease = true);
	void Release(std::shared_ptr<CWeenieObject> pEntity);
	void ExchangePVS(std::shared_ptr<CWeenieObject> pSource, WORD old_block_id);
	void ExchangeData(std::shared_ptr<CWeenieObject> pSource);
	void ExchangeDataForCellID(std::shared_ptr<CWeenieObject> pSource, DWORD cell_id);
	void ExchangeDataForStabChange(std::shared_ptr<CWeenieObject> pSource, DWORD old_cell_id, DWORD new_cell_id);

	std::shared_ptr<CPlayerWeenie> FindPlayer(DWORD dwGUID);
	std::shared_ptr<CWeenieObject> FindEntity(DWORD dwGUID);

	void Broadcast(void *_data, DWORD _len, WORD _group, DWORD ignore_ent, BOOL _game_event);

	DWORD PlayerCount() { return (DWORD)m_PlayerList.size(); }
	DWORD LiveCount() { return (DWORD)m_EntityList.size(); }

	void EnumNearbyFastNoSphere(const Position &pos, float range, std::list<std::shared_ptr<CWeenieObject> > *results);
	void EnumNearby(const Position &pos, float range, std::list<std::shared_ptr<CWeenieObject> > *results);
	void EnumNearby(std::shared_ptr<CWeenieObject> source, float range, std::list<std::shared_ptr<CWeenieObject> > *results);
	void EnumNearbyPlayers(const Position &pos, float range, std::list<std::shared_ptr<CWeenieObject> > *results);
	void EnumNearbyPlayers(std::shared_ptr<CWeenieObject> source, float range, std::list<std::shared_ptr<CWeenieObject> > *results);

	class CObjCell *GetObjCell(WORD cell_id, bool bDoPostLoad = true); // , bool bActivate = false);

	bool HasPlayers();
	LandblockDormancyStatus GetDormancyStatus() { return m_DormancyStatus; }
	double GetDormancyTime() { return m_fTimeToGoDormant; }

	bool IsWaterBlock();
	bool HasAnySeenOutside();
	bool PossiblyVisibleToOutdoors(DWORD cell_id);

	bool IsTickingWithWorld() { return m_bTickingWithWorld; }
	void SetIsTickingWithWorld(bool ticking);

	void UnloadSpawnsUntilNextTick();

protected:
	void MakeNotDormant();

	void LoadLandBlock();
	void SpawnDynamics();

	bool PlayerWithinPVS();
	bool CanGoDormant();

	void ActivateLandblocksWithinPVS(DWORD cell_id);

	/*
	* Helper function that will remove an entity from the lists and maps
	* Also removes any nullptrs
	* @param pEntity The entity to remove
	* @param bCheckEntityList Whether to run the remove over m_EntityList. For optimisation.
	*/
	void RemoveEntity(std::shared_ptr<CWeenieObject> pEntity, bool bCheckEntityList = true);

	CWorld *m_pWorld;

	WORD m_wHeader;


	PlayerWeenieWeakMap m_PlayerMap;
	PlayerWeenieWeakVector m_PlayerList; // Players, used for message broadcasting.

	WeenieWeakMap m_EntityMap;
	WeenieWeakVector m_EntitiesToAdd;
	WeenieWeakVector m_EntityList;

	class CLandBlock *m_LoadedLandBlock = NULL;
	std::unordered_map<WORD, class CEnvCell *> m_LoadedEnvCells;

	bool m_bSpawnOnNextTick = false;

	LandblockDormancyStatus m_DormancyStatus = LandblockDormancyStatus::DoNotGoDormant;
	double m_fNextDormancyCheck;
	double m_fTimeToGoDormant;

	bool m_bTickingWithWorld = true;

	bool _cached_any_seen_outside = true;

};