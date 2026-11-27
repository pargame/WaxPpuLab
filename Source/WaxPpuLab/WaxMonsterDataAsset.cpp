#include "WaxMonsterDataAsset.h"

#include "WaxLocomotionComponent.h"

UWaxMonsterDataAsset::UWaxMonsterDataAsset()
{
	ShellPart.BreakMode = EWaxBreakMode::ChipAndRetainBody;

	EyePart.BreakMode = EWaxBreakMode::DetachWholeOnBreak;
	EyePart.DamageThreshold = 180000.0f;
	EyePart.HitAcceptRadius = 15.6f;
	EyePart.MinImpactRadius = 39.0f;
}

float UWaxMonsterDataAsset::GetSpawnHalfHeight() const
{
	if (LocomotionClass && LocomotionClass->IsChildOf(UWaxButterLocomotionComponent::StaticClass()))
	{
		return FMath::Max(10.0f, ButterLocomotion.HalfZ);
	}
	return FMath::Max(10.0f, CapsuleHalfHeight);
}
