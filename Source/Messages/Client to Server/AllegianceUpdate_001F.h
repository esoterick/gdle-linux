#pragma once
#include "BinaryReader.h"
#include "Messages\IClientMessage.h"
#include "Player.h"

// Allegiance_UpdateRequest | 001F
// Request for updated allegiance information
class MAllegianceUpdate_001F : public IClientMessage
{
public:
	MAllegianceUpdate_001F(CPlayerWeenie * player);
	void Parse(BinaryReader *reader);

private:
	void Process();
	CPlayerWeenie* m_pPlayer;
	bool m_bOn;
};