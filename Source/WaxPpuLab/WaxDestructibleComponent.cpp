#include "WaxDestructibleComponent.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Field/FieldSystemObjects.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UWaxDestructibleComponent::UWaxDestructibleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UWaxDestructibleComponent::InitializeWithCollection(UGeometryCollectionComponent* InShellCollection, UWaxMonsterDataAsset* InDataAsset)
{
	ShellCollection = InShellCollection;
	DataAsset = InDataAsset;

	BrokenLeafIndices.Reset();
	PendingGrains.Reset();
	ZoneAccumulatedDamage.Reset();
	CollisionCellNextTime.Reset();
	RecentCollisionTimes.Reset();
	TotalBreakEvents = 0;
	TotalCollisionEvents = 0;
	CollapseTimestamp = -1.0;
	bHasCollapsed = false;
	bDevAudioOverride = false;

	if (DataAsset)
	{
		ActiveAudioProfile = DataAsset->DefaultAudioProfile;
		BreakGrainSpreadMinSec = DataAsset->BreakGrainSpreadMinSec;
		BreakGrainSpreadMaxSec = DataAsset->BreakGrainSpreadMaxSec;
		MaxQueuedGrains = DataAsset->MaxQueuedGrains;
		CollapseGrainSpreadMaxSec = DataAsset->CollapseGrainSpreadMaxSec;
		CollapseMaxQueuedGrains = DataAsset->CollapseMaxQueuedGrains;
		CollisionSoundSpeedThreshold = DataAsset->CollisionSoundSpeedThreshold;
		MinCollisionAudioInterval = DataAsset->MinCollisionAudioInterval;
		CollisionSpatialCellSize = DataAsset->CollisionSpatialCellSize;
		CollisionSpatialCooldownSec = DataAsset->CollisionSpatialCooldownSec;
		CollisionCrackleScale = DataAsset->CollisionCrackleScale;
		CollisionSnapScale = DataAsset->CollisionSnapScale;
		CollisionBodyScale = DataAsset->CollisionBodyScale;
		bAllowKickAfterCollapse = DataAsset->bAllowKickAfterCollapse;
		bMuteCollisionAudioAfterCollapse = DataAsset->bMuteCollisionAudioAfterCollapse;
		CollapseCollisionMaxGraceSec = DataAsset->CollapseCollisionMaxGraceSec;
		ZoneAccumulatedDamage.Init(0.0f, DataAsset->HardnessZones.Num());
	}

	if (!ShellCollection)
	{
		return;
	}

	if (DataAsset && DataAsset->ShellCollectionAsset)
	{
		if (DataAsset->ShellCollectionAsset->DamageThreshold.Num() > 0)
		{
			ShellCollection->DamageThreshold = DataAsset->ShellCollectionAsset->DamageThreshold;
		}
		ShellCollection->SetRestCollection(DataAsset->ShellCollectionAsset, true);
	}

	ShellCollection->SetSimulatePhysics(true);
	ShellCollection->SetCustomPrimitiveDataFloat(0, 0.0f);
	ShellCollection->bAllowRemovalOnSleep = false;
	ShellCollection->bAllowRemovalOnBreak = true;
	ShellCollection->bNotifyCollisions = true;
	ShellCollection->SetNotifyRigidBodyCollision(true);
	ShellCollection->SetNotifyBreaks(true);

	ShellCollection->OnChaosBreakEvent.RemoveDynamic(this, &UWaxDestructibleComponent::HandleChaosBreakEvent);
	ShellCollection->OnChaosPhysicsCollision.RemoveDynamic(this, &UWaxDestructibleComponent::HandleChaosCollisionEvent);
	ShellCollection->OnChaosBreakEvent.AddDynamic(this, &UWaxDestructibleComponent::HandleChaosBreakEvent);
	ShellCollection->OnChaosPhysicsCollision.AddDynamic(this, &UWaxDestructibleComponent::HandleChaosCollisionEvent);

	BuildLeafHierarchyCache();
	EnsureFieldNodes();

	// Anchor all pieces Kinematic via field (keeping Component ObjectType = Dynamic so ExternalClusterStrain breaks cleanly)
	CollapseDynamicState->SetUniformInteger(static_cast<int32>(EObjectStateTypeEnum::Chaos_Object_Kinematic));
	ShellCollection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_DynamicState, nullptr, CollapseDynamicState);

	RefreshTickEnabled();
}

