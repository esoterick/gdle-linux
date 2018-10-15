
#include "StdAfx.h"
#include "PhatSDK.h"
#include "CraftTable.h"
#include "PackableJson.h"


DEFINE_UNPACK_JSON(CCraftOperation)
{
	_unk = reader["Unknown"];
	_skill = (STypeSkill)reader["Skill"];
	_difficulty = reader["Difficulty"];
	_SkillCheckFormulaType = reader["SkillCheckFormulaType"];
	_successWcid = reader["SuccessWcid"];
	_successAmount = reader["SuccessAmount"];
	_successMessage = reader["SuccessMessage"];
	_failWcid = reader["FailWcid"];
	_failAmount = reader["FailAmount"];
	_failMessage = reader["FailMessage"];

	_successConsumeTargetChance = reader["SuccessConsumeTargetChance"];
	_successConsumeTargetAmount = reader["SuccessConsumeTargetAmount"];
	_successConsumeTargetMessage = reader["SuccessConsumeTargetMessage"];

	_successConsumeToolChance = reader["SuccessConsumeToolChance"];
	_successConsumeToolAmount = reader["SuccessConsumeToolAmount"];
	_successConsumeToolMessage = reader["SuccessConsumeToolMessage"];

	_failureConsumeTargetChance = reader["FailureConsumeTargetChance"];
	_failureConsumeTargetAmount = reader["FailureConsumeTargetAmount"];
	_failureConsumeTargetMessage = reader["FailureConsumeTargetMessage"];

	_failureConsumeToolChance = reader["FailureConsumeToolChance"];
	_failureConsumeToolAmount = reader["FailureConsumeToolAmount"];
	_failureConsumeToolMessage = reader["FailureConsumeToolMessage"];

	const json &reqs = reader["Requirements"];
	if (reqs.is_array())
	{
		// must have 3, null is valid
		for (int i = 0; i < 3; i++)
		{
			_requirements[i].UnPackJson(reqs[i]);
		}
	}

	const json &mods = reader["Mods"];
	if (mods.is_array())
	{
		// must have 8, null is valid
		for (int i = 0; i < 8; i++)
		{
			_mods[i].UnPackJson(mods[i]);
		}
	}

	_dataID = reader["DataID"];
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
	writer["Unknown"] = _unk;
	writer["Skill"] = _skill;
	writer["Difficulty"] = _difficulty;
	writer["SkillCheckFormulaType"] = _SkillCheckFormulaType;
	writer["SuccessWcid"] = _successWcid;
	writer["SuccessAmount"] = _successAmount;
	writer["SuccessMessage"] = _successMessage;
	writer["FailWcid"] = _failWcid;
	writer["FailAmount"] = _failAmount;
	writer["FailMessage"] = _failMessage;
	writer["SuccessConsumeTargetChance"] = _successConsumeTargetChance;
	writer["SuccessConsumeTargetAmount"] = _successConsumeTargetAmount;
	writer["SuccessConsumeTargetMessage"] = _successConsumeTargetMessage;
	writer["SuccessConsumeToolChance"] = _successConsumeToolChance;
	writer["SuccessConsumeToolAmount"] = _successConsumeToolAmount;
	writer["SuccessConsumeToolMessage"] = _successConsumeToolMessage;
	writer["FailureConsumeTargetChance"] = _failureConsumeTargetChance;
	writer["FailureConsumeTargetAmount"] = _failureConsumeTargetAmount;
	writer["FailureConsumeTargetMessage"] = _failureConsumeTargetMessage;
	writer["FailureConsumeToolAmount"] = _failureConsumeToolAmount;
	writer["FailureConsumeToolMessage"] = _failureConsumeToolMessage;

	json requirements;

	for (DWORD i = 0; i < 3; i++)
	{
		json req;
		_requirements[i].PackJson(req);
		requirements.push_back(req);
	}

	writer["Requirements"] = requirements;

	json mods;

	for (DWORD i = 0; i < 8; i++)
	{
		json mod;
		_mods[i].PackJson(mod);
		mods.push_back(mod);
	}

	writer["Mods"] = mods;

	writer["DataID"] = _dataID;
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
	if (_intRequirement.size() > 0)
	{
		json ints;
		_intRequirement.PackJson(ints);

		writer["IntRequirements"] = ints;
	}

	if (_didRequirement.size() > 0)
	{
		json dids;
		_didRequirement.PackJson(dids);

		writer["DIDRequirements"] = dids;
	}

	if (_iidRequirement.size() > 0)
	{
		json iids;
		_iidRequirement.PackJson(iids);

		writer["IIDRequirements"] = iids;
	}

	if (_floatRequirement.size() > 0)
	{
		json floats;
		_floatRequirement.PackJson(floats);

		writer["FloatRequirements"] = floats;
	}

	if (_stringRequirement.size() > 0)
	{
		json strings;
		_stringRequirement.PackJson(strings);

		writer["StringRequirements"] = strings;
	}

	if (_boolRequirement.size() > 0)
	{
		json bools;
		_boolRequirement.PackJson(bools);

		writer["BoolRequirements"] = bools;
	}

}

