
#include "StdAfx.h"
#include "Database2.h"
#include "DatabaseIO.h"
#include "SHA512.h"

DWORD64 g_RandomAdminPassword = 0; // this is used for the admin login

CDatabaseIO::CDatabaseIO()
{
	blob_query_buffer = new BYTE[BLOB_QUERY_BUFFER_LENGTH];
}

CDatabaseIO::~CDatabaseIO()
{
	SafeDeleteArray (blob_query_buffer);
}

bool CDatabaseIO::VerifyAccount(const char *username, const char *password, AccountInformation_t *pAccountInfo, int *pError)
{
	*pError = DBIO_ERROR_NONE;

	std::string usernameEscaped = g_pDB2->EscapeString(username);

	std::string passwordSalt;
	if (g_pDB2->Query("SELECT password_salt FROM accounts WHERE username='%s' LIMIT 1", usernameEscaped.c_str()))
	{
		CSQLResult *pResult = g_pDB2->GetResult();
		if (pResult)
		{
			SQLResultRow_t ResultRow = pResult->FetchRow();

			if (ResultRow)
			{
				passwordSalt = ResultRow[0];
				delete pResult;
			}
			else
			{
				delete pResult;

				*pError = VERIFYACCOUNT_ERROR_DOESNT_EXIST;
				return false;
			}
		}
		else
		{
			*pError = VERIFYACCOUNT_ERROR_QUERY_FAILED;
			return false;
		}
	}
	else
	{
		*pError = VERIFYACCOUNT_ERROR_QUERY_FAILED;
		return false;
	}

	bool bIsAdmin = false;
	if (!_stricmp(username, "admin"))
	{
		if (!strcmp(password, csprintf("%I64u", g_RandomAdminPassword)))
		{
			bIsAdmin = true;
			//*pError = VERIFYACCOUNT_ERROR_BAD_LOGIN;
			//return false;
		}

		//bIsAdmin = true;
	}

	std::string saltedPassword = std::string(password) + passwordSalt;
	std::string hashedSaltedPassword = SHA512(saltedPassword.c_str(), saltedPassword.length()).substr(0, 64);
	if (g_pDB2->Query("SELECT id, username, date_created, access FROM accounts WHERE (username!='admin' AND username='%s' AND password='%s') OR (username='admin' AND %d) LIMIT 1", usernameEscaped.c_str(), hashedSaltedPassword.c_str(), bIsAdmin))
	{
		CSQLResult *pResult = g_pDB2->GetResult();
		if (pResult)
		{
			SQLResultRow_t ResultRow = pResult->FetchRow();

			if (ResultRow)
			{
				pAccountInfo->id = CSQLResult::SafeUInt(ResultRow[0]);
				pAccountInfo->username = CSQLResult::SafeString(ResultRow[1]);
				pAccountInfo->dateCreated = CSQLResult::SafeUInt(ResultRow[2]);
				pAccountInfo->access = CSQLResult::SafeUInt(ResultRow[3]);
				delete pResult;
			}
			else
			{
				delete pResult;

				*pError = VERIFYACCOUNT_ERROR_BAD_LOGIN;
				return false;
			}
		}
		else
		{
			*pError = VERIFYACCOUNT_ERROR_QUERY_FAILED;
			return false;
		}
	}
	else
	{
		*pError = VERIFYACCOUNT_ERROR_QUERY_FAILED;
		return false;
	}

	return true;
}

