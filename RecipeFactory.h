#pragma once

using recipe_map_t = std::unordered_map<uint32_t, JsonCraftOperation>;

class RecipeFactory
{
public:
	RecipeFactory();
	~RecipeFactory();

	void Reset();
	void Initialize();
	void UpdateCraftTableData();
	void UpdateExitingRecipes();


private:
	//PackableHashTableWithJson<DWORD, JsonCraftOperation> *_recipesFromJson = NULL;
	PackableListWithJson<CraftPrecursor> _jsonPrecursorMap;
	//PackableListWithJson<JsonCraftOperation> _jsonRecipes;

	recipe_map_t m_recipes;

	bool RecipeInJson(DWORD recipeid, JsonCraftOperation* recipe);
	CCraftOperation GetCraftOpertionFromNewRecipe(JsonCraftOperation* recipe);
};

