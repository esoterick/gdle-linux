
#include "StdAfx.h"
#include "PhatSDK.h"
#include "CraftTable.h"
#include "PackableJson.h"


DEFINE_UNPACK_JSON(CCraftOperation)
{
	_unk = reader["_unk"];
	_skill = (STypeSkill)reader["_skill"];
	_difficulty = reader["_difficulty"];
	_SkillCheckFormulaType = reader["_SkillCheckFormulaType"];
	_successWcid = reader["_successWcid"];
	_successAmount = reader["_successAmount"];
	_successMessage = reader["_successMessage"];
	_failWcid = reader["_failWcid"];
	_failAmount = reader["_failAmount"];
	_failMessage = reader["_failMessage"];

	_successConsumeTargetChance = reader["_successConsumeTargetChance"];
	_successConsumeTargetAmount = reader["_successConsumeTargetAmount"];
	_successConsumeTargetMessage = reader["_successConsumeTargetMessage"];

	_successConsumeToolChance = reader["_successConsumeToolChance"];
	_successConsumeToolAmount = reader["_successConsumeToolAmount"];
	_successConsumeToolMessage = reader["_successConsumeToolMessage"];

	_failureConsumeTargetChance = reader["_failureConsumeTargetChance"];
	_failureConsumeTargetAmount = reader["_failureConsumeTargetAmount"];
	_failureConsumeTargetMessage = reader["_failureConsumeTargetMessage"];

	_failureConsumeToolChance = reader["_failureConsumeToolChance"];
	_failureConsumeToolAmount = reader["_failureConsumeToolAmount"];
	_failureConsumeToolMessage = reader["_failureConsumeToolMessage"];

	const json &reqs = reader["Requirements"];

	for (const json &r : reqs)
	{
		_requirements->UnPackJson(r);
	}

	const json &mods = reader["Mods"];

	for (const json &m : mods)
	{
		_mods->UnPackJson(m);
	}

	_dataID = reader["_dataID"];
	return true;
}

DEFINE_PACK(CCraftOperation)
{
	UNFINISHED();
}

DEFINE_UNPACK(CCraftOperation)
{
	_unk = pReader->Read<DWORD>();
	_skill = (STypeSkill)pReader->Read<int>();
	_difficulty = pReader->Read<int>();
	_SkillCheckFormulaType = pReader->Read<DWORD>();
	_successWcid = pReader->Read<DWORD>();
	_successAmount = pReader->Read<DWORD>();
	_successMessage = pReader->ReadString();
	_failWcid = pReader->Read<DWORD>();
	_failAmount = pReader->Read<DWORD>();
	_failMessage = pReader->ReadString();

	_successConsumeTargetChance = pReader->Read<double>();
	_successConsumeTargetAmount = pReader->Read<int>();
	_successConsumeTargetMessage = pReader->ReadString();

	_successConsumeToolChance = pReader->Read<double>();
	_successConsumeToolAmount = pReader->Read<int>();
	_successConsumeToolMessage = pReader->ReadString();

	_failureConsumeTargetChance = pReader->Read<double>();
	_failureConsumeTargetAmount = pReader->Read<int>();
	_failureConsumeTargetMessage = pReader->ReadString();

	_failureConsumeToolChance = pReader->Read<double>();
	_failureConsumeToolAmount = pReader->Read<int>();
	_failureConsumeToolMessage = pReader->ReadString();

	for (DWORD i = 0; i < 3; i++)
		_requirements[i].UnPack(pReader);

	for (DWORD i = 0; i < 8; i++)
		_mods[i].UnPack(pReader);

	_dataID = pReader->Read<DWORD>();
	return true;
}

