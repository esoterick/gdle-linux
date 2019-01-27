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

	precursor_list_t jsonPrecursors;
	recipe_list_t jsonRecipes;

	if(precursorData.size() > 0)
		jsonPrecursors.UnPackJson(precursorData);

	if (recipeData.size() > 0)
		jsonRecipes.UnPackJson(recipeData);

	if(jsonPrecursors.size() > 0)
		UpdateCraftTableData(jsonPrecursors);

	if (jsonRecipes.size() > 0)
		UpdateExitingRecipes(jsonRecipes);

	t = clock() - t;
	float s = (float)t / CLOCKS_PER_SEC;

	WINLOG(Data, Normal, "Finished loading recipes in %fs\n", s);
	SERVER_INFO << "Finished loading recipes in " << s;
}

void RecipeFactory::UpdateCraftTableData(precursor_list_t &precursors)
{
	// for each precursor see if it exists in the table
	for (auto pc : precursors)
	{
		DWORD64 toolTarget = ((DWORD64)pc.Tool << 32) | pc.Target;
		g_pPortalDataEx->_craftTableData._precursorMap[toolTarget] = pc.RecipeID;
	}
}

void RecipeFactory::UpdateExitingRecipes(recipe_list_t &recipes)
{
	for (auto pc : recipes)
	{
		DWORD recipeIdToUpdate = pc._recipeID;
		g_pPortalDataEx->_craftTableData._operations[recipeIdToUpdate] = GetCraftOpertionFromNewRecipe(&pc);
	}
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




