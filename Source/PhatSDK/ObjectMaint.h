
#pragma once

class CPhysicsObj;
class CObjectInventory;

class CObjectMaint
{
public:
	CObjectMaint();

	void AddObject(std::shared_ptr<CPhysicsObj> object);
	void GotoLostCell(std::shared_ptr<CPhysicsObj> , unsigned long);
	void RemoveFromLostCell(std::shared_ptr<CPhysicsObj> );
	void AddObjectToBeDestroyed(unsigned long);
	void RemoveObjectToBeDestroyed(unsigned long);
	std::shared_ptr<CPhysicsObj> GetObject(DWORD object_id);

	// Turbine_RefCount m_cTurbineRefCount;
	// int is_active;
	std::map<DWORD, class CLostCell *> lost_cell_table; // IntrusiveHashTable<unsigned long, CLostCell *, 0>
	LongHash<std::shared_ptr<CPhysicsObj>> object_table;
	LongHash<std::shared_ptr<CPhysicsObj>> null_object_table;
	LongHash<CWeenieObject> weenie_object_table;
	LongHash<CWeenieObject> null_weenie_object_table;
	std::set<DWORD> visible_object_table; // HashSet<unsigned long>
	std::map<DWORD, double> destruction_object_table; // HashTable<unsigned long, double, 0>
	LongHash<CObjectInventory> object_inventory_table;
	std::queue<double> object_destruction_queue; // AC1Legacy::PQueueArray<double>
};