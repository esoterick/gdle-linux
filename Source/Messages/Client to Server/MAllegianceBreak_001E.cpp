#include "stdafx.h"
#include "MAllegianceBreak_001E.h"
#include "AllegianceManager.h"

MAllegianceBreak_001E::MAllegianceBreak_001E(CPlayerWeenie * player)
{
	m_pPlayer = player;
}

void MAllegianceBreak_001E::Parse(BinaryReader * reader)
{
	m_dwPatronID = reader->ReadDWORD();

	if (reader->GetLastError())
	{
		SERVER_ERROR << "Error parsing a break allegiance message (0x001E) from the client.";
		return;
	}

	Process();
}

void MAllegianceBreak_001E::Process()
{
	g_pAllegianceManager->TryBreakAllegiance(m_pPlayer, m_dwPatronID); // Allegiance manager does the sanity checks for this
	g_pAllegianceManager->SendAllegianceProfile(m_pPlayer);
}
