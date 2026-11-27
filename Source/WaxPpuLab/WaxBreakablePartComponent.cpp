#include "WaxBreakablePartComponent.h"

#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Field/FieldSystemObjects.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Kismet/GameplayStatics.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "Sound/SoundBase.h"

UWaxBreakablePartComponent::UWaxBreakablePartComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

const FWaxBreakablePartSettings& UWaxBreakablePartComponent::GetPartSettings() const
{
	static const FWaxBreakablePartSettings DefaultSettings;
	return Settings ? *Settings : DefaultSettings;
}

float UWaxBreakablePartComponent::GetNow() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void UWaxBreakablePartComponent::InitializePart(UGeometryCollectionComponent* InCollection, UGeometryCollection* RestAsset, const FWaxBreakablePartSettings* InSettings, UObject* InSettingsOwner, float InLateralSign)
{
	if (Collection && Collection != InCollection)
	{
		Collection->OnChaosBreakEvent.RemoveDynamic(this, &UWaxBreakablePartComponent::HandleChaosBreakEvent);
	}

	Collection = InCollection;
	Settings = InSettings;
	SettingsOwner = InSettingsOwner;
	LateralSign = InLateralSign;

	BrokenLeafIndices.Reset();
	ReanchorUntil = -1.0f;
	DetachedExpireTime = -1.0f;
	HitCount = 0;
	bBroken = false;
	bDetached = false;
	bReleased = false;

	if (!Collection)
	{
		RefreshTickEnabled();
		return;
	}

	const FWaxBreakablePartSettings& S = GetPartSettings();
	if (RestAsset)
	{
		// 파괴 임계값은 물리 상태 생성 시점에 반영되므로 RestCollection 지정 전에 설정
		if (S.DamageThreshold > 0.0f)
		{
			Collection->DamageThreshold = { S.DamageThreshold, S.DamageThreshold * S.SubLevelThresholdRatios.X, S.DamageThreshold * S.SubLevelThresholdRatios.Y };
		}
		else if (RestAsset->DamageThreshold.Num() > 0)
		{
			Collection->DamageThreshold = RestAsset->DamageThreshold;
		}
		Collection->SetRestCollection(RestAsset, true);
	}
	Collection->SetSimulatePhysics(true);

	Collection->bAllowRemovalOnBreak = true;
	Collection->bAllowRemovalOnSleep = false;
	Collection->SetNotifyBreaks(true);
	Collection->OnChaosBreakEvent.RemoveDynamic(this, &UWaxBreakablePartComponent::HandleChaosBreakEvent);
	Collection->OnChaosBreakEvent.AddDynamic(this, &UWaxBreakablePartComponent::HandleChaosBreakEvent);

	BuildLeafCache();
	EnsureFieldNodes();

	// 컴포넌트 ObjectType은 Dynamic으로 두고(깨진 조각이 떨어지도록) 활성 루트만 Kinematic으로 고정
	StateValue->SetUniformInteger(static_cast<int32>(EObjectStateTypeEnum::Chaos_Object_Kinematic));
	Collection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_DynamicState, nullptr, StateValue);

	RefreshTickEnabled();
}

void UWaxBreakablePartComponent::BuildLeafCache()
{
	LeafNodeIndices.Reset();
	if (!Collection || !Collection->GetRestCollection())
	{
		return;
	}

	const TSharedPtr<const FGeometryCollection, ESPMode::ThreadSafe> GeomCol = Collection->GetRestCollection()->GetGeometryCollection();
	if (!GeomCol.IsValid())
	{
		return;
	}

	const TManagedArray<TSet<int32>>& Children = GeomCol->Children;
	const int32 NumTransforms = GeomCol->NumElements(FGeometryCollection::TransformGroup);
	for (int32 i = 0; i < NumTransforms; ++i)
	{
		if (Children[i].Num() == 0)
		{
			LeafNodeIndices.Add(i);
		}
	}
}

void UWaxBreakablePartComponent::EnsureFieldNodes()
{
	if (!MaskFalloff)
	{
		MaskFalloff = NewObject<URadialFalloff>(this);
		StrainFalloff = NewObject<URadialFalloff>(this);
		StateValue = NewObject<UUniformInteger>(this);
		UniformStrain = NewObject<UUniformScalar>(this);
		UniformVelocity = NewObject<UUniformVector>(this);
		LocalVelocity = NewObject<UCullingField>(this);
	}
}