void UWaxDestructibleComponent::BuildLeafHierarchyCache()
{
	LeafNodeIndices.Reset();
	TotalLeafCount = 0;

	if (!ShellCollection || !ShellCollection->GetRestCollection())
	{
		return;
	}

	const TSharedPtr<const FGeometryCollection, ESPMode::ThreadSafe> GeomCol = ShellCollection->GetRestCollection()->GetGeometryCollection();
	if (!GeomCol.IsValid())
	{
		return;
	}

	const int32 NumTransforms = GeomCol->NumElements(FGeometryCollection::TransformGroup);
	const TManagedArray<TSet<int32>>& Children = GeomCol->Children;

	for (int32 i = 0; i < NumTransforms; ++i)
	{
		if (Children[i].Num() == 0)
		{
			LeafNodeIndices.Add(i);
		}
	}

	TotalLeafCount = LeafNodeIndices.Num();
}

void UWaxDestructibleComponent::EnsureFieldNodes()
{
	if (!ImpactMaskFalloff)
	{
		ImpactMaskFalloff = NewObject<URadialFalloff>(this);
		ImpactStrainFalloff = NewObject<URadialFalloff>(this);
		ImpactDynamicState = NewObject<UUniformInteger>(this);
		ImpactLocalDynamic = NewObject<UCullingField>(this);
		KickMaskFalloff = NewObject<URadialFalloff>(this);
		KickUniformVel = NewObject<UUniformVector>(this);
		KickLocalVel = NewObject<UCullingField>(this);
		CollapseDynamicState = NewObject<UUniformInteger>(this);
		CollapseUniformStrain = NewObject<UUniformScalar>(this);
	}
}

int32 UWaxDestructibleComponent::FindMatchingZoneIndex(const FVector& WorldPoint) const
{
	if (!ShellCollection || !DataAsset || DataAsset->HardnessZones.Num() == 0)
	{
		return INDEX_NONE;
	}

	const FVector LocalPoint = ShellCollection->GetComponentTransform().InverseTransformPosition(WorldPoint);
	int32 BestZoneIdx = INDEX_NONE;
	float BestRadius = TNumericLimits<float>::Max();

	for (int32 i = 0; i < DataAsset->HardnessZones.Num(); ++i)
	{
		const FWaxHardnessZone& Zone = DataAsset->HardnessZones[i];
		const float DistSq = FVector::DistSquared(LocalPoint, Zone.LocalCenter);
		if (DistSq <= FMath::Square(Zone.LocalRadius) && Zone.LocalRadius < BestRadius)
		{
			BestRadius = Zone.LocalRadius;
			BestZoneIdx = i;
		}
	}

	return BestZoneIdx;
}

const FWaxAudioProfile& UWaxDestructibleComponent::ResolveAudioProfileAt(const FVector& WorldPoint) const
{
	if (!bDevAudioOverride)
	{
		const int32 ZoneIdx = FindMatchingZoneIndex(WorldPoint);
		if (ZoneIdx != INDEX_NONE && DataAsset && DataAsset->HardnessZones[ZoneIdx].bUseZoneAudioProfile && DataAsset->HardnessZones[ZoneIdx].AudioProfile.CrackSound)
		{
			return DataAsset->HardnessZones[ZoneIdx].AudioProfile;
		}
	}
	return ActiveAudioProfile;
}

