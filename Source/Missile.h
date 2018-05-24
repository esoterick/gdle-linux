
#pragma once

#include "ammunition.h"

class CMissileWeenie : public CAmmunitionWeenie
{
public:
	CMissileWeenie();
	virtual ~CMissileWeenie() override;

	virtual class std::shared_ptr<CMissileWeenie> AsMissile() { return GetPointer<CMissileWeenie>(); }

protected:
};

