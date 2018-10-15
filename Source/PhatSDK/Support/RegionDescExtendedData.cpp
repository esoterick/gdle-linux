
#include "stdafx.h"
#include "PhatSDK.h"

CRegionDescExtendedDataTable::CRegionDescExtendedDataTable()
{
}

CRegionDescExtendedDataTable::~CRegionDescExtendedDataTable()
{
	Destroy();
}

void CRegionDescExtendedDataTable::Destroy()
{
	_encounterTableSize = 0;
	_numTableEntries = 0;
	//SafeDeleteArray(_encounterTable);
	//SafeDeleteArray(_encounterMap);
}

DWORD CRegionDescExtendedDataTable::GetEncounter(long x, long y, int index)
{
	if (!_encounterTableSize)
		return 0;

	//DWORD *encounterTableData = &_encounterTable[_encounterMap[(x * 255) + y] * 16];
	encounter_list_t &table = _encounterTable[_encounterMap[(x * 255) + y]];
	
	//DWORD wcid = encounterTableData[index];
	DWORD wcid = table[index];

	//assert(wcid != 0xFFFFFFFF);
	if (wcid == 0xFFFFFFFF)
		wcid = 0;

	return wcid;
}

DEFINE_PACK(CRegionDescExtendedDataTable)
{
	pWriter->Write<DWORD>(_encounterTableSize);
	if (_encounterTableSize)
	{
		pWriter->Write<DWORD>(_numTableEntries);

		DWORD numWritten = 0;
		for (DWORD i = 0; i < _encounterTableSize; i++)
		{
			encounter_list_t& table = _encounterTable[i];
			if (table[0] != 0xFFFFFFFF)
			{
				pWriter->Write<DWORD>(i);
				pWriter->Write(table.data(), sizeof(DWORD) * 16);
				numWritten++;
			}
		}

		assert(numWritten == _numTableEntries);

		pWriter->Write(_encounterMap.data(), 255 * 255);
	}
}

DEFINE_UNPACK(CRegionDescExtendedDataTable)
{
	Destroy();

	_encounterTableSize = pReader->Read<DWORD>();
	if (_encounterTableSize)
	{
		//_encounterTable = new DWORD[_encounterTableSize * 16];
		//memset(_encounterTable, 0xFF, sizeof(DWORD) * _encounterTableSize * 16);

		_encounterTable.resize(_encounterTableSize);

		_numTableEntries = pReader->Read<DWORD>();
		for (DWORD i = 0; i < _numTableEntries; i++)
		{
			DWORD index = pReader->Read<DWORD>();
			encounter_list_t &table = _encounterTable[index];
			table.fill(0xffffffff);

			memcpy(table.data(), pReader->ReadArray(sizeof(DWORD) * 16), sizeof(DWORD) * 16);
		}
		//_encounterMap = new BYTE[255 * 255];
		memcpy(_encounterMap.data(), pReader->ReadArray(sizeof(BYTE) * 255 * 255), sizeof(BYTE) * 255 * 255);
	}
	return true;
}

DEFINE_PACK_JSON(CRegionDescExtendedDataTable)
{
	writer["tableSize"] = _encounterTableSize;
	writer["tableCount"] = _numTableEntries;

	if (_encounterTableSize > 0)
	{
		json tableWriter;
		for (int i = 0; i < _encounterTableSize; i++)
		{
			encounter_list_t &table = _encounterTable[i];
			if (table[0] != 0xffffffff)
			{
				json entry;
				json value;

				// TODO: Can this be done w/o a loop?
				for (int j = 0; j < table.size(); j++)
					value[j] = table[j];

				entry["key"] = i;
				entry["value"] = value;

				tableWriter.push_back(entry);
			}
		}
		writer["encounters"] = tableWriter;

		json encounterMap;
		// TODO: Can this be done w/o a loop?
		for (int j = 0; j < _encounterMap.size(); j++)
			encounterMap[j] = _encounterMap[j];

		writer["encounterMap"] = encounterMap;
	}
}

DEFINE_UNPACK_JSON(CRegionDescExtendedDataTable)
{
	_encounterTableSize = reader.value<DWORD>("tableSize", 0);
	_numTableEntries = reader.value<DWORD>("tableCount", 0);

	if (_encounterTableSize > 0)
	{
		json::const_iterator end = reader.end();
		json::const_iterator itr = reader.find("encounters");
		if (itr != end)
		{
			for (encounter_list_t &table : _encounterTable)
			{
				table.fill(0xffffffff);
			}

			for (auto &entry : *itr)
			{
				int i = entry["key"];
				json value = entry["value"];

				encounter_list_t &table = _encounterTable[i];
				for (int i = 0; i < table.size(); i++)
					table[i] = value[i];
			}
		}

		itr = reader.find("encounterMap");
		if (itr != end)
		{
			const json &encounterMap = *itr;
			// TODO: Can this be done w/o a loop?
			for (int j = 0; j < _encounterMap.size(); j++)
				_encounterMap[j] = encounterMap[j];
		}
	}
	return true;
}

