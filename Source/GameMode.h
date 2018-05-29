
#pragma once

class CGameMode;

class CGameMode
{
public:
	CGameMode();
	virtual ~CGameMode();

	virtual const char *GetName() = 0;
	virtual void Think() = 0;

	virtual void OnRemoveEntity(std::shared_ptr<CWeenieObject> pEntity) = 0;
	virtual void OnTargetAttacked(std::shared_ptr<CWeenieObject> pTarget, std::shared_ptr<CWeenieObject> pSource) = 0;
};

class CGameMode_Tag : public CGameMode
{
public:
	CGameMode_Tag();
	virtual ~CGameMode_Tag() override;

	virtual const char *GetName();
	virtual void Think() override;

	virtual void OnRemoveEntity(std::shared_ptr<CWeenieObject> pEntity) override;
	virtual void OnTargetAttacked(std::shared_ptr<CWeenieObject> pTarget, std::shared_ptr<CWeenieObject> pSource) override;

protected:
	void SelectPlayer(std::shared_ptr<CPlayerWeenie> pPlayer);
	void UnselectPlayer();

	std::weak_ptr<CPlayerWeenie> m_pSelectedPlayer;
};
