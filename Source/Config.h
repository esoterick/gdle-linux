
#pragma once

class CKeyValueConfig
{
public:
	CKeyValueConfig(const char *filepath = "");
	virtual ~CKeyValueConfig();

	void Destroy();

	bool Load();
	bool Load(const char *filepath);
	
	const char *GetValue(const char *valuename, const char *defaultvalue = "");
	const char *GetConfigPath() { return m_strFile.c_str(); }

private:
	virtual void PostLoad() { }

	std::string m_strFile;
	std::map<std::string, std::string> m_KeyValues;
	
};

class CPhatACServerConfig : public CKeyValueConfig
{
public:
	CPhatACServerConfig(const char *filepath = "");
	virtual ~CPhatACServerConfig() override;
	
	unsigned long BindIP() { return m_BindIP; }
	unsigned int BindPort() { return m_BindPort; }

	const char *DatabaseIP() { return m_DatabaseIP.c_str(); }
	unsigned int DatabasePort() { return m_DatabasePort; }
	const char *DatabaseUsername() { return m_DatabaseUsername.c_str(); }
	const char *DatabasePassword() { return m_DatabasePassword.c_str(); }
	const char *DatabaseName() { return m_DatabaseName.c_str(); }

	const char *WorldName() { return m_WorldName.c_str(); }
	const char *WelcomePopup() { return m_WelcomePopup.c_str(); }
	const char *WelcomeMessage() { return m_WelcomeMessage.c_str(); }

	virtual bool FastTick() { return m_bFastTick; }
	virtual bool UseIncrementalID() { return m_bUseIncrementalIDs; }

	virtual bool HardcoreMode() { return m_bHardcoreMode; }
	virtual bool HardcoreModePlayersOnly() { return m_bHardcoreModePlayersOnly; }
	virtual bool PKOnly() { return m_bPKOnly; }
	virtual bool ColoredSentinels() { return m_bColoredSentinels; }

	virtual bool SpawnLandscape() { return m_bSpawnLandscape; }
	virtual bool SpawnStaticCreatures() { return m_bSpawnStaticCreatures; }
	virtual bool EverythingUnlocked() { return m_bEverythingUnlocked; }
	virtual bool TownCrierBuffs() { return m_bTownCrierBuffs; }

	virtual bool EnableTeleCommands() { return m_bEnableTeleCommands; }
	virtual bool EnableXPCommands() { return m_bEnableXPCommands; }
	virtual bool EnableAttackableCommand() { return m_bEnableAttackableCommand; }
	virtual bool EnableGodlyCommand() { return m_bEnableGodlyCommand; }

	virtual double GetMultiplierForQuestTime(int questTime);
	virtual int UseMultiplierForQuestTime(int questTime);

	virtual double KillXPMultiplier(int level);
	virtual double RewardXPMultiplier(int level);
	virtual double DropRateMultiplier() { return m_fDropRateMultiplier; }
	virtual double RespawnTimeMultiplier() { return m_fRespawnTimeMultiplier; }
	virtual bool IsSpellFociEnabled() { return m_bSpellFociEnabled; }

	virtual bool AutoCreateAccounts() { return m_bAutoCreateAccounts; }

	virtual unsigned int MaxDormantLandblocks() { return m_MaxDormantLandblocks; }
	virtual unsigned int DormantLandblockCleanupTime() { return m_DormantLandblockCleanupTime; }

	virtual bool ShowLogins() { return m_bShowLogins; }
	virtual bool SpeedHackKicking() { return m_bSpeedHackKicking; }
	virtual double SpeedHackKickThreshold() { return m_fSpeedHackKickThreshold; }
	virtual bool ShowDeathMessagesGlobally() { return m_bShowDeathMessagesGlobally; }
	virtual bool ShowPlayerDeathMessagesGlobally() { return m_bShowPlayerDeathMessagesGlobally; }

	virtual const char *HoltburgStartPosition() { return m_HoltburgStartPosition.c_str(); }
	virtual const char *YaraqStartPosition() { return m_YaraqStartPosition.c_str(); }
	virtual const char *ShoushiStartPosition() { return m_ShoushiStartPosition.c_str(); }
	virtual const char *SanamarStartPosition() { return m_SanamarStartPosition.c_str(); }
	virtual const char *OlthoiStartPosition() { return m_OlthoiStartPosition.c_str(); }

	virtual int PKRespiteTime() { return m_PKRespiteTime; }
	virtual bool SpellPurgeOnLogin() { return m_bSpellPurgeOnLogin; }
	virtual bool UpdateAllegianceData() { return m_bUpdateAllegianceData; }
	virtual bool InventoryPurgeOnLogin() { return m_bInventoryPurgeOnLogin; }

