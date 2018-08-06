
#include "StdAfx.h"
#include "AllegianceManager.h"
#include "World.h"
#include "Player.h"
#include "DatabaseIO.h"
#include "ChatMsgs.h"
#include "InferredPortalData.h"
#include "Util.h"

#define RELEASE_ASSERT(x) if (!(x)) DebugBreak();

DEFINE_PACK(AllegianceInfo)
{
	_info.Pack(pWriter);
}

DEFINE_UNPACK(AllegianceInfo)
{
	_info.UnPack(pReader);
	return true;
}

AllegianceTreeNode::AllegianceTreeNode()
{
}

AllegianceTreeNode::~AllegianceTreeNode()
{
	for (auto &entry : _vassals)
		delete entry.second;
	_vassals.clear();
}

AllegianceTreeNode *AllegianceTreeNode::FindCharByNameRecursivelySlow(const std::string &charName)
{
	const char *targetName = charName.c_str();
	if (targetName[0] == '+')
		targetName++;

	const char *compareName = _charName.c_str();
	if (compareName[0] == '+')
		compareName++;

	if (!_stricmp(targetName, compareName))
	{
		return this;
	}

	AllegianceTreeNode *node = nullptr;
	for (auto &entry : _vassals)
	{
		if (node = entry.second->FindCharByNameRecursivelySlow(charName))
			break;
	}

	return node;
}

void AllegianceTreeNode::FillAllegianceNode(AllegianceNode *node)
{
	node->_data._rank = _rank;
	node->_data._level = _level;
	node->_data._cp_cached = min(4294967295, _cp_cached);
	node->_data._cp_tithed = min(4294967295, _cp_tithed);
	node->_data._gender = _gender;
	node->_data._hg = _hg;
	node->_data._leadership = _leadership;
	node->_data._loyalty = _loyalty;
	node->_data._name = _charName;
	node->_data._id = _charID;
//	node->_data._allegiance_age = _unixTimeSwornAt; what unit is this and what does the client do with this information?
//	node->_data._time_online = _ingameSecondsSworn; what unit is this and what does the client do with this information?
}

void AllegianceTreeNode::UpdateWithWeenie(CWeenieObject *weenie)
{
	_charID = weenie->GetID();
	_charName = weenie->GetName();	
	_hg = static_cast<HeritageGroup>(weenie->InqIntQuality(HERITAGE_GROUP_INT, Invalid_HeritageGroup));
	_gender = static_cast<Gender>(weenie->InqIntQuality(GENDER_INT, Invalid_Gender));
	_level = weenie->InqIntQuality(LEVEL_INT, 1);
	_leadership = 0;
	weenie->m_Qualities.InqSkill(LEADERSHIP_SKILL, *reinterpret_cast<DWORD *>(&_leadership), FALSE);
	_loyalty = 0;
	weenie->m_Qualities.InqSkill(LOYALTY_SKILL, *reinterpret_cast<DWORD *>(&_loyalty), FALSE);
}

DEFINE_PACK(AllegianceTreeNode)
{
	pWriter->Write<DWORD>(2); // version
	pWriter->Write<DWORD>(_charID);
	pWriter->WriteString(_charName);

	pWriter->Write<DWORD>(_monarchID);
	pWriter->Write<DWORD>(_patronID);
	pWriter->Write<int>(_hg);
	pWriter->Write<int>(_gender);
	pWriter->Write<DWORD>(_rank);
	pWriter->Write<DWORD>(_level);
	pWriter->Write<DWORD>(_leadership);
	pWriter->Write<DWORD>(_loyalty);
	pWriter->Write<DWORD>(_numFollowers);
	pWriter->Write<DWORD64>(_cp_cached);
	pWriter->Write<DWORD64>(_cp_tithed);
	pWriter->Write<DWORD64>(_cp_pool_to_unload);
	pWriter->Write<DWORD64>(_unixTimeSwornAt);
	pWriter->Write<DWORD64>(_ingameSecondsSworn);

	pWriter->Write<DWORD>(_vassals.size());
	for (auto &entry : _vassals)
		entry.second->Pack(pWriter);
}

DEFINE_UNPACK(AllegianceTreeNode)
{
	DWORD version = pReader->Read<DWORD>(); // version
	_charID = pReader->Read<DWORD>();
	_charName = pReader->ReadString();

	_monarchID = pReader->Read<DWORD>();
	_patronID = pReader->Read<DWORD>();
	_hg = (HeritageGroup)pReader->Read<int>();
	_gender = (Gender)pReader->Read<int>();
	_rank = pReader->Read<DWORD>();
	_level = pReader->Read<DWORD>();
	_leadership = pReader->Read<DWORD>();
	_loyalty = pReader->Read<DWORD>();
	_numFollowers = pReader->Read<DWORD>();
	_cp_cached = pReader->Read<DWORD64>();
	_cp_tithed = pReader->Read<DWORD64>();
	_cp_pool_to_unload = pReader->Read<DWORD64>();
	if (version < 2) {
		_unixTimeSwornAt = time(0); // set to now
		_ingameSecondsSworn = 1;
	} else {
		_unixTimeSwornAt = pReader->Read<DWORD64>();
		_ingameSecondsSworn = pReader->Read<DWORD64>();
	}

	DWORD numVassals = pReader->Read<DWORD>();
	for (DWORD i = 0; i < numVassals; i++)
	{
		AllegianceTreeNode *node = new AllegianceTreeNode();
		node->UnPack(pReader);
		_vassals[node->_charID] = node;

		assert(node->_charID >= 0x50000000 && node->_charID < 0x70000000);
	}

	assert(_charID >= 0x50000000 && _charID < 0x70000000);
	return true;
}

AllegianceManager::AllegianceManager()
{
	Load();
}

AllegianceManager::~AllegianceManager()
{
	Save();

	for (auto &entry : _monarchs)
		delete entry.second;
	_monarchs.clear();
	_directNodes.clear();

	for (auto &entry : _allegInfos)
		delete entry.second;
	_allegInfos.clear();
}

void AllegianceManager::Load()
{
	void *data = NULL;
	DWORD length = 0;
	if (g_pDBIO->GetGlobalData(DBIO_GLOBAL_ALLEGIANCE_DATA, &data, &length))
	{
		BinaryReader reader(data, length);
		UnPack(&reader);
	}

	m_LastSave = Timer::cur_time;
}

void AllegianceManager::Save()
{
	BinaryWriter data;
	Pack(&data);
	g_pDBIO->CreateOrUpdateGlobalData(DBIO_GLOBAL_ALLEGIANCE_DATA, data.GetData(), data.GetSize());
}

void AllegianceManager::WalkTreeAndBumpOnlineTime(AllegianceTreeNode *node, int onlineSecondsDelta) {
	// Walk allegiance tree and bump _ingameSecondsSworn for all online characters. 
	// There's probably a better way of maintaining number of seconds of online-time.
	for (auto &entry : node->_vassals) {
		AllegianceTreeNode *vassal = entry.second;
		CPlayerWeenie *target = g_pWorld->FindPlayer(vassal->_charID);
		if (target)
			vassal->_ingameSecondsSworn += onlineSecondsDelta;
		WalkTreeAndBumpOnlineTime(vassal, onlineSecondsDelta);
	}
}

void AllegianceManager::Tick()
{
	if (m_LastSave <= (Timer::cur_time - 300.0)) // every 5 minutes save
	{
		for (auto &entry : _monarchs)
			WalkTreeAndBumpOnlineTime(entry.second, ((int)round(Timer::cur_time - m_LastSave)));
		Save();
		m_LastSave = Timer::cur_time;
	}
}

