
#include "StdAfx.h"
#include "ObjectIDGen.h"
#include "DatabaseIO.h"
#include "Server.h"

const uint32_t IDQUEUEMAX = 250000;
const uint32_t IDQUEUEMIN = 50000;
const uint32_t IDRANGESTART = 0x80000000;
const uint32_t IDRANGEEND = 0xff000000;

const uint32_t IDEPHEMERALSTART = 0x60000000;
const uint32_t IDEPHEMERALMASK = 0x6fffffff;


CObjectIDGenerator::CObjectIDGenerator()
{
	m_ephemeral = IDEPHEMERALSTART;
	if (g_pConfig->UseIncrementalID())
	{
		WINLOG(Data, Normal, "Using Incremental ID system.....\n");
	}
	else
	{
		// Verify IDRanges tables exists and has values
		if (g_pDBIO->IDRangeTableExistsAndValid())
		{
			WINLOG(Data, Normal, "Using ID Mined system.....\n");
		}
		else
		{
			isIdRangeValid = false;
		}
	}
	LoadRangeStart();
	//LoadState();
}

CObjectIDGenerator::~CObjectIDGenerator()
{
}

CMYSQLQuery* CObjectIDGenerator::GetQuery()
{
	CMYSQLQuery *query = nullptr;

	switch (g_pConfig->IDScanType())
	{
	case 0:
		query = new IdRangeTableQuery(
			m_dwHintDynamicGUID, IDQUEUEMAX, listOfIdsForWeenies, m_lock, m_bLoadingState);
		break;

	case 1:
		query = new ScanWeenieTableQuery(
			m_dwHintDynamicGUID, IDQUEUEMAX, listOfIdsForWeenies, m_lock, m_bLoadingState);
		break;
	}

	return query;
}

void CObjectIDGenerator::LoadState()
{
	if (m_bLoadingState)
		return;

	if (!g_pConfig->UseIncrementalID())
	{
		DEBUG_DATA << "Queue ID Scan Type: " << g_pConfig->IDScanType();

		g_pDB2->QueueAsyncQuery(GetQuery());

		//queryInProgress = true;
		//std::list<unsigned int> startupRange = g_pDBIO->GetNextIDRange(m_dwHintDynamicGUID, IDQUEUEMAX);
		//m_dwHintDynamicGUID += IDQUEUEMAX;
		SaveRangeStart();

		//DEBUG_DATA << "New Group count:" << startupRange.size();

		//for (std::list<unsigned int>::iterator it = startupRange.begin(); it != startupRange.end(); ++it)
		//	listOfIdsForWeenies.push(*it);

		//if (listOfIdsForWeenies.empty())
		//	outOfIds = true;

		//startupRange.clear();

		//m_bLoadingState = false;
		//queryInProgress = false;
	}
	else
		m_dwHintDynamicGUID = g_pDBIO->GetHighestWeenieID(IDRANGESTART, IDRANGEEND);
}

DWORD CObjectIDGenerator::GenerateGUID(eGUIDClass type)
{
	switch (type)
	{
	case eDynamicGUID:
	{

		DWORD result = 0;

		if (!g_pConfig->UseIncrementalID())
		{
			if (outOfIds || listOfIdsForWeenies.empty())
			{
				SERVER_ERROR << "Dynamic GUID overflow!";
				return 0;
			}

			{
				std::scoped_lock lock(m_lock);
				result = listOfIdsForWeenies.front();
				listOfIdsForWeenies.pop();
			}
		}
		else
		{
			if (m_dwHintDynamicGUID >= IDRANGEEND)
			{
				SERVER_ERROR << "Dynamic GUID overflow!";
				return 0;
			}
			result = ++m_dwHintDynamicGUID;
		}

		return result;
	}
	case eEphemeral:
	{
		m_ephemeral &= IDEPHEMERALMASK;
		return m_ephemeral++;
	} 
	}

	return 0;
}

void CObjectIDGenerator::SaveRangeStart()
{
	BinaryWriter banData;
	banData.Write<DWORD>(m_dwHintDynamicGUID);
	if (!g_pConfig->UseIncrementalID())
	{
		g_pDBIO->CreateOrUpdateGlobalData(DBIO_GLOBAL_ID_RANGE_START, banData.GetData(), banData.GetSize());
	}
}

void CObjectIDGenerator::LoadRangeStart()
{
	void *data = NULL;
	DWORD length = 0;
	if (!g_pConfig->UseIncrementalID())
	{
		if (g_pConfig->IDScanType() == 0)
		{
			g_pDBIO->GetGlobalData(DBIO_GLOBAL_ID_RANGE_START, &data, &length);
			BinaryReader reader(data, length);
			m_dwHintDynamicGUID = reader.ReadDWORD() + IDQUEUEMIN;
			if (m_dwHintDynamicGUID < IDRANGESTART)
				m_dwHintDynamicGUID = IDRANGESTART;
		}

		m_bLoadingState = true;

		std::unique_ptr<CMYSQLQuery> query(GetQuery());
		query->PerformQuery((MYSQL*)g_pDB2->GetInternalConnection());
	}
	else
		m_dwHintDynamicGUID = g_pDBIO->GetHighestWeenieID(IDRANGESTART, IDRANGEEND);
}

void CObjectIDGenerator::Think()
{
	if (!m_bLoadingState && listOfIdsForWeenies.size() < IDQUEUEMIN)
	{
		m_bLoadingState = true;
		LoadState();
	}
}

