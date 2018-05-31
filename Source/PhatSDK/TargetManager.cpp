
#include "StdAfx.h"
#include "PhatSDK.h"
#include "TargetManager.h"

TargetManager::TargetManager(std::shared_ptr<CPhysicsObj> object)
{
	physobj = object;
	target_info = 0;
	voyeur_table = NULL;
	last_update_time = 0;
}

TargetManager::~TargetManager()
{
	if (target_info)
	{
		delete target_info;
		target_info = NULL;
	}

	if (voyeur_table)
	{
		delete voyeur_table;
	}
}

void TargetManager::SetTargetQuantum(double new_quantum)
{
	std::shared_ptr<CPhysicsObj> pPhysObj = physobj.lock();
	if (!pPhysObj)
	{
		return;
	}

	float quantum;

	if (target_info)
	{
		quantum = new_quantum;

		std::shared_ptr<CPhysicsObj> ptarget = CPhysicsObj::GetObject(target_info->object_id);

		if (ptarget)
		{
			quantum = target_info->quantum;
			ptarget->add_voyeur(pPhysObj->id, target_info->radius, quantum);
		}
	}
}

void TargetManager::AddVoyeur(DWORD object_id, float radius, double quantum)
{
	std::shared_ptr<CPhysicsObj> pPhysObj = physobj.lock();
	if (!pPhysObj)
	{
		return;
	}

	if (voyeur_table)
	{
		TargettedVoyeurInfo *existing_info = voyeur_table->lookup(object_id);

		if (existing_info)
		{
			existing_info->radius = radius;
			existing_info->quantum = quantum;
			return;
		}
	}
	else
	{
		voyeur_table = new LongNIHash<TargettedVoyeurInfo>(4);
	}

	TargettedVoyeurInfo *info = new TargettedVoyeurInfo;
	info->object_id = object_id;
	info->radius = radius;
	info->quantum = quantum;
	voyeur_table->add(info, object_id);

	SendVoyeurUpdate(info, &pPhysObj->m_Position, Ok_TargetStatus);
}

void TargetManager::SendVoyeurUpdate(TargettedVoyeurInfo *voyeur, Position *p, TargetStatus status)
{
	std::shared_ptr<CPhysicsObj> pPhysObj = physobj.lock();
	if (!pPhysObj)
	{
		return;
	}
	voyeur->last_sent_position = *p;

	TargetInfo info;
	info.context_id = 0;
	info.object_id = pPhysObj->id;
	info.quantum = voyeur->quantum;
	info.radius = voyeur->radius;
	info.target_position = pPhysObj->m_Position;
	info.interpolated_position = *p;	
	info.velocity = pPhysObj->get_velocity();
	info.status = status;

	std::shared_ptr<CPhysicsObj> voyObj = CPhysicsObj::GetObject(voyeur->object_id);
	if (voyObj)
		voyObj->receive_target_update(&info);
}

BOOL TargetManager::RemoveVoyeur(DWORD object_id)
{
	if (voyeur_table)
	{
		TargettedVoyeurInfo *info = voyeur_table->remove(object_id);

		if (info)
		{
			delete info;
			return TRUE;
		}
	}

	return FALSE;
}

void TargetManager::ReceiveUpdate(TargetInfo *target_update)
{
	std::shared_ptr<CPhysicsObj> pPhysObj = physobj.lock();
	if (!pPhysObj)
	{
		return;
	}

	if (target_info)
	{
		if (target_update->object_id == target_info->object_id)
		{
			*target_info = *target_update;
			target_info->last_update_time = Timer::cur_time;

			target_info->interpolated_heading = pPhysObj->m_Position.get_offset(target_info->interpolated_position);
			

			if (target_info->interpolated_heading.normalize_check_small())
				target_info->interpolated_heading = Vector(0, 0, 1.0f);

			pPhysObj->HandleUpdateTarget(TargetInfo(*target_info));

			if (target_update->status == ExitWorld_TargetStatus)
				ClearTarget();
		}
	}
}

