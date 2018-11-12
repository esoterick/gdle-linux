#include "StdAfx.h"
#include "easylogging++.h"
#include "Client.h"
#include "WeenieObject.h"
#include "Monster.h"
#include "Player.h"
#include "World.h"

#include "ClientCommands.h"

//CLIENT_COMMAND(setq, "type id value", "Set a quality of last assessed item", ADMIN_ACCESS)
//{
//	if (argc != 3)
//		return true;
//
//	STypeInt stat = (STypeInt)atoi(argv[0]);
//	int32_t value = atoi(argv[1]);
//
//	CWeenieObject* item = g_pWorld->FindObject(pPlayer->m_LastAssessed);
//
//	if (item) {
//		item->m_Qualities.SetInt(stat, value);
//		item->NotifyIntStatUpdated(stat);
//	}
//
//	return false;
//}

CLIENT_COMMAND(setint, "statid value", "Set Int Stat of last assessed item", ADMIN_ACCESS, SERVER_CATEGORY)
{
	if (argc < 2)
		return true;

	STypeInt stat = (STypeInt)atoi(argv[0]);
	int32_t value = atoi(argv[1]);

	CWeenieObject* item = g_pWorld->FindObject(pPlayer->m_LastAssessed);

	if (item) {
		item->m_Qualities.SetInt(stat, value);
		item->NotifyIntStatUpdated(stat);
	}

	return false;
}

CLIENT_COMMAND(setint64, "statid value", "Set Int64 Stat of last assessed item", ADMIN_ACCESS, SERVER_CATEGORY)
{
	if (argc < 2)
		return true;

	STypeInt64 stat = (STypeInt64)atoi(argv[0]);
	int64_t value = atol(argv[1]);

	CWeenieObject* item = g_pWorld->FindObject(pPlayer->m_LastAssessed);

	if (item) {
		item->m_Qualities.SetInt64(stat, value);
		item->NotifyInt64StatUpdated(stat);
	}

	return false;
}

CLIENT_COMMAND(setfloat, "statid value", "Set Float Stat of last assessed item", ADMIN_ACCESS, SERVER_CATEGORY)
{
	if (argc < 2)
		return true;

	STypeFloat stat = (STypeFloat)atoi(argv[0]);
	float value = atof(argv[1]);

	CWeenieObject* item = g_pWorld->FindObject(pPlayer->m_LastAssessed);

	if (item) {
		item->m_Qualities.SetFloat(stat, value);
		item->NotifyFloatStatUpdated(stat);
	}

	return false;
}

CLIENT_COMMAND(setdid, "statid value", "Set DID Stat of last assessed item", ADMIN_ACCESS, SERVER_CATEGORY)
{
	if (argc < 2)
		return true;

	STypeDID stat = (STypeDID)atoi(argv[0]);
	uint32_t value = (uint32_t)atoi(argv[1]);

	CWeenieObject* item = g_pWorld->FindObject(pPlayer->m_LastAssessed);

	if (item) {
		item->m_Qualities.SetDataID(stat, value);
		item->NotifyDIDStatUpdated(stat);
	}

	return false;
}

CLIENT_COMMAND(setiid, "statid value", "Set IID Stat of last assessed item", ADMIN_ACCESS, SERVER_CATEGORY)
{
	if (argc < 2)
		return true;

	STypeIID stat = (STypeIID)atoi(argv[0]);
	uint32_t value = (uint32_t)atoi(argv[1]);

	CWeenieObject* item = g_pWorld->FindObject(pPlayer->m_LastAssessed);

	if (item) {
		item->m_Qualities.SetInstanceID(stat, value);
		item->NotifyIIDStatUpdated(stat);
	}

	return false;
}

CLIENT_COMMAND(setbool, "statid value (0, 1)", "Set Bool Stat of last assessed item", ADMIN_ACCESS, SERVER_CATEGORY)
{
	if (argc < 2)
		return true;

	STypeBool stat = (STypeBool)atoi(argv[0]);
	int32_t value = atoi(argv[1]);

	CWeenieObject* item = g_pWorld->FindObject(pPlayer->m_LastAssessed);

	if (item) {
		item->m_Qualities.SetBool(stat, value);
		item->NotifyBoolStatUpdated(stat);
	}

	return false;
}

CLIENT_COMMAND(setstring, "statid value", "Set String Stat of last assessed item", ADMIN_ACCESS, SERVER_CATEGORY)
{
	if (argc < 2)
		return true;

	STypeString stat = (STypeString)atoi(argv[0]);

	CWeenieObject* item = g_pWorld->FindObject(pPlayer->m_LastAssessed);

	if (item) {
		item->m_Qualities.SetString(stat, argv[1]);
		item->NotifyStringStatUpdated(stat);
	}

	return false;
}

CLIENT_COMMAND(setattr, "attrid value", "Set Attribute base value of last assessed item", ADMIN_ACCESS, SERVER_CATEGORY)
{
	if (argc < 2)
		return true;

	STypeAttribute attr = (STypeAttribute)atoi(argv[0]);
	uint32_t value = (uint32_t)atoi(argv[1]);

	CWeenieObject* item = g_pWorld->FindObject(pPlayer->m_LastAssessed);

	if (item) {
		item->m_Qualities.SetAttribute(attr, value);
		item->NotifyAttributeStatUpdated(attr);
	}

	return false;
}

CLIENT_COMMAND(setvital, "vitalid value", "Set Vital base value of last assessed item", ADMIN_ACCESS, SERVER_CATEGORY)
{
	if (argc < 2)
		return true;

	STypeAttribute2nd attr = (STypeAttribute2nd)atoi(argv[0]);
	uint32_t value = (uint32_t)atoi(argv[1]);

	CWeenieObject* item = g_pWorld->FindObject(pPlayer->m_LastAssessed);

	if (item) {
		item->m_Qualities.SetAttribute2nd(attr, value);
		item->NotifyAttribute2ndStatUpdated(attr);
	}

	return false;
}

CLIENT_COMMAND(setskill, "skillid value", "Set Skill base value of last assessed item", ADMIN_ACCESS, SERVER_CATEGORY)
{
	if (argc < 2)
		return true;

	STypeSkill attr = (STypeSkill)atoi(argv[0]);
	uint32_t value = (uint32_t)atoi(argv[1]);

	CWeenieObject* item = g_pWorld->FindObject(pPlayer->m_LastAssessed);

	if (item) {
		Skill skill;
		item->m_Qualities.InqSkill(attr, skill);
		if (skill._sac == SKILL_ADVANCEMENT_CLASS::UNTRAINED_SKILL_ADVANCEMENT_CLASS)
			skill._sac = SKILL_ADVANCEMENT_CLASS::TRAINED_SKILL_ADVANCEMENT_CLASS;

		skill._init_level = value;

		item->m_Qualities.SetSkill(attr, skill);
		item->NotifySkillStatUpdated(attr);
	}

	return false;
}
