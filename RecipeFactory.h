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
	PackableHashTableWithJson<DWORD, CCraftOperation> *_recipesFromJson = NULL;

};