float UWaxDestructibleComponent::GetRemainingRatio() const
{
	if (TotalLeafCount <= 0)
	{
		return 1.0f;
	}
	return FMath::Clamp(1.0f - (static_cast<float>(BrokenLeafIndices.Num()) / static_cast<float>(TotalLeafCount)), 0.0f, 1.0f);
}

void UWaxDestructibleComponent::ApplyWaxImpact(const FVector& WorldImpactPoint, float BaseStrain, float Radius)
{
	if (!ShellCollection || bHasCollapsed || Radius <= 0.0f)
	{
		return;
	}

	EnsureFieldNodes();
	OnWaxImpactApplied.Broadcast(WorldImpactPoint);

	float EffectiveStrain = BaseStrain;
	const int32 ZoneIdx = FindMatchingZoneIndex(WorldImpactPoint);
	if (ZoneIdx != INDEX_NONE && DataAsset && ZoneAccumulatedDamage.IsValidIndex(ZoneIdx))
	{
		const FWaxHardnessZone& Zone = DataAsset->HardnessZones[ZoneIdx];
		if (ZoneAccumulatedDamage[ZoneIdx] < Zone.Durability)
		{
			ZoneAccumulatedDamage[ZoneIdx] += 1.0f;
			const float Remaining = FMath::Max(0.0f, Zone.Durability - ZoneAccumulatedDamage[ZoneIdx]);
			OnWaxZoneDamaged.Broadcast(Zone.ZoneName, WorldImpactPoint, Remaining);

#if !UE_BUILD_SHIPPING
			if (bShowDebugCounters && GEngine)
			{
				GEngine->AddOnScreenDebugMessage(9995, 1.0f, FColor::Yellow,
					FString::Printf(TEXT("[WaxZone] %s hit %.0f/%.0f"), *Zone.ZoneName.ToString(), ZoneAccumulatedDamage[ZoneIdx], Zone.Durability));
			}
#endif

			if (ZoneAccumulatedDamage[ZoneIdx] < Zone.Durability)
			{
				PlayOneShotCrack(WorldImpactPoint, 0.50f, 6500.0f, 0.06f, 1.0f, ResolveAudioProfileAt(WorldImpactPoint));
				return;
			}
		}

		EffectiveStrain *= Zone.StrainMultiplier;
	}

#if !UE_BUILD_SHIPPING
	if (bShowDebugCounters && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9996, 1.0f, FColor::Cyan,
			FString::Printf(TEXT("[WaxImpact] Strain=%.0f R=%.0f ObjType=%d"), EffectiveStrain, Radius, static_cast<int32>(ShellCollection->ObjectType)));
	}
#endif

	ImpactMaskFalloff->SetRadialFalloff(1.0f, 0.0f, 1.0f, 0.0f, Radius, WorldImpactPoint, EFieldFalloffType::Field_FallOff_None);
	ImpactDynamicState->SetUniformInteger(static_cast<int32>(EObjectStateTypeEnum::Chaos_Object_Dynamic));
	ImpactLocalDynamic->SetCullingField(ImpactMaskFalloff, ImpactDynamicState, EFieldCullingOperationType::Field_Culling_Outside);
	ShellCollection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_DynamicState, nullptr, ImpactLocalDynamic);

	ImpactStrainFalloff->SetRadialFalloff(EffectiveStrain, 0.85f, 1.0f, 0.0f, Radius, WorldImpactPoint, EFieldFalloffType::Field_Falloff_Linear);
	ShellCollection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain, nullptr, ImpactStrainFalloff);
}

