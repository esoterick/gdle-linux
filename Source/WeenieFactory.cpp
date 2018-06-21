
#include "StdAfx.h"
#include "WeenieFactory.h"
#include "Door.h"
#include "Lifestone.h"
#include "BindStone.h"
#include "Portal.h"
#include "Monster.h"
#include "TownCrier.h"
#include "Vendor.h"
#include "World.h"
#include "ClothingCache.h"
#include "Player.h"
#include "HotSpot.h"
#include "PressurePlate.h"
#include "Switch.h"
#include "Book.h"
#include "Food.h"
#include "Caster.h"
#include "Clothing.h"
#include "Corpse.h"
#include "Healer.h"
#include "House.h"
#include "Scroll.h"
#include "ManaStone.h"
#include "MeleeWeapon.h"
#include "Missile.h"
#include "MissileLauncher.h"
#include "Config.h"
#include "Key.h"
#include "Lockpick.h"
#include "Gem.h"
#include "PKModifier.h"
#include "Ammunition.h"
#include "AttributeTransferDevice.h"
#include "SkillAlterationDevice.h"
#include "GameEventManager.h"
#include "TreasureFactory.h"
#include "AugmentationDevice.h"

CWeenieFactory::CWeenieFactory()
{
}

CWeenieFactory::~CWeenieFactory()
{
	Reset();
}

void CWeenieFactory::Reset()
{
	for (auto entry : m_WeenieDefaults)
		delete entry.second;

	m_WeenieDefaults.clear();
	m_WeenieDefaultsByName.clear();
	m_ScrollWeenies.clear();
}

void CWeenieFactory::Initialize()
{
	LOG_PRIVATE(Data, Normal, "Loading weenies...\n");
	LoadLocalStorage();
	LoadLocalStorageIndexed();
	LoadAvatarData();

	MapScrollWCIDs();
	LOG_PRIVATE(Data, Normal, "Loaded %d weenie defaults...\n", m_WeenieDefaults.size());
}

DWORD CWeenieFactory::GetScrollSpellForWCID(DWORD wcid)
{
	if (CWeenieDefaults *defaults = GetWeenieDefaults(wcid))
	{
		if (defaults->m_Qualities.m_WeenieType == Scroll_WeenieType)
		{
			DWORD spell_id = 0;
			if (defaults->m_Qualities.InqDataID(SPELL_DID, spell_id))
			{
				return spell_id;
			}
		}
	}

	return 0;
}

DWORD CWeenieFactory::GetWCIDForScrollSpell(DWORD spell_id)
{
	auto i = m_ScrollWeenies.find(spell_id);
	if (i != m_ScrollWeenies.end())
		return i->second;

	return 0;
}

void CWeenieFactory::MapScrollWCIDs()
{
	m_ScrollWeenies.clear();
	for (auto &entry : m_WeenieDefaults)
	{
		if (entry.second->m_Qualities.m_WeenieType != Scroll_WeenieType)
		{
			continue;
		}

		DWORD spell_id;
		if (entry.second->m_Qualities.InqDataID(SPELL_DID, spell_id))
		{
			m_ScrollWeenies[spell_id] = entry.first;
		}
	}
}

std::list<DWORD> CWeenieFactory::GetWCIDsWithMotionTable(DWORD mtable)
{
	std::list<DWORD> results;

	for (auto &entry : m_WeenieDefaults)
	{
		DWORD mid = 0;
		if (entry.second->m_Qualities.InqDataID(MOTION_TABLE_DID, mid))
		{
			if (mtable == mid)
			{
				results.push_back(entry.first);
			}
		}
	}

	return results;
}

CWeenieDefaults *CWeenieFactory::GetWeenieDefaults(DWORD wcid)
{
	auto i = m_WeenieDefaults.find(wcid);
	if (i != m_WeenieDefaults.end())
		return i->second;

	return NULL;
}

DWORD CWeenieFactory::GetWCIDByName(const char *name, int index)
{
	CWeenieDefaults *defaults = GetWeenieDefaults(name, index);

	if (!defaults)
		return 0;

	return defaults->m_Qualities.GetID();
}

CWeenieDefaults *CWeenieFactory::GetWeenieDefaults(const char *name, int index)
{
	std::string search = name;
	
	// convert name for easier matching, lowercase and remove any spaces
	std::transform(search.begin(), search.end(), search.begin(), ::tolower);
	search.erase(remove_if(search.begin(), search.end(), isspace), search.end());
	
	std::pair<
		std::multimap<std::string, CWeenieDefaults *>::iterator,
		std::multimap<std::string, CWeenieDefaults *>::iterator> range = m_WeenieDefaultsByName.equal_range(search);

	for (std::multimap<std::string, CWeenieDefaults *>::iterator i = range.first; i != range.second; i++)
	{
		if (!index)
			return i->second;

		index--;
	}

	return NULL;
}

bool CWeenieFactory::ApplyWeenieDefaults(std::shared_ptr<CWeenieObject> weenie, DWORD wcid)
{
	auto defaults = GetWeenieDefaults(wcid);

	if (defaults)
	{
		ApplyWeenieDefaults(weenie, defaults);
		return true;
	}
	else
	{
		LOG_PRIVATE(Data, Warning, "Failed to find defaults for WCID %u! This is bad!\n", wcid);
		return false;
	}
}

