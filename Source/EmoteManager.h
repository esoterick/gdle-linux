
#pragma once

class QueuedEmote
{
public:
	Emote _data;
	DWORD _target_id;
	double _executeTime;
};

class EmoteManager
{
public:
	EmoteManager(class CWeenieObject *weenie);
		
	std::string ReplaceEmoteText(const std::string &text, DWORD target_id, DWORD source_id);
	void killTask(std::string mobName, std::string kCountName, DWORD target_id);
	void killTaskSub(std::string &mobName, std::string &kCountName, CWeenieObject *targormember);
	bool ChanceExecuteEmoteSet(EmoteCategory category, std::string msg, DWORD target_id);
	bool ChanceExecuteEmoteSet(EmoteCategory category, DWORD target_id);
	void ExecuteEmoteSet(const EmoteSet &emoteSet, DWORD target_id);
	void ExecuteEmote(const Emote &emote, DWORD target_id);
	void ConfirmationResponse(bool accepted, DWORD target_id);
	void Tick();

	void Cancel();
	void OnDeath(DWORD killer_id);
	bool IsExecutingAlready(); 
	bool HasQueue();

	std::map<DWORD, std::string> _confirmMsgList;

	void SkillRefundValidationLog(DWORD target_id, STypeSkill skillToAlter, SKILL_ADVANCEMENT_CLASS debugSkillSacInit, int debugInitAvailCredits); //Validation Debug

protected:
	class CWeenieObject *_weenie = NULL;

	double _emoteEndTime;
	std::list<QueuedEmote> _emoteQueue;
};