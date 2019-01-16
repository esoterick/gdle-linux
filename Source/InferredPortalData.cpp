
#include "StdAfx.h"
#include "PhatSDK.h"
#include "InferredPortalData.h"

CInferredPortalData::CInferredPortalData()
{
}

CInferredPortalData::~CInferredPortalData()
{
	for (auto &entry : _mutationFilters)
	{
		delete entry.second;
	}
	_mutationFilters.clear();
}

bool CInferredPortalData::LoadJsonData(std::filesystem::path path, load_func_t cb)
{
	std::ifstream fileStream(path);

	if (fileStream.is_open())
	{
		json temp;
		fileStream >> temp;
		fileStream.close();
		cb(temp);

		return true;
	}

	return false;
}

bool CInferredPortalData::LoadJsonData(std::filesystem::path path, PackableJson &data)
{
	return LoadJsonData(path, [&data](json& json) { data.UnPackJson(json); });
}

bool CInferredPortalData::LoadCacheData(DWORD id, DWORD magic1, DWORD magic2, PackObj &data)
{
	BYTE *buffer = NULL;
	DWORD length = 0;
	if (LoadDataFromPhatDataBin(id, &buffer, &length, magic1, magic2))
	{
		BinaryReader reader(buffer, length);
		data.UnPack(&reader);
		delete[] buffer;


		/*json test;
		_treasureTableData._treasureList.PackJson(test);

		std::ofstream out("data/json/wieldedTreasure.json");
		out << std::setw(4) << test << std::endl;
		out.flush();
		out.close();*/

		return true;
	}

	return false;

}

void CInferredPortalData::Init()
{
#ifndef PUBLIC_BUILD
	SERVER_INFO << "Loading inferred portal data...";
#endif

	std::filesystem::path dataPath("data/json");

	std::filesystem::create_directories(dataPath);

	_regionData.Destroy();

	if (!LoadJsonData(dataPath / "region.json", _regionData))
	{
		LoadCacheData(1, 0xe8b00434, 0x82092270, _regionData);
	}

	if (!LoadJsonData(dataPath / "spells.json", _spellTableData))
	{
		LoadCacheData(2, 0x5D97BAEC, 0x41675123, _spellTableData);
	}

	// Treasure Factory
	LoadCacheData(3, 0x7DC126EB, 0x5F41B9AD, _treasureTableData);

	// WieldedTreasureType DIDs
	LoadJsonData(dataPath / "wieldedTreasure.json", _treasureTableData._treasureList);

	// Recipe Factory
	LoadCacheData(4, 0x5F41B9AD, 0x7DC126EB, _craftTableData);

	if (!LoadJsonData(dataPath / "housePortalDestinations.json", _housePortalDests))
	{
		LoadCacheData(5, 0x887aef9c, 0xa92ec9ac, _housePortalDests);
	}

	if (!LoadJsonData(dataPath / "quests.json", _questDefDB))
	{
		LoadCacheData(8, 0xE80D81CA, 0x8ECA9786, _questDefDB);
	}

	// Mutations

	//{
	//	BYTE *data = NULL;
	//	DWORD length = 0;
	//	if (LoadDataFromPhatDataBin(10, &data, &length, 0x5f1fa913, 0xe345c74c))
	//	{
	//		BinaryReader reader(data, length);

	//		DWORD numEntries = reader.Read<DWORD>();
	//		for (DWORD i = 0; i < numEntries; i++)
	//		{
	//			DWORD key = reader.Read<DWORD>();

	//			DWORD dataLength = reader.Read<DWORD>();

	//			BinaryReader entryReader(reader.ReadArray(dataLength), dataLength);
	//			CMutationFilter *val = new CMutationFilter();
	//			val->UnPack(&entryReader);

	//			_mutationFilters[key] = val;
	//		}

	//		delete[] data;
	//	}
	//}

	if (!LoadJsonData(dataPath / "events.json", _gameEvents))
	{
		LoadCacheData(11, 0x812a7823, 0x8b28e107, _gameEvents);
	}

	std::vector<std::string> &words = _bannedWords;
	LoadJsonData(dataPath / "bannedwords.json", [&words](json &data)
	{
		words = data.at("badwords").get<std::vector<std::string>>();
	});

	std::set<DWORD> &restrictions = _restrictedLBData;
	LoadJsonData(dataPath / "restrictedlandblocks.json", [&restrictions](json &data)
	{
		restrictions = data.at("restrictedlandblocks").get<std::set<DWORD>>();
	});
	//{
	//	std::ifstream fileStream("data\\json\\restrictedlandblocks.json");

	//	if (fileStream.is_open())
	//	{
	//		json jsonrestricLBData;
	//		fileStream >> jsonrestricLBData;
	//		fileStream.close();
	//		
	//		if (jsonrestricLBData.size() > 0)
	//			_restrictedLBData = jsonrestricLBData.at("restrictedlandblocks").get<std::set<DWORD>>();
	//	}
	//}

#ifndef PUBLIC_BUILD
	SERVER_INFO << "Finished loading inferred cell data.";
#endif
}

DWORD CInferredPortalData::GetWCIDForTerrain(long x, long y, int index)
{
	return _regionData.GetEncounter(x, y, index);
}

CSpellTableEx *CInferredPortalData::GetSpellTableEx()
{
	return &_spellTableData._table;
}

CCraftOperation *CInferredPortalData::GetCraftOperation(DWORD source_wcid, DWORD dest_wcid)
{
	DWORD64 toolComboKey = ((DWORD64)source_wcid << 32) | dest_wcid;
	const DWORD *opKey = g_pPortalDataEx->_craftTableData._precursorMap.lookup(toolComboKey);

	if (!opKey)
		return NULL;

	return g_pPortalDataEx->_craftTableData._operations.lookup(*opKey);
}

Position *CInferredPortalData::GetHousePortalDest(DWORD house_id, DWORD ignore_cell_id)
{
	house_portal_table_t::mapped_type *dests = _housePortalDests.lookup(house_id);

	if (dests)
	{
		for (auto &entry : *dests)
		{
			if (entry.objcell_id != ignore_cell_id)
				return &entry;
		}
	}

	return NULL;
}

CMutationFilter *CInferredPortalData::GetMutationFilter(DWORD id)
{
	mutation_table_t::iterator entry = _mutationFilters.find(id & 0xFFFFFF);
	
	if (entry != _mutationFilters.end())
	{
		return entry->second;
	}

	return NULL;
}

 

std::vector<std::string> CInferredPortalData::GetBannedWords()
{
	return _bannedWords;
}

 std::set<DWORD> CInferredPortalData::GetRestrictedLandblocks()
{
	return _restrictedLBData;
}