DEFINE_PACK(AllegianceManager)
{
	pWriter->Write<DWORD>(1); // version
	pWriter->Write<DWORD>(_monarchs.size());

	for (auto &entry : _monarchs)
		entry.second->Pack(pWriter);

	pWriter->Write<DWORD>(_allegInfos.size());

	for (auto &entry : _allegInfos)
	{
		pWriter->Write<DWORD>(entry.first);
		entry.second->Pack(pWriter);
	}
}

DEFINE_UNPACK(AllegianceManager)
{
	DWORD version = pReader->Read<DWORD>();
	DWORD numMonarchs = pReader->Read<DWORD>();

	for (DWORD i = 0; i < numMonarchs; i++)
	{
		AllegianceTreeNode *node = new AllegianceTreeNode();
		node->UnPack(pReader);
		_monarchs[node->_charID] = node;
		CacheInitialDataRecursively(node, NULL);
	}

	DWORD numInfos = pReader->Read<DWORD>();

	for (DWORD i = 0; i < numInfos; i++)
	{
		DWORD monarchID = pReader->Read<DWORD>();

		AllegianceInfo *info = new AllegianceInfo();
		info->UnPack(pReader);
		_allegInfos[monarchID] = info;
	}

	return true;
}

void AllegianceManager::CacheInitialDataRecursively(AllegianceTreeNode *node, AllegianceTreeNode *parent)
{
	if (!node)
		return;

	_directNodes[node->_charID] = node;

	node->_monarchID = parent ? parent->_monarchID : node->_charID;
	node->_patronID = parent ? parent->_charID : 0;
	node->_rank = 1;
	node->_numFollowers = 0;

	unsigned int highestVassalRank = 0;
	bool bRankUp = false;

	for (auto &entry : node->_vassals)
	{
		AllegianceTreeNode *vassal = entry.second;
		CacheInitialDataRecursively(vassal, node);

		if (vassal->_rank > highestVassalRank)
		{
			highestVassalRank = vassal->_rank;
			bRankUp = false;
		}
		else if (vassal->_rank == highestVassalRank)
		{
			bRankUp = true;
		}

		node->_numFollowers += vassal->_numFollowers + 1;
	}

	if (!highestVassalRank)
	{
		node->_rank = 1;
	}
	else
	{
		node->_rank = highestVassalRank;
		if (bRankUp)
			node->_rank++;
	}
}

void AllegianceManager::CacheDataRecursively(AllegianceTreeNode *node, AllegianceTreeNode *parent)
{
	if (!node)
		return;

	node->_monarchID = parent ? parent->_monarchID : node->_charID;
	node->_patronID = parent ? parent->_charID : 0;
	node->_rank = 1;
	node->_numFollowers = 0;

	unsigned int highestVassalRank = 0;
	bool bRankUp = false;

	for (auto &entry : node->_vassals)
	{
		AllegianceTreeNode *vassal = entry.second;
		CacheDataRecursively(vassal, node);

		if (vassal->_rank > highestVassalRank)
		{
			highestVassalRank = vassal->_rank;
			bRankUp = false;
		}
		else if (vassal->_rank == highestVassalRank)
		{
			bRankUp = true;
		}

		node->_numFollowers += vassal->_numFollowers + 1;
	}

	if (!highestVassalRank)
	{
		node->_rank = 1;
	}
	else
	{
		node->_rank = highestVassalRank;
		if (bRankUp)
			node->_rank++;
	}
}


void AllegianceManager::NotifyTreeRefreshRecursively(AllegianceTreeNode *node)
{
	if (!node)
		return;

	CWeenieObject *weenie = g_pWorld->FindPlayer(node->_charID);
	if (weenie)
	{
		/*
		unsigned int rank = 0;
		AllegianceProfile *prof = CreateAllegianceProfile(weenie, &rank);

		BinaryWriter allegianceUpdate;
		allegianceUpdate.Write<DWORD>(0x20);
		allegianceUpdate.Write<DWORD>(rank);
		prof->Pack(&allegianceUpdate);
		weenie->SendNetMessage(&allegianceUpdate, PRIVATE_MSG, TRUE, FALSE);
		delete prof;
		*/
		SetWeenieAllegianceQualities(weenie);
	}

	for (auto &entry : node->_vassals)
		NotifyTreeRefreshRecursively(entry.second);
}

AllegianceTreeNode *AllegianceManager::GetTreeNode(DWORD charID)
{
	auto i = _directNodes.find(charID);
	if (i != _directNodes.end())
		return i->second;

	return NULL;
}

AllegianceInfo *AllegianceManager::GetInfo(DWORD monarchID)
{
	auto i = _allegInfos.find(monarchID);
	if (i != _allegInfos.end())
		return i->second;

	return NULL;
}

void AllegianceManager::SetWeenieAllegianceQualities(CWeenieObject *weenie)
{
	if (!weenie)
		return;
	
	AllegianceTreeNode *monarch;
	AllegianceTreeNode *patron;

	AllegianceTreeNode *node = GetTreeNode(weenie->GetID());
	if (node)
	{
		monarch = GetTreeNode(node->_monarchID);
		patron = GetTreeNode(node->_patronID);

		weenie->m_Qualities.SetInt(ALLEGIANCE_FOLLOWERS_INT, node->_numFollowers);
		weenie->m_Qualities.SetInt(ALLEGIANCE_RANK_INT, node->_rank);
		// wrong weenie->m_Qualities.SetInt(ALLEGIANCE_CP_POOL_INT, node->_cp_cached);
	}
	else
	{
		monarch = NULL;
		patron = NULL;

		weenie->m_Qualities.RemoveInt(ALLEGIANCE_FOLLOWERS_INT);
		weenie->m_Qualities.RemoveInt(ALLEGIANCE_RANK_INT);
		// wrong weenie->m_Qualities.RemoveInt(ALLEGIANCE_CP_POOL_INT);
	}

	if (monarch)
	{
		weenie->m_Qualities.SetInstanceID(MONARCH_IID, monarch->_charID);
		weenie->m_Qualities.SetInt(MONARCHS_RANK_INT, monarch->_rank);

		if (monarch->_charID != weenie->GetID())
		{
			weenie->m_Qualities.SetString(MONARCHS_NAME_STRING, monarch->_charName);
			weenie->m_Qualities.SetString(MONARCHS_TITLE_STRING, monarch->_charName); // TODO prefix
		}
		else
		{
			weenie->m_Qualities.RemoveString(MONARCHS_NAME_STRING);
			weenie->m_Qualities.RemoveString(MONARCHS_TITLE_STRING);
		}
	}
	else
	{
		weenie->m_Qualities.RemoveInstanceID(MONARCH_IID);
		weenie->m_Qualities.RemoveInt(MONARCHS_RANK_INT);
		weenie->m_Qualities.RemoveString(MONARCHS_NAME_STRING);
		weenie->m_Qualities.RemoveString(MONARCHS_TITLE_STRING);
	}

	if (patron)
	{
		weenie->m_Qualities.SetInstanceID(PATRON_IID, patron->_charID);
		weenie->m_Qualities.SetString(PATRONS_TITLE_STRING, patron->_charName); // TODO prefix
	}
	else
	{
		weenie->m_Qualities.RemoveInstanceID(PATRON_IID);
		weenie->m_Qualities.RemoveString(PATRONS_TITLE_STRING);
	}
}

