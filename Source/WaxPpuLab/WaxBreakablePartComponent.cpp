#include "WaxBreakablePartComponent.h"

#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Field/FieldSystemObjects.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/Facades/CollectionRemoveOnBreakFacade.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionEngineRemoval.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollectionProxyData.h"
#include "Kismet/GameplayStatics.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/FieldSystemProxyHelper.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogWaxBreakable, Log, All);

namespace
{
	const FName PostBreakDurationName(TEXT("PostBreakDuration"));
	const FName BreakRemovalDurationName(TEXT("BreakRemovalDuration"));

	float RandInRange(FRandomStream& Rand, const FVector2D& Range)
	{
		const float Lo = FMath::Max(0.0f, static_cast<float>(FMath::Min(Range.X, Range.Y)));
		const float Hi = FMath::Max(Lo, static_cast<float>(FMath::Max(Range.X, Range.Y)));
		return Rand.FRandRange(Lo, Hi);
	}
}

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
	LooseRuleUntil = -1.0f;
	RemovalSeed = static_cast<int32>(GetUniqueID());
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

	// GC 틱(엔진 RemoveOnBreak 타이머 갱신) 뒤에 실행돼야 덮어쓴 수명 값이 유지됨
	AddTickPrerequisiteComponent(Collection);

	if (Collection->GetRestCollection() && !Collection->GetRestCollection()->bScaleOnRemoval)
	{
		UE_LOG(LogWaxBreakable, Warning, TEXT("%s: GC 에셋 %s의 bScaleOnRemoval이 꺼져 있어 조각이 줄지 않고 바로 사라집니다"),
			*GetPathName(), *Collection->GetRestCollection()->GetName());
	}

	BuildLeafCache();
	EnsureFieldNodes();

	// 컴포넌트 ObjectType은 Dynamic으로 두고 활성 루트(와 직계 자식)만 Kinematic으로 고정. 이 Kinematic 덩어리가 몸 본체이며 컴포넌트 이동을 따라감
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

	// 타격은 Strain만 가함. 반경 안을 Dynamic으로 바꾸면 반경이 큰 공격(발차기)에서 본체까지 Dynamic이 되어 몸에서 떨어져 나가고 수명 타이머가 흐름.
	// 떨어진 조각의 Dynamic 전환은 EnforceAnchorRule이 맡음
	StrainFalloff->SetRadialFalloff(Strain, GetPartSettings().StrainEdgeRatio, 1.0f, 0.0f, Radius, WorldImpactPoint, EFieldFalloffType::Field_Falloff_Linear);
	Collection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain, nullptr, StrainFalloff);
	RefreshTickEnabled();
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

	OnFragmentBroken(BreakEvent);
	RefreshTickEnabled();
}

bool UWaxBreakablePartComponent::NeedsAnchorRule() const
{
	if (!Collection)
	{
		return false;
	}
	if (bDetached || bReleased)
	{
		return LooseRuleUntil >= GetNow();
	}
	return HasBrokenPieces() || bBroken;
}

void UWaxBreakablePartComponent::EnforceAnchorRule()
{
	// 규칙: Kinematic(몸에 붙어 따라감)은 남은 본체 하나만. 나머지 활성 조각·덩어리는 모두 Dynamic으로 떨어뜨려야 엔진 RemoveOnBreak 수명이 흐름.
	// 떼어 냄·전체 붕괴 이후에는 본체도 없음
	FGeometryCollectionPhysicsProxy* Proxy = Collection ? Collection->GetPhysicsProxy() : nullptr;
	Chaos::FPhysicsSolver* Solver = Proxy ? Proxy->GetSolver<Chaos::FPhysicsSolver>() : nullptr;
	if (!Solver)
	{
		return;
	}

	const bool bKeepBody = GetPartSettings().BreakMode == EWaxBreakMode::ChipAndRetainBody && !bDetached && !bReleased;
	Solver->EnqueueCommandImmediate([Proxy, Solver, bKeepBody]()
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

		// 깨질 때 생긴 내부 클러스터는 프록시 파티클 목록에 없으므로 각 파티클의 최상위 부모로 활성 덩어리를 모음 (낱개 조각 포함)
		TSet<Chaos::FPBDRigidClusteredParticleHandle*> ActiveTops;
		for (Chaos::FPBDRigidClusteredParticleHandle* Handle : Proxy->GetUnorderedParticles_Internal())
		{
			Chaos::FPBDRigidClusteredParticleHandle* Top = Handle;
			while (Top && Top->Parent())
			{
				Top = Top->Parent();
			}
			if (Top && !Top->Disabled())
			{
				ActiveTops.Add(Top);
			}
		}

		// 본체 = 이미 Kinematic인 덩어리 중 조각 수가 가장 많은 것. Kinematic 덩어리가 하나도 없으면(깨질 때 남은 덩어리가 Dynamic으로 생성된 경우) 가장 큰 덩어리를 다시 고정
		Chaos::FPBDRigidClusteredParticleHandle* Body = nullptr;
		if (bKeepBody)
		{
			int32 KinematicLeafCount = 0;
			int32 AnyLeafCount = 0;
			Chaos::FPBDRigidClusteredParticleHandle* LargestAny = nullptr;
			for (Chaos::FPBDRigidClusteredParticleHandle* Top : ActiveTops)
			{
				const int32 LeafCount = CountLeaves(Top);
				if (LeafCount > AnyLeafCount)
				{
					LargestAny = Top;
					AnyLeafCount = LeafCount;
				}
				if (Top->ObjectState() == Chaos::EObjectStateType::Kinematic && LeafCount > KinematicLeafCount)
				{
					Body = Top;
					KinematicLeafCount = LeafCount;
				}
			}
			if (!Body && LargestAny && AnyLeafCount > 1)
			{
				Body = LargestAny;
				// 엔진 필드와 같은 경로로 전환 (GC 파티클은 SetParticleObjectState를 쓰면 활성 배열이 어긋나 속도가 폭주함). Kinematic 전환 시 속도도 0으로
				Chaos::ReportDynamicStateResult(Solver, Chaos::EObjectStateType::Kinematic, Body, false, Chaos::FVec3(0), false, Chaos::FVec3(0));
			}
		}

		for (Chaos::FPBDRigidClusteredParticleHandle* Top : ActiveTops)
		{
			// 엔진 필드와 같은 경로. 질량이 0인 파티클은 Dynamic으로 바꾸지 않음
			if (Top != Body && Top->ObjectState() == Chaos::EObjectStateType::Kinematic
				&& Chaos::ReportDynamicStateResult(Solver, Chaos::EObjectStateType::Dynamic, Top, false, Chaos::FVec3(0), false, Chaos::FVec3(0)))
			{
				Evolution->SetParticleKinematicTarget(Top, Chaos::FKinematicTarget());
			}
		}
	});
}

