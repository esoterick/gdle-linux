
#pragma once

#include <queue>
#include <thread>

enum eGUIDClass {
	ePresetGUID = 0,
	ePlayerGUID = 1,
	// eStaticGUID = 2,
	eDynamicGUID = 3,
	// eItemGUID = 4
};

class CObjectIDGenerator
{
public:
	CObjectIDGenerator();
	~CObjectIDGenerator();

	void LoadState();

	DWORD GenerateGUID(eGUIDClass guidClass);

	void ReleaseObjectId(DWORD id);

protected:
	std::list<std::pair<unsigned int, unsigned int>> m_lpuiIntervals;

	bool m_bLoadingState = false;
	std::thread m_thrLoadState;
};