void CWeenieFactory::ApplyWeenieDefaults(std::shared_ptr<CWeenieObject> weenie, CWeenieDefaults *defaults)
{
	weenie->m_Qualities.CopyFrom(&defaults->m_Qualities);

	weenie->m_Qualities.RemoveInt(PARENT_LOCATION_INT);

	int weaponSkill;
	if (weenie->m_Qualities.InqInt(WEAPON_SKILL_INT, weaponSkill, TRUE, FALSE))
		weenie->m_Qualities.SetInt(WEAPON_SKILL_INT, SkillTable::OldToNewSkill((STypeSkill)weaponSkill));

	int wieldReq;
	wieldReq = weenie->m_Qualities.GetInt(WIELD_REQUIREMENTS_INT, 0);
	if (wieldReq == 1 || wieldReq == 2 || wieldReq == 8)
	{
		if (weenie->m_Qualities.InqInt(WIELD_SKILLTYPE_INT, weaponSkill, TRUE, FALSE))
			weenie->m_Qualities.SetInt(WIELD_SKILLTYPE_INT, SkillTable::OldToNewSkill((STypeSkill)weaponSkill));
	}

	wieldReq = weenie->m_Qualities.GetInt(WIELD_REQUIREMENTS_2_INT, 0);
	if (wieldReq == 1 || wieldReq == 2 || wieldReq == 8)
	{
		if (weenie->m_Qualities.InqInt(WIELD_SKILLTYPE_2_INT, weaponSkill, TRUE, FALSE))
			weenie->m_Qualities.SetInt(WIELD_SKILLTYPE_2_INT, SkillTable::OldToNewSkill((STypeSkill)weaponSkill));
	}

	wieldReq = weenie->m_Qualities.GetInt(WIELD_REQUIREMENTS_3_INT, 0);
	if (wieldReq == 1 || wieldReq == 2 || wieldReq == 8)
	{
		if (weenie->m_Qualities.InqInt(WIELD_SKILLTYPE_3_INT, weaponSkill, TRUE, FALSE))
			weenie->m_Qualities.SetInt(WIELD_SKILLTYPE_3_INT, SkillTable::OldToNewSkill((STypeSkill)weaponSkill));
	}

	wieldReq = weenie->m_Qualities.GetInt(WIELD_REQUIREMENTS_4_INT, 0);
	if (wieldReq == 1 || wieldReq == 2 || wieldReq == 8)
	{
		if (weenie->m_Qualities.InqInt(WIELD_SKILLTYPE_4_INT, weaponSkill, TRUE, FALSE))
			weenie->m_Qualities.SetInt(WIELD_SKILLTYPE_4_INT, SkillTable::OldToNewSkill((STypeSkill)weaponSkill));
	}

	Skill uaSkill;
	if (weenie->m_Qualities.InqSkill(UNARMED_COMBAT_SKILL, uaSkill))
	{
		Skill finSkill;
		if (!weenie->m_Qualities.InqSkill(LIGHT_WEAPONS_SKILL, finSkill))
		{
			weenie->m_Qualities.SetSkill(LIGHT_WEAPONS_SKILL, uaSkill);
		}
	}

	if (weenie->m_Qualities._skillStatsTable)
	{
		for (auto &entry : *weenie->m_Qualities._skillStatsTable)
		{
			STypeSkill newSkill = SkillTable::OldToNewSkill((STypeSkill)entry.first);

			if (newSkill != entry.first)
			{
				weenie->m_Qualities.SetSkill(newSkill, entry.second);
			}
		}
	}

	std::string eventString;
	if (weenie->m_Qualities.InqString(GENERATOR_EVENT_STRING, eventString))
	{
		eventString = g_pGameEventManager->NormalizeEventName(eventString.c_str());
		weenie->m_Qualities.SetString(GENERATOR_EVENT_STRING, eventString);
	}

	weenie->m_ObjDescOverride = defaults->m_WorldObjDesc;
	weenie->m_WornObjDesc = defaults->m_WornObjDesc;
}

void CWeenieFactory::RefreshLocalStorage()
{
	LoadLocalStorage(true);
	MapScrollWCIDs();
}

