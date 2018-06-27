#pragma once

class CPhysicsObj;

class CServerObjectMaint
{
public:
	void GotoLostCell(std::shared_ptr<CPhysicsObj> , unsigned long);
	void RemoveFromLostCell(std::shared_ptr<CPhysicsObj> );
	void AddObjectToBeDestroyed(unsigned long);
	void RemoveObjectToBeDestroyed(unsigned long);
	std::shared_ptr<CPhysicsObj> GetObject(DWORD object_id);
};