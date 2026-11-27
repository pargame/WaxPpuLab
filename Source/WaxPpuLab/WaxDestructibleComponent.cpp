#include "WaxDestructibleComponent.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogWaxDestruction, Log, All);

UWaxDestructibleComponent::UWaxDestructibleComponent()
{
}

const UWaxMonsterDataAsset& UWaxDestructibleComponent::Data() const
{
	return DataAsset ? *DataAsset : *GetDefault<UWaxMonsterDataAsset>();
}

void UWaxDestructibleComponent::InitializeWithCollection(UGeometryCollectionComponent* InShellCollection, UWaxMonsterDataAsset* InDataAsset)
{
	if (Collection && Collection != InShellCollection)
	{
		Collection->OnChaosPhysicsCollision.RemoveDynamic(this, &UWaxDestructibleComponent::HandleChaosCollisionEvent);
	}

	DataAsset = InDataAsset;
	PendingGrains.Reset();
	CollisionCellNextTime.Reset();
	RecentCollisionTimes.Reset();
	ZoneAccumulatedDamage.Init(0.0f, Data().HardnessZones.Num());
	ActiveAudioProfile = Data().DefaultAudioProfile;
	NextAllowedCollisionSoundTime = 0.0;
	CollapseTimestamp = -1.0;
	TotalBreakEvents = 0;
	TotalCollisionEvents = 0;
	bHasCollapsed = false;
	bDevAudioOverride = false;

	if (!InShellCollection)
	{
		InitializePart(nullptr, nullptr, nullptr, nullptr);
		return;
	}

	InShellCollection->SetCustomPrimitiveDataFloat(0, 0.0f);
	InShellCollection->bNotifyCollisions = true;
	InShellCollection->SetNotifyRigidBodyCollision(true);
	InShellCollection->OnChaosPhysicsCollision.RemoveDynamic(this, &UWaxDestructibleComponent::HandleChaosCollisionEvent);
	InShellCollection->OnChaosPhysicsCollision.AddDynamic(this, &UWaxDestructibleComponent::HandleChaosCollisionEvent);

	InitializePart(InShellCollection, Data().ShellCollectionAsset, DataAsset ? &DataAsset->ShellPart : nullptr, DataAsset);
}

int32 UWaxDestructibleComponent::FindMatchingZoneIndex(const FVector& WorldPoint) const
{
	if (!Collection || Data().HardnessZones.Num() == 0)
	{
		return INDEX_NONE;
	}

	const FVector LocalPoint = Collection->GetComponentTransform().InverseTransformPosition(WorldPoint);
	int32 BestZoneIdx = INDEX_NONE;
	float BestRadius = TNumericLimits<float>::Max();

	for (int32 i = 0; i < Data().HardnessZones.Num(); ++i)
	{
		const FWaxHardnessZone& Zone = Data().HardnessZones[i];
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
		if (ZoneIdx != INDEX_NONE && Data().HardnessZones[ZoneIdx].bUseZoneAudioProfile && Data().HardnessZones[ZoneIdx].AudioProfile.CrackSound)
		{
			return Data().HardnessZones[ZoneIdx].AudioProfile;
		}
	}
	return ActiveAudioProfile;
}

bool UWaxDestructibleComponent::ApplyImpact(const FVector& WorldImpactPoint, float Strain, float Radius)
{
	if (!Collection || bHasCollapsed || Radius <= 0.0f)
	{
		return false;
	}

	float EffectiveStrain = Strain;
	const int32 ZoneIdx = FindMatchingZoneIndex(WorldImpactPoint);
	if (ZoneIdx != INDEX_NONE && ZoneAccumulatedDamage.IsValidIndex(ZoneIdx))
	{
		const FWaxHardnessZone& Zone = Data().HardnessZones[ZoneIdx];
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
				const FWaxCrackSynthTuning& T = Data().CrackSynth;
				OnWaxImpactApplied.Broadcast(WorldImpactPoint);
				PlayOneShotCrack(WorldImpactPoint, T.ZoneHitIntensity, T.ZoneHitCutoffHz, T.ZoneHitDecaySec, T.ZoneHitPitchOffset, ResolveAudioProfileAt(WorldImpactPoint));
				return true;
			}
		}

		EffectiveStrain *= Zone.StrainMultiplier;
	}

#if !UE_BUILD_SHIPPING
	if (bShowDebugCounters && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9996, 1.0f, FColor::Cyan, FString::Printf(TEXT("[WaxImpact] Strain=%.0f R=%.0f"), EffectiveStrain, Radius));
	}
#endif

	return Super::ApplyImpact(WorldImpactPoint, EffectiveStrain, Radius);
}