bool UWaxBreakablePartComponent::ReceiveHit_Implementation(const FWaxHitPayload& Payload)
{
	return ApplyImpact(Payload.Center, Payload.Strain, Payload.Radius);
}

bool UWaxBreakablePartComponent::ApplyImpact(const FVector& WorldImpactPoint, float Strain, float Radius)
{
	if (!Collection || !Collection->IsVisible() || bDetached || bReleased || Radius <= 0.0f)
	{
		return false;
	}

	const FWaxBreakablePartSettings& S = GetPartSettings();
	if (S.HitAcceptRadius > 0.0f && FVector::Dist(WorldImpactPoint, Collection->GetComponentLocation()) > Radius + S.HitAcceptRadius)
	{
		return false;
	}

	OnWaxImpactApplied.Broadcast(WorldImpactPoint);

	++HitCount;
	if (!bBroken && HitCount < S.HitsToBreak)
	{
		PlayPartSound(S.HitSound, Collection->GetComponentLocation(), S.HitSoundVolume, S.HitSoundPitchRange);
		return true;
	}

	ApplyStrain(WorldImpactPoint, Strain, FMath::Max(Radius, S.MinImpactRadius));
	return true;
}

void UWaxBreakablePartComponent::ApplyStrain(const FVector& WorldImpactPoint, float Strain, float Radius)
{
	EnsureFieldNodes();
	StrainFalloff->SetRadialFalloff(Strain, GetPartSettings().StrainEdgeRatio, 1.0f, 0.0f, Radius, WorldImpactPoint, EFieldFalloffType::Field_Falloff_Linear);
	Collection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain, nullptr, StrainFalloff);
	RequestReanchor();
}

void UWaxBreakablePartComponent::HandleChaosBreakEvent(const FChaosBreakEvent& BreakEvent)
{
	const FWaxBreakablePartSettings& S = GetPartSettings();
	if (LeafNodeIndices.Contains(BreakEvent.Index))
	{
		BrokenLeafIndices.Add(BreakEvent.Index);
	}

	if (!bBroken)
	{
		bBroken = true;
		PlayPartSound(S.BreakSound, BreakEvent.Location, S.BreakSoundVolume, S.BreakSoundPitchRange);
		OnPartBroken.Broadcast(this, BreakEvent.Location);

		if (S.BreakMode == EWaxBreakMode::DetachWholeOnBreak && !bDetached && GetOwner())
		{
			DetachWhole(ComputeDetachPopVelocity(GetOwner()->GetActorForwardVector()));
		}
	}

	RequestReanchor();
	OnFragmentBroken(BreakEvent);
	RefreshTickEnabled();
}

void UWaxBreakablePartComponent::RequestReanchor()
{
	const FWaxBreakablePartSettings& S = GetPartSettings();
	if (S.BreakMode != EWaxBreakMode::ChipAndRetainBody || bReleased || !GetWorld())
	{
		return;
	}

	// 같은 물리 스텝에서는 아직 깨지기 전이므로 다음 프레임부터 검사. 이미 대기 중이면 시작 프레임을 미루지 않음
	if (ReanchorUntil < GetNow())
	{
		ReanchorEarliestFrame = GFrameCounter + 1;
	}
	ReanchorUntil = FMath::Max(ReanchorUntil, GetNow() + S.ReanchorWindowSec);
	RefreshTickEnabled();
}

