
#pragma once

#include "Container.h"

class CChestWeenie : public CContainerWeenie
{
public:
	CChestWeenie();
	virtual ~CChestWeenie() override;

	virtual class std::shared_ptr<CChestWeenie> AsChest() { return std::dynamic_pointer_cast<CChestWeenie>(m_spThis.lock()); }

	virtual void PostSpawn() override;

	virtual void OnContainerOpened(std::shared_ptr<CWeenieObject> other) override;
	virtual void OnContainerClosed(std::shared_ptr<CWeenieObject> other) override;
};
