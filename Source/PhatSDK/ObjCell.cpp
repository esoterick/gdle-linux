
#include "StdAfx.h"
#include "ObjCell.h"
#include "PartArray.h"
#include "PhysicsObj.h"
#include "Transition.h"
#include "Frame.h"
#include "BSPData.h"
#include "LandCell.h"
#include "EnvCell.h"
#include "LandDefs.h"

#if PHATSDK_RENDER_AVAILABLE
#include "Render.h"
#endif

CObjCell::CObjCell() : 
	num_objects(0), object_list(128),
	num_lights(0), light_list(128),
	num_shadow_objects(0), shadow_object_list()
{
	restriction_obj = 0;
	clip_planes = 0;
	num_stabs = 0;
	stab_list = NULL;
	seen_outside = 0;
	voyeur_table = 0;
	myLandBlock_ = NULL;
}

CObjCell::~CObjCell()
{
	if (voyeur_table)
	{
		delete voyeur_table;
		voyeur_table = NULL;
	}

	if (clip_planes)
	{
		if (clip_planes[0])
		{
			delete clip_planes[0];
		}
		delete [] clip_planes;
	}
}

std::shared_ptr<CPhysicsObj> CObjCell::get_object(DWORD iid)
{
	for (DWORD i = 0; i < num_objects; i++)
	{
		std::shared_ptr<CPhysicsObj> pObject = object_list[i];

		if (pObject && pObject->GetID() == iid)
			return pObject;
	}

	return NULL;
}

void CObjCell::add_object(std::shared_ptr<CPhysicsObj> pObject)
{
	object_list[num_objects++] = pObject;

	if (pObject->id && !pObject->parent.lock())
	{
		if (!(pObject->m_PhysicsState & HIDDEN_PS))
		{
			if (voyeur_table)
			{
				LongNIValHashIter<GlobalVoyeurInfo> it(voyeur_table);

				while (!it.EndReached())
				{
					try
					{
						DWORD voyeur_id = it.GetCurrent()->id;

						if (voyeur_id != pObject->id && voyeur_id)
						{
							std::shared_ptr<CPhysicsObj> pVoyeur = CPhysicsObj::GetObjectA(voyeur_id);

							if (pVoyeur)
							{
								DetectionInfo info;
								info.object_id = pObject->id;
								info.object_status = EnteredDetection;
								pVoyeur->receive_detection_update(&info);
							}
						}

						it.Next();
					}
					catch (...)
					{
						SERVER_ERROR << "Error in Add Object";
					}
				}
			}
		}
	}
}

void CObjCell::remove_object(std::shared_ptr<CPhysicsObj> pObject)
{
	for (DWORD i = 0; i < num_objects; i++)
	{
		if (pObject == object_list[i])
		{
			object_list[i] = object_list[--num_objects];

			break;
		}
	}
}

void CObjCell::add_shadow_object(CShadowObj *_object, unsigned int num_shadow_cells)
{
	shadow_object_list[num_shadow_objects] = _object;
	num_shadow_objects++;
	_object->cell = this;
}

void CObjCell::remove_shadow_object(CShadowObj *_object)
{
	for (DWORD i = 0; i < num_shadow_objects; i++)
	{
		if (_object == shadow_object_list[i])
		{
			_object->cell = NULL;
			shadow_object_list[i] = shadow_object_list[--num_shadow_objects];
		}
	}
}

void CObjCell::add_light(LIGHTOBJ *Light)
{
#if PHATSDK_RENDER_AVAILABLE
	// if (m_Position.objcell_id != 0x0120010E)
	//     return;

	if (num_lights >= light_list.alloc_size)
		light_list.grow(light_list.alloc_size + 5);

	light_list.array_data[num_lights++] = Light;

	// DEBUGOUT("Adding light(Mem@%08X) on cell %08X.\r\n", (DWORD)Light, m_Position.objcell_id);
	Render::pLightManager->AddLight(Light, &pos);
#endif
}

void CObjCell::remove_light(LIGHTOBJ *Light)
{
#if PHATSDK_RENDER_AVAILABLE
	for (DWORD i = 0; i < num_lights; i++)
	{
		if (Light == light_list.array_data[i])
		{
			// DEBUGOUT("Removing index %u light (total was %u)\r\n", i, num_lights);
			light_list.array_data[i] = light_list.array_data[--num_lights];

			if ((num_lights + 10) < light_list.alloc_size)
				light_list.shrink(num_lights + 5);

			if (Render::pLightManager)
				Render::pLightManager->RemoveLight(Light);
			break;
		}
	}
#endif
}

