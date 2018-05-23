
#include "StdAfx.h"
#include "Caster.h"

CCasterWeenie::CCasterWeenie()
{
}

void CCasterWeenie::ApplyQualityOverrides()
{
}

int CCasterWeenie::Use(std::shared_ptr<CPlayerWeenie> other)
{
	return CWeenieObject::Use(other);
}