void CWeenieFactory::LoadLocalStorage(bool refresh)
{
#if 0 // old
	std::list<std::string> filePaths;
	// EnumerateFolderFilePaths(filePaths, "data\\weenie\\defaults", "*.wqd", true);
	EnumerateFolderFilePaths(filePaths, "data\\weenie\\defaults\\custom", "*.wqd", true);

	if (atoi(g_pConfig->GetValue("weapons_testing", "0")) != 0)
	{
		EnumerateFolderFilePaths(filePaths, "data\\weenie\\defaults\\manual_content", "*.wqd", true);
	}

	for (auto filePath : filePaths)
	{
		BYTE *data = NULL;
		DWORD length = 0;
		if (LoadDataFromFile(filePath.c_str(), &data, &length))
		{
			BinaryReader reader(data, length);
			
			std::shared_ptr<CWeenieDefaults> pDefaults = std::shared_ptr<CWeenieDefaults>((new CWeenieDefaults())->GetPointer<CWeenieDefaults>(true));
			if (pDefaults->UnPack(&reader))
			{
				bool bShouldInsert = true;
				bool bDuplicate = false;

				auto existing = m_WeenieDefaults.find(pDefaults->m_WCID);
				if (existing != m_WeenieDefaults.end())
				{
					if (refresh)
					{
					}
					else
					{
						if (existing->second->m_bIsAutoGenerated)
						{
							if (pDefaults->m_bIsAutoGenerated)
							{
								bDuplicate = true;
								bShouldInsert = false;
							}
						}
						else
						{
							// existing was not auto-generated
							bShouldInsert = false;

							if (!pDefaults->m_bIsAutoGenerated)
							{
								bDuplicate = true;
							}
						}
					}
				}

				if (bShouldInsert)
				{
					CWeenieDefaults *toDelete = NULL;

					auto existing = m_WeenieDefaults.find(pDefaults->m_WCID);
					if (existing != m_WeenieDefaults.end())
					{
						toDelete = existing->second;
					}

					pDefaults->m_sourceFile = filePath;
					m_WeenieDefaults[pDefaults->m_WCID] = pDefaults;

					std::string name;
					if (pDefaults->m_Qualities.InqString(NAME_STRING, name))
					{
						// convert name for easier matching, lowercase and remove any spaces
						std::transform(name.begin(), name.end(), name.begin(), ::tolower);
						name.erase(remove_if(name.begin(), name.end(), isspace), name.end());

						// remove if this WCID is already in here
						for (std::multimap<std::string, CWeenieDefaults *>::iterator i = m_WeenieDefaultsByName.begin(); i != m_WeenieDefaultsByName.end(); i++)
						{
							if (i->second->m_Qualities.GetID() == pDefaults->m_Qualities.GetID())
							{
								m_WeenieDefaultsByName.erase(i);
								break;
							}
						}

						m_WeenieDefaultsByName.insert(std::pair<std::string, CWeenieDefaults *>(name, pDefaults));
					}

					if (toDelete)
					{
						delete toDelete;
					}
				}
				else
				{
					if (bDuplicate)
					{
						LOG_PRIVATE(Data, Warning, "Duplicate WCID %d, ignoring WQD file: \"%s\" due to \"%s\"\n", pDefaults->m_WCID, filePath.c_str(), m_WeenieDefaults[pDefaults->m_WCID]->m_sourceFile.c_str());
					}

					delete pDefaults;
				}
			}
			else
			{
				LOG_PRIVATE(Data, Warning, "Error parsing WQD file: %s\n", filePath.c_str());
				delete pDefaults;
			}

			delete [] data;
		}
	}
#endif

	std::list<std::string> filePaths;
	EnumerateFolderFilePaths(filePaths, "data\\json\\weenies\\", "*.json", true);

	for (const auto &filePath : filePaths)
	{
		std::ifstream fileStream(filePath.c_str());

		if (fileStream.is_open())
		{
			json jsonData;
			fileStream >> jsonData;
			fileStream.close();

			CWeenieDefaults *pDefaults = new CWeenieDefaults();
			if (pDefaults->m_Qualities.UnPackJson(jsonData))
			{
				pDefaults->m_WCID = pDefaults->m_Qualities.id;
				pDefaults->m_sourceFile = filePath;

				bool bShouldInsert = true;
				bool bDuplicate = false;

				auto existing = m_WeenieDefaults.find(pDefaults->m_WCID);
				if (existing != m_WeenieDefaults.end())
				{
					if (refresh)
					{
					}
					else
					{
						if (existing->second->m_bIsAutoGenerated)
						{
							if (pDefaults->m_bIsAutoGenerated)
							{
								bDuplicate = true;
								bShouldInsert = false;
							}
						}
						else
						{
							// existing was not auto-generated
							bShouldInsert = false;

							if (!pDefaults->m_bIsAutoGenerated)
							{
								bDuplicate = true;
							}
						}
					}
				}

				if (bShouldInsert)
				{
					CWeenieDefaults *toDelete = NULL;

					auto existing = m_WeenieDefaults.find(pDefaults->m_WCID);
					if (existing != m_WeenieDefaults.end())
					{
						toDelete = existing->second;
					}

					pDefaults->m_sourceFile = filePath;
					m_WeenieDefaults[pDefaults->m_WCID] = pDefaults;

					std::string name;
					if (pDefaults->m_Qualities.InqString(NAME_STRING, name))
					{
						// convert name for easier matching, lowercase and remove any spaces
						std::transform(name.begin(), name.end(), name.begin(), ::tolower);
						name.erase(remove_if(name.begin(), name.end(), isspace), name.end());

						// remove if this WCID is already in here
						for (std::multimap<std::string, CWeenieDefaults *>::iterator i = m_WeenieDefaultsByName.begin(); i != m_WeenieDefaultsByName.end(); i++)
						{
							if (i->second->m_Qualities.GetID() == pDefaults->m_Qualities.GetID())
							{
								m_WeenieDefaultsByName.erase(i);
								break;
							}
						}

						m_WeenieDefaultsByName.insert(std::pair<std::string, CWeenieDefaults *>(name, pDefaults));
					}

					if (toDelete)
					{
						delete toDelete;
					}
				}
				else
				{
					if (bDuplicate)
					{
						LOG_PRIVATE(Data, Warning, "Duplicate WCID %d, ignoring WQF file: \"%s\" due to \"%s\"\n", pDefaults->m_WCID, filePath.c_str(), m_WeenieDefaults[pDefaults->m_WCID]->m_sourceFile.c_str());
					}

					delete pDefaults;
				}
			}
			else
			{
				delete pDefaults;
			}
		}
	}
}

void CWeenieFactory::LoadLocalStorageIndexed()
{
	BYTE *data = NULL;
	DWORD length = 0;
	if (LoadDataFromPhatDataBin(9, &data, &length, 0xd8fd6b02, 0xa0427974))
	{
		BinaryReader reader(data, length);

		DWORD count = reader.Read<DWORD>();
		for (DWORD i = 0; i < count; i++)
		{
			DWORD the_wcid = reader.Read<DWORD>();

			CWeenieDefaults *pDefaults = new CWeenieDefaults();
			if (pDefaults->UnPack(&reader))
			{
				/*
				if (pDefaults->m_Qualities.m_IntStats)
				{
					for (auto &entry : *pDefaults->m_Qualities.m_IntStats)
					{
						if (entry.first == IMBUED_EFFECT_INT)
						{
							if (entry.second & ArmorRending_ImbuedEffectType)
							{
								DebugBreak();
							}
						}
					}
				}
				*/

				bool bShouldInsert = true;
				bool bDuplicate = false;

				auto existing = m_WeenieDefaults.find(pDefaults->m_WCID);
				if (existing != m_WeenieDefaults.end())
				{
					if (existing->second->m_bIsAutoGenerated)
					{
						if (pDefaults->m_bIsAutoGenerated)
						{
							bDuplicate = true;
							bShouldInsert = false;
						}
					}
					else
					{
						// existing was not auto-generated
						bShouldInsert = false;

						if (!pDefaults->m_bIsAutoGenerated)
						{
							bDuplicate = true;
						}
					}
				}

				if (bShouldInsert)
				{
					auto existing = m_WeenieDefaults.find(pDefaults->m_WCID);
					if (existing != m_WeenieDefaults.end())
						delete existing->second;

					//pDefaults->m_sourceFile = filePath;
					m_WeenieDefaults[pDefaults->m_WCID] = pDefaults;

					std::string name;
					if (pDefaults->m_Qualities.InqString(NAME_STRING, name))
					{
						// convert name for easier matching, lowercase and remove any spaces
						std::transform(name.begin(), name.end(), name.begin(), ::tolower);
						name.erase(remove_if(name.begin(), name.end(), isspace), name.end());

						// remove if this WCID is already in here
						/*
						for (std::multimap<std::string, CWeenieDefaults *>::iterator i = m_WeenieDefaultsByName.begin(); i != m_WeenieDefaultsByName.end(); i++)
						{
							if (i->second->m_Qualities.GetID() == pDefaults->m_Qualities.GetID())
							{
								m_WeenieDefaultsByName.erase(i);
								break;
							}
						}
						*/

						m_WeenieDefaultsByName.insert(std::pair<std::string, CWeenieDefaults *>(name, pDefaults));
					}
				}
				else
				{
					if (bDuplicate)
					{
						SERVER_ERROR << "Duplicate WCID" << pDefaults->m_WCID << ", ignoring due to"<< m_WeenieDefaults[pDefaults->m_WCID]->m_sourceFile.c_str();
					}

					delete pDefaults;
				}
			}
			else
			{
				SERVER_ERROR << "Error parsing weenie defaults file";
				delete pDefaults;
			}
		}

		delete[] data;
	}	
}