AllegianceProfile *AllegianceManager::CreateAllegianceProfile(DWORD char_id, unsigned int *pRank)
{
	AllegianceProfile *prof = new AllegianceProfile;
	*pRank = 0;

	AllegianceTreeNode *node = GetTreeNode(char_id);
	if (node)
	{
		*pRank = node->_rank;

		AllegianceTreeNode *monarch = GetTreeNode(node->_monarchID);
		AllegianceTreeNode *patron = GetTreeNode(node->_patronID);
		AllegianceInfo *info = GetInfo(node->_monarchID);

		RELEASE_ASSERT(monarch);
		if (monarch)
			prof->_total_members = monarch->_numFollowers;

		prof->_total_vassals = node->_numFollowers;

		if (!info)
		{
			info = new AllegianceInfo();
			_allegInfos[node->_monarchID] = info;
		}

		if (info)
		{
			RELEASE_ASSERT(!info->_info._nodes.size());
			prof->_allegiance = info->_info;
		}

		if (monarch)
		{
			AllegianceNode *patronNode = NULL;

			if (monarch != node)
			{
				AllegianceNode *monarchNode = new AllegianceNode;
				monarch->FillAllegianceNode(monarchNode);

				if (g_pWorld->FindPlayer(monarch->_charID))
					monarchNode->_data._bitfield |= LoggedIn_AllegianceIndex;

				prof->_allegiance._nodes.push_back(monarchNode);

				patronNode = monarchNode;
				if (patron && patron != monarch)
				{
					patronNode = new AllegianceNode;
					patron->FillAllegianceNode(patronNode);
					patronNode->_patron = monarchNode;

					if (g_pWorld->FindPlayer(patron->_charID))
						patronNode->_data._bitfield |= LoggedIn_AllegianceIndex;

					prof->_allegiance._nodes.push_back(patronNode);
				}
			}

			AllegianceNode *selfNode = new AllegianceNode;
			node->FillAllegianceNode(selfNode);
			selfNode->_patron = patronNode;
			selfNode->_data._bitfield |= LoggedIn_AllegianceIndex;
			prof->_allegiance._nodes.push_back(selfNode);

			for (auto &entry : node->_vassals)
			{
				AllegianceTreeNode *vassal = entry.second;

				AllegianceNode *vassalNode = new AllegianceNode;
				vassal->FillAllegianceNode(vassalNode);
				vassalNode->_patron = selfNode;

				if (g_pWorld->FindPlayer(vassal->_charID))
					vassalNode->_data._bitfield |= LoggedIn_AllegianceIndex;
				
				prof->_allegiance._nodes.push_back(vassalNode);
			}
		}
	}

	return prof;
}

const unsigned int MAX_DIRECT_VASSALS = 11;

void AllegianceManager::TrySwearAllegiance(CWeenieObject *source, CWeenieObject *target)
{
	if (source->IsBusyOrInAction())
	{
		source->NotifyWeenieError(WERROR_ACTIONS_LOCKED);
		return;
	}
	if (target->IsBusyOrInAction())
	{
		source->NotifyWeenieError(WERROR_ALLEGIANCE_PATRON_BUSY);
		return;
	}
	if (source->DistanceTo(target) >= 4.0)
	{
		source->NotifyWeenieError(WERROR_MISSILE_OUT_OF_RANGE);
		return;
	}
	if (source->GetID() == target->GetID())
		return;

	if (CPlayerWeenie *player = target->AsPlayer())
	{
		if (player->GetCharacterOptions() & IgnoreAllegianceRequests_CharacterOption)
		{
			source->NotifyWeenieError(WERROR_ALLEGIANCE_IGNORING_REQUESTS);
			return;
		}
	}

	if (source->InqIntQuality(LEVEL_INT, 0) > target->InqIntQuality(LEVEL_INT, 0))
	{
		source->NotifyWeenieError(WERROR_ALLEGIANCE_ILLEGAL_LEVEL);
		return;
	}

	AllegianceTreeNode *selfTreeNode = GetTreeNode(source->GetID());
	if (selfTreeNode && selfTreeNode->_patronID)
	{
		// already sworn to someone
		source->NotifyWeenieError(WERROR_ALLEGIANCE_PATRON_EXISTS);
		if (selfTreeNode->_patronID == target->GetID())
		{
			target->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_ADD_HIERARCHY_FAILURE, source->GetName().c_str());
		}
		return;
	}

	AllegianceTreeNode *targetTreeNode = GetTreeNode(target->GetID());
	if (targetTreeNode)
	{
		if (IsBanned(source->GetID(), targetTreeNode->_monarchID))
		{
			target->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_BANNED, source->GetName().c_str());
			return;
		}
		AllegianceInfo* ai = GetInfo(targetTreeNode->_monarchID);
		if (ai && ai->_info.m_isLocked && (source->GetID() != ai->_info.m_ApprovedVassal)) // bypass the lock if it's the approved vassal
		{
			source->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_LOCK_PREVENTS_VASSAL, target->GetName().c_str());
			target->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_LOCK_PREVENTS_PATRON, source->GetName().c_str());
			return;
		}

		if (selfTreeNode && selfTreeNode->_charID == targetTreeNode->_monarchID)
		{
			// Clearly he doesn't have updated data.
			SendAllegianceProfile(source);

			return;
		}

		if (targetTreeNode->_vassals.size() >= MAX_DIRECT_VASSALS)
		{
			// too many vassals
			target->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_MAX_VASSALS, "You");
			source->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_MAX_VASSALS, target->GetName().c_str());
			return;
		}
	}

	if (!targetTreeNode)
	{
		// target wasn't in an allegiance already

		// make one...
		targetTreeNode = new AllegianceTreeNode();
		targetTreeNode->_monarchID = target->GetID();
		_directNodes[target->GetID()] = targetTreeNode;
		_monarchs[target->GetID()] = targetTreeNode;

		AllegianceInfo *info = new AllegianceInfo();
		_allegInfos[target->GetID()] = info;
	}
	targetTreeNode->UpdateWithWeenie(target);

	if (selfTreeNode)
	{
		// must be a current monarch
		_monarchs.erase(source->GetID());

		auto i = _allegInfos.find(source->GetID());
		if (i != _allegInfos.end())
		{
			delete i->second;
			_allegInfos.erase(i);
		}
	}
	else
	{
		selfTreeNode = new AllegianceTreeNode();
		_directNodes[source->GetID()] = selfTreeNode;
	}

	selfTreeNode->UpdateWithWeenie(source);
	selfTreeNode->_rank = 1;
	selfTreeNode->_numFollowers = 0;
	selfTreeNode->_patronID = targetTreeNode->_charID;
	selfTreeNode->_monarchID = targetTreeNode->_monarchID;
	selfTreeNode->_unixTimeSwornAt = time(0);
	selfTreeNode->_ingameSecondsSworn = 1;

	targetTreeNode->_vassals[selfTreeNode->_charID] = selfTreeNode;

	// not efficient, can revise later
	RELEASE_ASSERT(_monarchs.find(targetTreeNode->_monarchID) != _monarchs.end());

	AllegianceTreeNode *monarchTreeNode = _monarchs[targetTreeNode->_monarchID];
	CacheDataRecursively(monarchTreeNode, NULL);
	NotifyTreeRefreshRecursively(monarchTreeNode);

	source->SendText(csprintf("%s has accepted your oath of Allegiance!", target->GetName().c_str()), LTT_DEFAULT);
	target->SendText(csprintf("%s has sworn Allegiance to you.", source->GetName().c_str()), LTT_DEFAULT);

	source->DoForcedMotion(Motion_Kneel);

	Save();
}

bool AllegianceManager::ShouldRemoveAllegianceNode(AllegianceTreeNode *node)
{
	if (!node->_patronID && !node->_vassals.size())
		return true;

	return false;
}

void AllegianceManager::RemoveAllegianceNode(AllegianceTreeNode *node)
{
	_monarchs.erase(node->_charID);
	_directNodes.erase(node->_charID);

	auto allegInfoEntry = _allegInfos.find(node->_charID);
	if (allegInfoEntry != _allegInfos.end())
	{
		delete allegInfoEntry->second;
		_allegInfos.erase(allegInfoEntry);
	}

	delete node;
}

