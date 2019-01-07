
#pragma once

class CInferredPortalData
{
public:
	CInferredPortalData();
	~CInferredPortalData();

	void Init();

	DWORD GetWCIDForTerrain(long x, long y, int index);
	CSpellTableEx *GetSpellTableEx();
	class CCraftOperation *GetCraftOperation(DWORD source_wcid, DWORD dest_wcid);
	Position *GetHousePortalDest(DWORD house_id, DWORD ignore_cell_id);
	CMutationFilter *GetMutationFilter(DWORD id);
	std::vector<std::string> GetBannedWords();
	std::set<DWORD> GetRestrictedLandblocks();

	using position_list_t = PackableListWithJson<Position>;
	using house_portal_table_t = PackableHashTableWithJson<DWORD, position_list_t>;
	//using mutation_table_t = PackableHashTableWithJson<DWORD, CMutationFilter *>;
	using mutation_table_t = std::unordered_map<DWORD, CMutationFilter *>;

	CRegionDescExtendedDataTable _regionData;
	CSpellTableExtendedDataTable _spellTableData;
	TreasureTable _treasureTableData;
	CCraftTable _craftTableData;
	house_portal_table_t _housePortalDests;
	CQuestDefDB _questDefDB;
	mutation_table_t _mutationFilters;
	GameEventDefDB _gameEvents;
	std::vector<std::string> _bannedWords;
	std::set<DWORD> _restrictedLBData;
};