DEFINE_PACK_JSON(CCraftOperation)
{
	writer["_unk"] = _unk;
	writer["_skill"] = _skill;
	writer["_difficulty"] = _difficulty;
	writer["_SkillCheckFormulaType"] = _SkillCheckFormulaType;
	writer["_successWcid"] = _successWcid;
	writer["_successAmount"] = _successAmount;
	writer["_successMessage"] = _successMessage;
	writer["_failWcid"] = _failWcid;
	writer["_failAmount"] = _failAmount;
	writer["_failMessage"] = _failMessage;
	writer["_successConsumeTargetChance"] = _successConsumeTargetChance;
	writer["_successConsumeTargetAmount"] = _successConsumeTargetAmount;
	writer["_successConsumeTargetMessage"] = _successConsumeTargetMessage;
	writer["_successConsumeToolChance"] = _successConsumeToolChance;
	writer["_successConsumeToolAmount"] = _successConsumeToolAmount;
	writer["_successConsumeToolMessage"] = _successConsumeToolMessage;
	writer["_failureConsumeTargetChance"] = _failureConsumeTargetChance;
	writer["_failureConsumeTargetAmount"] = _failureConsumeTargetAmount;
	writer["_failureConsumeTargetMessage"] = _failureConsumeTargetMessage;
	writer["_failureConsumeToolChance"] = _failureConsumeToolChance;
	writer["_failureConsumeToolAmount"] = _failureConsumeToolAmount;
	writer["_failureConsumeToolMessage"] = _failureConsumeToolMessage;

	json requirements;

	for (DWORD i = 0; i < 3; i++)
	{
		json req;
		_requirements->PackJson(req);
		requirements.push_back(req);
	}

	writer["Requirements"] = requirements;

	json mods;

	for (DWORD i = 0; i < 8; i++)
	{
		json mod;
		_mods->PackJson(mod);
		mods.push_back(mod);
	}

	writer["Mods"] = mods;

	writer["_dataID"] = _dataID;
}

CCraftTable::CCraftTable()
{
}

CCraftTable::~CCraftTable()
{
}

DEFINE_PACK(CCraftTable)
{
	UNFINISHED();
}

DEFINE_UNPACK(CCraftTable)
{
#ifdef PHATSDK_USE_INFERRED_SPELL_DATA
	pReader->Read<DWORD>();
#endif

	_operations.UnPack(pReader);

	DWORD numEntries = pReader->Read<DWORD>();
	for (DWORD i = 0; i < numEntries; i++)
	{
		DWORD64 key = pReader->Read<DWORD64>();
		DWORD val = pReader->Read<DWORD>();
		_precursorMap[key] = val;
	}

	return true;
}

DEFINE_PACK_JSON(CCraftTable)
{}

DEFINE_UNPACK_JSON(CCraftTable)
{
	return false;
}

DEFINE_PACK(CraftRequirements) {}
DEFINE_UNPACK(CraftRequirements)
{
	_intRequirement.UnPack(pReader);
	_didRequirement.UnPack(pReader);
	_iidRequirement.UnPack(pReader);
	_floatRequirement.UnPack(pReader);
	_stringRequirement.UnPack(pReader);
	_boolRequirement.UnPack(pReader);
	return true;
}

DEFINE_PACK_JSON(CraftRequirements)
{
	_intRequirement.PackJson(writer);
	_didRequirement.PackJson(writer);
	_iidRequirement.PackJson(writer);
	_floatRequirement.PackJson(writer);
	_stringRequirement.PackJson(writer);
	_boolRequirement.PackJson(writer);
}

DEFINE_UNPACK_JSON(CraftRequirements) { return false; }


DEFINE_UNPACK_JSON(CraftPrecursor)
{
	Tool = reader["Tool"];
	Target = reader["Target"];
	RecipeID = reader["RecipeID"];
	return true;
}

DEFINE_PACK_JSON(CraftPrecursor)
{
	writer["Tool"] = Tool;
	writer["Target"] = Target;
	writer["RecipeID"] = RecipeID;
}

