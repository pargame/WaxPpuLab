#include "WaxMonsterDataAsset.h"

#include "WaxLocomotionComponent.h"

float UWaxMonsterDataAsset::GetSpawnHalfHeight() const
{
	if (LocomotionClass && LocomotionClass->IsChildOf(UWaxButterLocomotionComponent::StaticClass()))
	{
		return FMath::Max(10.0f, ButterLocomotion.HalfZ);
	}
	return FMath::Max(10.0f, CapsuleHalfHeight);
}