void UWaxDestructibleComponent::ApplyDebrisKick(const FVector& KickOrigin, const FVector& KickVelocity, float Radius)
{
	if (!ShellCollection || BrokenLeafIndices.Num() == 0 || (bHasCollapsed && !bAllowKickAfterCollapse) || Radius <= 0.0f || KickVelocity.IsNearlyZero())
	{
		return;
	}

	const FBoxSphereBounds Bounds = ShellCollection->Bounds;
	const float MaxDist = Bounds.SphereRadius + Radius + 150.0f;
	if (FVector::DistSquared(KickOrigin, Bounds.Origin) > FMath::Square(MaxDist))
	{
		return;
	}

	EnsureFieldNodes();

	KickMaskFalloff->SetRadialFalloff(1.0f, 0.25f, 1.0f, 0.0f, Radius, KickOrigin, EFieldFalloffType::Field_Falloff_Linear);
	KickUniformVel->SetUniformVector(KickVelocity.Size(), KickVelocity.GetSafeNormal());
	KickLocalVel->SetCullingField(KickMaskFalloff, KickUniformVel, EFieldCullingOperationType::Field_Culling_Outside);
	ShellCollection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_LinearVelocity, nullptr, KickLocalVel);
}

void UWaxDestructibleComponent::TriggerRemainingCollapse()
{
	if (!ShellCollection || bHasCollapsed)
	{
		return;
	}

	bHasCollapsed = true;
	CollapseTimestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	EnsureFieldNodes();

	ShellCollection->SetCustomPrimitiveDataFloat(0, 1.0f);

	CollapseDynamicState->SetUniformInteger(static_cast<int32>(EObjectStateTypeEnum::Chaos_Object_Dynamic));
	ShellCollection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_DynamicState, nullptr, CollapseDynamicState);

	const float ReleaseStrain = DataAsset ? DataAsset->CollapseReleaseStrain : 350000.0f;
	if (ReleaseStrain > 0.0f)
	{
		CollapseUniformStrain->SetUniformScalar(ReleaseStrain);
		ShellCollection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain, nullptr, CollapseUniformStrain);
	}

	OnWaxCollapsed.Broadcast(GetOwner());
}

void UWaxDestructibleComponent::SelectDevAudioProfile(int32 ProfileIndex)
{
	if (!DataAsset || !DataAsset->DevAudioProfiles.IsValidIndex(ProfileIndex))
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(9992, 1.8f, FColor::Silver,
				FString::Printf(TEXT("[WaxAudio] Slot #%d empty (configured: %d)"),
					ProfileIndex + 1, DataAsset ? DataAsset->DevAudioProfiles.Num() : 0));
		}
#endif
		return;
	}
	bDevAudioOverride = true;
	ActiveAudioProfile = DataAsset->DevAudioProfiles[ProfileIndex];
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9992, 2.2f, FColor::Yellow,
			FString::Printf(TEXT("[WaxAudio] #%d: %s (Crackle=%.2f Snap=%.2f Body=%.2f)"),
				ProfileIndex + 1, *ActiveAudioProfile.ProfileName.ToString(),
				ActiveAudioProfile.CrackleGain, ActiveAudioProfile.SnapGain, ActiveAudioProfile.BodyGain));
	}
#endif
}

