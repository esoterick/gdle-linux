
#include "StdAfx.h"
#include "Corpse.h"
#include "DatabaseIO.h"
#include "WorldLandBlock.h"

#define CORPSE_EXIST_TIME 300.0 // how long before a corpse disappears in seconds

CCorpseWeenie::CCorpseWeenie()
{
	_begin_destroy_at = Timer::cur_time + CORPSE_EXIST_TIME;
}

CCorpseWeenie::~CCorpseWeenie()
{
}

void CCorpseWeenie::ApplyQualityOverrides()
{
	CContainerWeenie::ApplyQualityOverrides();
}

void CCorpseWeenie::SetObjDesc(const ObjDesc &desc)
{
	_objDesc = desc;
}

void CCorpseWeenie::GetObjDesc(ObjDesc &desc)
{
	desc = _objDesc;
}

int CCorpseWeenie::CheckOpenContainer(std::shared_ptr<CWeenieObject> other)
{
	int error = CContainerWeenie::CheckOpenContainer(other);

	if (error != WERROR_NONE)
		return error;

	if (_begun_destroy)
		return WERROR_OBJECT_GONE;

	if (!_hasBeenOpened)
	{
		DWORD killerId = InqIIDQuality(KILLER_IID, 0);
		DWORD victimId = InqIIDQuality(VICTIM_IID, 0);
		if (killerId == other->GetID() || victimId == other->GetID())
		{
			return WERROR_NONE;
		}
		if (_begin_destroy_at - (CORPSE_EXIST_TIME/2) <= Timer::cur_time) // If the corpse hasn't been opened after half its life, open to anyone
		{
			return WERROR_NONE;
		}
		std::shared_ptr<CPlayerWeenie> owner = g_pWorld->FindPlayer(victimId);
		std::shared_ptr<CPlayerWeenie> looter = other->AsPlayer();
		bool killedByPK = m_Qualities.GetBool(PK_KILLER_BOOL, 0);
		if (owner && !killedByPK && looter) // Make sure we're both players & don't let corpse permissions work on PK kills
		{
			if (!owner->m_umCorpsePermissions.empty()) // if the corpse owner has players on their permissions list
			{
				if (owner->m_umCorpsePermissions.find(looter) != owner->m_umCorpsePermissions.end()) // if the looter is on the owners permissions list
				{
					owner->RemoveCorpsePermission(looter); // revoke permission now you've looted 1 corpse
					looter->RemoveConsent(owner); // remove from looter consent list
					return WERROR_NONE;
				}
			}
		}
		if (Fellowship *fellowship = other->GetFellowship())
		{
			if (fellowship->_share_loot)
			{
				for (auto &entry : fellowship->_fellowship_table)
				{
					if (killerId == entry.first)
						return WERROR_NONE;
				}

				for (auto &entry : fellowship->_fellows_departed)
				{
					if (killerId == entry.first)
						return WERROR_NONE;
				}
			}
		}
	}
	else
		return WERROR_NONE;

	other->SendText("You do not have permission to loot that corpse!", LTT_ERROR);

	return WERROR_CHEST_WRONG_KEY;
}

void CCorpseWeenie::OnContainerOpened(std::shared_ptr<CWeenieObject> other)
{
	CContainerWeenie::OnContainerOpened(other);

	_hasBeenOpened = true;
}

void CCorpseWeenie::OnContainerClosed(std::shared_ptr<CWeenieObject> other)
{
	CContainerWeenie::OnContainerClosed(other);

	if (!m_Items.size() && !m_Packs.size())
	{
		BeginGracefulDestroy();
	}
}

void CCorpseWeenie::SaveEx(class CWeenieSave &save)
{
	m_Qualities.SetFloat(TIME_TO_ROT_FLOAT, _begin_destroy_at - Timer::cur_time);
	CContainerWeenie::SaveEx(save);

	save.m_ObjDesc = _objDesc;

	g_pDBIO->AddOrUpdateWeenieToBlock(GetID(), m_Position.objcell_id >> 16);
}

void CCorpseWeenie::RemoveEx()
{
	g_pDBIO->RemoveWeenieFromBlock(GetID());
}

void CCorpseWeenie::LoadEx(class CWeenieSave &save)
{
	CContainerWeenie::LoadEx(save);

	_objDesc = save.m_ObjDesc;
	_begin_destroy_at = Timer::cur_time + m_Qualities.GetFloat(TIME_TO_ROT_FLOAT, 0.0);
	_shouldSave = true;
	m_bDontClear = true;

	InitPhysicsObj();
	MakeMovementManager(TRUE);
	MovementParameters params;
	params.autonomous = 0;
	last_move_was_autonomous = false;
	DoMotion(GetCommandID(17), &params, 0);
}

bool CCorpseWeenie::ShouldSave()
{
	return _shouldSave;
}

void CCorpseWeenie::Tick()
{
	CContainerWeenie::Tick();

	if (!_begun_destroy)
	{
		if (!_openedById && _begin_destroy_at <= Timer::cur_time)
		{
			BeginGracefulDestroy();
		}
	}
	else
	{
		if (_mark_for_destroy_at <= Timer::cur_time)
		{
			g_pWorld->RemoveEntity(AsWeenie());
		}
	}
}

void CCorpseWeenie::BeginGracefulDestroy()
{
	if (_begun_destroy)
	{
		return;
	}

	EmitEffect(PS_Destroy, 1.0f);

	// TODO drop inventory items on the ground

	_shouldSave = false; //we're on our way out, it's no longer necessary to save us to the database.
	RemoveEx(); // and in fact, delete entries in the db

	_mark_for_destroy_at = Timer::cur_time + 2.0;
	_begun_destroy = true;
}


