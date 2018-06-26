
#include "StdAfx.h"
#include "ServerObjectMaint.h"
#include "PhysicsObj.h"
#include "World.h"

void CServerObjectMaint::GotoLostCell(class std::shared_ptr<CPhysicsObj> , unsigned long)
{
}

void CServerObjectMaint::RemoveFromLostCell(class std::shared_ptr<CPhysicsObj> )
{
}

void CServerObjectMaint::AddObjectToBeDestroyed(unsigned long)
{
}

void CServerObjectMaint::RemoveObjectToBeDestroyed(unsigned long)
{
}

std::shared_ptr<CPhysicsObj> CServerObjectMaint::GetObject(DWORD object_id)
{
	// TODO convert CPhysObject to shared pointers?
	return g_pWorld->FindObject(object_id);
}
