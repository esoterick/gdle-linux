
#include "StdAfx.h"
#include "ObjectIDGen.h"
#include "DatabaseIO.h"

const uint32_t IDQUEUEMAX = 1000000;
const uint32_t IDQUEUEMIN = 5000;
const uint32_t IDRANGESTART = 0x80000000;
const uint32_t IDRANGEEND = 0xff000000;

CObjectIDGenerator::CObjectIDGenerator()
{
	LoadState();
}

CObjectIDGenerator::~CObjectIDGenerator()
{
	if (m_thrLoadState.joinable())
	{
		m_thrLoadState.join();
	}
}

void CObjectIDGenerator::LoadState()
{
	uint32_t start = IDRANGESTART;

	if (!m_lpuiIntervals.empty())
	{
		start = m_lpuiIntervals.end()->second;

		if (start >= IDRANGEEND)
		{
			start = IDRANGESTART;
		}
	}

	// get list from db
	m_lpuiIntervals.merge(g_pDBIO->GetUnusedIdRanges(start, IDRANGEEND));

	// if it returned nothing, we have the full requested range, or we're screwed
	if (m_lpuiIntervals.empty())
		m_lpuiIntervals.push_back(std::pair<unsigned int, unsigned int>(IDRANGESTART, IDRANGEEND));

	m_bLoadingState = false;
}

DWORD CObjectIDGenerator::GenerateGUID(eGUIDClass type)
{
	switch (type)
	{
	case eDynamicGUID:
	{
		if (m_lpuiIntervals.empty())
		{
			m_thrLoadState.join();
		}

		auto puiInterval = m_lpuiIntervals.begin();
		DWORD result = puiInterval->first;

		puiInterval->first++;

		if (puiInterval->first == puiInterval->second)
		{
			m_lpuiIntervals.pop_front();
		}

		if (!m_bLoadingState && m_lpuiIntervals.size() < 500)
		{
			if (m_thrLoadState.joinable())
			{
				m_thrLoadState.join();
			}

			m_bLoadingState = true;

			m_thrLoadState = std::thread(&CObjectIDGenerator::LoadState, this);
		}

		return result;
	}	// case eDynamicGUID

	}	// switch (type)

	return 0;
}