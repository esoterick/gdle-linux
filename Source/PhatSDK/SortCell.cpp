
#include "StdAfx.h"
#include "SortCell.h"

CSortCell::CSortCell()
{
	building = std::shared_ptr<CBuildingObj>();
}

void CSortCell::add_building(std::shared_ptr<CBuildingObj> _object)
{
	if (!building.lock())
		building = _object;
}

void CSortCell::remove_building(std::shared_ptr<CBuildingObj> _object)
{
	building = std::shared_ptr<CBuildingObj>();
}

BOOL CSortCell::has_building()
{
	return (!building.lock());
}

std::shared_ptr<CPhysicsObj> CSortCell::get_object(DWORD obj_iid)
{
	std::shared_ptr<CPhysicsObj> result = CObjCell::get_object(obj_iid);

	if (!result)
	{
		if (std::shared_ptr<CBuildingObj> pBuilding = building.lock())
			result = pBuilding->get_object(obj_iid);
	}

	return result;
}

TransitionState CSortCell::find_collisions(CTransition *transit)
{
	if (std::shared_ptr<CBuildingObj> pBuilding = building.lock())
	{
		return pBuilding->find_building_collisions(transit);
	}

	return OK_TS;
}

void CSortCell::find_transit_cells(const unsigned int num_parts, CPhysicsPart **parts, CELLARRAY *cell_array)
{
	if (std::shared_ptr<CBuildingObj> pBuilding = building.lock())
	{
		pBuilding->find_building_transit_cells(num_parts, parts, cell_array);
	}
}

void CSortCell::find_transit_cells(Position *p, const unsigned int num_sphere, CSphere *sphere, CELLARRAY *cell_array, SPHEREPATH *path)
{
	if (std::shared_ptr<CBuildingObj> pBuilding = building.lock())
	{
		pBuilding->find_building_transit_cells(p, num_sphere, sphere, cell_array, path);
	}
}