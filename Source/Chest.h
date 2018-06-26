
#pragma once

#include "Container.h"

class CChestWeenie : public CContainerWeenie
{
public:
	CChestWeenie();
	virtual ~CChestWeenie() override;

	virtual class std::shared_ptr<CChestWeenie> AsChest() { return std::static_pointer_cast<CChestWeenie>(GetPointer()); }

	virtual void PostSpawn() override;

	virtual void OnContainerOpened(std::shared_ptr<CWeenieObject> other) override;
	virtual void OnContainerClosed(std::shared_ptr<CWeenieObject> other) override;
};
