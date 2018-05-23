
#pragma once

#include "SArray.h"

class CHILDLIST
{
public:
	CHILDLIST();
	~CHILDLIST();

	void add_child(std::shared_ptr<CPhysicsObj> pObj, Frame *pFrame, DWORD part_number, DWORD location_id);
	void remove_child(std::shared_ptr<CPhysicsObj> pChild);

	BOOL FindChildIndex(std::shared_ptr<CPhysicsObj> pObj, WORD *Index);

	WORD num_objects;
	SArray<std::shared_ptr<CPhysicsObj> > objects;
	SArray<Frame> frames;
	SArray<DWORD> part_numbers;
	SArray<DWORD> location_ids;
};