void TargetManager::ClearTarget()
{
	std::shared_ptr<CPhysicsObj> pPhysObj = physobj.lock();
	if (!pPhysObj)
	{
		return;
	}
	if (target_info)
	{
		std::shared_ptr<CPhysicsObj> targetObj = CPhysicsObj::GetObjectA(target_info->object_id);
		if (targetObj)
			targetObj->remove_voyeur(pPhysObj->id);

		if (target_info)
			delete target_info;
	
		target_info = NULL;
	}
}

void TargetManager::SetTarget(DWORD context_id, DWORD object_id, float radius, double quantum)
{
	std::shared_ptr<CPhysicsObj> pPhysObj = physobj.lock();
	if (!pPhysObj)
	{
		return;
	}
	ClearTarget();

	if (object_id)
	{
		target_info = new TargetInfo();

		target_info->context_id = context_id;
		target_info->object_id = object_id;
		target_info->radius = radius;
		target_info->quantum = quantum;
		target_info->last_update_time = Timer::cur_time;

		std::shared_ptr<CPhysicsObj> ptarget = CPhysicsObj::GetObject(target_info->object_id);
		if (ptarget)
			ptarget->add_voyeur(pPhysObj->id, target_info->radius, target_info->quantum);
	}
	else
	{
		TargetInfo failed_target_info;
		failed_target_info.context_id = context_id;
		failed_target_info.object_id = 0;
		failed_target_info.status = TimedOut_TargetStatus;
		pPhysObj->HandleUpdateTarget(TargetInfo(failed_target_info));
	}
}

void TargetManager::HandleTargetting()
{
	std::shared_ptr<CPhysicsObj> pPhysObj = physobj.lock();
	if (!pPhysObj)
	{
		return;
	}
	TargetInfo v5;

	if ((PhysicsTimer::curr_time - last_update_time) >= 0.5)
	{
		if (target_info && target_info->status == Undef_TargetStatus && target_info->last_update_time + 10.0 < Timer::cur_time)
		{
			target_info->status = TimedOut_TargetStatus;
			pPhysObj->HandleUpdateTarget(TargetInfo(*target_info));
		}

		if (voyeur_table)
		{
			LongNIHashIter<TargettedVoyeurInfo> iter(voyeur_table);

			while (!iter.EndReached())
			{
				try
				{

					TargettedVoyeurInfo *pVoyeurInfo = iter.GetCurrentData();

					iter.Next();

					CheckAndUpdateVoyeur(pVoyeurInfo);
				}
				catch (...)
				{
					SERVER_ERROR << "Error in targetting";
				}
			}
		}

		last_update_time = PhysicsTimer::curr_time;
	}
}

void TargetManager::CheckAndUpdateVoyeur(TargettedVoyeurInfo *voyeur)
{
	Position p;
	GetInterpolatedPosition(voyeur->quantum, &p);

	if (p.distance(voyeur->last_sent_position) > voyeur->radius)
		SendVoyeurUpdate(voyeur, &p, Ok_TargetStatus);
}

void TargetManager::GetInterpolatedPosition(double quantum, Position *p)
{
	std::shared_ptr<CPhysicsObj> pPhysObj = physobj.lock();
	if (!pPhysObj)
	{
		return;
	}

	*p = pPhysObj->m_Position;
	p->frame.m_origin += pPhysObj->get_velocity() * quantum;
}

void TargetManager::NotifyVoyeurOfEvent(TargetStatus _event)
{
	std::shared_ptr<CPhysicsObj> pPhysObj = physobj.lock();
	if (!pPhysObj)
	{
		return;
	}

	if (voyeur_table)
	{
		LongNIHashIter<TargettedVoyeurInfo> iter(voyeur_table);

		while (!iter.EndReached())
		{
			try
			{
				TargettedVoyeurInfo *pInfo = iter.GetCurrentData();
				iter.Next();

				SendVoyeurUpdate(pInfo, &pPhysObj->m_Position, _event);
			}
			catch(...)
			{
				SERVER_ERROR << "Error in TargetManager.";
			}
		}
	}
}