void UWaxBreakablePartComponent::ReanchorBody()
{
	// 깨지면서 새로 생긴 남은 클러스터는 Dynamic이 되어 몸을 안 따라가므로, 가장 큰 활성 클러스터(몸통)만 Kinematic으로 되돌리고 나머지 조각·섬은 그대로 떨어지게 둠
	FGeometryCollectionPhysicsProxy* Proxy = Collection ? Collection->GetPhysicsProxy() : nullptr;
	Chaos::FPhysicsSolver* Solver = Proxy ? Proxy->GetSolver<Chaos::FPhysicsSolver>() : nullptr;
	if (!Solver)
	{
		return;
	}

	Solver->EnqueueCommandImmediate([Proxy, Solver]()
	{
		Chaos::FPBDRigidsEvolution* Evolution = Solver->GetEvolution();
		if (!Evolution)
		{
			return;
		}

		const Chaos::FRigidClustering::FClusterMap& ChildrenMap = Evolution->GetRigidClustering().GetChildrenMap();
		TFunction<int32(Chaos::FPBDRigidParticleHandle*)> CountLeaves = [&ChildrenMap, &CountLeaves](Chaos::FPBDRigidParticleHandle* Handle) -> int32
		{
			Chaos::FPBDRigidClusteredParticleHandle* Clustered = Handle ? Handle->CastToClustered() : nullptr;
			const TArray<Chaos::FPBDRigidParticleHandle*>* Children = Clustered ? ChildrenMap.Find(Clustered) : nullptr;
			if (!Children || Children->Num() == 0)
			{
				return 1;
			}
			int32 Count = 0;
			for (Chaos::FPBDRigidParticleHandle* Child : *Children)
			{
				Count += CountLeaves(Child);
			}
			return Count;
		};
		TArray<Chaos::FPBDRigidClusteredParticleHandle*> ActiveClusters;
		Chaos::FPBDRigidClusteredParticleHandle* Body = nullptr;
		int32 BodyLeafCount = 0;
		for (Chaos::FPBDRigidClusteredParticleHandle* Handle : Proxy->GetUnorderedParticles_Internal())
		{
			if (!Handle || Handle->Disabled())
			{
				continue;
			}
			const TArray<Chaos::FPBDRigidParticleHandle*>* Children = ChildrenMap.Find(Handle);
			if (!Children || Children->Num() == 0)
			{
				continue;
			}
			ActiveClusters.Add(Handle);
			const int32 LeafCount = CountLeaves(Handle);
			if (LeafCount > BodyLeafCount)
			{
				Body = Handle;
				BodyLeafCount = LeafCount;
			}
		}

		for (Chaos::FPBDRigidClusteredParticleHandle* Cluster : ActiveClusters)
		{
			if (Cluster == Body)
			{
				if (Cluster->ObjectState() != Chaos::EObjectStateType::Kinematic)
				{
					Evolution->SetParticleObjectState(Cluster, Chaos::EObjectStateType::Kinematic);
				}
				continue;
			}
			if (Cluster->ObjectState() == Chaos::EObjectStateType::Kinematic)
			{
				Evolution->SetParticleObjectState(Cluster, Chaos::EObjectStateType::Dynamic);
				Evolution->SetParticleKinematicTarget(Cluster, Chaos::FKinematicTarget());
			}
		}
	});
}

void UWaxBreakablePartComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float Now = GetNow();
	if (!bReleased && ReanchorUntil >= Now && GFrameCounter >= ReanchorEarliestFrame)
	{
		ReanchorBody();
	}

	if (bDetached && DetachedExpireTime >= 0.0f && Now >= DetachedExpireTime && Collection)
	{
		DetachedExpireTime = -1.0f;
		Collection->SetVisibility(false);
		Collection->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	RefreshTickEnabled();
}

bool UWaxBreakablePartComponent::NeedsTick() const
{
	const float Now = GetNow();
	return (!bReleased && ReanchorUntil >= Now) || DetachedExpireTime >= 0.0f;
}

void UWaxBreakablePartComponent::RefreshTickEnabled()
{
	const bool bNeed = NeedsTick();
	if (IsComponentTickEnabled() != bNeed)
	{
		SetComponentTickEnabled(bNeed);
	}
}

void UWaxBreakablePartComponent::ApplyDebrisVelocity(const FVector& Origin, const FVector& Velocity, float Radius)
{
	if (!Collection || (!HasBrokenPieces() && !bDetached) || Radius <= 0.0f || Velocity.IsNearlyZero())
	{
		return;
	}

	const FWaxBreakablePartSettings& S = GetPartSettings();
	const FBoxSphereBounds Bounds = Collection->Bounds;
	const float MaxDist = Bounds.SphereRadius + Radius + S.DebrisKickRangeMargin;
	if (FVector::DistSquared(Origin, Bounds.Origin) > FMath::Square(MaxDist))
	{
		return;
	}

	EnsureFieldNodes();
	MaskFalloff->SetRadialFalloff(1.0f, S.DebrisKickEdgeRatio, 1.0f, 0.0f, Radius, Origin, EFieldFalloffType::Field_Falloff_Linear);
	UniformVelocity->SetUniformVector(Velocity.Size(), Velocity.GetSafeNormal());
	LocalVelocity->SetCullingField(MaskFalloff, UniformVelocity, EFieldCullingOperationType::Field_Culling_Outside);
	// 몸통은 Kinematic이라 속도 필드의 영향을 받지 않고, 떨어진 조각만 날아감
	Collection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_LinearVelocity, nullptr, LocalVelocity);
}

