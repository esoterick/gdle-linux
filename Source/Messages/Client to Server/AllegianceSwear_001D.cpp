#include "stdafx.h"
#include "AllegianceSwear_001D.h"
#include "AllegianceManager.h"
#include "World.h"

MAllegianceSwear_001D::MAllegianceSwear_001D(CPlayerWeenie * player)
{
	m_pPlayer = player;
}

void MAllegianceSwear_001D::Parse(BinaryReader * reader)
{
	DWORD m_dwTarget = reader->ReadDWORD();

	if (reader->GetLastError())
	{
		SERVER_ERROR << "Error parsing a swear allegiance message (0x001D) from the client.";
		return;
	}

	m_pTarget = g_pWorld->FindPlayer(m_dwTarget);
	if (!m_pTarget)
	{
		m_pPlayer->NotifyWeenieError(WERROR_NO_OBJECT);
		return;
	}

	Process();
}

void MAllegianceSwear_001D::Process()
{
	int error = g_pAllegianceManager->TrySwearAllegiance(m_pPlayer, m_pTarget);
		g_pAllegianceManager->SendAllegianceProfile(m_pPlayer);
}