bool CDatabaseIO::CreateAccount(const char *username, const char *password, int *pError, const char *ipaddress)
{
	unsigned int usernameLength = (unsigned int)strlen(username);
	if (usernameLength < MIN_USERNAME_LENGTH || usernameLength > MAX_USERNAME_LENGTH)
	{
		*pError = CREATEACCOUNT_ERROR_BAD_USERNAME;
		return false;
	}

	// Check if username is valid characters.
	for (char &c : std::string(username))
	{
		if (c >= 'a' && c <= 'z')
			continue;
		if (c >= 'A' && c <= 'Z')
			continue;
		if (c >= '0' && c <= '9')
			continue;
		if (c == '_')
			continue;
		if (c == '-')
			continue;

		*pError = CREATEACCOUNT_ERROR_BAD_USERNAME;
		return false;
	}

	unsigned int passwordLength = (unsigned int)strlen(password);
	if (passwordLength < MIN_PASSWORD_LENGTH || passwordLength > MAX_PASSWORD_LENGTH)
	{
		*pError = CREATEACCOUNT_ERROR_BAD_PASSWORD;
		return false;
	}

	*pError = DBIO_ERROR_NONE;

	std::string usernameEscaped = g_pDB2->EscapeString(username);
	std::string ipaddressEscaped = g_pDB2->EscapeString(ipaddress);

	// Create random salt
	DWORD randomValue = Random::GenUInt(0, 100000);
	std::string passwordSalt = SHA512(&randomValue, sizeof(randomValue)).substr(0, 16);

	// Apply salt
	std::string saltedPassword = password + passwordSalt;
	std::string hashedSaltedPassword = SHA512(saltedPassword.c_str(), saltedPassword.length()).substr(0, 64);

	uint64_t accountID = 0;
	if (g_pDB2->Query("INSERT INTO accounts (username, password, password_salt, date_created, access, created_ip_address) VALUES ('%s', '%s', '%s', UNIX_TIMESTAMP(), 1, '%s')", usernameEscaped.c_str(), hashedSaltedPassword.c_str(), passwordSalt.c_str(), ipaddressEscaped.c_str()))
	{
		accountID = g_pDB2->GetInsertID();

		if (!accountID)
		{
			*pError = CREATEACCOUNT_ERROR_QUERY_FAILED;
			return false;
		}
	}
	else
	{
		*pError = CREATEACCOUNT_ERROR_QUERY_FAILED;
		return false;
	}

	return true;
}

std::list<unsigned int> CDatabaseIO::GetWeeniesAt(unsigned int block_id)
{
	std::list<unsigned int> results;

	if (g_pDB2->Query("SELECT weenie_id FROM blocks WHERE block_id = %u", block_id))
	{
		CSQLResult *pQueryResult = g_pDB2->GetResult();
		if (pQueryResult)
		{
			while (SQLResultRow_t Row = pQueryResult->FetchRow())
			{
				results.push_back(strtoul(Row[0], NULL, 10));
			}

			delete pQueryResult;
		}
	}

	return results;
}

bool CDatabaseIO::AddOrUpdateWeenieToBlock(unsigned int weenie_id, unsigned int block_id)
{
	return g_pDB2->Query("INSERT INTO blocks (weenie_id, block_id) VALUES (%u, %u) ON DUPLICATE KEY UPDATE block_id = %u", weenie_id, block_id, block_id);
}

bool CDatabaseIO::RemoveWeenieFromBlock(unsigned int weenie_id)
{
	return g_pDB2->Query("DELETE FROM blocks WHERE weenie_id = %u", weenie_id);
}

std::list<CharacterDesc_t> CDatabaseIO::GetCharacterList(unsigned int account_id)
{
	std::list<CharacterDesc_t> results;

	mysql_statement<1> statement = g_pDB2->QueryEx(
		"SELECT account_id, weenie_id, name, date_created, instance_ts FROM characters WHERE account_id = ?",
		account_id);

	if (statement)
	{
		CharacterDesc_t desc = { 0, 0, "", 0, 0 };
		
		desc.name.resize(64);
		unsigned long length = 0;

		mysql_statement_results<5> row;
		row.bind(0, desc.account_id);
		row.bind(1, desc.weenie_id);
		row.bind(2, desc.name, desc.name.capacity(), &length);
		row.bind(3, desc.date_created);
		row.bind(4, desc.instance_ts);

		if (statement.bindResults(row))
		{
			while (row.next())
			{
				CharacterDesc_t entry = desc;
				entry.name.resize(length);
				
				results.push_back(entry);
			}
		}
	}

	return results;
}