void UWaxDestructibleComponent::ApplyDebrisVelocity(const FVector& Origin, const FVector& Velocity, float Radius)
{
	if (bHasCollapsed && !Data().bAllowKickAfterCollapse)
	{
		return;
	}
	Super::ApplyDebrisVelocity(Origin, Velocity, Radius);
}

void UWaxDestructibleComponent::TriggerRemainingCollapse()
{
	if (!Collection || bHasCollapsed)
	{
		return;
	}

	bHasCollapsed = true;
	CollapseTimestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	const float BrokenPct = (1.0f - GetRemainingRatio()) * 100.0f;
	const float ThresholdBrokenPct = (1.0f - Data().CollapseRemainingRatio) * 100.0f;
	UE_LOG(LogWaxDestruction, Log, TEXT("WaxDestruction Collapse actor=%s broken=%.0f%% (%d/%d pieces) threshold=%.0f%% broken (remaining<=%.0f%%) t=%.2f"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("None"), BrokenPct, BrokenLeafIndices.Num(), LeafNodeIndices.Num(),
		ThresholdBrokenPct, Data().CollapseRemainingRatio * 100.0f, CollapseTimestamp);
	Collection->SetCustomPrimitiveDataFloat(0, 1.0f);
	ReleaseAll(Data().CollapseReleaseStrain);
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

void UWaxDestructibleComponent::OnFragmentBroken(const FChaosBreakEvent& BreakEvent)
{
	TotalBreakEvents++;

	if (!bHasCollapsed && GetRemainingRatio() <= Data().CollapseRemainingRatio)
	{
		TriggerRemainingCollapse();
	}

	const int32 ActiveMaxQueue = bHasCollapsed ? FMath::Max(Data().MaxQueuedGrains, Data().CollapseMaxQueuedGrains) : Data().MaxQueuedGrains;
	if (GetWorld() && PendingGrains.Num() < ActiveMaxQueue)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		const FWaxCrackSynthTuning& T = Data().CrackSynth;
		const float MassAlpha = FMath::Clamp(BreakEvent.Mass / FMath::Max(0.1f, T.MassReferenceKg), 0.0f, 1.0f);
		const float SpeedAlpha = FMath::Clamp(BreakEvent.Velocity.Size() / FMath::Max(1.0f, T.BreakSpeedReference), T.BreakSpeedAlphaMin, 1.0f);

		float Delay = 0.0f;
		float DelayIntensityScale = 1.0f;
		if (PendingGrains.Num() > 0)
		{
			if (bHasCollapsed)
			{
				const float U = FMath::FRand();
				Delay = Data().CollapseGrainSpreadMaxSec * U * U;
				DelayIntensityScale = FMath::Lerp(1.0f, T.CollapseTailIntensityScale, Delay / FMath::Max(0.05f, Data().CollapseGrainSpreadMaxSec));
			}
			else
			{
				Delay = FMath::FRandRange(Data().BreakGrainSpreadMinSec, Data().BreakGrainSpreadMaxSec);
			}
		}

		const float QueueDensityScale = 1.0f / FMath::Pow(static_cast<float>(FMath::Max(1, PendingGrains.Num() + 1)), T.QueueDensityExponent);

		FWaxDestructibleGrain Grain;
		Grain.FireTime = Now + Delay;
		Grain.Location = BreakEvent.Location;
		Grain.Intensity = FMath::Clamp(T.BreakIntensityBase + T.BreakIntensityGain * FMath::Max(MassAlpha, SpeedAlpha), T.BreakIntensityClamp.X, T.BreakIntensityClamp.Y)
			* FMath::FRandRange(T.BreakIntensityJitter.X, T.BreakIntensityJitter.Y) * DelayIntensityScale * QueueDensityScale;
		Grain.CutoffHz = FMath::Lerp(T.CutoffHzLightToHeavy.X, T.CutoffHzLightToHeavy.Y, MassAlpha) + FMath::FRandRange(-T.CutoffJitterHz, T.CutoffJitterHz);
		Grain.DecaySec = FMath::Lerp(T.DecaySecLightToHeavy.X, T.DecaySecLightToHeavy.Y, MassAlpha) * FMath::FRandRange(T.DecayJitterScale.X, T.DecayJitterScale.Y);
		Grain.PitchOffset = FMath::Lerp(T.PitchLightToHeavy.X, T.PitchLightToHeavy.Y, MassAlpha) + FMath::FRandRange(-T.PitchJitter, T.PitchJitter);
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
	if (bHasCollapsed && Data().bMuteCollisionAudioAfterCollapse)
	{
		if (CollapseTimestamp < 0.0 || (Now - CollapseTimestamp) > Data().CollapseCollisionMaxGraceSec)
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

	if (NormalSpeed < Data().CollisionSoundSpeedThreshold)
	{
		return;
	}

	if (Now < NextAllowedCollisionSoundTime)
	{
		return;
	}

	const float CellSize = FMath::Max(2.0f, Data().CollisionSpatialCellSize);
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
	CollisionCellNextTime.Add(Cell, Now + Data().CollisionSpatialCooldownSec);

	const FWaxCrackSynthTuning& T = Data().CrackSynth;
	const double DensityWindow = T.CollisionDensityWindowSec;
	RecentCollisionTimes.RemoveAllSwap([Now, DensityWindow](double Time) { return (Now - Time) > DensityWindow; });
	RecentCollisionTimes.Add(Now);
	const float DensityGain = FMath::Max(T.CollisionDensityMinGain, 1.0f / FMath::Pow(static_cast<float>(FMath::Max(1, RecentCollisionTimes.Num())), T.CollisionDensityExponent));

	const float Mass = FMath::Max(T.CollisionMinMassKg, CollisionInfo.Mass);
	const float MassAlpha = FMath::Clamp(Mass / FMath::Max(0.1f, T.MassReferenceKg), 0.0f, 1.0f);
	const float ImpulseMag = CollisionInfo.AccumulatedImpulse.Size();
	const float ImpactEnergy = (ImpulseMag > KINDA_SMALL_NUMBER)
		? (0.5f * ImpulseMag * NormalSpeed)
		: (0.5f * Mass * NormalSpeed * NormalSpeed);
	const float Pressure = FMath::Sqrt(FMath::Max(0.0f, ImpactEnergy));
	const float NormPressure = FMath::Clamp((Pressure - T.CollisionPressureOffset) / FMath::Max(1.0f, T.CollisionPressureRange), 0.0f, 1.5f);
	const float Curve = FMath::Max(0.1f, T.CollisionLoudnessCurve);
	const float LoudnessAlpha = FMath::Clamp(FMath::Loge(1.0f + Curve * NormPressure) / FMath::Loge(1.0f + Curve), 0.0f, 1.0f);

	const float Intensity = FMath::Lerp(T.CollisionIntensityRange.X, T.CollisionIntensityRange.Y, LoudnessAlpha) * FMath::FRandRange(T.CollisionIntensityJitter.X, T.CollisionIntensityJitter.Y) * DensityGain;
	const float CutoffHz = FMath::Lerp(T.CutoffHzLightToHeavy.X, T.CutoffHzLightToHeavy.Y, MassAlpha) + FMath::FRandRange(-T.CutoffJitterHz, T.CutoffJitterHz);
	const float DecaySec = FMath::Lerp(T.DecaySecLightToHeavy.X, T.DecaySecLightToHeavy.Y, MassAlpha) * FMath::FRandRange(T.DecayJitterScale.X, T.DecayJitterScale.Y);
	const float PitchOffset = FMath::Lerp(T.PitchLightToHeavy.X, T.PitchLightToHeavy.Y, MassAlpha) + FMath::FRandRange(-T.PitchJitter, T.PitchJitter);

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

	ColProfile.CrackleGain *= Data().CollisionCrackleScale;
	ColProfile.SnapGain *= Data().CollisionSnapScale;
	ColProfile.BodyGain *= Data().CollisionBodyScale;

	PlayOneShotCrack(CollisionInfo.Location, Intensity, CutoffHz, DecaySec, PitchOffset, ColProfile);
	const float ExpDelay = -FMath::Loge(FMath::Max(0.05f, FMath::FRand())) * Data().MinCollisionAudioInterval;
	NextAllowedCollisionSoundTime = Now + FMath::Clamp(ExpDelay, Data().MinCollisionAudioInterval * 0.5f, Data().MinCollisionAudioInterval * 3.0f);
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
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetOwner()->GetUniqueID()),
			0.2f,
			bHasCollapsed ? FColor::Orange : FColor::Green,
			FString::Printf(TEXT("[%s] Remain: %.0f%% (%d/%d broken) | %s"),
				*GetOwner()->GetName(), GetRemainingRatio() * 100.0f, BrokenLeafIndices.Num(), LeafNodeIndices.Num(), bHasCollapsed ? TEXT("COLLAPSED") : TEXT("ACTIVE")));
	}
#endif

	RefreshTickEnabled();
}

bool UWaxDestructibleComponent::NeedsTick() const
{
	return Super::NeedsTick() || PendingGrains.Num() > 0 || (bShowDebugCounters && TotalBreakEvents > 0 && !bHasCollapsed);
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
