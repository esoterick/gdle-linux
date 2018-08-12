
#pragma once

typedef std::unordered_map<DWORD, class AllegianceTreeNode *> AllegianceTreeNodeMap;
typedef std::unordered_map<DWORD, class AllegianceInfo *> AllegianceInfoMap;

class AllegianceInfo : public PackObj
{
public:
	DECLARE_PACKABLE()

	AllegianceHierarchy _info;
};

class AllegianceTreeNode : public PackObj
{
public:
	AllegianceTreeNode();
	virtual ~AllegianceTreeNode();

	DECLARE_PACKABLE()

	AllegianceTreeNode *FindCharByNameRecursivelySlow(const std::string &charName);
	void FillAllegianceNode(AllegianceNode *node);
	void UpdateWithWeenie(CWeenieObject *weenie);
	
	unsigned int _charID = 0;
	std::string _charName;

	unsigned int _monarchID = 0;
	unsigned int _patronID = 0;
	HeritageGroup _hg = Invalid_HeritageGroup;
	Gender _gender = Invalid_Gender;
	unsigned int _rank = 0;
	unsigned int _level = 0;
	unsigned int _leadership = 0;
	unsigned int _loyalty = 0;
	unsigned int _numFollowers = 0;
	unsigned __int64 _cp_cached = 0;
	unsigned __int64 _cp_tithed = 0;

	unsigned __int64 _cp_pool_to_unload = 0;
	unsigned __int64 _unixTimeSwornAt = 0;
	unsigned __int64 _ingameSecondsSworn = 0;
	AllegianceTreeNodeMap _vassals;
};

class AllegianceManager : public PackObj
{
public:
	AllegianceManager();
	virtual ~AllegianceManager();

	DECLARE_PACKABLE()
	
	void Load();
	void Save();
	void Tick();

	void CacheInitialDataRecursively(AllegianceTreeNode *node, AllegianceTreeNode *parent);
	void WalkTreeAndBumpOnlineTime(AllegianceTreeNode *node, int onlineSecondsDelta);
	void CacheDataRecursively(AllegianceTreeNode *node, AllegianceTreeNode *parent);
	void NotifyTreeRefreshRecursively(AllegianceTreeNode *node);

	AllegianceTreeNode *GetTreeNode(DWORD charID);
	AllegianceInfo *GetInfo(DWORD monarchID);

	void SetWeenieAllegianceQualities(CWeenieObject *weenie);
	AllegianceProfile *CreateAllegianceProfile(DWORD char_id, unsigned int *pRank);
	void SendAllegianceProfile(CWeenieObject *pPlayer);
	void TrySwearAllegiance(CWeenieObject *source, CWeenieObject *target);
	void TryBreakAllegiance(CWeenieObject *source, DWORD target_id);
	void TryBreakAllegiance(DWORD source_id, DWORD target_id);
	void BreakAllAllegiance(DWORD char_id);

	void ChatMonarch(DWORD sender_id, const char *text);
	void ChatPatron(DWORD sender_id, const char *text);
	void ChatVassals(DWORD sender_id, const char *text);
	void ChatCovassals(DWORD sender_id, const char *text);

	AllegianceInfoMap _allegInfos;
	AllegianceTreeNodeMap _monarchs;
	AllegianceTreeNodeMap _directNodes;

	void HandleAllegiancePassup(DWORD source_id, long long amount, bool direct);

	DWORD GetCachedMonarchIDForPlayer(CPlayerWeenie *player);

	bool IsMonarch(AllegianceTreeNode* playerNode);
	bool IsOfficer(AllegianceTreeNode* playerNode);
	eAllegianceOfficerLevel GetOfficerLevel(std::string player_name);
	eAllegianceOfficerLevel GetOfficerLevel(DWORD player_id);
	std::string GetOfficerTitle(std::string player_name);
	std::string GetOfficerTitle(DWORD player_id);
	bool IsOfficerWithLevel(AllegianceTreeNode* playerNode, eAllegianceOfficerLevel min, eAllegianceOfficerLevel max = Castellan_AllegianceOfficerLevel);

	void SetMOTD(CPlayerWeenie * player, std::string msg);
	void LoginMOTD(CPlayerWeenie* player);
	void ClearMOTD(CPlayerWeenie * player);
	void QueryMOTD(CPlayerWeenie * player);

	void SetAllegianceName(CPlayerWeenie * player, std::string name);
	void ClearAllegianceName(CPlayerWeenie * player);
	void QueryAllegianceName(CPlayerWeenie * player);

	void SetOfficerTitle(CPlayerWeenie * player, int level, std::string title);
	void ClearOfficerTitles(CPlayerWeenie * player);
	void ListOfficerTitles(CPlayerWeenie * player);

	void SetOfficer(CPlayerWeenie * player, std::string officer_name, eAllegianceOfficerLevel level);
	void RemoveOfficer(CPlayerWeenie * player, std::string officer_name);
	void ListOfficers(CPlayerWeenie * player);
	void ClearOfficers(CPlayerWeenie* player);

	void AllegianceInfoRequest(CPlayerWeenie* player, std::string target_name);
	void AllegianceLockAction(CPlayerWeenie* player, DWORD lock_action);
	void RecallHometown(CPlayerWeenie* player);

	void ApproveVassal(CPlayerWeenie * player, std::string vassal_name);
	void BootPlayer(CPlayerWeenie* player, std::string bootee, bool whole_account);
	
	bool IsGagged(DWORD player_id);
	void ChatGag(CPlayerWeenie* player, std::string target, bool toggle);
	void ChatBoot(CPlayerWeenie* player, std::string target, std::string reason);

	bool IsBanned(DWORD player_to_check_id, DWORD monarch_id);
	void AddBan(CPlayerWeenie * player, std::string char_name);
	void RemoveBan(CPlayerWeenie * player, std::string char_name);
	void GetBanList(CPlayerWeenie * player);

	bool isForDB;
private:
	void BreakAllegiance(AllegianceTreeNode *patron, AllegianceTreeNode *vassal);

	bool ShouldRemoveAllegianceNode(AllegianceTreeNode *node);
	void RemoveAllegianceNode(AllegianceTreeNode *node);

	double m_LastSave = 0.0;
	double m_LastGagCheck = 0.0;
};