std::list<CharacterSquelch_t> CDatabaseIO::GetCharacterSquelch(unsigned int character_id)
{
	std::list<CharacterSquelch_t> results;

	if (g_pDB2->Query("SELECT squelched_id, account_id, isip, isspeech, istell, iscombat, ismagic, isemote, isadvancement, isappraisal, isspellcasting, isallegiance, isfellowhip, iscombatenemy, isrecall, iscrafting FROM character_squelch WHERE character_id = %u", character_id))
	{
		CSQLResult *pQueryResult = g_pDB2->GetResult();
		if (pQueryResult)
		{
			while (SQLResultRow_t Row = pQueryResult->FetchRow())
			{
				CharacterSquelch_t entry;
				entry.squelched_id = strtoul(Row[0], NULL, 10);
				entry.account_id = strtoul(Row[1], NULL, 10);
				entry.isIp = Row[2];
				entry.isSpeech = Row[3];
				entry.isTell = Row[4];
				entry.isCombat = Row[5];
				entry.isMagic = Row[6];
				entry.isEmote = Row[7];
				entry.isAdvancement = Row[8];
				entry.isAppraisal = Row[9];
				entry.isSpellcasting = Row[10];
				entry.isAllegiance = Row[11];
				entry.isFellowship = Row[12];
				entry.isCombatEnemy = Row[13];
				entry.isRecall = Row[14];
				entry.isCrafting = Row[15];
				results.push_back(entry);
			}
			delete pQueryResult;
		}
	}

	return results;
}

bool CDatabaseIO::SaveCharacterSquelch(unsigned int character_id, CharacterSquelch_t data)
{
	return g_pDB2->Query("INSERT INTO character_squelch(character_id, squelched_id, account_id, isip, isspeech, istell, iscombat, ismagic, isemote, isadvancement, isappraisal, isspellcasting, isallegiance, isfellowhip, iscombatenemy, isrecall, iscrafting) VALUES"
		"(%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u) ON DUPLICATE KEY UPDATE "
		"isip = %u, isspeech = %u, istell = %u, iscombat = %u, ismagic = %u, isemote = %u, isadvancement = %u, isappraisal = %u, isspellcasting = %u, isallegiance = %u, isfellowhip = %u, iscombatenemy = %u, isrecall = %u, iscrafting = %u;",
		character_id, data.squelched_id, data.account_id, data.isIp, data.isSpeech, data.isTell, data.isCombat, data.isMagic, data.isEmote, data.isAdvancement,
		data.isAppraisal, data.isSpellcasting, data.isAllegiance, data.isFellowship, data.isCombatEnemy, data.isRecall, data.isCrafting, 
		data.isIp, data.isSpeech, data.isTell, data.isCombat, data.isMagic, data.isEmote, data.isAdvancement,
		data.isAppraisal, data.isSpellcasting, data.isAllegiance, data.isFellowship, data.isCombatEnemy, data.isRecall, data.isCrafting);
}

bool CDatabaseIO::RemoveCharacterSquelch(unsigned int character_id, CharacterSquelch_t data)
{
	return g_pDB2->Query("DELETE FROM character_squelch WHERE character_id = %u AND squelched_id = %u;", character_id, data.squelched_id);
}

DWORD CDatabaseIO::GetPlayerAccountId(unsigned int character_id)
{
	DWORD accountToSquelch = 0;

	if (g_pDB2->Query("SELECT account_id FROM characters WHERE weenie_id = %u;", character_id))
	{
		CSQLResult *pQueryResult = g_pDB2->GetResult();
		if (pQueryResult)
		{
			SQLResultRow_t Row = pQueryResult->FetchRow();
			if (Row)
			{
				accountToSquelch = strtoul(Row[0], NULL, 10);
			}
			delete pQueryResult;
		}
	}

	return accountToSquelch;
}