void UWaxDestructibleComponent::HandleChaosBreakEvent(const FChaosBreakEvent& BreakEvent)
{
	TotalBreakEvents++;

	if (LeafNodeIndices.Contains(BreakEvent.Index))
	{
		BrokenLeafIndices.Add(BreakEvent.Index);
	}

	const float RemainingThreshold = DataAsset ? DataAsset->CollapseRemainingRatio : 0.30f;
	if (!bHasCollapsed && TotalLeafCount > 0 && GetRemainingRatio() <= RemainingThreshold)
	{
		TriggerRemainingCollapse();
	}

	const int32 ActiveMaxQueue = bHasCollapsed ? FMath::Max(MaxQueuedGrains, CollapseMaxQueuedGrains) : MaxQueuedGrains;
	if (GetWorld() && PendingGrains.Num() < ActiveMaxQueue)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		const float MassAlpha = FMath::Clamp(BreakEvent.Mass / 8.0f, 0.0f, 1.0f);
		const float SpeedAlpha = FMath::Clamp(BreakEvent.Velocity.Size() / 450.0f, 0.2f, 1.0f);

		float Delay = 0.0f;
		float DelayIntensityScale = 1.0f;
		if (PendingGrains.Num() > 0)
		{
			if (bHasCollapsed)
			{
				const float U = FMath::FRand();
				Delay = CollapseGrainSpreadMaxSec * U * U;
				DelayIntensityScale = FMath::Lerp(1.0f, 0.38f, Delay / FMath::Max(0.05f, CollapseGrainSpreadMaxSec));
			}
			else
			{
				Delay = FMath::FRandRange(BreakGrainSpreadMinSec, BreakGrainSpreadMaxSec);
			}
		}

		const float QueueDensityScale = 1.0f / FMath::Pow(static_cast<float>(FMath::Max(1, PendingGrains.Num() + 1)), 0.22f);

		FWaxDestructibleGrain Grain;
		Grain.FireTime = Now + Delay;
		Grain.Location = BreakEvent.Location;
		Grain.Intensity = FMath::Clamp(0.35f + 0.50f * FMath::Max(MassAlpha, SpeedAlpha), 0.25f, 0.90f) * FMath::FRandRange(0.85f, 1.08f) * DelayIntensityScale * QueueDensityScale;
		Grain.CutoffHz = FMath::Lerp(13500.0f, 4200.0f, MassAlpha) + FMath::FRandRange(-900.0f, 900.0f);
		Grain.DecaySec = FMath::Lerp(0.075f, 0.22f, MassAlpha) * FMath::FRandRange(0.85f, 1.15f);
		Grain.PitchOffset = FMath::Lerp(2.8f, -2.4f, MassAlpha) + FMath::FRandRange(-0.9f, 0.9f);
		Grain.Profile = ResolveAudioProfileAt(BreakEvent.Location);
		PendingGrains.Add(Grain);
	}

	RefreshTickEnabled();
}

