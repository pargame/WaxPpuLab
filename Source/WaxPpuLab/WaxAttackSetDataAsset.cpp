#include "WaxAttackSetDataAsset.h"

const FWaxAttackSpec* UWaxAttackSetDataAsset::FindSpec(EWaxAttackSlot Slot) const
{
	switch (Slot)
	{
	case EWaxAttackSlot::Basic: return &Basic;
	case EWaxAttackSlot::AerialStrike: return &JumpAttack.AerialStrike;
	case EWaxAttackSlot::Slam: return &JumpAttack.Slam;
	case EWaxAttackSlot::Kick: return &Kick;
	default: return nullptr;
	}
}
