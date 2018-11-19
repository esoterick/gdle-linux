#include "StdAfx.h"
#include "AttackManager.h"
#include "WeenieObject.h"
#include "World.h"
#include "Player.h"
#include "WeenieFactory.h"
#include "Ammunition.h"
#include "CombatFormulas.h"

#include "combat/MeleeAttackEventData.h"
#include "combat/DualWieldAttackEventData.h"
#include "combat/MissileAttackEventData.h"

// TODO fix memory leak with attack data

AttackManager::AttackManager(CWeenieObject *weenie)
{
	_weenie = weenie;
}

AttackManager::~AttackManager()
{
	SafeDelete(_attackData);
	SafeDelete(_cleanupData);
	SafeDelete(_queuedAttackData);
}

void AttackManager::MarkForCleanup(CAttackEventData *data)
{
	if (_cleanupData && _cleanupData != data)
	{
		delete _cleanupData;
	}

	_cleanupData = data;
}

void AttackManager::Cancel()
{
	if (_attackData)
		_attackData->Cancel();
}

void AttackManager::OnAttackCancelled(DWORD error)
{
	if (_attackData)
	{
		_weenie->NotifyAttackDone();
		_weenie->DoForcedMotion(_weenie->get_minterp()->InqStyle());
		_weenie->unstick_from_object();

		MarkForCleanup(_attackData);
		_attackData = NULL;
	}
}

bool AttackManager::RepeatAttacks()
{
	if (_weenie->AsPlayer())
	{
		CPlayerWeenie *player = (CPlayerWeenie *)_weenie;
		return player->ShouldRepeatAttacks();
	}

	return false;
}

void AttackManager::OnAttackDone(DWORD error)
{
	//if(_weenie->_blockNewAttacksUntil < Timer::cur_time) //fix for cancelling reload animation making attacking faster 
		//_weenie->_blockNewAttacksUntil = Timer::cur_time + 1.0;
	if (_attackData)
	{
		if (RepeatAttacks() && _attackData->IsValidTarget())
		{

			if (_queuedAttackData != NULL)
			{
				//we have a queued attack, change to that.
				SafeDelete(_attackData);
				_attackData = _queuedAttackData;
				_queuedAttackData = NULL;
			}

			if (!_attackData->AsMissileAttackEvent())
			{
				_weenie->NotifyAttackDone();
			}

			//_attackData->_attack_charge_time = Timer::cur_time + (_attackData->_attack_power);
			_attackData->Begin();
		}
		else
		{
			_weenie->NotifyAttackDone();

			MarkForCleanup(_attackData);
			_attackData = NULL;
		}
	}
}

void AttackManager::Update()
{
	if (_attackData)
	{
		_attackData->Update();
	}

	SafeDelete(_cleanupData);
}

void AttackManager::OnDeath(DWORD killer_id)
{
	Cancel();
}

void AttackManager::HandleMoveToDone(DWORD error)
{
	if (_attackData)
	{
		_attackData->HandleMoveToDone(error);
	}
}

void AttackManager::HandleAttackHook(const AttackCone &cone)
{
	if (_attackData)
	{
		_attackData->HandleAttackHook(cone);
	}
}

void AttackManager::OnMotionDone(DWORD motion, BOOL success)
{
	if (_attackData)
	{
		_attackData->OnMotionDone(motion, success);

		if (motion == Motion_Reload)
		{
			_weenie->NotifyAttackDone();
		}
	}
}

bool AttackManager::IsAttacking()
{
	return _attackData != NULL ? true : false;
}

void AttackManager::BeginAttack(CAttackEventData *data)
{
	if (_attackData != NULL)
	{
		//we're already attacking, queue this, or change current queued attack to this.
		SafeDelete(_queuedAttackData);
		_queuedAttackData = data;
	}
	else
	{
		_attackData = data;
		_attackData->Begin();
	}
}

void AttackManager::BeginMeleeAttack(DWORD target_id, ATTACK_HEIGHT height, float power, float chase_distance, DWORD motion)
{
	CMeleeAttackEvent *attackEvent = nullptr;

	if (_weenie->GetWieldedCombat(COMBAT_USE_TWO_HANDED))
		attackEvent = new CTwoHandAttackEvent();
	else if (_weenie->GetWieldedCombat(COMBAT_USE_OFFHAND))
		attackEvent = new CDualWieldAttackEvent();
	else
		attackEvent = new CMeleeAttackEvent();

	attackEvent->_weenie = _weenie;
	attackEvent->_manager = this;
	attackEvent->_target_id = target_id;
	attackEvent->_attack_height = height;
	attackEvent->_attack_power = power;
	attackEvent->_do_attack_animation = motion;
	attackEvent->_fail_distance = chase_distance;

	BeginAttack(attackEvent);
}

void AttackManager::BeginMissileAttack(DWORD target_id, ATTACK_HEIGHT height, float power, DWORD motion)
{
	CMissileAttackEvent *attackEvent = new CMissileAttackEvent();

	attackEvent->_weenie = _weenie;
	attackEvent->_manager = this;
	attackEvent->_target_id = target_id;
	attackEvent->_attack_height = height;
	attackEvent->_attack_power = power;
	attackEvent->_do_attack_animation = motion;

	BeginAttack(attackEvent);
}