CShadowObj::CShadowObj()
{
	m_CellID = 0;
	cell = NULL;
}

CShadowObj::~CShadowObj()
{
}

void CShadowObj::set_physobj(std::shared_ptr<CPhysicsObj> pObject)
{
	physobj = pObject;
	id = pObject->GetID();
}

TransitionState CObjCell::find_collisions(class CTransition *)
{
	return TransitionState::INVALID_TS;
}

TransitionState CObjCell::find_obj_collisions(CTransition *transition)
{
	TransitionState ts = OK_TS;

	if (transition->sphere_path.insert_type != SPHEREPATH::INITIAL_PLACEMENT_INSERT)
	{
		for (DWORD i = 0; i < num_shadow_objects; i++)
		{
			std::shared_ptr<CPhysicsObj> pobj = shadow_object_list[i]->physobj.lock();

			if (pobj && !pobj->parent.lock() && pobj != transition->object_info.object.lock())
			{
				ts = pobj->FindObjCollisions(transition);
				if (ts != OK_TS)
					break;
			}
		}
	}

	return ts;
}

TransitionState CObjCell::find_env_collisions(CTransition *transition)
{
	return TransitionState::INVALID_TS;
}

TransitionState CObjCell::check_entry_restrictions(CTransition *transition)
{
	if (!transition->object_info.object.lock())
		return COLLIDED_TS;

	/*
	if (transition->object_info.object->weenie_obj)
	{
		v4 = ((int(__thiscall *)(std::shared_ptr<CWeenieObject> ))v3->vfptr[18].__vecDelDtor)(transition->object_info.object->weenie_obj);
		v5 = transition->object_info.state;
		if (BYTE1(v5) & 1)
		{
			if (v2->restriction_obj && !v4)
			{
				v6 = CPhysicsObj::GetObjectA(v2->restriction_obj);
				if (!v6)
					return 2;
				v7 = v6->weenie_obj;
				if (!v7)
					return 2;
				if (!((int(__stdcall *)(std::shared_ptr<CWeenieObject> ))v7->vfptr[17].__vecDelDtor)(v3))
				{
					((void(__thiscall *)(CObjCell *, CTransition *))v2->vfptr[6].IUnknown_QueryInterface)(v2, transition);
					return 2;
				}
			}
		}
	}
	*/
	// TODO
	//UNFINISHED();

	return OK_TS;
}

void CObjCell::find_transit_cells(const unsigned int num_parts, CPhysicsPart **parts, CELLARRAY *cell_array)
{
	// should be overridden
	assert(0);
}

void CObjCell::find_transit_cells(Position *p, const unsigned int num_sphere, CSphere *sphere, CELLARRAY *cell_array, SPHEREPATH *path)
{
	// should be overridden
	assert(0);
}

void CObjCell::find_cell_list(Position *p, const unsigned int num_sphere, CSphere *sphere, CELLARRAY *cell_array, CObjCell **curr_cell, SPHEREPATH *path)
{
	CObjCell *pCell = NULL;

	cell_array->num_cells = 0;
	cell_array->added_outside = 0;
	if (p->objcell_id)
	{
		if (((WORD)p->objcell_id) >= 0x100)
			pCell = CEnvCell::GetVisible(p->objcell_id);
		else
			pCell = CLandCell::Get(p->objcell_id);
	}

	if (((WORD)p->objcell_id) >= 0x100)
	{
		if (path)
			path->hits_interior_cell = 1;

		cell_array->add_cell(p->objcell_id, pCell);
	}
	else
	{
		CLandCell::add_all_outside_cells(p, num_sphere, sphere, cell_array);
	}
	
	if (pCell && num_sphere)
	{
		for (DWORD i = 0; i < cell_array->num_cells; i++)
		{
			CObjCell *otherCell = cell_array->cells.data[i].cell;
			if (otherCell)
				otherCell->find_transit_cells(p, num_sphere, sphere, cell_array, path);
		}

		if (curr_cell)
		{
			*curr_cell = NULL;

			for (DWORD i = 0; i < cell_array->num_cells; i++)
			{
				CObjCell *otherCell = cell_array->cells.data[i].cell;
				if (otherCell)
				{
					Vector blockOffset = LandDefs::get_block_offset(p->objcell_id, otherCell->id);
					Vector localpoint = sphere->center - blockOffset;

					if (otherCell->point_in_cell(localpoint))
					{
						*curr_cell = otherCell;
						if ((otherCell->id & 0xFFFF) >= 0x100)
						{
							if (path)
								path->hits_interior_cell = 1;
							return;
						}
					}
				}
			}

			// assert(*curr_cell);
		}

		if (cell_array->do_not_load_cells)
		{
			if ((p->objcell_id & 0xFFFF) >= 0x100)
			{
				for (DWORD i = 0; i < cell_array->num_cells; i++)
				{
					DWORD cellID = cell_array->cells.data[i].cell_id;

					if (pCell->id != cellID)
						continue;

					bool found = false;
					for (DWORD j = 0; j < pCell->num_stabs; j++)
					{
						if (cellID == pCell->stab_list[j])
						{
							found = true;
							break;
						}
					}
					if (!found)
						cell_array->remove_cell(i);
				}
			}
		}
	}
}