CharacterDesc_t CDatabaseIO::GetCharacterInfo(unsigned int weenie_id)
{
	CharacterDesc_t result = { 0,0,"",0,0 };

	mysql_statement<1> statement = g_pDB2->QueryEx(
		"SELECT account_id, weenie_id, name, date_created, instance_ts FROM characters WHERE weenie_id = ?;",
		weenie_id);

	if (statement)
	{
		result.name.resize(64);
		unsigned long length = 0;
		
		mysql_statement_results<5> row;
		row.bind(0, result.account_id);
		row.bind(1, result.weenie_id);
		row.bind(2, result.name, result.name.capacity(), &length);
		row.bind(3, result.date_created);
		row.bind(4, result.instance_ts);

		if (statement.bindResults(row))
		{
			if (row.next())
			{
				result.name.resize(length);
			}
		}
	}
	return result;
}

bool CDatabaseIO::CreateCharacter(unsigned int account_id, unsigned int weenie_id, const char *name)
{
	return g_pDB2->Query("INSERT INTO characters (account_id, weenie_id, name, date_created, instance_ts) VALUES (%u, %u, '%s', UNIX_TIMESTAMP(), 1)",
		account_id, weenie_id, g_pDB2->EscapeString(name).c_str());
}

bool CDatabaseIO::DeleteCharacter(unsigned int weenie_id)
{
	return g_pDB2->Query("DELETE FROM characters WHERE weenie_id = %u", weenie_id);
}

bool CDatabaseIO::SetCharacterInstanceTS(unsigned int weenie_id, unsigned int instance_ts)
{
	return g_pDB2->Query("UPDATE characters SET instance_ts = %u WHERE weenie_id = %u", instance_ts, weenie_id);
}

bool CDatabaseIO::IDRangeTableExistsAndValid()
{
	bool retval = false;

	mysql_statement<0> statement = g_pDB2->QueryEx("SELECT unused FROM idranges WHERE unused > 2147999999 LIMIT 1");
	if (statement)
	{
		unsigned int current = 0;
		mysql_statement_results<1> result;
		result.bind(0, current);

		if (statement.bindResults(result))
		{
			if (result.next())
			{
				retval = true;
			}
		}
	}

	return retval;
}


std::list<unsigned int> CDatabaseIO::GetNextIDRange(unsigned int rangeStart, unsigned int count)
{
	mysql_statement<2> statement = g_pDB2->QueryEx(
		"SELECT unused FROM idranges WHERE unused > ? limit ?;",
		rangeStart, count);

	std::list< unsigned int> found;
	if (statement)
	{
		unsigned int current = 0;
		mysql_statement_results<1> result;
		result.bind(0, current);

		if (statement.bindResults(result))
		{
			while (result.next())
			{
				found.push_back(current);
			}
		}
	}

	return found;
}

unsigned int CDatabaseIO::GetHighestWeenieID(unsigned int min_range, unsigned int max_range)
{
	mysql_statement<2> statement = g_pDB2->QueryEx(
		"SELECT id FROM weenies WHERE id >= ? AND id < ? ORDER BY id DESC LIMIT 1",
		min_range, max_range);

	unsigned int found = 0;
	if (statement)
	{
		mysql_statement_results<1> result;
		result.bind(0, found);

		if (statement.bindResults(result))
		{
			if (result.next())
			{
				return found;
			}
		}
	}

	SERVER_ERROR << "Count not get highest weenie ID for range" << min_range << "-" << max_range << " : " << statement.error();
	return min_range;
}

bool CDatabaseIO::IsCharacterNameOpen(const char *name)
{
	std::string tmp(name);
	mysql_statement<1> statement = g_pDB2->QueryEx(
		"SELECT weenie_id FROM characters WHERE name = ?",
		tmp);

	if (statement)
	{
		unsigned int id = 0;
		mysql_statement_results<1> result;
		result.bind(0, id);

		if (statement.bindResults(result))
		{
			if (result.next())
			{
				return false;
			}
			return true;
		}
	}

	SERVER_ERROR << "Error on IsCharacterNameOpen:" << statement.error();
	return false;
}