void UWaxDestructibleComponent::HandleChaosCollisionEvent(const FChaosPhysicsCollisionInfo& CollisionInfo)
{
	if (!GetWorld())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (bHasCollapsed && bMuteCollisionAudioAfterCollapse)
	{
		if (CollapseTimestamp < 0.0 || (Now - CollapseTimestamp) > CollapseCollisionMaxGraceSec)
		{
			return;
		}
	}

	TotalCollisionEvents++;

	float NormalSpeed = 0.0f;
	if (!CollisionInfo.Normal.IsNearlyZero())
	{
		const FVector RelVel = CollisionInfo.Velocity - CollisionInfo.OtherVelocity;
		NormalSpeed = FMath::Abs(FVector::DotProduct(RelVel, CollisionInfo.Normal.GetSafeNormal()));
	}
	else
	{
		NormalSpeed = CollisionInfo.Velocity.Size();
	}

	if (NormalSpeed < CollisionSoundSpeedThreshold)
	{
		return;
	}

	if (Now < NextAllowedCollisionSoundTime)
	{
		return;
	}

	const float CellSize = FMath::Max(2.0f, CollisionSpatialCellSize);
	const FIntVector Cell(
		FMath::FloorToInt(CollisionInfo.Location.X / CellSize),
		FMath::FloorToInt(CollisionInfo.Location.Y / CellSize),
		FMath::FloorToInt(CollisionInfo.Location.Z / CellSize));
	if (const double* NextCellTime = CollisionCellNextTime.Find(Cell))
	{
		if (Now < *NextCellTime)
		{
			return;
		}
	}
	if (CollisionCellNextTime.Num() > 128)
	{
		CollisionCellNextTime.Reset();
	}
	CollisionCellNextTime.Add(Cell, Now + CollisionSpatialCooldownSec);

	RecentCollisionTimes.RemoveAllSwap([Now](double T) { return (Now - T) > 0.12; });
	RecentCollisionTimes.Add(Now);
	const float DensityGain = FMath::Max(0.65f, 1.0f / FMath::Pow(static_cast<float>(FMath::Max(1, RecentCollisionTimes.Num())), 0.35f));

	const float Mass = FMath::Max(0.2f, CollisionInfo.Mass);
	const float MassAlpha = FMath::Clamp(Mass / 8.0f, 0.0f, 1.0f);
	const float ImpulseMag = CollisionInfo.AccumulatedImpulse.Size();
	const float ImpactEnergy = (ImpulseMag > KINDA_SMALL_NUMBER)
		? (0.5f * ImpulseMag * NormalSpeed)
		: (0.5f * Mass * NormalSpeed * NormalSpeed);
	const float Pressure = FMath::Sqrt(FMath::Max(0.0f, ImpactEnergy));
	const float NormPressure = FMath::Clamp((Pressure - 40.0f) / 700.0f, 0.0f, 1.5f);
	const float LoudnessAlpha = FMath::Clamp(FMath::Loge(1.0f + 5.0f * NormPressure) / FMath::Loge(6.0f), 0.0f, 1.0f);

	const float Intensity = FMath::Lerp(0.06f, 0.36f, LoudnessAlpha) * FMath::FRandRange(0.88f, 1.10f) * DensityGain;
	const float CutoffHz = FMath::Lerp(13500.0f, 4200.0f, MassAlpha) + FMath::FRandRange(-900.0f, 900.0f);
	const float DecaySec = FMath::Lerp(0.075f, 0.22f, MassAlpha) * FMath::FRandRange(0.85f, 1.15f);
	const float PitchOffset = FMath::Lerp(2.8f, -2.4f, MassAlpha) + FMath::FRandRange(-0.9f, 0.9f);

	FWaxAudioProfile ColProfile = ResolveAudioProfileAt(CollisionInfo.Location);
	if (ColProfile.CollisionVariants.Num() > 0)
	{
		TArray<int32, TInlineAllocator<8>> ValidIndices;
		for (int32 i = 0; i < ColProfile.CollisionVariants.Num(); ++i)
		{
			if (ColProfile.CollisionVariants[i].Sound)
			{
				ValidIndices.Add(i);
			}
		}
		if (ValidIndices.Num() > 0)
		{
			const FWaxCollisionVariant& Chosen = ColProfile.CollisionVariants[ValidIndices[FMath::RandRange(0, ValidIndices.Num() - 1)]];
			ColProfile.CrackSound = Chosen.Sound;
			ColProfile.MasterIntensity = Chosen.MasterIntensity;
			ColProfile.CrackleGain = Chosen.CrackleGain;
			ColProfile.SnapGain = Chosen.SnapGain;
			ColProfile.BodyGain = Chosen.BodyGain;
		}
	}

	ColProfile.CrackleGain *= CollisionCrackleScale;
	ColProfile.SnapGain *= CollisionSnapScale;
	ColProfile.BodyGain *= CollisionBodyScale;

	PlayOneShotCrack(CollisionInfo.Location, Intensity, CutoffHz, DecaySec, PitchOffset, ColProfile);
	const float ExpDelay = -FMath::Loge(FMath::Max(0.05f, FMath::FRand())) * MinCollisionAudioInterval;
	NextAllowedCollisionSoundTime = Now + FMath::Clamp(ExpDelay, MinCollisionAudioInterval * 0.5f, MinCollisionAudioInterval * 3.0f);
}

void UWaxDestructibleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetWorld())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 i = PendingGrains.Num() - 1; i >= 0; --i)
	{
		if (PendingGrains[i].FireTime <= Now)
		{
			const FWaxDestructibleGrain Grain = PendingGrains[i];
			PendingGrains.RemoveAtSwap(i);
			PlayOneShotCrack(Grain.Location, Grain.Intensity, Grain.CutoffHz, Grain.DecaySec, Grain.PitchOffset, Grain.Profile);
		}
	}