void CWeenieFactory::LoadAvatarData()
{
	m_NumAvatars = 0;

	BYTE *data = NULL;
	DWORD length = 0;
	if (LoadDataFromPhatDataBin(7, &data, &length, 0x591c34e9, 0x2250b020))
	{
		BinaryReader reader(data, length);
		PackableHashTable<std::string, ObjDesc, std::string> avatarTable;
		avatarTable.UnPack(&reader);
		CWeenieDefaults *pRobeWithHood = GetWeenieDefaults(5851);
		
		DWORD autoWcid = m_FirstAvatarWCID = 7000000;

		if (pRobeWithHood)
		{
			for (auto &avatar : avatarTable)
			{
				autoWcid++;
				m_NumAvatars++;

				CWeenieDefaults *pDefaults = new CWeenieDefaults();
				pDefaults->m_Qualities.CopyFrom(&pRobeWithHood->m_Qualities);
				pDefaults->m_Qualities.SetString(NAME_STRING, csprintf("%s Jumpsuit", avatar.first.c_str()));
				pDefaults->m_Qualities.SetInt(CLOTHING_PRIORITY_INT, pDefaults->m_Qualities.GetInt(CLOTHING_PRIORITY_INT, 0) | HAND_WEAR_CLOTHING_PRIORITY);
				pDefaults->m_Qualities.SetInt(LOCATIONS_INT, pDefaults->m_Qualities.GetInt(LOCATIONS_INT, 0) | HAND_WEAR_LOC);
				pDefaults->m_Qualities.SetDataID(ICON_DID, 0x6001036);
				pDefaults->m_Qualities.SetBool(IGNORE_CLO_ICONS_BOOL, TRUE);
				pDefaults->m_WornObjDesc = avatar.second;
				pDefaults->m_WCID = autoWcid;
				pDefaults->m_Qualities.id = autoWcid;

				CWeenieDefaults *toDelete = NULL;

				auto existing = m_WeenieDefaults.find(pDefaults->m_WCID);
				if (existing != m_WeenieDefaults.end())
				{
					toDelete = existing->second;
				}

				m_WeenieDefaults[pDefaults->m_WCID] = pDefaults;

				std::string name;
				if (pDefaults->m_Qualities.InqString(NAME_STRING, name))
				{
					// convert name for easier matching, lowercase and remove any spaces
					std::transform(name.begin(), name.end(), name.begin(), ::tolower);
					name.erase(remove_if(name.begin(), name.end(), isspace), name.end());

					// remove if this WCID is already in here
					/*
					for (std::multimap<std::string, CWeenieDefaults *>::iterator i = m_WeenieDefaultsByName.begin(); i != m_WeenieDefaultsByName.end(); i++)
					{
						if (i->second->m_Qualities.GetID() == pDefaults->m_Qualities.GetID())
						{
							m_WeenieDefaultsByName.erase(i);
							break;
						}
					}
					*/

					m_WeenieDefaultsByName.insert(std::pair<std::string, CWeenieDefaults *>(name, pDefaults));
				}

				if (toDelete)
				{
					delete toDelete;
				}
			}
		}

		delete[] data;
	}
}

std::shared_ptr<CWeenieObject> CWeenieFactory::CreateWeenieByClassID(DWORD wcid, const Position *pos, bool bSpawn)
{
	if (!wcid)
		return NULL;

	CWeenieDefaults *defaults = GetWeenieDefaults(wcid);
	if (!defaults)
		return NULL;

	return CreateWeenie(defaults, pos, bSpawn);
}

std::shared_ptr<CWeenieObject> CWeenieFactory::CreateWeenieByName(const char *name, const Position *pos, bool bSpawn)
{
	CWeenieDefaults *defaults = GetWeenieDefaults(name);
	if (!defaults)
		return NULL;

	return CreateWeenie(defaults, pos, bSpawn);
}