bool CDatabaseIO::IsPlayerCharacter(unsigned int weenie_id)
{
	mysql_statement<1> statement = g_pDB2->QueryEx(
		"SELECT weenie_id FROM characters WHERE weenie_id = ?",
		weenie_id);

	if (statement)
	{
		uint32_t result_id = 0;
		mysql_statement_results<1> result;
		result.bind(0, result_id);

		if (statement.bindResults(result))
		{
			if (result.next())
			{
				return true;
			}
		}
	}

	return false;
}

DWORD CDatabaseIO::GetPlayerCharacterId(const char *name)
{
	uint32_t id = 0;

	mysql_statement<1> statement = g_pDB2->QueryEx(
		"SELECT weenie_id FROM characters WHERE name = ?",
		name);

	if (statement)
	{
		mysql_statement_results<1> result;
		result.bind(0, id);

		if (statement.bindResults(result))
		{
			result.next();
		}
	}

	return (DWORD)id;
}

std::string CDatabaseIO::GetPlayerCharacterName(DWORD weenie_id)
{
	std::string result;

	mysql_statement<1> statement = g_pDB2->QueryEx(
		"SELECT name FROM characters WHERE weenie_id = ?",
		weenie_id);

	if (statement)
	{
		unsigned long length = 0;
		mysql_statement_results<1> results;
		results.bind(0, result, 64, &length);

		if (statement.bindResults(results))
		{
			if (results.next())
			{
				result.resize(length);
			}
		}
	}

	return result;
}

/*
bool CDatabaseIO::CreateOrUpdateWeenie(unsigned int weenie_id, unsigned int top_level_object_id, unsigned int block_id, void *data, unsigned int data_length)
{
	// synchronous, deprecated
	return CreateOrUpdateWeenie((MYSQL *)g_pDB2->GetInternalConnection(), weenie_id, top_level_object_id, block_id, data, data_length);
}
*/

bool CDatabaseIO::CreateOrUpdateWeenie(unsigned int weenie_id, unsigned int top_level_object_id, unsigned int block_id, void *data, unsigned int data_length)
{
	IncrementPendingSave(weenie_id);
	g_pDB2->QueueAsyncQuery(new CMYSQLSaveWeenieQuery(weenie_id, top_level_object_id, block_id, data, data_length));
	return true;
}

bool CDatabaseIO::GetWeenie(unsigned int weenie_id, unsigned int *top_level_object_id, unsigned int *block_id, void **data, unsigned long *data_length)
{
	// this is so bad...
	while (GetNumPendingSaves(weenie_id))
		Sleep(0);

	mysql_statement<1> statement = g_pDB2->QueryEx(
		"SELECT top_level_object_id, block_id, data FROM weenies WHERE id = ?",
		weenie_id);

	if (statement)
	{
		mysql_statement_results<3> result;
		result.bind(0, top_level_object_id);
		result.bind(1, block_id);
		result.bind(2, blob_query_buffer, BLOB_QUERY_BUFFER_LENGTH, data_length);

		if (statement.bindResults(result))
		{
			if (result.next())
			{
				*data = blob_query_buffer;
				return true;
			}
		}
	}

	SERVER_ERROR << "Error on GetWeenie:" << statement.error();
	return false;
}

bool CDatabaseIO::DeleteWeenie(unsigned int weenie_id)
{
	mysql_statement<2> statement = g_pDB2->QueryEx(
		"DELETE FROM weenies WHERE id = ? OR top_level_object_id = ?",
		weenie_id, weenie_id);

	if (statement)
	{
		return true;
	}

	SERVER_ERROR << "Error on DeleteWeenie: " << statement.error();
	return false;

}

bool CDatabaseIO::IsWeenieInDatabase(unsigned int weenie_id)
{
	mysql_statement<1> statement = g_pDB2->QueryEx(
		"SELECT id FROM weenies WHERE id = ?",
		weenie_id);

	if (statement)
	{
		uint32_t result_id = 0;
		mysql_statement_results<1> result;
		result.bind(0, result_id);

		if (statement.bindResults(result))
		{
			if (result.next())
			{
				return true;
			}
		}
	}

	return false;
}

