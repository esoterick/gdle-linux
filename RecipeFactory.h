#pragma once

using precursor_list_t = PackableListWithJson<CraftPrecursor>;
using recipe_list_t = PackableListWithJson<JsonCraftOperation>;

class RecipeFactory
{
public:
	RecipeFactory();
	~RecipeFactory();

	void Reset();
	void Initialize();

private:
	void UpdateCraftTableData(precursor_list_t &precursors);
	void UpdateExitingRecipes(recipe_list_t &recipes);

	CCraftOperation GetCraftOpertionFromNewRecipe(JsonCraftOperation* recipe);
};