DEFINE_UNPACK_JSON(JsonCraftOperation)
{
	_recipeID = reader["RecipeID"];
	_unk = reader["_unk"];
	_skill = (STypeSkill)reader["_skill"];
	_difficulty = reader["_difficulty"];
	_SkillCheckFormulaType = reader["_SkillCheckFormulaType"];
	_successWcid = reader["_successWcid"];
	_successAmount = reader["_successAmount"];
	_successMessage = reader["_successMessage"];
	_failWcid = reader["_failWcid"];
	_failAmount = reader["_failAmount"];
	_failMessage = reader["_failMessage"];

	_successConsumeTargetChance = reader["_successConsumeTargetChance"];
	_successConsumeTargetAmount = reader["_successConsumeTargetAmount"];
	_successConsumeTargetMessage = reader["_successConsumeTargetMessage"];

	_successConsumeToolChance = reader["_successConsumeToolChance"];
	_successConsumeToolAmount = reader["_successConsumeToolAmount"];
	_successConsumeToolMessage = reader["_successConsumeToolMessage"];

	_failureConsumeTargetChance = reader["_failureConsumeTargetChance"];
	_failureConsumeTargetAmount = reader["_failureConsumeTargetAmount"];
	_failureConsumeTargetMessage = reader["_failureConsumeTargetMessage"];

	_failureConsumeToolChance = reader["_failureConsumeToolChance"];
	_failureConsumeToolAmount = reader["_failureConsumeToolAmount"];
	_failureConsumeToolMessage = reader["_failureConsumeToolMessage"];

	const json &reqs = reader["Requirements"];

	for (const json &r : reqs)
	{
		_requirements->UnPackJson(r);
	}

	const json &mods = reader["Mods"];

	for (const json &m : mods)
	{
		_mods->UnPackJson(m);
	}

	_dataID = reader["_dataID"];
	return true;
}

DEFINE_PACK_JSON(JsonCraftOperation)
{

}

template<typename TStatType, typename TDataType>
void TYPEMod<TStatType, TDataType>::Pack(BinaryWriter * pWriter)
{
}

template<typename TStatType, typename TDataType>
bool TYPEMod<TStatType, TDataType>::UnPack(BinaryReader * pReader)
{
	_unk = pReader->Read<int>();
	_operationType = pReader->Read<int>();
	_stat = (TStatType)pReader->Read<int>();
	_value = pReader->Read<TDataType>();
	return true;
}

template<typename TStatType, typename TDataType>
inline void TYPEMod<TStatType, TDataType>::PackJson(json & writer)
{
	writer["_unk"] = _unk;
	writer["_operationType"] = _operationType;
	writer["_stat"] = _stat;
	writer["_value"] = _value;
}

template<typename TStatType, typename TDataType>
inline bool TYPEMod<TStatType, TDataType>::UnPackJson(const json & reader)
{
	_unk = reader["_unk"];
	_operationType = reader["_operationType"];
	_stat = (TStatType)reader["_stat"];
	_value = reader["_value"];
	return false;
}

template<typename TStatType, typename TDataType>
void TYPERequirement<TStatType, TDataType>::Pack(BinaryWriter * pWriter)
{
}

template<typename TStatType, typename TDataType>
bool TYPERequirement<TStatType, TDataType>::UnPack(BinaryReader * pReader)
{
	_stat = (TStatType)pReader->Read<int>();
	_value = pReader->Read<TDataType>();
	_operationType = pReader->Read<int>();
	_message = pReader->ReadString();
	return true;
}

template<typename TStatType, typename TDataType>
inline void TYPERequirement<TStatType, TDataType>::PackJson(json & writer)
{
	writer["_stat"] = _stat;
	writer["_value"] = _value;
	writer["_operationType"] = _operationType;
	writer["_message"] = _message;
}

template<typename TStatType, typename TDataType>
inline bool TYPERequirement<TStatType, TDataType>::UnPackJson(const json & reader)
{
	_stat = (TStatType)reader["_stat"];
	_value = reader["_value"];
	_operationType = reader["_operationType"];
	_message = reader["_message"];
	return false;
}

void CraftMods::Pack(BinaryWriter * pWriter)
{
}