bool CDatabaseIO::CreateOrUpdateGlobalData(DBIOGlobalDataID id, void *data, unsigned long data_length)
{
	mysql_statement<2> statement = g_pDB2->CreateQuery<2>("REPLACE INTO globals (id, data) VALUES (?, ?)");
	
	if (statement)
	{
		uint32_t gid = (uint32_t)id;
		statement.bind(0, gid);
		statement.bind(1, data, data_length);

		if (statement.execute())
			return true;
	}
	
	SERVER_ERROR << "Error on CreateOrUpdateGlobalData for" << id << ":" << statement.error();
	return false;
}

bool CDatabaseIO::GetGlobalData(DBIOGlobalDataID id, void **data, unsigned long *data_length)
{
	uint32_t gid = (uint32_t)id;
	mysql_statement<1> statement = g_pDB2->QueryEx(
		"SELECT data FROM globals WHERE id = ?",
		gid);

	if (statement)
	{
		mysql_statement_results<1> result;
		result.bind(0, blob_query_buffer, BLOB_QUERY_BUFFER_LENGTH, data_length);

		if (statement.bindResults(result))
		{
			if (result.next())
			{
				*data = blob_query_buffer;
				return true;
			}
		}
	}

	SERVER_ERROR << "Error on GetGlobalData:" << statement.error();
	return false;
}

bool CDatabaseIO::CreateOrUpdateHouseData(unsigned int house_id, void *data, unsigned int data_length)
{
	IncrementPendingSave(house_id);
	g_pDB2->QueueAsyncQuery(new CMYSQLSaveHouseQuery(house_id, data, data_length));
	return true;
}

bool CDatabaseIO::GetHouseData(unsigned int house_id, void **data, unsigned long *data_length)
{
	// this is so bad...
	while (GetNumPendingSaves(house_id))
		Sleep(0);

	mysql_statement<1> statement = g_pDB2->QueryEx(
		"SELECT data FROM houses WHERE house_id = ?",
		house_id);

	if (statement)
	{
		mysql_statement_results<1> result;
		result.bind(0, blob_query_buffer, BLOB_QUERY_BUFFER_LENGTH, data_length);

		if (statement.bindResults(result))
		{
			if (result.next())
			{
				*data = blob_query_buffer;
				return true;
			}
		}
	}

	SERVER_ERROR << "Error on GetHouseData:" << statement.error();
	return false;
}

// CLockable _pendingSavesLock;
// std::unordered_map<DWORD, DWORD> _pendingSaves;

void CDatabaseIO::IncrementPendingSave(DWORD weenie_id)
{
	_pendingSavesLock.Lock();

	std::unordered_map<DWORD, DWORD>::iterator i = _pendingSaves.find(weenie_id);
	if (i != _pendingSaves.end())
	{
		i->second++;
	}
	else
	{
		_pendingSaves.insert(std::pair<DWORD, DWORD>(weenie_id, 1));
	}

	_pendingSavesLock.Unlock();
}

void CDatabaseIO::DecrementPendingSave(DWORD weenie_id)
{
	_pendingSavesLock.Lock();

	std::unordered_map<DWORD, DWORD>::iterator i = _pendingSaves.find(weenie_id);
	if (i != _pendingSaves.end())
	{
		if (i->second <= 1)
			_pendingSaves.erase(i);
		else
			i->second--;
	}

	_pendingSavesLock.Unlock();
}

DWORD CDatabaseIO::GetNumPendingSaves(DWORD weenie_id)
{
	_pendingSavesLock.Lock();
	std::unordered_map<DWORD, DWORD>::iterator i = _pendingSaves.find(weenie_id);
	
	DWORD numSaves = 0;
	if (i != _pendingSaves.end())
	{
		numSaves = i->second;
	}

	_pendingSavesLock.Unlock();
	return numSaves;
}