std::shared_ptr<CWeenieObject> CWeenieFactory::CreateBaseWeenieByType(int weenieType, unsigned int wcid, const char *weenieName)
{
	//Disabled this as it's only used during character creation, and at that point we don't want to treat 
	//the weenie as a CPlayerWeenie as that entails client communication and both the client and the weenie
	//are not ready for that at that moment.
	//if (wcid == 1)
	//	return new CPlayerWeenie(NULL, 0, 0);

	std::shared_ptr<CWeenieObject> weenie;
	switch (weenieType)
	{
	case Generic_WeenieType:
	case Admin_WeenieType:
		{
			weenie = std::shared_ptr<CWeenieObject>((new CWeenieObject())->GetPointer<CWeenieObject>(true));
			break;
		}

	case Cow_WeenieType:
	case Creature_WeenieType:
		{
		if (!strcmp(weenieName, "Town Crier"))
			weenie = std::shared_ptr<CTownCrier>((new CTownCrier())->GetPointer<CTownCrier>(true));
			else
				weenie = std::shared_ptr<CMonsterWeenie>((new CMonsterWeenie())->GetPointer<CMonsterWeenie>(true));
			break;
		}
	case AttributeTransferDevice_WeenieType:
		{
			weenie = std::shared_ptr<CAttributeTransferDeviceWeenie>((new CAttributeTransferDeviceWeenie())->GetPointer<CAttributeTransferDeviceWeenie>(true));
			break;
		}
	case SkillAlterationDevice_WeenieType:
		{
			weenie = std::shared_ptr<CSkillAlterationDeviceWeenie>((new CSkillAlterationDeviceWeenie())->GetPointer<CSkillAlterationDeviceWeenie>(true));
			break;
		}
	case Container_WeenieType:
		{
			weenie = std::shared_ptr<CContainerWeenie>((new CContainerWeenie())->GetPointer<CContainerWeenie>(true));
			break;
		}
	case Portal_WeenieType:
		{
			weenie = std::shared_ptr<CPortal>((new CPortal())->GetPointer<CPortal>(true));
			break;
		}
	case LifeStone_WeenieType:
		{
			weenie = std::shared_ptr<CBaseLifestone>((new CBaseLifestone())->GetPointer<CBaseLifestone>(true));
			break;
		}
	case AllegianceBindstone_WeenieType:
		{
			weenie = std::shared_ptr<CBindStone>((new CBindStone())->GetPointer<CBindStone>(true));
			break;
		}
	case Door_WeenieType:
		{
			weenie = std::shared_ptr<CBaseDoor>((new CBaseDoor())->GetPointer<CBaseDoor>(true));
			break;
		}
	case HotSpot_WeenieType:
		{
			weenie = std::shared_ptr<CHotSpotWeenie>((new CHotSpotWeenie())->GetPointer<CHotSpotWeenie>(true));
			break;
		}
	case Switch_WeenieType:
		{
			weenie = std::shared_ptr<CSwitchWeenie>((new CSwitchWeenie())->GetPointer<CSwitchWeenie>(true));
			break;
		}
	case PressurePlate_WeenieType:
		{
			weenie = std::shared_ptr<CPressurePlateWeenie>((new CPressurePlateWeenie())->GetPointer<CPressurePlateWeenie>(true));
			break;
		}
	case Vendor_WeenieType:
		{
			/*
			if (wcid == 719)
				weenie = std::shared_ptr<CAvatarVendor>((new CAvatarVendor())->GetPointer<CAvatarVendor>(true)); // holtburg avatar vendor
			else
			*/
			weenie = std::shared_ptr<CVendor>((new CVendor())->GetPointer<CVendor>(true));
			break;
		}

	case Clothing_WeenieType:
		{
			weenie = std::shared_ptr<CClothingWeenie>((new CClothingWeenie())->GetPointer<CClothingWeenie>(true));
			break;
		}
	case Caster_WeenieType:
		{
			weenie = std::shared_ptr<CCasterWeenie>((new CCasterWeenie())->GetPointer<CCasterWeenie>(true));
			break;
		}
	case Healer_WeenieType:
		{
			weenie = std::shared_ptr<CHealerWeenie>((new CHealerWeenie())->GetPointer<CHealerWeenie>(true));
			break;
		}
	case House_WeenieType:
		{
			weenie = std::shared_ptr<CHouseWeenie>((new CHouseWeenie())->GetPointer<CHouseWeenie>(true));
			break;
		}
	case SlumLord_WeenieType:
		{
			weenie = std::shared_ptr<CSlumLordWeenie>((new CSlumLordWeenie())->GetPointer<CSlumLordWeenie>(true));
			break;
		}
	case Scroll_WeenieType:
		{
			weenie = std::shared_ptr<CScrollWeenie>((new CScrollWeenie())->GetPointer<CScrollWeenie>(true));
			break;
		}

	case Ammunition_WeenieType:
		{
			weenie = std::shared_ptr<CAmmunitionWeenie>((new CAmmunitionWeenie())->GetPointer<CAmmunitionWeenie>(true));
			break;
		}

	case Missile_WeenieType:
		{
			weenie = std::shared_ptr<CMissileWeenie>((new CMissileWeenie())->GetPointer<CMissileWeenie>(true));
			break;
		}

	case ManaStone_WeenieType:
		{
			weenie = std::shared_ptr<CManaStoneWeenie>((new CManaStoneWeenie())->GetPointer<CManaStoneWeenie>(true));
			break;
		}
	case Coin_WeenieType:
		{
			weenie = std::shared_ptr<CWeenieObject>((new CWeenieObject())->GetPointer<CWeenieObject>(true));
			break;
		}
	case Book_WeenieType:
		{
			weenie = std::shared_ptr<CBookWeenie>((new CBookWeenie())->GetPointer<CBookWeenie>(true));
			break;
		}
	case Food_WeenieType:
		{
			weenie = std::shared_ptr<CFoodWeenie>((new CFoodWeenie())->GetPointer<CFoodWeenie>(true));
			break;
		}
	case PKModifier_WeenieType:
		{
			weenie = std::shared_ptr<CPKModifierWeenie>((new CPKModifierWeenie())->GetPointer<CPKModifierWeenie>(true));
			break;
		}
	case Gem_WeenieType:
		{
			weenie = std::shared_ptr<CGemWeenie>((new CGemWeenie())->GetPointer<CGemWeenie>(true));
			break;
		}
	case Key_WeenieType:
		{
			weenie = std::shared_ptr<CKeyWeenie>((new CKeyWeenie())->GetPointer<CKeyWeenie>(true));
			break;
		}
	case Lockpick_WeenieType:
		{
			weenie = std::shared_ptr<CLockpickWeenie>((new CLockpickWeenie())->GetPointer<CLockpickWeenie>(true));
			break;
		}
	case Corpse_WeenieType:
		{
			weenie = std::shared_ptr<CCorpseWeenie>((new CCorpseWeenie())->GetPointer<CCorpseWeenie>(true));
			break;
		}
	case MeleeWeapon_WeenieType:
		{
			weenie = std::shared_ptr<CMeleeWeaponWeenie>((new CMeleeWeaponWeenie())->GetPointer<CMeleeWeaponWeenie>(true));
			break;
		}
	case MissileLauncher_WeenieType:
		{
			weenie = std::shared_ptr<CMissileLauncherWeenie>((new CMissileLauncherWeenie())->GetPointer<CMissileLauncherWeenie>(true));
			break;
		}
	case Hook_WeenieType:
		{
			weenie = std::shared_ptr<CHookWeenie>((new CHookWeenie())->GetPointer<CHookWeenie>(true));
			break;
		}
	case Deed_WeenieType:
		{
			weenie = std::shared_ptr<CDeedWeenie>((new CDeedWeenie())->GetPointer<CDeedWeenie>(true));
			break;
		}
	case BootSpot_WeenieType:
		{
			weenie = std::shared_ptr<CBootSpotWeenie>((new CBootSpotWeenie())->GetPointer<CBootSpotWeenie>(true));
			break;
		}
	case HousePortal_WeenieType:
		{
			weenie = std::shared_ptr<CHousePortalWeenie>((new CHousePortalWeenie())->GetPointer<CHousePortalWeenie>(true));
			break;
		}
	case Chest_WeenieType:
		{
			weenie = std::shared_ptr<CChestWeenie>((new CChestWeenie())->GetPointer<CChestWeenie>(true));
			break;
		}
	case Storage_WeenieType:
		{
			weenie = std::shared_ptr<CStorageWeenie>((new CStorageWeenie())->GetPointer<CStorageWeenie>(true));
			break;
		}
	case AugmentationDevice_WeenieType:
		{
			weenie = std::shared_ptr<CAugmentationDeviceWeenie>((new CAugmentationDeviceWeenie())->GetPointer<CAugmentationDeviceWeenie>(true));
			break;
		}
	default:
		weenie = std::shared_ptr<CWeenieObject>((new CWeenieObject())->GetPointer<CWeenieObject>(true));
		break;
	}

	return weenie;
}