void AllegianceManager::BreakAllegiance(AllegianceTreeNode *patronNode, AllegianceTreeNode *vassalNode)
{
	// the target is a vassal
	patronNode->_vassals.erase(vassalNode->_charID);

	vassalNode->_monarchID = vassalNode->_charID;
	vassalNode->_patronID = 0;

	_monarchs[vassalNode->_charID] = vassalNode;

	AllegianceInfo *info = new AllegianceInfo();
	_allegInfos[vassalNode->_charID] = info;

	{
		RELEASE_ASSERT(_monarchs.find(patronNode->_monarchID) != _monarchs.end());

		AllegianceTreeNode *patronMonarchNode = _monarchs[patronNode->_monarchID];
		CacheDataRecursively(patronMonarchNode, NULL);
		NotifyTreeRefreshRecursively(patronMonarchNode);

		// check if i should even have allegiance data anymore (no vassals/patron)
		if (ShouldRemoveAllegianceNode(patronNode))
		{
			// don't need allegiance data anymore 
			RemoveAllegianceNode(patronNode);
		}
	}

	{
		RELEASE_ASSERT(_monarchs.find(vassalNode->_monarchID) != _monarchs.end());

		// check if i should even have allegiance data anymore (no vassals/patron)
		AllegianceTreeNode *vassalMonarchNode = _monarchs[vassalNode->_monarchID];
		CacheDataRecursively(vassalMonarchNode, NULL);
		NotifyTreeRefreshRecursively(vassalMonarchNode);

		// check if i should even have allegiance data anymore (no vassals/patron)
		if (ShouldRemoveAllegianceNode(vassalNode))
		{
			// don't need allegiance data anymore 
			RemoveAllegianceNode(vassalNode);
		}
	}

	Save();
}

void AllegianceManager::TryBreakAllegiance(CWeenieObject *source, DWORD target_id)
{
	TryBreakAllegiance(source->GetID(), target_id);
}

void AllegianceManager::TryBreakAllegiance(DWORD source_id, DWORD target_id)
{
	AllegianceTreeNode *selfTreeNode = GetTreeNode(source_id);
	if (!selfTreeNode)
	{
		// source doesn't have patron or vassals, shouldn't be here
		return;
	}

	AllegianceTreeNode *targetTreeNode = GetTreeNode(target_id);
	if (!targetTreeNode)
	{
		// target doesn't have patron or vassals, shouldn't be here
		return;
	}

	std::string targetCharName = targetTreeNode->_charName;

	if (selfTreeNode->_charID == targetTreeNode->_patronID)
	{
		// the target is a vassal
		BreakAllegiance(selfTreeNode, targetTreeNode);
	}
	else if (selfTreeNode->_patronID == target_id)
	{
		// the target is the patron
		BreakAllegiance(targetTreeNode, selfTreeNode);
	}
	else
	{
		return;
	}

	CWeenieObject *source = g_pWorld->FindPlayer(source_id);
	if (source)
	{
		source->SendText(csprintf(" You have broken your Allegiance to %s!", targetCharName.c_str()), LTT_DEFAULT);
	}

	CWeenieObject *target = g_pWorld->FindPlayer(target_id);
	if (target)
	{
		target->SendText(csprintf("%s has broken their Allegiance to you!", source->GetName().c_str()), LTT_DEFAULT);
	}
}

void AllegianceManager::BreakAllAllegiance(DWORD char_id)
{
	AllegianceTreeNode *selfTreeNode = GetTreeNode(char_id);
	if (!selfTreeNode)
	{
		// not in an allegiance
		return;
	}

	if (selfTreeNode->_patronID)
	{
		AllegianceTreeNode *patronTreeNode = GetTreeNode(selfTreeNode->_patronID);
		if (patronTreeNode)
		{
			BreakAllegiance(patronTreeNode, selfTreeNode);
		}
	}

	while ((selfTreeNode = GetTreeNode(char_id)) && !selfTreeNode->_vassals.empty())
	{
		AllegianceTreeNodeMap::iterator vassalEntry = selfTreeNode->_vassals.begin();

		if (vassalEntry->second)
		{
			BreakAllegiance(selfTreeNode, vassalEntry->second);
		}
		else
		{
			selfTreeNode->_vassals.erase(vassalEntry);
		}
	}

	// should not be necessary
	assert(selfTreeNode != GetTreeNode(char_id));

	if (selfTreeNode = GetTreeNode(char_id))
	{
		if (ShouldRemoveAllegianceNode(selfTreeNode))
		{
			RemoveAllegianceNode(selfTreeNode);
		}
	}
}

void AllegianceManager::HandleAllegiancePassup(DWORD source_id, long long amount, bool direct)
{
	AllegianceTreeNode *node = GetTreeNode(source_id);
	if (!node) // no allegiance
		return;

	if (!node->_patronID) // no patron (only vassals)
		return;

	AllegianceTreeNode *patron = GetTreeNode(node->_patronID);
	RELEASE_ASSERT(patron);

	if (!patron) // shouldn't happen
		return;

	time_t currentTime = time(0);

	double realDaysSworn = 0;
	double ingameHoursSworn = 0;

	double avgRealDaysVassalsSworn = 0;
	double avgIngameHoursVassalsSworn = 0;
	for (auto &entry : patron->_vassals) {
		AllegianceTreeNode *vassal = entry.second;
		double vassalDaysSworn = ((currentTime - vassal->_unixTimeSwornAt) / 86400.0);
		double vassalIngameHours = (vassal->_ingameSecondsSworn / 3600.0);
		if (vassal->_charID == node->_charID) {
			realDaysSworn = vassalDaysSworn;
			ingameHoursSworn = vassalIngameHours;
		}
		avgRealDaysVassalsSworn += vassalDaysSworn;
		avgIngameHoursVassalsSworn += vassalIngameHours;
	}

	avgRealDaysVassalsSworn /= max(1, patron->_vassals.size());
	avgIngameHoursVassalsSworn /= max(1, patron->_vassals.size());

	double vassalFactor = min(1.0, max(0.0, 0.25 * patron->_vassals.size()));
	double realDaysSwornFactor = min(realDaysSworn, 730.0) / 730.0;
	double ingameHoursSwornFactor = min(ingameHoursSworn, 720.0) / 720.0;
	double avgRealDaysVassalsSwornFactor = min(avgRealDaysVassalsSworn, 730.0) / 730.0;
	double avgIngameHoursVassalsSwornFactor = min(avgIngameHoursVassalsSworn, 720.0) / 720.0;
	double loyaltyFactor = min(node->_loyalty, 291.0) / 291.0;
	double leadershipFactor = min(patron->_leadership, 291.0) / 291.0;

	double factor1 = direct ? 50.0 : 16.0;
	double factor2 = direct ? 22.5 : 8.0;

	double generatedPercent = 0.01 * (factor1 + factor2 * loyaltyFactor * (1.0 + realDaysSwornFactor * ingameHoursSwornFactor));
	double receivedPercent = 0.01 * (factor1 + factor2 * leadershipFactor * (1.0 + vassalFactor * avgRealDaysVassalsSwornFactor * avgIngameHoursVassalsSwornFactor));
	
	double passup = generatedPercent * receivedPercent;

	unsigned long long passupAmount = amount * passup;

	if (passup > 0)
	{
		node->_cp_tithed += passupAmount;
		patron->_cp_cached += passupAmount;
		patron->_cp_pool_to_unload = min(4294967295ull, patron->_cp_pool_to_unload + passupAmount);

		CWeenieObject *patron_weenie = g_pWorld->FindPlayer(patron->_charID);
		if (patron_weenie)
			patron_weenie->TryToUnloadAllegianceXP(false);

		HandleAllegiancePassup(patron->_charID, passupAmount, false);
	}
}

