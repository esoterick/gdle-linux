
#pragma once

class CWeenieFactory
{
public:
	CWeenieFactory();
	virtual ~CWeenieFactory();

	void Reset();
	void Initialize();

	std::shared_ptr<CWeenieObject> CreateWeenieByClassID(DWORD wcid, const Position *pos = NULL, bool bSpawn = false);
	std::shared_ptr<CWeenieObject> CreateWeenieByName(const char *name, const Position *pos = NULL, bool bSpawn = false);
	std::shared_ptr<CWeenieObject> CloneWeenie(std::shared_ptr<CWeenieObject> weenie);

	void AddWeenieToDestination(std::shared_ptr<CWeenieObject> weenie, std::shared_ptr<CWeenieObject> parent, DWORD destinationType, bool isRegenLocationType, const GeneratorProfile *profile = NULL);
	int AddFromCreateList(std::shared_ptr<CWeenieObject> parent, PackableListWithJson<CreationProfile> *createList, DestinationType validDestinationTypes = DestinationType::Undef_DestinationType);
	int AddFromGeneratorTable(std::shared_ptr<CWeenieObject> parent, bool isInit);

	int GenerateFromTypeOrWcid(std::shared_ptr<CWeenieObject> parent, const GeneratorProfile *profile);
	int GenerateFromTypeOrWcid(std::shared_ptr<CWeenieObject> parent, RegenLocationType destinationType, DWORD treasureType, unsigned int ptid = 0, float shade = 0.0f);
	int GenerateFromTypeOrWcid(std::shared_ptr<CWeenieObject> parent, DestinationType destinationType, DWORD treasureType, unsigned int ptid = 0, float shade = 0.0f);

	bool ApplyWeenieDefaults(std::shared_ptr<CWeenieObject> weenie, DWORD wcid);

	DWORD GetWCIDByName(const char *name, int index = 0);

	CWeenieDefaults *GetWeenieDefaults(DWORD wcid);
	CWeenieDefaults *GetWeenieDefaults(const char *name, int index = 0);
	
	bool TryToResolveAppearanceData(std::shared_ptr<CWeenieObject> weenie);

	std::shared_ptr<CWeenieObject> CreateBaseWeenieByType(int weenieType, unsigned int wcid, const char *weenieName = "");

	DWORD GetScrollSpellForWCID(DWORD wcid);
	DWORD GetWCIDForScrollSpell(DWORD spell_id);

	void RefreshLocalStorage();

	DWORD m_FirstAvatarWCID = 0;
	DWORD m_NumAvatars = 0;

	std::list<DWORD> GetWCIDsWithMotionTable(DWORD mtable);

protected:
	std::shared_ptr<CWeenieObject> CreateWeenie(CWeenieDefaults *defaults, const Position *pos = NULL, bool bSpawn = false);

	void ApplyWeenieDefaults(std::shared_ptr<CWeenieObject> weenie, CWeenieDefaults *defaults);

	void LoadLocalStorage(bool refresh = false);
	void LoadLocalStorageIndexed();
	void LoadAvatarData();

	void MapScrollWCIDs();

	std::unordered_map<DWORD, CWeenieDefaults *> m_WeenieDefaults;
	std::multimap<std::string, CWeenieDefaults *> m_WeenieDefaultsByName;

	std::unordered_map<DWORD, DWORD> m_ScrollWeenies; // keyed by spell ID
};