bool IdRangeTableQuery::PerformQuery(MYSQL *c)
{
	if (!c)
		return false;

	MYSQL_STMT *statement = mysql_stmt_init(c);
	bool failed = false;

	if (statement)
	{
		std::string query = "SELECT unused FROM idranges WHERE unused > ? limit 250000" ;
		mysql_stmt_prepare(statement, query.c_str(), query.length());

		MYSQL_BIND params = { 0 };
		params.buffer = &m_start;
		params.buffer_length = sizeof(uint32_t);
		params.buffer_type = MYSQL_TYPE_LONG;
		params.is_unsigned = true;

		failed = mysql_stmt_bind_param(statement, &params);
		if (!failed)
		{
			DEBUG_DATA << "IdRangeTableQuery, start: " << m_start;

			failed = mysql_stmt_execute(statement);
			if (!failed)
			{
				uint32_t value = 0;
				MYSQL_BIND result = { 0 };
				result.buffer = &value;
				result.buffer_length = sizeof(uint32_t);
				result.buffer_type = MYSQL_TYPE_LONG;
				result.is_unsigned = true;

				failed = mysql_stmt_bind_result(statement, &result);
				bool done = failed;

				while (!done)
				{
					std::scoped_lock lock(m_lock);

					for (int i = 0; i < 2500 && !failed; i++)
					{
						if (!(done = mysql_stmt_fetch(statement)))
						{
							m_queue.push(value);
							m_start = value;
						}
					}
					std::this_thread::yield();
				}
			}

			DEBUG_DATA << "IdRangeTableQuery, complete";
		}

		mysql_stmt_close(statement);
	}

	m_busy = false;

	if (failed)
		SERVER_ERROR << "Error in IdRangeTableQuery::PerformQuery: " << mysql_error(c);

	return !failed;

}

bool ScanWeenieTableQuery::PerformQuery(MYSQL *c)
{
	if (!c)
		return false;

	MYSQL_STMT *statement = mysql_stmt_init(c);
	bool failed = false;

	if (statement)
	{
		std::string query =
			"SELECT wo.id + 1 AS l, "
			"IFNULL((SELECT id FROM weenies WHERE id > wo.id ORDER BY id LIMIT 1), 4278190080) AS u "
			"FROM ("
			"SELECT w.id FROM weenies w LEFT JOIN weenies u ON u.id = w.id + 1 "
			"WHERE w.id > ? AND u.id IS NULL "
			"UNION SELECT ? id) wo "
			"LIMIT 250;";

		//std::string query =
		//	"SELECT w.id + 1 AS l, "
		//	"(SELECT IFNULL(id, 4278190080) FROM weenies WHERE id > w.id ORDER BY id LIMIT 1) AS u "
		//	"FROM weenies w LEFT JOIN weenies l ON l.id = w.id + 1 "
		//	"WHERE w.id > ? and l.id is null "
		//	"ORDER BY w.id "
		//	"LIMIT 100;";

		mysql_stmt_prepare(statement, query.c_str(), query.length());

		MYSQL_BIND params[2] = { 0 };
		params[0].buffer = &m_start;
		params[0].buffer_length = sizeof(uint32_t);
		params[0].buffer_type = MYSQL_TYPE_LONG;
		params[0].is_unsigned = true;
		params[1].buffer = &m_start;
		params[1].buffer_length = sizeof(uint32_t);
		params[1].buffer_type = MYSQL_TYPE_LONG;
		params[1].is_unsigned = true;

		failed = mysql_stmt_bind_param(statement, params);
		if (!failed)
		{
			DEBUG_DATA << "ScanWeenieTableQuery, start: " << m_start;
			failed = mysql_stmt_execute(statement);
			if (!failed)
			{
				uint32_t l = 0;
				uint32_t u = -1;
				MYSQL_BIND result[2] = { 0 };
				result[0].buffer = &l;
				result[0].buffer_length = sizeof(uint32_t);
				result[0].buffer_type = MYSQL_TYPE_LONG;
				result[0].is_unsigned = true;

				result[1].buffer = &u;
				result[1].buffer_length = sizeof(uint32_t);
				result[1].buffer_type = MYSQL_TYPE_LONG;
				result[1].is_unsigned = true;

				failed = mysql_stmt_bind_result(statement, result);
				bool done = failed;

				while (!done)
				{
					if (!(done = mysql_stmt_fetch(statement)))
					{
						//DEBUG_DATA << "ScanWeenieTableQuery, result: " << l << " - " << u;
						
						// we need to save the lower bound
						// that way if this range is ever revisited by the query,
						// it won't be skipped entirely
						//m_start = l;

						// we need to keep reading results even it we met our target
						//   to ensure the buffers are all flushed
						while (l < u && m_queue.size() < IDQUEUEMAX)
						{
							{
								std::scoped_lock lock(m_lock);
								for (int i = 0; i < 2500 && l < u; i++, l++)
								{
									m_queue.push(l);
									m_start = l;
								}
							}
							// we want to yield after releasing the lock
							std::this_thread::yield();
						}
					}
				}

				DEBUG_DATA << "ScanWeenieTableQuery, complete";
			}
		}

		mysql_stmt_close(statement);
	}

	m_busy = false;

	if (failed)
		SERVER_ERROR << "Error in ScanWeenieTableQuery::PerformQuery: " << mysql_error(c);

	return !failed;

}