void AllegianceManager::ChatMonarch(DWORD sender_id, const char *text)
{
	AllegianceTreeNode *node = GetTreeNode(sender_id);
	if (!node || !node->_monarchID || node->_monarchID == sender_id) // no allegiance
		return;

	CWeenieObject *sender_weenie = g_pWorld->FindPlayer(sender_id);
	if (!sender_weenie)
		return;

	CWeenieObject *target = g_pWorld->FindPlayer(node->_monarchID);
	if (!target)
		return;

	sender_weenie->SendNetMessage(ServerText(csprintf("You say to your Monarch, \"%s\"", text), LTT_SPEECH_DIRECT_SEND), PRIVATE_MSG, FALSE, TRUE);
	target->SendNetMessage(ServerText(csprintf("Your follower %s says to you, \"%s\"", sender_weenie->GetName().c_str(), text), LTT_SPEECH_DIRECT), PRIVATE_MSG, FALSE, TRUE);
}

void AllegianceManager::ChatPatron(DWORD sender_id, const char *text)
{
	AllegianceTreeNode *node = GetTreeNode(sender_id);
	if (!node || !node->_patronID) // no allegiance
		return;

	CWeenieObject *sender_weenie = g_pWorld->FindPlayer(sender_id);
	if (!sender_weenie)
		return;

	CWeenieObject *target = g_pWorld->FindPlayer(node->_patronID);
	if (!target)
		return;

	sender_weenie->SendNetMessage(ServerText(csprintf("You say to your Patron, \"%s\"", text), LTT_SPEECH_DIRECT_SEND), PRIVATE_MSG, FALSE, TRUE);
	target->SendNetMessage(ServerText(csprintf("Your vassal %s says to you, \"%s\"", sender_weenie->GetName().c_str(), text), LTT_SPEECH_DIRECT), PRIVATE_MSG, FALSE, TRUE);
}

void AllegianceManager::ChatVassals(DWORD sender_id, const char *text)
{
	AllegianceTreeNode *node = GetTreeNode(sender_id);
	if (!node) // no allegiance
		return;

	CWeenieObject *sender_weenie = g_pWorld->FindPlayer(sender_id);
	if (!sender_weenie)
		return;

	sender_weenie->SendNetMessage(ServerText(csprintf("You say to your Vassals, \"%s\"", text), LTT_SPEECH_DIRECT_SEND), PRIVATE_MSG, FALSE, TRUE);

	for (auto &entry : node->_vassals)
	{
		CWeenieObject *target = g_pWorld->FindPlayer(entry.second->_charID);
		if (!target)
			continue;

		target->SendNetMessage(ServerText(csprintf("Your patron %s says to you, \"%s\"", sender_weenie->GetName().c_str(), text), LTT_SPEECH_DIRECT), PRIVATE_MSG, FALSE, TRUE);
	}
}

void AllegianceManager::ChatCovassals(DWORD sender_id, const char *text)
{
	AllegianceTreeNode *node = GetTreeNode(sender_id);
	if (!node || !node->_patronID) // no allegiance
		return;

	AllegianceTreeNode *patron_node = GetTreeNode(node->_patronID);
	if (!patron_node) // no patron
		return;

	CWeenieObject *sender_weenie = g_pWorld->FindPlayer(sender_id);
	if (!sender_weenie)
		return;

	sender_weenie->SendNetMessage(ServerText(csprintf("[Co-Vassals] You Say, \"%s\"", text), LTT_SPEECH_DIRECT_SEND), PRIVATE_MSG, FALSE, TRUE);

	for (auto &entry : patron_node->_vassals)
	{
		if (entry.second->_charID == sender_id)
			continue;

		CWeenieObject *target = g_pWorld->FindPlayer(entry.second->_charID);
		if (target)
			target->SendNetMessage(ServerText(csprintf("[Co-Vassals] %s says, \"%s\"", sender_weenie->GetName().c_str(), text), LTT_SPEECH_DIRECT), PRIVATE_MSG, FALSE, TRUE);
	}

	CWeenieObject *patron_weenie = g_pWorld->FindPlayer(node->_patronID);
	if (patron_weenie)
		patron_weenie->SendNetMessage(ServerText(csprintf("[Co-Vassals] %s says, \"%s\"", sender_weenie->GetName().c_str(), text), LTT_SPEECH_DIRECT), PRIVATE_MSG, FALSE, TRUE);

}

void AllegianceManager::SendAllegianceProfile(CWeenieObject *pPlayer)
{
	unsigned int rank = 0;
	AllegianceProfile *prof = g_pAllegianceManager->CreateAllegianceProfile(pPlayer->GetID(), &rank);

	BinaryWriter allegianceUpdate;
	allegianceUpdate.Write<DWORD>(0x20);
	allegianceUpdate.Write<DWORD>(rank);
	prof->Pack(&allegianceUpdate);
	pPlayer->SendNetMessage(&allegianceUpdate, PRIVATE_MSG, TRUE, FALSE);

	BinaryWriter allegianceUpdateDone;
	allegianceUpdateDone.Write<DWORD>(0x1C8);
	allegianceUpdateDone.Write<DWORD>(0);
	pPlayer->SendNetMessage(&allegianceUpdateDone, PRIVATE_MSG, TRUE, FALSE);

	delete prof;
}

DWORD AllegianceManager::GetCachedMonarchIDForPlayer(CPlayerWeenie * player)
{
	// this data may not be trustworthy, should use tree node for anything important
	return player->InqIIDQuality(MONARCH_IID, 0);
}

bool AllegianceManager::IsOfficer(AllegianceTreeNode* playerNode)
{
	if (playerNode && !IsMonarch(playerNode)) // monarch can't be an officer
	{
		AllegianceHierarchy hierarchy = GetInfo(playerNode->_monarchID)->_info;
		for (auto officer : hierarchy.m_AllegianceOfficers)
		{
			if (officer.first == playerNode->_charID)
			{
				return true;
			}
		}
	}
	return false;
}

bool AllegianceManager::IsMonarch(AllegianceTreeNode* playerNode)
{
	if (playerNode)
	{
		if (playerNode->_monarchID != playerNode->_charID)
		{
			return false;
		}
		return true;
	}
	return false;

}

eAllegianceOfficerLevel AllegianceManager::GetOfficerLevel(std::string player_name)
{
	DWORD playerID = g_pDBIO->GetPlayerCharacterId(player_name.c_str());

	return GetOfficerLevel(playerID);
}

eAllegianceOfficerLevel AllegianceManager::GetOfficerLevel(DWORD player_id)
{
	if (AllegianceTreeNode* playerNode = GetTreeNode(player_id))
	{
		if (IsOfficer(playerNode))
		{
			AllegianceHierarchy hierarchy = GetInfo(playerNode->_monarchID)->_info;
			for (auto officer : hierarchy.m_AllegianceOfficers)
			{
				if (officer.first == player_id)
				{
					return officer.second;
				}
			}
		}
	}
	return Undef_AllegianceOfficerLevel;
}

std::string AllegianceManager::GetOfficerTitle(std::string player_name)
{
	DWORD playerID = g_pDBIO->GetPlayerCharacterId(player_name.c_str());

	return GetOfficerTitle(playerID);
}

std::string AllegianceManager::GetOfficerTitle(DWORD player_id)
{
	eAllegianceOfficerLevel officerLevel = GetOfficerLevel(player_id);

	if (officerLevel) // player is actually an officer
	{
		if (AllegianceTreeNode* playerNode = GetTreeNode(player_id))
		{
			AllegianceHierarchy hierarchy = GetInfo(playerNode->_monarchID)->_info;
			return hierarchy.m_OfficerTitles.array_data[officerLevel]; // is this set to the default titles if the allegiance hasnt set any custom ones?
		}
	}
	return "";
}

bool AllegianceManager::IsOfficerWithLevel(AllegianceTreeNode* playerNode, eAllegianceOfficerLevel min, eAllegianceOfficerLevel max)
{
	if (playerNode && !IsMonarch(playerNode)) // monarch can't be an officer
	{
		AllegianceHierarchy hierarchy = GetInfo(playerNode->_monarchID)->_info;
		for (auto officer : hierarchy.m_AllegianceOfficers)
		{
			if (officer.first == playerNode->_charID && officer.second >= min && officer.second <= max)
			{
				return true;
			}
		}
	}
	return false;
}

