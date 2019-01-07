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
	clock_t t = clock();

	WINLOG(Data, Normal, "Loading recipes...\n");
	SERVER_INFO << "Loading recipes...";

	json precursorData;
	std::ifstream rpcStream(g_pGlobals->GetGameFile("data\\json\\recipeprecursors.json"));
	if (rpcStream.is_open())
	{

		rpcStream >> precursorData;
		rpcStream.close();

	}

	json recipeData;
	std::ifstream rcStream(g_pGlobals->GetGameFile("data\\json\\recipes.json"));
	if (rcStream.is_open())
	{
		rcStream >> recipeData;
		rcStream.close();
	}

	if(precursorData.size() > 0)
		_jsonPrecursorMap.UnPackJson(precursorData);

	if (recipeData.size() > 0)
	{
		PackableListWithJson<JsonCraftOperation> jsonRecipes;

		jsonRecipes.UnPackJson(recipeData);

		for (auto item : jsonRecipes)
		{
			m_recipes.emplace(item._recipeID, item);
		}
	}

	if(_jsonPrecursorMap.size() > 0 || m_recipes.size() > 0)
		UpdateCraftTableData();

	if (m_recipes.size() > 0)
		UpdateExitingRecipes();

	t = clock() - t;
	float s = (float)t / CLOCKS_PER_SEC;

	WINLOG(Data, Normal, "Finished loading recipes in %fs\n", s);
	SERVER_INFO << "Finished loading recipes in " << s;
}

void RecipeFactory::UpdateCraftTableData()
{
	// for each precursor see if it exists in the table
	for (auto pc : _jsonPrecursorMap)
	{
		DWORD64 toolTarget = ((DWORD64)pc.Tool << 32) | pc.Target;
		DWORD64 targetTool = ((DWORD64)pc.Target << 32) | pc.Tool;

		const DWORD *opKey = g_pPortalDataEx->_craftTableData._precursorMap.lookup(toolTarget);

		if (!opKey) // Check backwards
		{
			opKey = g_pPortalDataEx->_craftTableData._precursorMap.lookup(targetTool);
		}

		if (opKey)
		{
			JsonCraftOperation existingrecipe;
			if (RecipeInJson(pc.RecipeID, &existingrecipe))
			{
				g_pPortalDataEx->_craftTableData._operations[existingrecipe._recipeID] = GetCraftOpertionFromNewRecipe(&existingrecipe);
			}
		}

		if (!opKey) // New recipe 
		{
			JsonCraftOperation newrecipe;
			if (RecipeInJson(pc.RecipeID, &newrecipe))
			{
				g_pPortalDataEx->_craftTableData._precursorMap[toolTarget] = pc.RecipeID;
				g_pPortalDataEx->_craftTableData._operations[newrecipe._recipeID] = GetCraftOpertionFromNewRecipe(&newrecipe);
			}
			else
			{
				if (g_pPortalDataEx->_craftTableData._operations.lookup(pc.RecipeID))
				{
					g_pPortalDataEx->_craftTableData._precursorMap[toolTarget] = pc.RecipeID;
				}
				else
				{
					SERVER_ERROR << "Recipe" << pc.RecipeID << "missing from json recipe file - not added";
				}
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

void RecipeFactory::UpdateExitingRecipes()
{
	for (auto pc : m_recipes)
	{
		DWORD recipeIdToUpdate = pc.first;
		bool alreadyAdded = false;
		for (auto rp : _jsonPrecursorMap)
		{
			if (pc.first == rp.RecipeID)
			{
				alreadyAdded = true;
				break;
			}
		}

		if (!alreadyAdded)
		{
			CCraftOperation *currentRecipe = g_pPortalDataEx->_craftTableData._operations.lookup(recipeIdToUpdate);

			if (currentRecipe) // recipe found so update it
			{
				g_pPortalDataEx->_craftTableData._operations[recipeIdToUpdate] = GetCraftOpertionFromNewRecipe(&pc.second);
			}
		}
	}
}

bool RecipeFactory::RecipeInJson(DWORD recipeid, JsonCraftOperation *recipe)
{
	recipe_map_t::iterator itr = m_recipes.find(recipeid);
	if (itr != m_recipes.end())
	{
		*recipe = itr->second;
		return true;
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