std::shared_ptr<CWeenieObject> CWeenieFactory::CreateWeenie(CWeenieDefaults *defaults, const Position *pos, bool bSpawn)
{
	std::shared_ptr<CWeenieObject> weenie = CreateBaseWeenieByType(
		(defaults->m_Qualities.GetInt(ITEM_TYPE_INT, 0) & TYPE_VESTEMENTS) ? Clothing_WeenieType : defaults->m_Qualities.m_WeenieType,
		defaults->m_Qualities.GetID(), defaults->m_Qualities.GetString(NAME_STRING, "").c_str());
	ApplyWeenieDefaults(weenie, defaults);
	weenie->ApplyQualityOverrides();

	if (!weenie->IsAvatarJumpsuit())
		TryToResolveAppearanceData(weenie);

	if (weenie->IsCreature()) 
		weenie->SetMaxVitals();

	if (pos)
		weenie->SetInitialPosition(*pos);

	if (bSpawn)
	{
		if (!g_pWorld->CreateEntity(weenie))
			return NULL;
	}

	return weenie;
}

std::shared_ptr<CWeenieObject> CWeenieFactory::CloneWeenie(std::shared_ptr<CWeenieObject> weenie)
{
	std::shared_ptr<CWeenieObject> clone = CreateBaseWeenieByType(
		(weenie->m_Qualities.GetInt(ITEM_TYPE_INT, 0) & TYPE_VESTEMENTS) ? Clothing_WeenieType : weenie->m_Qualities.m_WeenieType,
		weenie->m_Qualities.GetID(), weenie->InqStringQuality(NAME_STRING, "").c_str());
	clone->m_Qualities.CopyFrom(&weenie->m_Qualities);
	clone->m_bObjDescOverride = weenie->m_bObjDescOverride;
	clone->m_ObjDescOverride = weenie->m_ObjDescOverride;
	clone->m_WornObjDesc = weenie->m_WornObjDesc;
	return clone;
}

void CWeenieFactory::AddWeenieToDestination(std::shared_ptr<CWeenieObject> weenie, std::shared_ptr<CWeenieObject> parent, DWORD destinationType, bool isRegenLocationType, const GeneratorProfile *profile)
{
	if (!isRegenLocationType)
	{
		switch ((DestinationType)destinationType)
		{
		case Contain_DestinationType:
			destinationType = Contain_RegenLocationType;
			break;
		case ContainTreasure_DestinationType:
			destinationType = ContainTreasure_RegenLocationType;
			break;
		case Wield_DestinationType:
			destinationType = Wield_RegenLocationType;
			break;
		case WieldTreasure_DestinationType:
			destinationType = WieldTreasure_RegenLocationType;
			break;
		case Shop_DestinationType:
			destinationType = Shop_RegenLocationType;
			break;
		case ShopTreasure_DestinationType:
			destinationType = ShopTreasure_RegenLocationType;
			break;
		case Checkpoint_DestinationType:
			destinationType = Checkpoint_RegenLocationType;
			break;
		case Treasure_DestinationType:
			destinationType = Treasure_RegenLocationType;
			break;
		case HouseBuy_DestinationType:
		case HouseRent_DestinationType:
			return;
		}
	}

	Position pos = parent->GetPosition();

	switch ((RegenLocationType)destinationType)
	{
	case Contain_RegenLocationType:
	case ContainTreasure_RegenLocationType:
		if (std::shared_ptr<CContainerWeenie> container = parent->AsContainer())
			container->SpawnInContainer(weenie);
		break;
	case Wield_RegenLocationType:
	case WieldTreasure_RegenLocationType:
		if (std::shared_ptr<CMonsterWeenie> creature = parent->AsMonster())
			creature->SpawnWielded(weenie);
		else if (std::shared_ptr<CContainerWeenie> container = parent->AsContainer())
			container->SpawnInContainer(weenie);
		break;
	case Specific_RegenLocationType:
	case SpecificTreasure_RegenLocationType:
		if (!profile->pos_val.objcell_id)
			pos.frame.m_origin = pos.localtoglobal(profile->pos_val.frame.m_origin);
		else
			pos = profile->pos_val;

		weenie->SetInitialPosition(pos);
		if (!g_pWorld->CreateEntity(weenie))
		{
			SERVER_ERROR << "Failed creating generated spawn" << GetWCIDName(profile->type);
			return;
		}
		break;
	case Scatter_RegenLocationType:
	case ScatterTreasure_RegenLocationType:
		if ((pos.objcell_id & 0xFFFF) < 0x100)
		{
			// outdoors
			double genRadius = parent->InqFloatQuality(GENERATOR_RADIUS_FLOAT, 0.0f);
			double random_x = Random::GenFloat(-genRadius, genRadius);
			double random_y = Random::GenFloat(-genRadius, genRadius);

			pos.frame.m_origin += Vector(random_x, random_y, 0.0f);
			pos.frame.m_origin.z = CalcSurfaceZ(pos.objcell_id, pos.frame.m_origin.x, pos.frame.m_origin.y, false);

			if (pos.frame.m_origin.x < 0.5f)
				pos.frame.m_origin.x = 0.5f;
			if (pos.frame.m_origin.y < 0.5f)
				pos.frame.m_origin.y = 0.5f;
		}
		else
		{
			double genRadius = parent->InqFloatQuality(GENERATOR_RADIUS_FLOAT, 0.0f);
			double random_x = Random::GenFloat(-genRadius, genRadius);
			double random_y = Random::GenFloat(-genRadius, genRadius);
			pos.frame.m_origin += Vector(random_x, random_y, 0.0f);
		}

		weenie->SetInitialPosition(pos);
		if (!g_pWorld->CreateEntity(weenie))
		{
			SERVER_ERROR << "Failed creating generated spawn" << GetWCIDName(profile->type);
			return;
		}
		break;
	case OnTop_RegenLocationType:
	case OnTopTreasure_RegenLocationType:
		//untested
		if (!profile->pos_val.objcell_id)
			pos.frame.m_origin = pos.localtoglobal(profile->pos_val.frame.m_origin);
		else
			pos = profile->pos_val;

		if ((pos.objcell_id & 0xFFFF) < 0x100) //outdoors
		pos.frame.m_origin.z = CalcSurfaceZ(pos.objcell_id, pos.frame.m_origin.x, pos.frame.m_origin.y, false);

		weenie->SetInitialPosition(pos);
		if (!g_pWorld->CreateEntity(weenie))
		{
			SERVER_ERROR << "Failed creating generated spawn" << GetWCIDName(profile->type);
			return;
		}
		break;
	case Shop_RegenLocationType:
	case ShopTreasure_RegenLocationType:
		break;
	case Treasure_RegenLocationType:
		break;
	case Checkpoint_RegenLocationType:
		break;
	}

	if (isRegenLocationType && profile != NULL)
	{
		weenie->m_Qualities.SetInstanceID(GENERATOR_IID, parent->GetID());

		if (!weenie->IsContained() && !weenie->IsStuck() && weenie->m_Position.objcell_id && !weenie->cell)
		{
			g_pWorld->RemoveEntity(weenie);
			return;
		}

		GeneratorRegistryNode node;
		node.m_wcidOrTtype = profile->type;
		node.slot = profile->slot;
		node.ts = Timer::cur_time;
		node.amount = weenie->InqIntQuality(STACK_SIZE_INT, 1);
		node.checkpointed = 0;
		node.m_bTreasureType = profile->IsTreasureType();
		node.shop = 0;
		node.m_objectId = weenie->GetID();

		if (!parent->m_Qualities._generator_registry)
			parent->m_Qualities._generator_registry = new GeneratorRegistry();
		parent->m_Qualities._generator_registry->_registry.add(weenie->GetID(), &node);

		weenie->ChanceExecuteEmoteSet(parent->GetID(), Generation_EmoteCategory);
	}
}

