#include "stdafx.h"
#include "RecipeFactory.h"
#include "InferredPortalData.h"

RecipeFactory::RecipeFactory()
{
}


RecipeFactory::~RecipeFactory()
{
}

void RecipeFactory::Reset()
{
}

void RecipeFactory::Initialize()
{
	json precursorData;
	std::ifstream rpcStream("data\\json\\recipeprecursor.json");
	if (rpcStream.is_open())
	{

		rpcStream >> precursorData;
		rpcStream.close();

	}

	json recipeData;
	std::ifstream rcStream("data\\json\\recipes.json");
	if (rcStream.is_open())
	{
		rcStream >> recipeData;
		rcStream.close();
	}
	_jsonPrecursorMap.UnPackJson(precursorData);
	_jsonRecipes.UnPackJson(recipeData);

}

void RecipeFactory::UpdateCraftTableData(CCraftTable * craftData)
{
	// for each precursor see if it exists in the table
	for (auto pc : _jsonPrecursorMap)
	{
		DWORD toolTarget = ((DWORD64)pc.Tool << 32 | pc.Target);
		DWORD targetTool = ((DWORD64)pc.Target << 32 | pc.Tool);

		const DWORD *opKey = g_pPortalDataEx->_craftTableData._precursorMap.lookup(toolTarget);

		if (!opKey) // Check backwards
		{
			opKey = g_pPortalDataEx->_craftTableData._precursorMap.lookup(targetTool);
		}

		if (!opKey) // New recipe 
		{
			JsonCraftOperation* newrecipe = nullptr;
			RecipeInJson(pc.RecipeID, newrecipe);
			if (newrecipe != nullptr)
			{
				g_pPortalDataEx->_craftTableData._precursorMap[toolTarget] = pc.RecipeID;
				g_pPortalDataEx->_craftTableData._operations[newrecipe->_recipeID] = GetCraftOpertionFromNewRecipe(newrecipe);
			}
			else
			{
				SERVER_ERROR << "Recipe" << pc.RecipeID << "missing from json recipe file - not added";
			}
		}
		else 
		{
			if (g_pPortalDataEx->_craftTableData._operations.lookup(*opKey))
			{
				g_pPortalDataEx->_craftTableData._precursorMap[*opKey] = pc.RecipeID;
			}
		}
	}
	
}

bool RecipeFactory::RecipeInJson(DWORD recipeid, JsonCraftOperation *recipe)
{
	for (auto rp : _jsonRecipes)
	{
		if (rp._recipeID == recipeid)
		{
			recipe = &rp;
			return true;
		}
	}

	return false;
}

CCraftOperation RecipeFactory::GetCraftOpertionFromNewRecipe(JsonCraftOperation * recipe)
{
	CCraftOperation co;
	co._dataID = recipe->_dataID;
	co._difficulty = recipe->_difficulty;
	co._failAmount = recipe->_failAmount;
	co._failMessage = recipe->_failMessage;
	co._failureConsumeTargetAmount = recipe->_failureConsumeTargetAmount;
	co._failureConsumeTargetChance = recipe->_failureConsumeTargetChance;
	co._failureConsumeTargetMessage = recipe->_failureConsumeTargetMessage;
	co._failureConsumeToolAmount = recipe->_failureConsumeToolAmount;
	co._failureConsumeToolChance = recipe->_failureConsumeToolChance;
	co._failureConsumeToolMessage = recipe->_failureConsumeToolMessage;
	co._failWcid = recipe->_failWcid;
	for (int m = 0; m < 8; m++)
	{
		co._mods[m] = recipe->_mods[m];
	}
	for (int r = 0; r < 3; r++)
	{
		co._requirements[r] = recipe->_requirements[r];
	}
	co._skill = recipe->_skill;
	co._SkillCheckFormulaType = recipe->_SkillCheckFormulaType;
	co._successAmount = recipe->_successAmount;
	co._successConsumeTargetAmount = recipe->_successConsumeTargetAmount;
	co._successConsumeTargetChance = recipe->_successConsumeTargetChance;
	co._successConsumeTargetMessage = recipe->_successConsumeTargetMessage;
	co._successConsumeToolAmount = recipe->_successConsumeToolAmount;
	co._successConsumeToolChance = recipe->_successConsumeToolChance;
	co._successConsumeToolMessage = recipe->_successConsumeToolMessage;
	co._successMessage = recipe->_successMessage;
	co._successWcid = recipe->_successWcid;

	return co;
}