DEFINE_UNPACK_JSON(CraftRequirements)
{
	if (reader.is_null())
		return true;

	json::const_iterator itr = reader.end();
	json::const_iterator end = reader.end();

	itr = reader.find("IntRequirements");
	if (itr != end)
	{
		_intRequirement.UnPackJson(*itr);
	}

	itr = reader.find("DIDRequirements");
	if (itr != end)
	{
		_didRequirement.UnPackJson(*itr);
	}

	itr = reader.find("IIDRequirements");
	if (itr != end)
	{
		_iidRequirement.UnPackJson(*itr);
	}

	itr = reader.find("FloatRequirements");
	if (itr != end)
	{
		_floatRequirement.UnPackJson(*itr);
	}

	itr = reader.find("StringRequirements");
	if (itr != end)
	{
		_stringRequirement.UnPackJson(*itr);
	}

	itr = reader.find("BoolRequirements");
	if (itr != end)
	{
		_boolRequirement.UnPackJson(*itr);
	}

	return true; 
}


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
	_unk = reader["Unknown"];
	_skill = (STypeSkill)reader["Skill"];
	_difficulty = reader["Difficulty"];
	_SkillCheckFormulaType = reader["SkillCheckFormulaType"];
	_successWcid = reader["SuccessWcid"];
	_successAmount = reader["SuccessAmount"];
	_successMessage = reader["SuccessMessage"];
	_failWcid = reader["FailWcid"];
	_failAmount = reader["FailAmount"];
	_failMessage = reader["FailMessage"];

	_successConsumeTargetChance = reader["SuccessConsumeTargetChance"];
	_successConsumeTargetAmount = reader["SuccessConsumeTargetAmount"];
	_successConsumeTargetMessage = reader["SuccessConsumeTargetMessage"];

	_successConsumeToolChance = reader["SuccessConsumeToolChance"];
	_successConsumeToolAmount = reader["SuccessConsumeToolAmount"];
	_successConsumeToolMessage = reader["SuccessConsumeToolMessage"];

	_failureConsumeTargetChance = reader["FailureConsumeTargetChance"];
	_failureConsumeTargetAmount = reader["FailureConsumeTargetAmount"];
	_failureConsumeTargetMessage = reader["FailureConsumeTargetMessage"];

	_failureConsumeToolChance = reader["FailureConsumeToolChance"];
	_failureConsumeToolAmount = reader["FailureConsumeToolAmount"];
	_failureConsumeToolMessage = reader["FailureConsumeToolMessage"];

	const json &reqs = reader["Requirements"];
	if (reqs.is_array())
	{
		// must have 3, null is valid
		for (int i = 0; i < 3; i++)
		{
			_requirements[i].UnPackJson(reqs[i]);
		}
	}

	const json &mods = reader["Mods"];
	if (mods.is_array())
	{
		// must have 8, null is valid
		for (int i = 0; i < 8; i++)
		{
			_mods[i].UnPackJson(mods[i]);
		}
	}

	_dataID = reader["DataID"];
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
	writer["Unknown"] = _unk;
	writer["OperationType"] = _operationType;
	writer["Stat"] = _stat;
	writer["Value"] = _value;
}

