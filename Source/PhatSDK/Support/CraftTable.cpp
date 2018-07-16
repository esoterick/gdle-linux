
#include "StdAfx.h"
#include "PhatSDK.h"
#include "CraftTable.h"
#include "PackableJson.h"

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
{}

DEFINE_UNPACK_JSON(CCraftOperation)
{
	return false;
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
DEFINE_PACK_JSON(CraftRequirements) {}
DEFINE_UNPACK_JSON(CraftRequirements) { return false; }

DEFINE_PACK(CraftMods) {}
DEFINE_UNPACK(CraftMods)
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

										 /*
										 printf("%u %u %u %u %u %u %d %d %d %d %d %d %d %u %d %u\n",
										 (DWORD)_intMod.size(), (DWORD)_didMod.size(), (DWORD)_iidMod.size(),
										 (DWORD)_floatMod.size(), (DWORD)_stringMod.size(), (DWORD)_boolMod.size(),
										 a, b, c, d, e, f, g, h, i, j);
										 */

	return true;

}
DEFINE_PACK_JSON(CraftMods) {}
DEFINE_UNPACK_JSON(CraftMods) { return false; }

DEFINE_UNPACK_JSON(CraftPrecursor)
{
	if (reader.find("Tool") != reader.end())
	{
		Tool = reader["Tool"];
	}
	else
	{
		return false;
	}
	if (reader.find("Target") != reader.end())
	{
		Target = reader["Target"];
	}
	else
	{
		return false;
	}
	if (reader.find("RecipeID") != reader.end())
	{
		RecipeID = reader["RecipeID"];
	}
	else
	{
		return false;
	}
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
	_unk = reader["unknown_1"];
	_skill = (STypeSkill)reader["Skill"];
	_difficulty = reader["Difficulty"];
	_SkillCheckFormulaType = reader["SkillCheckFormulaType"];
	_successWcid = reader["SuccessWCID"];
	_successAmount = reader["SuccessAmount"];
	_successMessage = reader["SuccessMessage"];
	_failWcid = reader["FailWCID"];
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

	for (DWORD i = 0; i < 3; i++)
		_requirements[i].UnPackJson(&reader);

	for (DWORD i = 0; i < 8; i++)
		_mods[i].UnPackJson(&reader);

	_dataID = reader["DataID"];
	return true;
}

DEFINE_PACK_JSON(JsonCraftOperation)
{

}