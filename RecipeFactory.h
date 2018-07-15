#pragma once
class RecipeFactory
{
public:
	RecipeFactory();
	~RecipeFactory();

	void Reset();
	void Initialize();
	void UpdateCraftTableData(CCraftTable *craftData);

private:
	PackableHashTableWithJson<DWORD, JsonCraftOperation> *_recipesFromJson = NULL;
	PackableListWithJson<CraftPrecursor> _jsonPrecursorMap;
	PackableListWithJson<JsonCraftOperation> _jsonRecipes;

	JsonCraftOperation* RecipeInJson(DWORD recipeid);
	CCraftOperation GetCraftOpertionFromNewRecipe(JsonCraftOperation* recipe);
};

