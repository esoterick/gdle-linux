
#include "StdAfx.h"
#include "SmartArray.h"
#include "PhysicsObj.h"


template<class T>
BOOL SmartArray<T>::RemoveUnOrdered(T *pdata)
{
	for (long i = 0; i < num_used; i++)
	{
		if (array_data[i] == *pdata)
		{
			array_data[i] = array_data[num_used - 1];
			num_used--;
			return TRUE;
		}
	}

	return FALSE;
}

template<>
BOOL SmartArray<std::weak_ptr<CPhysicsObj>>::RemoveUnOrdered(std::weak_ptr<CPhysicsObj> *pdata)
{
	for (long i = 0; i < num_used; i++)
	{
		if (array_data[i].lock() == pdata->lock())
		{
			array_data[i] = array_data[num_used - 1];
			num_used--;
			return TRUE;
		}
	}

	return FALSE;
}