void AllegianceManager::SetMOTD(CPlayerWeenie* player, std::string msg)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	if (!(IsMonarch(playerNode) || IsOfficer(playerNode)))
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
		return;
	}

	if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
	{
		info->_info.m_motd = msg;
		info->_info.m_motdSetBy = player->GetName();
		player->SendText("Your allegiance message of the day has been set.", LTT_DEFAULT);
	}
}

void AllegianceManager::LoginMOTD(CPlayerWeenie * player)
{
	AllegianceTreeNode *self = GetTreeNode(player->GetID());
	if (!self)
		return;

	AllegianceInfo *info = GetInfo(self->_monarchID);
	if (!info)
		return;

	if (!info->_info.m_motd.empty())
	{
		player->SendText(csprintf("%s -- %s %s", info->_info.m_motd.c_str(), GetOfficerTitle(info->_info.m_motdSetBy).c_str(), info->_info.m_motdSetBy.c_str()), LTT_DEFAULT);
	}
}

void AllegianceManager::ClearMOTD(CPlayerWeenie * player)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	if (!(IsMonarch(playerNode) || IsOfficer(playerNode)))
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
		return;
	}

	if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
	{
		info->_info.m_motd.clear();
		info->_info.m_motdSetBy.clear();
		player->SendText("Your allegiance message of the day has been cleared.", LTT_DEFAULT);
	}
}

void AllegianceManager::QueryMOTD(CPlayerWeenie * player)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}
	if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
	{
		if (info->_info.m_motd.empty())
		{
			player->SendText("Your allegiance has not set a message of the day.", LTT_DEFAULT);
		}
		else
		{
			player->SendText(csprintf("%s -- %s %s", info->_info.m_motd.c_str(), GetOfficerTitle(info->_info.m_motdSetBy).c_str(), info->_info.m_motdSetBy.c_str()), LTT_DEFAULT);
		}
	}
}

void AllegianceManager::SetAllegianceName(CPlayerWeenie * player, std::string name)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	if (!(IsMonarch(playerNode) || IsOfficerWithLevel(playerNode, Castellan_AllegianceOfficerLevel)))
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
		return;
	}

	if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
	{
		if (info->_info.m_NameLastSetTime + 86400 <= time(0)) // check if 24 hours since last change
		{
			if (name == info->_info.m_AllegianceName)
			{
				player->NotifyWeenieError(WERROR_ALLEGIANCE_NAME_SAME_NAME);
				return;
			}
			if (name.empty())
			{
				player->NotifyWeenieError(WERROR_ALLEGIANCE_NAME_EMPTY);
				return;
			}
			if (name.length() > 40)
			{
				player->NotifyWeenieError(WERROR_ALLEGIANCE_NAME_TOO_LONG);
			}

			for (auto info : _allegInfos)
			{
				if (info.second->_info.m_AllegianceName == name)
				{
					player->NotifyWeenieError(WERROR_ALLEGIANCE_NAME_IN_USE);
					return;
				}
			}

			for (auto c : name)
			{
				if (!(c == 32 || c == 39 || c == 45 || (64 < c && c < 91) || (96 < c && c < 123))) // space, single quote, hyphen, A-Z, a-z
				{
					player->NotifyWeenieError(WERROR_ALLEGIANCE_NAME_BAD_CHARACTER);
					return;
				}
			}

			for (auto const& value : g_pPortalDataEx->GetBannedWords())
			{
				if (name.find(value) != std::string::npos)
				{
					player->NotifyWeenieError(WERROR_ALLEGIANCE_NAME_NOT_APPROPRIATE);
					return;
				}
			}

			info->_info.m_AllegianceName = name;
			player->SendText("Your allegiance name has been set.", LTT_DEFAULT);
			info->_info.m_NameLastSetTime = time(0);
		}
		else
		{
			int nextSetTime = (info->_info.m_NameLastSetTime + 86400) - time(0); // how long before it can be changed again in seconds

			player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_NAME_TIMER, TimeToString(nextSetTime).c_str());
		}
	}
}

void AllegianceManager::ClearAllegianceName(CPlayerWeenie * player)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	if (!(IsMonarch(playerNode) || IsOfficerWithLevel(playerNode, Castellan_AllegianceOfficerLevel)))
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
		return;
	}

	if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
	{
		info->_info.m_AllegianceName.clear();
		player->SendText("Your allegiance name has been cleared.", LTT_DEFAULT);
	}
}

void AllegianceManager::QueryAllegianceName(CPlayerWeenie * player)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
	{
		if (info->_info.m_AllegianceName.empty())
		{
			player->SendText("Your allegiance hasn't set an allegiance name.", LTT_DEFAULT);
		}
		else
		{
			player->SendText(csprintf("Your allegiance name is %s", info->_info.m_AllegianceName.c_str()), LTT_DEFAULT);
		}
	}
}

void AllegianceManager::SetOfficerTitle(CPlayerWeenie * player, int level, std::string title)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	if (!(IsMonarch(playerNode) || IsOfficer(playerNode)))
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
		return;
	}

	if (level > 3 || level < 1)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_OFFICER_INVALID_LEVEL);
		return;
	}

	if (title.length() > 20)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_OFFICER_TITLE_TOO_LONG);
		return;
	}

	for (auto c : title)
	{
		if (!(c == 32 || c == 39 || c == 45 || (64 < c && c < 91) || (96 < c && c < 123))) // space, single quote, hyphen, A-Z, a-z
		{
			player->NotifyWeenieError(WERROR_ALLEGIANCE_OFFICER_TITLE_BAD_CHARACTER);
			return;
		}
	}

	for (auto const& value : g_pPortalDataEx->GetBannedWords())
	{
		if (title.find(value) != std::string::npos)
		{
			player->NotifyWeenieError(WERROR_ALLEGIANCE_OFFICER_TITLE_NOT_APPROPRIATE);
			return;
		}
	}

	if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
	{
		info->_info.m_OfficerTitles.array_data[level] = title;

		player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_OFFICER_TITLE_SET, title.c_str());
	}
}

void AllegianceManager::ClearOfficerTitles(CPlayerWeenie * player)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	if (!(IsMonarch(playerNode) || IsOfficerWithLevel(playerNode, Castellan_AllegianceOfficerLevel)))
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
		return;
	}

	if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
	{
		info->_info.m_OfficerTitles.array_data[1] = "Speaker";
		info->_info.m_OfficerTitles.array_data[2] = "Seneschal";
		info->_info.m_OfficerTitles.array_data[3] = "Castellan";
		player->NotifyWeenieError(WERROR_ALLEGIANCE_OFFICER_TITLE_CLEARED);
	}
}

void AllegianceManager::ListOfficerTitles(CPlayerWeenie * player)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
	{
		player->SendText("Allegiance Officer Titles:", LTT_DEFAULT);
		for (int i = 1; i < 4; i++)
		{
			player->SendText(csprintf("%d. %s", i, info->_info.m_OfficerTitles.array_data[i]), LTT_DEFAULT);
		}
	}
}

