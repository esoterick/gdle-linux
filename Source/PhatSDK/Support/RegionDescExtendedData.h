
#pragma once

class CRegionDescExtendedDataTable : public PackObj, public PackableJson
{
public:
	CRegionDescExtendedDataTable();
	virtual ~CRegionDescExtendedDataTable();

	DECLARE_PACKABLE()
	DECLARE_PACKABLE_JSON()

	void Destroy();

	DWORD GetEncounter(long x, long y, int index);

	DWORD _encounterTableSize = 0;
	DWORD _numTableEntries = 0;

	using encounter_list_t = std::array<DWORD, 16>;
	using encounter_table_t = std::vector<encounter_list_t>;
	using encounter_map_t = std::array<BYTE, 255 * 255>;

	encounter_table_t _encounterTable;
	encounter_map_t _encounterMap;
	//DWORD *_encounterTable = NULL;
	//BYTE *_encounterMap = NULL;
};