template<typename TStatType, typename TDataType>
inline bool TYPEMod<TStatType, TDataType>::UnPackJson(const json & reader)
{
	_unk = reader["Unknown"];
	_operationType = reader["OperationType"];
	_stat = (TStatType)reader["Stat"];
	_value = reader["Value"];
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
	writer["Stat"] = _stat;
	writer["Value"] = _value;
	writer["OperationType"] = _operationType;
	writer["Message"] = _message;
}

template<typename TStatType, typename TDataType>
inline bool TYPERequirement<TStatType, TDataType>::UnPackJson(const json & reader)
{
	_stat = (TStatType)reader["Stat"];
	_value = reader["Value"];
	_operationType = reader["OperationType"];
	_message = reader["Message"];
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
	bool isnull = true;

	if (_intMod.size() > 0)
	{
		json intMods;
		_intMod.PackJson(intMods);

		writer["IntRequirements"] = intMods;
		
		isnull = false;
	}

	if (_didMod.size() > 0)
	{
		json didMod;
		_didMod.PackJson(didMod);

		writer["DIDRequirements"] = didMod;

		isnull = false;
	}

	if (_iidMod.size() > 0)
	{
		json iidMod;
		_iidMod.PackJson(iidMod);

		writer["IIDRequirements"] = iidMod;

		isnull = false;
	}

	if (_floatMod.size() > 0)
	{
		json floatMod;
		_floatMod.PackJson(floatMod);

		writer["FloatRequirements"] = floatMod;

		isnull = false;
	}

	if (_stringMod.size() > 0)
	{
		json stringMod;
		_stringMod.PackJson(stringMod);

		writer["StringRequirements"] = stringMod;

		isnull = false;
	}

	if (_boolMod.size() > 0)
	{
		json boolMod;
		_boolMod.PackJson(boolMod);

		writer["BoolRequirements"] = boolMod;

		isnull = false;
	}

	isnull &= _ModifyHealth == 0 && _ModifyStamina == 0 &&
		_ModifyMana == 0 && _RequiresHealth == 0 &&
		_RequiresStamina == 0 && _RequiresMana == 0 &&
		_unknown7 == false && _modificationScriptId == 0 &&
		_unknown9 == 0 && _unknown10 == 0;

	if (!isnull)
	{
		writer["ModifyHealth"] = _ModifyHealth;
		writer["ModifyStamina"] = _ModifyStamina;
		writer["ModifyMana"] = _ModifyMana;
		writer["RequiresHealth"] = _RequiresHealth;
		writer["RequiresStamina"] = _RequiresStamina;
		writer["RequiresMana"] = _RequiresMana;
		writer["Unknown7"] = _unknown7;
		writer["ModificationScriptId"] = _modificationScriptId;
		writer["Unknown9"] = _unknown9;
		writer["Unknown10"] = _unknown10;
	}
}

bool CraftMods::UnPackJson(const json & reader)
{
	if (reader.is_null())
		return true;

	json::const_iterator itr = reader.end();
	json::const_iterator end = reader.end();

	itr = reader.find("IntRequirements");
	if (itr != end)
	{
		_intMod.UnPackJson(*itr);
	}

	itr = reader.find("DIDRequirements");
	if (itr != end)
	{
		_didMod.UnPackJson(*itr);
	}

	itr = reader.find("IIDRequirements");
	if (itr != end)
	{
		_iidMod.UnPackJson(*itr);
	}

	itr = reader.find("FloatRequirements");
	if (itr != end)
	{
		_floatMod.UnPackJson(*itr);
	}

	itr = reader.find("StringRequirements");
	if (itr != end)
	{
		_stringMod.UnPackJson(*itr);
	}

	itr = reader.find("BoolRequirements");
	if (itr != end)
	{
		_boolMod.UnPackJson(*itr);
	}

	_ModifyHealth = reader["ModifyHealth"];
	_ModifyStamina = reader["ModifyStamina"];
	_ModifyMana = reader["ModifyMana"];
	_RequiresHealth = reader["RequiresHealth"];
	_RequiresStamina = reader["RequiresStamina"];
	_RequiresMana = reader["RequiresMana"];
	_unknown7 = reader["Unknown7"];
	_modificationScriptId = reader["ModificationScriptId"];
	_unknown9 = reader["Unknown9"];
	_unknown10 = reader["Unknown10"];
	return true;
}
