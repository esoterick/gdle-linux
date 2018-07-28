#include "stdafx.h"
#include "MAllegianceUpdate_001F.h"
#include "Client.h"
#include "ClientEvents.h"

MAllegianceUpdate_001F::MAllegianceUpdate_001F(CPlayerWeenie * player)
{
	m_pPlayer = player;
}

void MAllegianceUpdate_001F::Parse(BinaryReader * reader)
{
	m_bOn = reader->Read<bool>();

	if (reader->GetLastError())
	{
		SERVER_ERROR << "Error parsing an allegiance update request message (0x001F) from the client.";
		return;
	}

	Process();
}

void MAllegianceUpdate_001F::Process()
{
	// need to re-implement this
	m_pPlayer->GetClient()->GetEvents()->SetRequestAllegianceUpdate(static_cast<int>(m_bOn));
	// TODO: replace with server to client message
}