int CWeenieFactory::AddFromCreateList(std::shared_ptr<CWeenieObject> parent, PackableListWithJson<CreationProfile> *createList, DestinationType validDestinationTypes)
{
	int amountCreated = 0;

	if (!createList)
		return amountCreated;

	bool seekingNext = true;
	bool forceNext = true;
	double diceRoll;
	double accumulatedChance;

	for (auto i = createList->begin(); i != createList->end(); i++)
	{
		float probability = 0.0;
		bool isTreasureDestination = false;
		if (i->destination & Treasure_DestinationType)
		{
			isTreasureDestination = true;
			probability = i->shade * g_pConfig->DropRateMultiplier();
		}
		// By virtue of the way the loot system works increasing the drop rate may cause some items to actually drop less often.
		// Items are grouped in "sets" and only one item per set will actually drop, if the drop chance of the first item is increased
		// the chance of the subsequent items decreases. Although this isn't used very often in the data. Most of the time there's just one item
		// per set. For a good use case see the Drudge Robber Baron.
		// If we wanted we could do this properly by checking the wcid == 0 entries and reallocating the chance of nothing dropping to the items.
		// This would increase the drop rate as much as it is possible without lowering other item drop rates. This would be best done as a modification
		// to the data and not on the fly.

		if (seekingNext)
		{
			if (forceNext || accumulatedChance >= 1.0)
			{
				// We've reached the end of this set.
				seekingNext = false;
				forceNext = false;
				diceRoll = Random::RollDice(0.0, 1.0);
				accumulatedChance = 0.0;
			}
			else
			{
				// Just keep going.
				accumulatedChance += probability;
				continue;
			}
		}

		if (!isTreasureDestination || diceRoll <= accumulatedChance + probability || probability <= 0.0)
		{
			// We're the one to spawn!
			if (i->wcid != 0) // Unless we're a big pile of nothing.
			{
				if (validDestinationTypes == DestinationType::Undef_DestinationType || (i->destination & validDestinationTypes)) // Or we're not of the correct destinationType.
				{
					if (std::shared_ptr<CWeenieObject> newItem = g_pWeenieFactory->CreateWeenieByClassID(i->wcid, NULL, false))
					{
						if (i->palette)
							newItem->m_Qualities.SetInt(PALETTE_TEMPLATE_INT, i->palette);

						if(!isTreasureDestination && i->shade > 0.0) // treasure destinations use the shade variable as a probability to spawn.
							newItem->m_Qualities.SetFloat(SHADE_FLOAT, i->shade);

						int maxStackSize = newItem->InqIntQuality(MAX_STACK_SIZE_INT, 1);
						if (i->amount > 1)
						{
							if (i->amount <= maxStackSize)
								newItem->SetStackSize(i->amount);
							else
								newItem->SetStackSize(maxStackSize);
						}

						AddWeenieToDestination(newItem, parent, i->destination, false);
						amountCreated++;
					}
				}
			}
			accumulatedChance += probability;
			seekingNext = true;
			if (!isTreasureDestination)
				forceNext = true;
		}
		else
			accumulatedChance += probability;

		if (isTreasureDestination && i->wcid == 0)
		{
			//we've reached the end of this set even if the probabilities didn't add up to 1.0.
			forceNext = true;
			seekingNext = true;
		}
	}

	return amountCreated;
}