	virtual unsigned int WcidForPurge() { return m_WcidForPurge; }


	virtual bool AllowGeneralChat() { return m_bAllowGeneralChat; }

	virtual double RareDropMultiplier() { return m_fRareDropMultiplier; }
	virtual bool RealTimeRares() { return m_bRealTimeRares; }

	virtual bool LoginAtLS() { return m_bLoginAtLS; }
	virtual bool CreateTemplates() { return m_bCreateTemplates; }
	virtual bool AllowPKCommands() { return m_bAllowPKCommands; }
	virtual bool AllowOlthoi() { return m_bAllowOlthoi; }

	virtual bool FixOldChars() { return m_bFixOldChars; }

protected:
	virtual void PostLoad() override;

	unsigned long m_BindIP = 0;
	unsigned int m_BindPort = 0;

	std::string m_DatabaseIP;
	unsigned int m_DatabasePort = 0;
	std::string m_DatabaseUsername;
	std::string m_DatabasePassword;
	std::string m_DatabaseName;

	std::string m_WorldName;
	std::string m_WelcomePopup;
	std::string m_WelcomeMessage;

	bool m_bFastTick = false;
	bool m_bUseIncrementalIDs = false;

	bool m_bHardcoreMode = false;
	bool m_bHardcoreModePlayersOnly = false;
	bool m_bPKOnly = false;
	bool m_bColoredSentinels = false;
	bool m_bSpawnLandscape = true;
	bool m_bSpawnStaticCreatures = true;
	bool m_bEverythingUnlocked = true;
	bool m_bTownCrierBuffs = true;

	bool m_bEnableTeleCommands = false;
	bool m_bEnableXPCommands = false;
	bool m_bEnableAttackableCommand = false;
	bool m_bEnableGodlyCommand = false;
	
	double m_fQuestMultiplierLessThan1Day = 1.0;
	double m_fQuestMultiplier1Day = 1.0;
	double m_fQuestMultiplier3Day = 1.0;
	double m_fQuestMultiplier7Day = 1.0;
	double m_fQuestMultiplier14Day = 1.0;
	double m_fQuestMultiplier30Day = 1.0;
	double m_fQuestMultiplier60Day = 1.0;

	double m_fKillXPMultiplierT1 = 1.0;
	double m_fKillXPMultiplierT2 = 1.0;
	double m_fKillXPMultiplierT3 = 1.0;
	double m_fKillXPMultiplierT4 = 1.0;
	double m_fKillXPMultiplierT5 = 1.0;
	double m_fKillXPMultiplierT6 = 1.0;

	double m_fRewardXPMultiplierT1 = 1.0;
	double m_fRewardXPMultiplierT2 = 1.0;
	double m_fRewardXPMultiplierT3 = 1.0;
	double m_fRewardXPMultiplierT4 = 1.0;
	double m_fRewardXPMultiplierT5 = 1.0;
	double m_fRewardXPMultiplierT6 = 1.0;

	double m_fDropRateMultiplier = 1.0;
	double m_fRespawnTimeMultiplier = 1.0;
	bool m_bSpellFociEnabled = true;

	bool m_bAutoCreateAccounts = true;

	unsigned int m_MaxDormantLandblocks = 1000;
	unsigned int m_DormantLandblockCleanupTime = 1800;

	bool m_bShowLogins = true;
	bool m_bSpeedHackKicking = true;
	double m_fSpeedHackKickThreshold = 1.2;

	bool m_bShowDeathMessagesGlobally = false;
	bool m_bShowPlayerDeathMessagesGlobally = false;

	std::string m_HoltburgStartPosition;
	std::string m_YaraqStartPosition;
	std::string m_ShoushiStartPosition;
	std::string m_SanamarStartPosition;
	std::string m_OlthoiStartPosition;

	int m_PKRespiteTime = 300;
	bool m_bSpellPurgeOnLogin = false;
	bool m_bUpdateAllegianceData = false;
	bool m_bInventoryPurgeOnLogin = false;

	unsigned int m_WcidForPurge = 100000;

	bool m_bAllowGeneralChat = 1;
	bool m_bRealTimeRares = 0;
	double m_fRareDropMultiplier = 0.0;

	bool m_bLoginAtLS = 0;
	bool m_bCreateTemplates = 0;
	bool m_bAllowPKCommands = 0;
	bool m_bAllowOlthoi = 0;
	bool m_bFixOldChars = 1;
};

extern CPhatACServerConfig *g_pConfig;