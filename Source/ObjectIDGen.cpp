
#include "StdAfx.h"
#include "ObjectIDGen.h"
#include "DatabaseIO.h"

const uint32_t IDQUEUEMAX = 1000;
const uint32_t IDQUEUEMIN = 10;

CObjectIDGenerator::CObjectIDGenerator()
{
	m_dwHintDynamicGUID = 0x80000000;
	LoadState();
}

CObjectIDGenerator::~CObjectIDGenerator()
{
}

void CObjectIDGenerator::LoadState()
{
	// get list from db
	std::list< std::pair<uint32_t, uint32_t> > results = g_pDBIO->GetUnusedIdRanges(0x80000000, 0xff000000);

	// populate queue
	std::for_each(results.begin(), results.end(), [&](std::pair<uint32_t, uint32_t>& item)
		{
			size_t length = m_availableDynIds.size();
			for (uint32_t i = item.first; length < IDQUEUEMAX && i < item.second; i++, length++)
			{
				m_availableDynIds.push(i);
			}
		});
}

DWORD CObjectIDGenerator::GenerateGUID(eGUIDClass type)
{
	switch (type)
	{
	case eDynamicGUID:
		{
			if (m_availableDynIds.size() <= IDQUEUEMIN)
			{
				// get some more
				LoadState();
			}
			// if (m_dwHintDynamicGUID >= 0xFF000000)
			// {
			// 	SERVER_ERROR << "Dynamic GUID overflow!";
			// 	return 0;
			// }

			DWORD result = m_availableDynIds.front();
			m_availableDynIds.pop();

			return result;
			//return (++m_dwHintDynamicGUID);
		}
	}

	return 0;
}