BOOL CObjCell::point_in_cell(const Vector &pt)
{
	return FALSE;
}

void CObjCell::find_cell_list(Position *p, unsigned int num_cylsphere, CCylSphere *cylsphere, CELLARRAY *cell_array, SPHEREPATH *path)
{
	static CSphere sphere[10];

	if (num_cylsphere > 10)
		num_cylsphere = 10;

	for (DWORD i = 0; i < num_cylsphere; i++)
	{
		sphere[i].center = p->localtoglobal(*p, cylsphere[i].low_pt);
		sphere[i].radius = cylsphere[i].radius;
	}

	CObjCell::find_cell_list(p, num_cylsphere, sphere, cell_array, 0, path);
}

void CObjCell::find_cell_list(Position *p, CSphere *sphere, CELLARRAY *cell_array, SPHEREPATH *path)
{
	CSphere global_sphere;

	global_sphere.center = p->localtoglobal(*p, sphere->center);
	global_sphere.radius = sphere->radius;

	CObjCell::find_cell_list(p, 1, &global_sphere, cell_array, 0, path);
}

void CObjCell::find_cell_list(CELLARRAY *cell_array, CObjCell **check_cell, SPHEREPATH *path)
{
	CObjCell::find_cell_list(&path->check_pos, path->num_sphere, path->global_sphere, cell_array, check_cell, path);
}

CObjCell *CObjCell::GetVisible(DWORD cell_id)
{
	if (cell_id)
	{
		if ((WORD)cell_id >= 0x100)
			return CEnvCell::GetVisible(cell_id);
		else
			return CLandCell::Get(cell_id);
	}

	return NULL;
}

int CObjCell::check_collisions(std::shared_ptr<CPhysicsObj> object)
{
	for (DWORD i = 0; i < num_shadow_objects; i++)
	{
		std::shared_ptr<CPhysicsObj> pobj = shadow_object_list[i]->physobj.lock();
		if (!pobj->parent.lock() && pobj != object && pobj->check_collision(object))
			return 1;
	}

	return 0;
}

void CObjCell::release_objects()
{
	while (num_shadow_objects)
	{
		CShadowObj *obj = shadow_object_list[0];
		remove_shadow_object(obj);

		if (std::shared_ptr<CPhysicsObj> pPhysObj = obj->physobj.lock())
		{
			pPhysObj->remove_parts(this);
		}
	}
	
	if (num_objects)
	{
		//UNFINISHED
		//if (CObjCell::obj_maint)
		//	CObjectMaint::ReleaseObjCell(CObjCell::obj_maint, this);
		// Instead of doing that.. do this for now

		std::list<std::shared_ptr<CPhysicsObj> > objs_to_leave;

		for (DWORD i = 0; i < num_objects; i++)
		{
			objs_to_leave.push_back(object_list[i]);
		}

		for (auto toLeave : objs_to_leave)
		{
			toLeave->leave_visibility();
		}
	}
}

LandDefs::WaterType CObjCell::get_block_water_type()
{
	if (myLandBlock_)
		return myLandBlock_->water_type;
	
	return LandDefs::WaterType::NOT_WATER;
}

double CObjCell::get_water_depth(Vector *point)
{
	if (water_type == LandDefs::WaterType::NOT_WATER)
		return 0.0;

	if (water_type != LandDefs::WaterType::PARTIALLY_WATER)
	{
		if (water_type == LandDefs::WaterType::ENTIRELY_WATER)
			return 0.89999998;

		return 0.0;
	}

	if (myLandBlock_)
		return myLandBlock_->calc_water_depth(GetID(), point);

	return 0.1;
}