bool CraftMods::UnPack(BinaryReader * pReader)
{
	_intMod.UnPack(pReader);
	_didMod.UnPack(pReader);
	_iidMod.UnPack(pReader);
	_floatMod.UnPack(pReader);
	_stringMod.UnPack(pReader);
	_boolMod.UnPack(pReader);

	_ModifyHealth = pReader->Read<int>();
	_ModifyStamina = pReader->Read<int>();
	_ModifyMana = pReader->Read<int>();
	_RequiresHealth = pReader->Read<int>();
	_RequiresStamina = pReader->Read<int>();
	_RequiresMana = pReader->Read<int>();

	_unknown7 = pReader->Read<BOOL>();
	_modificationScriptId = pReader->Read<DWORD>(); // dataID

	_unknown9 = pReader->Read<int>();
	_unknown10 = pReader->Read<DWORD>(); // instanceID

	return true;
}

void CraftMods::PackJson(json & writer)
{
	if (_intMod.size() > 0)
	{
		json intMods;

		for (DWORD i = 0; i < 3; i++)
		{
			json req;
			_intMod.PackJson(req);
			intMods.push_back(req);
		}

		writer["IntRequirements"] = intMods;
	}

	if (_didMod.size() > 0)
	{
		json didMod;

		for (DWORD i = 0; i < 3; i++)
		{
			json req;
			_didMod.PackJson(req);
			didMod.push_back(req);
		}

		writer["DIDRequirements"] = didMod;
	}

	if (_iidMod.size() > 0)
	{
		json iidMod;

		for (DWORD i = 0; i < 3; i++)
		{
			json req;
			_iidMod.PackJson(req);
			iidMod.push_back(req);
		}

		writer["IIDRequirements"] = iidMod;
	}

	if (_floatMod.size() > 0)
	{
		json floatMod;

		for (DWORD i = 0; i < 3; i++)
		{
			json req;
			_floatMod.PackJson(req);
			floatMod.push_back(req);
		}

		writer["FloatRequirements"] = floatMod;
	}

	if (_stringMod.size() > 0)
	{
		json stringMod;

		for (DWORD i = 0; i < 3; i++)
		{
			json req;
			_stringMod.PackJson(req);
			stringMod.push_back(req);
		}

		writer["StringRequirements"] = stringMod;
	}

	if (_boolMod.size() > 0)
	{
		json boolMod;

		for (DWORD i = 0; i < 3; i++)
		{
			json req;
			_boolMod.PackJson(req);
			boolMod.push_back(req);
		}

		writer["BoolRequirements"] = boolMod;
	}

	writer["_ModifyHealth"] = _ModifyHealth;
	writer["_ModifyStamina"] = _ModifyStamina;
	writer["_ModifyMana"] = _ModifyMana;
	writer["_RequiresHealth"] = _RequiresHealth;
	writer["_RequiresStamina"] = _RequiresStamina;
	writer["_RequiresMana"] = _RequiresMana;
	writer["_unknown7"] = _unknown7;
	writer["_modificationScriptId"] = _modificationScriptId;
	writer["_unknown9"] = _unknown9;
	writer["_unknown10"] = _unknown10;
}

bool CraftMods::UnPackJson(const json & reader)
{
	if (reader.find("IntRequirements") != reader.end())
	{
		_intMod.UnPackJson(reader);
	}

	if (reader.find("DIDRequirements") != reader.end())
	{
		_didMod.UnPackJson(reader);
	}

	if (reader.find("IIDRequirements") != reader.end())
	{
		_iidMod.UnPackJson(reader);
	}

	if (reader.find("FloatRequirements") != reader.end())
	{
		_floatMod.UnPackJson(reader);
	}

	if (reader.find("StringRequirements") != reader.end())
	{
		_stringMod.UnPackJson(reader);
	}

	if (reader.find("BoolRequirements") != reader.end())
	{
		_boolMod.UnPackJson(reader);
	}

	_ModifyHealth = reader["_ModifyHealth"];
	_ModifyStamina = reader["_ModifyStamina"];
	_ModifyMana = reader["_ModifyMana"];
	_RequiresHealth = reader["_RequiresHealth"];
	_RequiresStamina = reader["_RequiresStamina"];
	_RequiresMana = reader["_RequiresMana"];
	_unknown7 = reader["_unknown7"];
	_modificationScriptId = reader["_modificationScriptId"];
	_unknown9 = reader["_unknown9"];
	_unknown10 = reader["_unknown10"];
	return true;
}
