#include "stdafx.h"
#include "RecipeFactory.h"


#include "RecipeFactory.h"

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

}

void RecipeFactory::UpdateCraftTableData(CCraftTable * craftData)
{
	// for each precursor see if it exists in the table
	// check backwards too
	// if found, update _operation flag
	// if not found, add to map and _operation flag



	//DWORD64 toolComboKey = ((DWORD64)source_wcid << 32) | dest_wcid;
}


