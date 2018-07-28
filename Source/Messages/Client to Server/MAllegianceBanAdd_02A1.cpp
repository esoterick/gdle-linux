#include "stdafx.h"
#include "MAllegianceBanAdd_02A1.h"
#include "AllegianceManager.h"
#include "Player.h"
#include "World.h"

MAllegianceBanAdd_02A1::MAllegianceBanAdd_02A1(CPlayerWeenie* player)
{
	m_pPlayer = player;
}

void MAllegianceBanAdd_02A1::Parse(BinaryReader * reader)
{
	m_szCharName = reader->ReadString();

	if (reader->GetLastError())
	{
		SERVER_ERROR << "Error parsing an add allegiance ban message (0x02A1) from the client.";
		return;
	}

	Process();
}

void MAllegianceBanAdd_02A1::Process()
{
	DWORD bannedPlayerID = g_pWorld->GetPlayerId(m_szCharName.c_str());

	g_pAllegianceManager->AddBan(m_pPlayer, bannedPlayerID);
	// TODO check for applicable WERRORs
}
