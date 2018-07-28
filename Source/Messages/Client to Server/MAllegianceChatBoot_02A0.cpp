#include "stdafx.h"
#include "MAllegianceChatBoot_02A0.h"
#include "AllegianceManager.h"

MAllegianceChatBoot_02A0::MAllegianceChatBoot_02A0(CPlayerWeenie * player)
{
	m_pPlayer = player;
}

void MAllegianceChatBoot_02A0::Parse(BinaryReader * reader)
{
	m_szCharName = reader->ReadString();
	m_szReason = reader->ReadString();

	if (reader->GetLastError())
	{
		SERVER_ERROR << "Error parsing an allegiance chat boot message (0x02A0) from the client.";
		return;
	}

	Process();
}

void MAllegianceChatBoot_02A0::Process()
{
	// TODO Allegiance Manager needs chat ban/gag functionality finished
}