void UWaxBreakablePartComponent::SyncFragmentRemovalTimers()
{
	// 엔진 RemoveOnBreak(조각마다 수명 → 축소 → 제거)는 그대로 쓰고, 시간 값만 DA 부위 설정으로 덮어씀.
	// 엔진은 첫 깨짐 뒤 GC 틱에서 에셋 Dataflow 값으로 동적 속성을 채우므로, 같은 시드로 매번 같은 값을 다시 씀(멱등)
	FGeometryDynamicCollection* Dyn = Collection ? Collection->GetDynamicCollection() : nullptr;
	const UGeometryCollection* Rest = Collection ? Collection->GetRestCollection() : nullptr;
	if (!Dyn || !Rest || !Rest->GetGeometryCollection().IsValid())
	{
		return;
	}

	FGeometryCollectionRemoveOnBreakDynamicFacade RemovalFacade(*Dyn);
	if (!RemovalFacade.IsValid())
	{
		const GeometryCollection::Facades::FCollectionRemoveOnBreakFacade RestFacade(*Rest->GetGeometryCollection());
		if (RestFacade.IsValid())
		{
			// 엔진이 곧 초기화함. 다음 틱에 덮어씀
			return;
		}
		// 에셋에 RemoveOnBreak 노드가 없어도 같은 규칙이 돌도록 동적 속성을 직접 정의
		FGeometryCollectionDecayDynamicFacade(*Dyn).AddAttributes();
		RemovalFacade.DefineSchema();
	}

	TManagedArray<float>* PostBreak = Dyn->FindAttributeTyped<float>(PostBreakDurationName, FGeometryCollection::TransformGroup);
	TManagedArray<float>* RemovalDuration = Dyn->FindAttributeTyped<float>(BreakRemovalDurationName, FGeometryCollection::TransformGroup);
	if (!PostBreak || !RemovalDuration)
	{
		return;
	}

	const FWaxBreakablePartSettings& S = GetPartSettings();
	const TManagedArray<TSet<int32>>& Children = Rest->GetGeometryCollection()->Children;
	const int32 Num = FMath::Min3(PostBreak->Num(), RemovalDuration->Num(), Children.Num());
	FRandomStream Rand(RemovalSeed);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const float Life = RandInRange(Rand, S.FragmentLifeSecRange);
		const float Shrink = RandInRange(Rand, S.FragmentShrinkSecRange);
		if (Children[Index].Num() > 0)
		{
			// 덩어리는 축소하지 않고 낱개로 흩어짐(크럼블) → 조각마다 따로 수명이 흐름
			(*PostBreak)[Index] = S.ClusterCrumbleDelaySec;
			(*RemovalDuration)[Index] = FGeometryCollectionRemoveOnBreakDynamicFacade::CrumblingRemovalTimer;
		}
		else
		{
			(*PostBreak)[Index] = Life;
			(*RemovalDuration)[Index] = Shrink;
		}
	}
}

void UWaxBreakablePartComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (NeedsAnchorRule())
	{
		EnforceAnchorRule();
		SyncFragmentRemovalTimers();
	}

	RefreshTickEnabled();
}

bool UWaxBreakablePartComponent::NeedsTick() const
{
	return NeedsAnchorRule();
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

	ExtendLooseRule();
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
	ExtendLooseRule();
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

void UWaxBreakablePartComponent::ExtendLooseRule()
{
	// 모든 조각이 흩어지고 수명·축소를 마칠 때까지 정리 규칙을 유지
	const FWaxBreakablePartSettings& S = GetPartSettings();
	const float MaxLife = FMath::Max(S.FragmentLifeSecRange.X, S.FragmentLifeSecRange.Y);
	const float MaxShrink = FMath::Max(S.FragmentShrinkSecRange.X, S.FragmentShrinkSecRange.Y);
	LooseRuleUntil = FMath::Max(LooseRuleUntil, GetNow() + S.ClusterCrumbleDelaySec + S.DetachShatterDelaySec + MaxLife + MaxShrink + 1.0f);
}