int CWeenieFactory::AddFromGeneratorTable(std::shared_ptr<CWeenieObject> parent, bool isInit)
{
	int numSpawned = 0;

	if (!parent->m_Qualities._generator_table)
		return numSpawned;

	int maxSpawns = isInit ? parent->InqIntQuality(INIT_GENERATED_OBJECTS_INT, 0) : parent->InqIntQuality(MAX_GENERATED_OBJECTS_INT, 0);

	if (!isInit)
	{
		if (parent->m_Qualities._generator_registry)
			numSpawned += (int)parent->m_Qualities._generator_registry->_registry.size();
	}

	if (numSpawned >= maxSpawns)
		return numSpawned;

	std::list<GeneratorProfile> genList = isInit ? parent->m_Qualities._generator_table->GetInitialGenerationList() : parent->m_Qualities._generator_table->GetGenerationList();

	// always spawn everything with zero or subzero values
	for (GeneratorProfile &entry : genList)
	{
		bool fullLoopWithNoValidEntries = true;
		for (GeneratorProfile &entry : genList)
		{
			if (!parent->IsGeneratorSlotReady(entry.slot))
				continue;

			if (entry.probability > 0 || entry.IsPlaceHolder())
				continue;

			fullLoopWithNoValidEntries = false;

			int maxCreate = isInit ? entry.initCreate : entry.maxNum;
			if (maxCreate < 0) //this is -1 for unlimited amount;
				maxCreate = maxSpawns;
			maxCreate = min(maxCreate, maxSpawns - numSpawned); //cap maxCreate to however many we can still spawn.

			int amountCreated;
			for (amountCreated = 0; amountCreated < maxCreate; amountCreated++)
			{
				int newAmount = g_pTreasureFactory->GenerateFromTypeOrWcid(parent, entry.whereCreate, true, entry.type, entry.ptid, entry.shade, &entry);

				//disabled the code below to make each treasure entry count as just 1 spawned item.
				//if (newAmount > 1)
				//	amountCreated += newAmount - 1;
			}

			numSpawned += amountCreated;
			if (numSpawned >= maxSpawns)
				break;
		}

		if (fullLoopWithNoValidEntries)
			break;
	}

	while (numSpawned < maxSpawns)
	{
		float diceRoll = Random::RollDice(0.0f, 1.0f);

		bool fullLoopWithNoValidEntries = true;
		for (GeneratorProfile &entry : genList)
		{
			if (!parent->IsGeneratorSlotReady(entry.slot))
				continue;

			if (entry.probability <= 0 || entry.IsPlaceHolder())
				continue;

			if (diceRoll > entry.probability)
				continue;

			fullLoopWithNoValidEntries = false;

			int maxCreate = isInit ? entry.initCreate : entry.maxNum;
			if (maxCreate < 0) //this is -1 for unlimited amount;
				maxCreate = maxSpawns;
			maxCreate = min(maxCreate, maxSpawns - numSpawned); //cap maxCreate to however many we can still spawn.

			int amountCreated;
			for (amountCreated = 0; amountCreated < maxCreate; amountCreated++)
			{
				int newAmount = g_pTreasureFactory->GenerateFromTypeOrWcid(parent, entry.whereCreate, true, entry.type, entry.ptid, entry.shade, &entry);

				//disabled the code below to make each treasure entry count as just 1 spawned item.
				//if(newAmount > 1)
				//	amountCreated += newAmount - 1;
			}

			numSpawned += amountCreated;
			if (numSpawned >= maxSpawns)
				break;
		}

		if (fullLoopWithNoValidEntries)
			break;
	}

	if (isInit)
	{
		int numSpawned = parent->m_Qualities._generator_registry ? (int)parent->m_Qualities._generator_registry->_registry.size() : 0;
		if ((!parent->m_Qualities._generator_queue || parent->m_Qualities._generator_queue->_queue.empty()) && numSpawned >= parent->InqIntQuality(MAX_GENERATED_OBJECTS_INT, 0, TRUE))
			parent->_nextRegen = -1.0;
		else
			parent->_nextRegen = Timer::cur_time + (parent->InqFloatQuality(REGENERATION_INTERVAL_FLOAT, -1.0, TRUE) * g_pConfig->RespawnTimeMultiplier());
	}

	return numSpawned;
}

int CWeenieFactory::GenerateFromTypeOrWcid(std::shared_ptr<CWeenieObject> parent, const GeneratorProfile *profile)
{
	return g_pTreasureFactory->GenerateFromTypeOrWcid(parent, profile->whereCreate, true, profile->type, profile->ptid, profile->shade, profile);
}

int CWeenieFactory::GenerateFromTypeOrWcid(std::shared_ptr<CWeenieObject> parent, RegenLocationType destinationType, DWORD treasureTypeOrWcid, unsigned int ptid, float shade)
{
	return g_pTreasureFactory->GenerateFromTypeOrWcid(parent, destinationType, true, treasureTypeOrWcid, ptid, shade);
}

int CWeenieFactory::GenerateFromTypeOrWcid(std::shared_ptr<CWeenieObject> parent, DestinationType destinationType, DWORD treasureTypeOrWcid, unsigned int ptid, float shade)
{
	return g_pTreasureFactory->GenerateFromTypeOrWcid(parent, destinationType, false, treasureTypeOrWcid, ptid, shade);
}

bool CWeenieFactory::TryToResolveAppearanceData(std::shared_ptr<CWeenieObject> weenie)
{
	// this function is temporary

	if (weenie->GetItemType() & (ITEM_TYPE::TYPE_ARMOR | ITEM_TYPE::TYPE_CLOTHING))
	{
		DWORD iconID = weenie->GetIcon();

		if (iconID)
		{
			DWORD paletteKey;
			ClothingTable *pCT = g_ClothingCache.GetTableByIndexOfIconID(iconID, 0, &paletteKey);

			if (pCT)
			{
				weenie->m_WornObjDesc.Clear();
				weenie->m_WornObjDesc.paletteID = 0x0400007E; // pObject->m_miBaseModel.dwBasePalette;

				double shade = 1.0;
				pCT->BuildObjDesc(0x02000001, paletteKey, &ShadePackage(shade)/* &ShadePackage(Random::GenFloat(0.0, 1.0))*/, &weenie->m_WornObjDesc);
				
				// use the original palettes specified?		
				weenie->m_WornObjDesc.ClearSubpalettes();
				Subpalette *pSPC = weenie->m_ObjDescOverride.firstSubpal;
				while (pSPC)
				{
					weenie->m_WornObjDesc.AddSubpalette(new Subpalette(*pSPC));
					pSPC = pSPC->next;
				}

				ClothingTable::Release(pCT);
				return true;
			}
		}
	}

	return false;
}