void AllegianceManager::SetOfficer(CPlayerWeenie * player, std::string officer_name, eAllegianceOfficerLevel level)
{
	DWORD officerID = g_pDBIO->GetPlayerCharacterId(officer_name.c_str());
	if (AllegianceTreeNode* officerNode = GetTreeNode(officerID))
	{

		if (IsOfficer(officerNode) && GetOfficerLevel(officerID) == level)
		{
			player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_OFFICER_ALREADY_OFFICER, officer_name.c_str());
			return;
		}

		if (level <= 0 || level > 3)
		{
			player->NotifyWeenieError(WERROR_ALLEGIANCE_OFFICER_INVALID_LEVEL);
			return;
		}

		AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
		if (!playerNode)
		{
			player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
			return;
		}

		if (!(IsMonarch(playerNode) || IsOfficer(playerNode)))
		{
			player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
			return;
		}

		if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
		{
			if (info->_info.m_AllegianceOfficers.size() == 12)
			{
				player->NotifyWeenieError(WERROR_ALLEGIANCE_OFFICER_FULL);
				return;
			}

			if (IsOfficer(officerNode))
			{
				info->_info.m_AllegianceOfficers[officerID] = level;
			}
			else
			{
				info->_info.m_AllegianceOfficers.emplace(officerID, level);
			}
			player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_OFFICER_SET, officer_name.c_str());
			g_pWorld->FindPlayer(officerID)->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_YOU_ARE_NOW_AN_OFFICER, GetOfficerTitle(officer_name).c_str());
		}
	}
}

void AllegianceManager::RemoveOfficer(CPlayerWeenie * player, std::string officer_name)
{
	DWORD officerID = g_pDBIO->GetPlayerCharacterId(officer_name.c_str());

	if (AllegianceTreeNode* officerNode = GetTreeNode(officerID))
	{
		if (!IsOfficer(officerNode))
		{
			player->SendText(csprintf("%s is not an allegiance officer!", officer_name.c_str()), LTT_DEFAULT);
			return;
		}

		AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
		if (!playerNode)
		{
			player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
			return;
		}
		// only monarchs and level 2+ officers are authorized to remove officers
		if (!(IsMonarch(playerNode) || IsOfficerWithLevel(playerNode, Seneschal_AllegianceOfficerLevel)))
		{
			player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
			return;
		}
		// only level 3 officers and monarchs can remove level 2+ officers
		if (IsOfficerWithLevel(officerNode, Seneschal_AllegianceOfficerLevel))
		{
			if (!(IsOfficerWithLevel(playerNode, Castellan_AllegianceOfficerLevel) || IsMonarch(playerNode)))
			{
				player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
				return;
			}
		}

		if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
		{
			for (auto officer : info->_info.m_AllegianceOfficers)
			{
				if (officer.first == officerID)
				{
					info->_info.m_AllegianceOfficers.remove(officerID);
					player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_OFFICER_REMOVED, officer_name.c_str());
					g_pWorld->FindPlayer(officerID)->NotifyWeenieError(WERROR_ALLEGIANCE_YOU_ARE_NO_LONGER_AN_OFFICER);
					break;
				}
			}	
		}
		player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_OFFICER_NOT_REMOVED, officer_name.c_str());
	}
}

void AllegianceManager::ListOfficers(CPlayerWeenie * player)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
	{
		player->SendText("Allegiance Officers:", LTT_DEFAULT);
		player->SendText(csprintf("%s (Monarch)", g_pDBIO->GetPlayerCharacterName(playerNode->_monarchID)), LTT_DEFAULT);
		for (auto officer : info->_info.m_AllegianceOfficers)
		{
			player->SendText(csprintf("%s (%s)", g_pDBIO->GetPlayerCharacterName(officer.first), GetOfficerTitle(officer.first)), LTT_DEFAULT);
		}
	}
}

void AllegianceManager::ClearOfficers(CPlayerWeenie * player)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}
	// only monarch or level 3 officers can clear officer list - TODO check if this is monarch only?
	if (!(IsMonarch(playerNode) || IsOfficerWithLevel(playerNode, Castellan_AllegianceOfficerLevel)))
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
		return;
	}

	if (AllegianceInfo* info = GetInfo(playerNode->_monarchID))
	{
		for (auto officer : info->_info.m_AllegianceOfficers)
		{
			if (CPlayerWeenie* off = g_pWorld->FindPlayer(officer.first))
			{
				off->NotifyWeenieError(WERROR_ALLEGIANCE_YOU_ARE_NO_LONGER_AN_OFFICER);
			}
		}
		info->_info.m_AllegianceOfficers.clear();
		player->NotifyWeenieError(WERROR_ALLEGIANCE_OFFICERS_CLEARED);
	}
}

void AllegianceManager::AllegianceInfoRequest(CPlayerWeenie * player, std::string target_name)
{
	AllegianceTreeNode *myNode = GetTreeNode(player->GetID());
	if (!myNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	if (!(IsMonarch(myNode) || IsOfficerWithLevel(myNode, Seneschal_AllegianceOfficerLevel)))
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
		return;
	}

	if (target_name.empty())
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_INFO_EMPTY_NAME);
		return;
	}

	if (target_name == player->GetName())
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_INFO_SELF);
		return;
	}

	DWORD targetID = g_pWorld->GetPlayerId(target_name.c_str(), true);

	AllegianceTreeNode *myTargetNode = GetTreeNode(targetID);
	if (!myTargetNode)
	{
		player->SendText("Could not find allegiance member.", LTT_DEFAULT);
		return;
	}

	if (myTargetNode->_monarchID != myNode->_monarchID)
	{
		player->SendText(csprintf("%s is not a member of your allegiance.", target_name.c_str()), LTT_DEFAULT);
		return;
	}
	

	// TODO: convert this to server to client message
	unsigned int rank = 0;
	if (AllegianceProfile *profile = CreateAllegianceProfile(myTargetNode->_charID, &rank))
	{
		BinaryWriter allegianceUpdate;
		allegianceUpdate.Write<DWORD>(0x27C);
		allegianceUpdate.Write<DWORD>(myTargetNode->_charID);
		profile->Pack(&allegianceUpdate);
		player->SendNetMessage(&allegianceUpdate, PRIVATE_MSG, TRUE, FALSE);

		delete profile;
	}
	else
	{
		player->SendText("Error retrieving allegiance member information.", LTT_DEFAULT);
	}
}

void AllegianceManager::AllegianceLockAction(CPlayerWeenie * player, DWORD lock_action)
{
	if (AllegianceTreeNode* playerNode = GetTreeNode(player->GetID()))
	{
		if (IsMonarch(playerNode) || IsOfficerWithLevel(playerNode, Seneschal_AllegianceOfficerLevel))
		{
			if (AllegianceInfo* allegianceInfo = GetInfo(playerNode->_monarchID))
			{
				switch (lock_action)
				{
				case LockedOff:
				{
					allegianceInfo->_info.m_isLocked = false;
					player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_LOCK_SET, "Unlocked");
					break;
				}
				case LockedOn:
				{
					allegianceInfo->_info.m_isLocked = true;
					player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_LOCK_SET, "Locked");
					break;
				}
				case ToggleLocked:
				{
					if (allegianceInfo->_info.m_isLocked == false)
					{
						allegianceInfo->_info.m_isLocked = true;
						player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_LOCK_SET, "Locked");
						break;
					}
					else
					{
						allegianceInfo->_info.m_isLocked = false;
						player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_LOCK_SET, "Unlocked");
						break;
					}
				}
				case CheckLocked:
				{
					player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_LOCK_DISPLAY, allegianceInfo->_info.m_isLocked ? "Locked" : "Unlocked");
					break;
				}
				case DisplayBypass: // show approved vassal
				{
					if (!allegianceInfo->_info.m_ApprovedVassal)
					{
						player->NotifyWeenieError(WERROR_ALLEGIANCE_LOCK_NO_APPROVED);
						break;
					}
					else
					{
						std::string vassal = g_pWorld->GetPlayerName(allegianceInfo->_info.m_ApprovedVassal, true);
						player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_LOCK_APPROVED_DISPLAY, vassal.c_str());
						break;
					}
				}
				case ClearBypass: // clear approved vassal
				{
					allegianceInfo->_info.m_ApprovedVassal = 0;
					player->NotifyWeenieError(WERROR_ALLEGIANCE_APPROVED_CLEARED);
					break;
				}
				}
			}
		}
		else
		{
			player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
		}
	}
}

