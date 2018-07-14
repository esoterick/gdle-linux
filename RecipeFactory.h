#pragma once
class RecipeFactory
{
public:
	RecipeFactory();
	~RecipeFactory();

	void Reset();
	void Initialize();

private:
	PackableHashTableWithJson<DWORD, CCraftOperation> *_recipesFromJson = NULL;

};

