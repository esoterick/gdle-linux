
#pragma once

#include "WeenieObject.h"

class CClothingWeenie : public CWeenieObject
{
public:
	CClothingWeenie();

	virtual class std::shared_ptr<CClothingWeenie> AsClothing() { return std::dynamic_pointer_cast<CClothingWeenie>(m_spThis.lock()); }

	virtual void ApplyQualityOverrides() override;
	virtual int Use(std::shared_ptr<CPlayerWeenie> other) override;

	virtual bool IsValidWieldLocation(DWORD location) override;
	virtual bool CanEquipWith(std::shared_ptr<CWeenieObject> other, DWORD otherLocation) override;

	bool CoversBodyPart(BODY_PART_ENUM part, float *factor);
	virtual float GetEffectiveArmorLevel(DamageEventData &damageData, bool bIgnoreMagicArmor) override;

	virtual bool IsHelm() override;
};