int32 UWaxBreakablePartComponent::KickFragmentsInRadius(UWorld* World, const FVector& Origin, const FVector& Velocity, float Radius, const AActor* IgnoreActor)
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

	TSet<UWaxBreakablePartComponent*> Kicked;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor)
		{
			continue;
		}
		TInlineComponentArray<UWaxBreakablePartComponent*> Parts(HitActor);
		for (UWaxBreakablePartComponent* Part : Parts)
		{
			if (Part && Part->GetCollection() == Overlap.GetComponent() && !Kicked.Contains(Part))
			{
				Kicked.Add(Part);
				Part->ApplyDebrisVelocity(Origin, Velocity, Radius);
			}
		}
	}
	return Kicked.Num();
}

FVector UWaxBreakablePartComponent::ComputeDetachPopVelocity(const FVector& BodyForward) const
{
	const FVector Pop = GetPartSettings().DetachPopVelocity;
	FVector Fwd = BodyForward.GetSafeNormal2D();
	if (Fwd.IsNearlyZero())
	{
		Fwd = FVector::ForwardVector;
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
	return Fwd * Pop.X + Right * (Pop.Y * LateralSign) + FVector::UpVector * Pop.Z;
}

void UWaxBreakablePartComponent::DetachWhole(const FVector& WorldPopVelocity)
{
	if (!Collection || !Collection->IsVisible() || bDetached)
	{
		return;
	}

	bDetached = true;
	EnsureFieldNodes();

	Collection->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	Collection->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	StateValue->SetUniformInteger(static_cast<int32>(EObjectStateTypeEnum::Chaos_Object_Dynamic));
	Collection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_DynamicState, nullptr, StateValue);

	if (!WorldPopVelocity.IsNearlyZero())
	{
		UniformVelocity->SetUniformVector(WorldPopVelocity.Size(), WorldPopVelocity.GetSafeNormal());
		Collection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_LinearVelocity, nullptr, UniformVelocity);
	}

	const float LifeSpan = GetPartSettings().DetachedLifeSpanSec;
	DetachedExpireTime = LifeSpan > 0.0f ? GetNow() + LifeSpan : -1.0f;
	RefreshTickEnabled();
}

void UWaxBreakablePartComponent::ShatterWhole()
{
	if (!Collection || !Collection->IsVisible() || bBroken)
	{
		return;
	}

	EnsureFieldNodes();
	const FWaxBreakablePartSettings& S = GetPartSettings();
	const float BaseThreshold = S.DamageThreshold > 0.0f ? S.DamageThreshold : (Collection->DamageThreshold.Num() > 0 ? Collection->DamageThreshold[0] : 0.0f);
	UniformStrain->SetUniformScalar(BaseThreshold * S.ShatterStrainScale);
	Collection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain, nullptr, UniformStrain);
}

void UWaxBreakablePartComponent::ReleaseAll(float ReleaseStrain)
{
	if (!Collection || bReleased)
	{
		return;
	}

	bReleased = true;
	EnsureFieldNodes();

	StateValue->SetUniformInteger(static_cast<int32>(EObjectStateTypeEnum::Chaos_Object_Dynamic));
	Collection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_DynamicState, nullptr, StateValue);

	if (ReleaseStrain > 0.0f)
	{
		UniformStrain->SetUniformScalar(ReleaseStrain);
		Collection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain, nullptr, UniformStrain);
	}
	RefreshTickEnabled();
}

float UWaxBreakablePartComponent::GetRemainingRatio() const
{
	if (LeafNodeIndices.Num() == 0)
	{
		return 1.0f;
	}
	return FMath::Clamp(1.0f - static_cast<float>(BrokenLeafIndices.Num()) / static_cast<float>(LeafNodeIndices.Num()), 0.0f, 1.0f);
}

void UWaxBreakablePartComponent::PlayPartSound(USoundBase* Sound, const FVector& Location, float Volume, const FVector2D& PitchRange) const
{
	if (Sound && GetWorld())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, Volume, FMath::FRandRange(PitchRange.X, PitchRange.Y));
	}
}