void AllegianceManager::RecallHometown(CPlayerWeenie * player)
{
	if (player->CheckPKActivity())
	{
		player->NotifyWeenieError(WERROR_PORTAL_PK_ATTACKED_TOO_RECENTLY);
		return;
	}

	AllegianceTreeNode *allegianceNode = GetTreeNode(player->GetID());

	if (!allegianceNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	AllegianceInfo *allegianceInfo = GetInfo(allegianceNode->_monarchID);

	if (allegianceInfo && allegianceInfo->_info.m_BindPoint.objcell_id)
	{
		player->ExecuteUseEvent(new CAllegianceHometownRecallUseEvent());
	}
	else
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_HOMETOWN_NOT_SET);
	}
}

void AllegianceManager::ApproveVassal(CPlayerWeenie * player, std::string vassal_name)
{
	DWORD approvedVassalID = g_pWorld->GetPlayerId(vassal_name.c_str(), true);
	DWORD playerID = player->GetID();
	AllegianceTreeNode* playerNode = GetTreeNode(playerID);

	if (playerNode && (playerNode->_monarchID == playerID || GetOfficerLevel(playerID) >= 2)) // either a monarch or a level 2 or 3 officer
	{
		if (IsBanned(approvedVassalID, playerNode->_monarchID)) // check potential approved vassal's account isn't banned from the allegiance
		{
			player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_BANNED, vassal_name.c_str());
			return;
		}

		GetInfo(playerID)->_info.m_ApprovedVassal = approvedVassalID;
		player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_APPROVED_SET, vassal_name.c_str());
	}
	else
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
	}
}

void AllegianceManager::BootPlayer(CPlayerWeenie * player, std::string bootee, bool whole_account)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	
	if (!playerNode) // check caller even has an allegiance
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	if (bootee.empty())
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_BOOT_EMPTY_NAME);
		return;
	}

	if (bootee == player->GetName())
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_BOOT_SELF);
		return;
	}

	if (IsMonarch(playerNode) || IsOfficerWithLevel(playerNode, Seneschal_AllegianceOfficerLevel)) // check caller has correct authority
	{
		DWORD booteeID = g_pWorld->GetPlayerId(bootee.c_str(), true);
		if (AllegianceTreeNode* booteeNode = GetTreeNode(booteeID))
		{
			if (booteeNode->_monarchID == playerNode->_monarchID)
			{
				if (whole_account)
				{
					std::list<CharacterDesc_t> bootees = g_pDBIO->GetCharacterList(g_pDBIO->GetCharacterInfo(booteeID).account_id);
					for (auto bootee : bootees)
					{
						if (AllegianceTreeNode* bNode = GetTreeNode(bootee.weenie_id))
						{
							if (bNode->_monarchID == playerNode->_monarchID)
							{
								TryBreakAllegiance(bNode->_charID, bNode->_patronID);
							}
						}
					}
					player->SendText(csprintf("You have successfully booted the account of %s from the allegiance.",bootee.c_str()), LTT_DEFAULT);
					return;
				}

				TryBreakAllegiance(booteeID, booteeNode->_patronID);
				player->SendText(csprintf("You have successfully booted %s from the allegiance.", bootee.c_str()), LTT_DEFAULT);
			}
		}
	}
	else
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
	}
}

void AllegianceManager::ChatBoot(CPlayerWeenie * player, std::string target, std::string reason)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode)
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	if (target.empty())
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_BOOT_EMPTY_NAME);
		return;
	}

	if (!(IsMonarch(playerNode) || IsOfficerWithLevel(playerNode, Seneschal_AllegianceOfficerLevel)))
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
		return;
	}

	// TODO: needs join/leave channel functionality

}

bool AllegianceManager::IsBanned(DWORD player_to_check_id, DWORD monarch_id)
{
	CharacterDesc_t potentialVassal = g_pDBIO->GetCharacterInfo(player_to_check_id); // get chardesc for account id
	if (AllegianceInfo* ai = GetInfo(monarch_id))
	{
		for (auto banned : ai->_info.m_BanList)
		{
			if (banned.first == potentialVassal.account_id) // if account id is on the ban list, return true
			{
				return true;
			}
		}
	}
	return false;
}

void AllegianceManager::AddBan(CPlayerWeenie* player, std::string char_name)
{
	DWORD bannedPlayerID = g_pWorld->GetPlayerId(char_name.c_str());
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode) // check caller even has an allegiance
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	DWORD monarchID = playerNode->_monarchID;

	if (IsMonarch(playerNode) || IsOfficerWithLevel(playerNode, Seneschal_AllegianceOfficerLevel)) // check caller has correct authority
	{
		CharacterDesc_t bannedCharInfo = g_pDBIO->GetCharacterInfo(bannedPlayerID);
		AllegianceHierarchy ah = GetInfo(monarchID)->_info;
		
		if (ah.m_BanList.size() > 50) 	// max ban list as of 01/06 patch notes
		{
			player->NotifyWeenieError(WERROR_ALLEGIANCE_BANNED_LIST_FULL);
			return;
		}

		for (auto pair : ah.m_BanList)
		{
			if (pair.first == bannedCharInfo.account_id) // check if their account is already banned
			{
				player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_CHAR_ALREADY_BANNED, bannedCharInfo.name.c_str());
				return;
			}
		}

		ah.m_BanList.emplace_back(bannedCharInfo.account_id, bannedCharInfo.name); // add account id & name of character to ban list of allegiance

		std::list<CharacterDesc_t> bannedCharacters = g_pDBIO->GetCharacterList(bannedCharInfo.account_id);
		for (auto character : bannedCharacters)
		{
			AllegianceTreeNode* characterNode = GetTreeNode(character.weenie_id);
			if (characterNode && characterNode->_monarchID == monarchID) // if any characters are currently in the allegiance...
			{
				TryBreakAllegiance(characterNode->_patronID, character.weenie_id); // ...boot them from allegiance
			}
		}
	}
	else
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
	}
}

void AllegianceManager::RemoveBan(CPlayerWeenie * player, std::string char_name)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode) // check caller even has an allegiance
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	DWORD monarchID = playerNode->_monarchID;

	if (IsMonarch(playerNode) || IsOfficerWithLevel(playerNode, Seneschal_AllegianceOfficerLevel)) // check caller has correct authority
	{
		AllegianceHierarchy ah = GetInfo(monarchID)->_info;

		for (int i=0; i <ah.m_BanList.size(); i++)
		{
			if (ah.m_BanList[i].second == char_name) // check if their account is already banned
			{
				ah.m_BanList.erase(ah.m_BanList.begin() + i);
				player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_CHAR_UNBANNED_SUCCESSFULLY, char_name.c_str());
				return;
			}
		}
		player->NotifyWeenieErrorWithString(WERROR_ALLEGIANCE_CHAR_NOT_BANNED, char_name.c_str());
	}
	else
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
	}
}

void AllegianceManager::GetBanList(CPlayerWeenie * player)
{
	AllegianceTreeNode* playerNode = GetTreeNode(player->GetID());
	if (!playerNode) // check caller even has an allegiance
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NONEXISTENT);
		return;
	}

	DWORD monarchID = playerNode->_monarchID;

	if (IsMonarch(playerNode) || IsOfficerWithLevel(playerNode, Seneschal_AllegianceOfficerLevel)) // check caller has correct authority
	{
		if (AllegianceInfo* ai = GetInfo(monarchID))
		{
			player->NotifyWeenieError(WERROR_ALLEGIANCE_LIST_BANNED_CHARACTERS);
			for (auto banned : ai->_info.m_BanList)
			{
				player->SendText(banned.second.c_str(), LTT_DEFAULT);
			}
		}
	}
	else
	{
		player->NotifyWeenieError(WERROR_ALLEGIANCE_NOT_AUTHORIZED);
	}
}