#if !UE_BUILD_SHIPPING
	if (bShowDebugCounters && GEngine && GetOwner() && TotalBreakEvents > 0)
	{
		const float RemainPct = GetRemainingRatio() * 100.0f;
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetOwner()->GetUniqueID()),
			0.2f,
			bHasCollapsed ? FColor::Orange : FColor::Green,
			FString::Printf(TEXT("[%s] Remain: %.0f%% (%d/%d broken) | %s"),
				*GetOwner()->GetName(), RemainPct, BrokenLeafIndices.Num(), TotalLeafCount, bHasCollapsed ? TEXT("COLLAPSED") : TEXT("ACTIVE"))
		);
	}
#endif

	RefreshTickEnabled();
}

void UWaxDestructibleComponent::PlayOneShotCrack(const FVector& Location, float Intensity, float CutoffHz, float DecaySec, float PitchOffset, const FWaxAudioProfile& Profile)
{
	if (!Profile.CrackSound || !GetWorld() || !GetOwner())
	{
		return;
	}

	UAudioComponent* AudioComp = NewObject<UAudioComponent>(GetOwner());
	if (!AudioComp)
	{
		return;
	}

	AudioComp->bAutoActivate = false;
	AudioComp->bAutoDestroy = true;
	AudioComp->SetSound(Profile.CrackSound);
	AudioComp->SetWorldLocation(Location);
	AudioComp->RegisterComponent();

	const float FinalIntensity = FMath::Clamp(Intensity * Profile.MasterIntensity, 0.02f, 1.5f);
	const float FinalPitchOffset = FMath::Clamp(PitchOffset, -6.0f, 6.0f);

	AudioComp->SetFloatParameter(TEXT("Intensity"), FinalIntensity);
	AudioComp->SetFloatParameter(TEXT("CutoffHz"), FMath::Clamp(CutoffHz, 2400.0f, 16000.0f));
	AudioComp->SetFloatParameter(TEXT("DecaySec"), FMath::Clamp(DecaySec, 0.025f, 0.28f));
	AudioComp->SetFloatParameter(TEXT("PitchOffset"), FinalPitchOffset);
	AudioComp->SetFloatParameter(TEXT("CrackleGain"), Profile.CrackleGain);
	AudioComp->SetFloatParameter(TEXT("SnapGain"), Profile.SnapGain);
	AudioComp->SetFloatParameter(TEXT("BodyGain"), Profile.BodyGain);
	AudioComp->Play(0.0f);
}

void UWaxDestructibleComponent::RefreshTickEnabled()
{
	const bool bNeedTick = (PendingGrains.Num() > 0) || (bShowDebugCounters && TotalBreakEvents > 0 && !bHasCollapsed);
	if (IsComponentTickEnabled() != bNeedTick)
	{
		SetComponentTickEnabled(bNeedTick);
	}
}

int32 UWaxDestructibleComponent::KickDebrisInRadius(UWorld* World, const FVector& Origin, const FVector& Velocity, float Radius, const AActor* IgnoreActor)
{
	if (!World || Radius <= 0.0f || Velocity.IsNearlyZero())
	{
		return 0;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WaxDebrisKick), false, IgnoreActor);
	if (!World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(Radius), QueryParams))
	{
		return 0;
	}

	int32 NumKicked = 0;
	TSet<AActor*> CheckedActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || CheckedActors.Contains(HitActor))
		{
			continue;
		}
		CheckedActors.Add(HitActor);

		UWaxDestructibleComponent* Destructible = HitActor->FindComponentByClass<UWaxDestructibleComponent>();
		if (Destructible && Destructible->HasBrokenPieces() && (!Destructible->HasCollapsed() || Destructible->bAllowKickAfterCollapse))
		{
			Destructible->ApplyDebrisKick(Origin, Velocity, Radius);
			++NumKicked;
		}
	}
	return NumKicked;
}
