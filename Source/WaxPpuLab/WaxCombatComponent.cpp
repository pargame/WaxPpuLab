#include "WaxCombatComponent.h"

#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyQueue.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "WaxAttackHitNotify.h"
#include "WaxAttackSetDataAsset.h"
#include "WaxCombatFlowInterface.h"
#include "WaxBreakablePartComponent.h"
#include "WaxHittable.h"
#include "WaxPpuLabCharacter.h"

DEFINE_LOG_CATEGORY(LogWaxCombat);

UWaxCombatComponent::UWaxCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UWaxCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	SetMarkerVisible(false);

	// 캐릭터 Tick(상반신 Pitch·카메라)이 이 컴포넌트가 보정한 회전을 같은 프레임에 읽도록 순서 고정
	if (OwnerCharacter)
	{
		OwnerCharacter->PrimaryActorTick.AddPrerequisite(this, PrimaryComponentTick);
	}

	if (!AttackSet)
	{
		UE_LOG(LogWaxCombat, Warning, TEXT("WaxCombat owner=%s AttackSet is not assigned."), *GetNameSafe(GetOwner()));
	}

	if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
	{
		BaseMaxWalkSpeed = OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed;
	}
}

double UWaxCombatComponent::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

const FWaxCombatSettings& UWaxCombatComponent::GetSettings() const
{
	static const FWaxCombatSettings DefaultSettings;
	return AttackSet ? AttackSet->Settings : DefaultSettings;
}

UAnimInstance* UWaxCombatComponent::GetAnimInstance() const
{
	USkeletalMeshComponent* BodyMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	return BodyMesh ? BodyMesh->GetAnimInstance() : nullptr;
}

const FWaxAttackSpec* UWaxCombatComponent::ResolveSpec(EWaxAttackSlot Slot) const
{
	return AttackSet ? AttackSet->FindSpec(Slot) : nullptr;
}

bool UWaxCombatComponent::IsFalling() const
{
	const UCharacterMovementComponent* Move = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	return Move && Move->IsFalling();
}

const TCHAR* UWaxCombatComponent::SlotName(EWaxAttackSlot Slot) const
{
	switch (Slot)
	{
	case EWaxAttackSlot::Basic: return TEXT("Basic");
	case EWaxAttackSlot::AerialStrike: return TEXT("AerialStrike");
	case EWaxAttackSlot::Slam: return TEXT("Slam");
	case EWaxAttackSlot::Kick: return TEXT("Kick");
	default: return TEXT("None");
	}
}

const TCHAR* UWaxCombatComponent::PhaseName(EWaxAttackPhase InPhase) const
{
	switch (InPhase)
	{
	case EWaxAttackPhase::Windup: return TEXT("Windup");
	case EWaxAttackPhase::Active: return TEXT("Active");
	case EWaxAttackPhase::Recovery: return TEXT("Recovery");
	default: return TEXT("Idle");
	}
}

bool UWaxCombatComponent::IsPermanentlyInvalid(EWaxAttackSlot Slot) const
{
	switch (Slot)
	{
	case EWaxAttackSlot::Kick: return IsFalling();
	case EWaxAttackSlot::AerialStrike:
	case EWaxAttackSlot::Slam: return !IsFalling();
	case EWaxAttackSlot::None: return true;
	default: return false;
	}
}

bool UWaxCombatComponent::CanExecute(EWaxAttackSlot Slot) const
{
	if (IsPermanentlyInvalid(Slot))
	{
		return false;
	}

	const FWaxAttackSpec* Spec = ResolveSpec(Slot);
	if (!Spec || Spec->Montages.Num() == 0 || !GetAnimInstance())
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (Owner && Owner->Implements<UWaxCombatFlowInterface>())
	{
		return IWaxCombatFlowInterface::Execute_CanExecuteAttack(Owner, Slot);
	}
	return true;
}

bool UWaxCombatComponent::IsAllowedNow(EWaxAttackSlot Slot) const
{
	if (Phase == EWaxAttackPhase::Idle)
	{
		return true;
	}
	return bCancelWindowOpen && CancelAllowedSlots.Contains(Slot);
}

bool UWaxCombatComponent::IsHoldFallbackActive() const
{
	return bAttackHeld && bHoldStartedGrounded && !IsFalling();
}

void UWaxCombatComponent::ClearBuffer(const TCHAR* Reason)
{
	if (BufferedSlot == EWaxAttackSlot::None)
	{
		return;
	}
	if (bVerboseLog)
	{
		UE_LOG(LogWaxCombat, Log, TEXT("WaxCombat Buffer drop slot=%s reason=%s age=%.3f"), SlotName(BufferedSlot), Reason, Now() - BufferedTime);
	}
	BufferedSlot = EWaxAttackSlot::None;
}

void UWaxCombatComponent::ExpireBuffer(double InNow)
{
	if (BufferedSlot != EWaxAttackSlot::None && InNow - BufferedTime > GetSettings().BufferWindowSec)
	{
		ClearBuffer(TEXT("expired"));
	}
}

void UWaxCombatComponent::PressAttackInput(EWaxAttackSlot Slot)
{
	if (CaptureIndex != INDEX_NONE || Slot == EWaxAttackSlot::None)
	{
		return;
	}

	if (IsPermanentlyInvalid(Slot))
	{
		if (bVerboseLog)
		{
			UE_LOG(LogWaxCombat, Log, TEXT("WaxCombat Input ignored slot=%s reason=state falling=%d"), SlotName(Slot), IsFalling() ? 1 : 0);
		}
		return;
	}

	if (bVerboseLog)
	{
		UE_LOG(LogWaxCombat, Log, TEXT("WaxCombat Buffer store slot=%s overwrote=%s phase=%s"), SlotName(Slot), SlotName(BufferedSlot), PhaseName(Phase));
	}
	BufferedSlot = Slot;
	BufferedTime = Now();
	TryConsume(EConsumePoint::Press);
}

void UWaxCombatComponent::SetAttackHeld(bool bHeld)
{
	if (bHeld && !bAttackHeld)
	{
		bHoldStartedGrounded = !IsFalling();
	}
	bAttackHeld = bHeld;
	if (!bHeld)
	{
		bHoldStartedGrounded = false;
	}
}

bool UWaxCombatComponent::TryConsume(EConsumePoint Point)
{
	ExpireBuffer(Now());

	if (BufferedSlot != EWaxAttackSlot::None)
	{
		if (IsPermanentlyInvalid(BufferedSlot))
		{
			ClearBuffer(TEXT("invalid_state"));
		}
		else if (IsAllowedNow(BufferedSlot) && CanExecute(BufferedSlot))
		{
			const EWaxAttackSlot Slot = BufferedSlot;
			BufferedSlot = EWaxAttackSlot::None;
			const bool bWasAttacking = Phase != EWaxAttackPhase::Idle || Point == EConsumePoint::AttackEnd;
			if (StartAttack(Slot, bWasAttacking))
			{
				if (bWasAttacking)
				{
					OnBufferConsumed.Broadcast(Slot);
				}
				return true;
			}
			return false;
		}
	}

	if (BufferedSlot == EWaxAttackSlot::None && IsHoldFallbackActive()
		&& IsAllowedNow(EWaxAttackSlot::Basic) && CanExecute(EWaxAttackSlot::Basic))
	{
		if (bVerboseLog)
		{
			UE_LOG(LogWaxCombat, Log, TEXT("WaxCombat Hold fallback slot=Basic phase=%s"), PhaseName(Phase));
		}
		return StartAttack(EWaxAttackSlot::Basic, true);
	}

	return false;
}

bool UWaxCombatComponent::StartAttack(EWaxAttackSlot Slot, bool bFromBuffer)
{
	const FWaxAttackSpec* Spec = ResolveSpec(Slot);
	UAnimInstance* AnimInst = GetAnimInstance();
	if (!Spec || Spec->Montages.Num() == 0 || !AnimInst)
	{
		UE_LOG(LogWaxCombat, Warning, TEXT("WaxCombat Start failed slot=%s reason=no_spec_or_anim"), SlotName(Slot));
		return false;
	}

	const FWaxCombatSettings& Settings = GetSettings();
	if (Slot != LastStartedSlot || (Phase == EWaxAttackPhase::Idle && Now() - LastAttackEndTime > Settings.ComboResetSec))
	{
		ComboIndex = 0;
	}

	const int32 NumMontages = Spec->Montages.Num();
	int32 MontageIndex = ComboIndex % NumMontages;
	AActor* Owner = GetOwner();
	if (Owner && Owner->Implements<UWaxCombatFlowInterface>())
	{
		MontageIndex = IWaxCombatFlowInterface::Execute_SelectAttackMontage(Owner, Slot, *Spec, MontageIndex);
	}
	MontageIndex = FMath::Clamp(MontageIndex, 0, NumMontages - 1);

	UAnimMontage* Montage = Spec->Montages[MontageIndex];
	if (!Montage)
	{
		UE_LOG(LogWaxCombat, Warning, TEXT("WaxCombat Start failed slot=%s reason=null_montage index=%d"), SlotName(Slot), MontageIndex);
		return false;
	}

	const EWaxAttackSlot PrevSlot = CurrentSlot;
	const bool bWasAttacking = Phase != EWaxAttackPhase::Idle;
	if (bWasAttacking)
	{
		// 리스너가 이전 공격 상태를 읽을 수 있도록 상태를 덮어쓰기 전에 알림
		OnAttackEnded.Broadcast(PrevSlot, true);
	}

	++AttackSerial;
	const int32 Serial = AttackSerial;
	CurrentSpec = *Spec;
	CurrentSlot = Slot;
	ActiveMontage = Montage;
	ActiveMontageInstanceId = INDEX_NONE;
	bSawActiveWindow = false;
	bCancelWindowOpen = false;
	CancelAllowedSlots.Reset();
	RotationOverrides.Reset();
	HitActorsThisAttack.Reset();

	const float Length = AnimInst->Montage_Play(Montage, Spec->PlayRate);
	if (Length <= 0.0f)
	{
		UE_LOG(LogWaxCombat, Warning, TEXT("WaxCombat Start failed slot=%s reason=montage_play montage=%s"), SlotName(Slot), *GetNameSafe(Montage));
		SetPhase(EWaxAttackPhase::Idle);
		CurrentSlot = EWaxAttackSlot::None;
		ActiveMontage = nullptr;
		LastAttackEndTime = Now();
		return false;
	}

	if (const FAnimMontageInstance* Instance = AnimInst->GetActiveInstanceForMontage(Montage))
	{
		ActiveMontageInstanceId = Instance->GetInstanceID();
	}

	FOnMontageBlendingOutStarted BlendOutDelegate = FOnMontageBlendingOutStarted::CreateUObject(this, &UWaxCombatComponent::OnMontageBlendingOut, Serial);
	AnimInst->Montage_SetBlendingOutDelegate(BlendOutDelegate, Montage);

	LastStartedSlot = Slot;
	ComboIndex = (MontageIndex + 1 < NumMontages) ? MontageIndex + 1 : FMath::Clamp(Spec->ComboLoopStartIndex, 0, NumMontages - 1);

	// 회전 통계는 연속 공격 묶음(첫 공격 ~ 몸 정렬 완료) 단위로 집계
	if (!bRotSessionActive)
	{
		bRotSessionActive = true;
		RotSessionAttacks = 0;
		RotMaxYawOffset = 0.0f;
		RotMaxPitchOffset = 0.0f;
		RotMinSensitivity = 1.0f;
		RotMinMoveScale = 1.0f;
		RotBoundaryFrames = 0;
	}
	++RotSessionAttacks;
	RotRecoverStartTime = -1.0;

	Phase = EWaxAttackPhase::Idle;
	SetPhase(EWaxAttackPhase::Windup);
	OnAttackStarted.Broadcast(Slot, Montage);

	if (bVerboseLog)
	{
		UE_LOG(LogWaxCombat, Log, TEXT("WaxCombat Start slot=%s montage=%s index=%d rate=%.2f buffered=%d interrupted=%s t=%.3f"),
			SlotName(Slot), *GetNameSafe(Montage), MontageIndex, Spec->PlayRate, bFromBuffer ? 1 : 0, bWasAttacking ? SlotName(PrevSlot) : TEXT("-"), Now());
	}
	return true;
}

void UWaxCombatComponent::EndAttack(bool bInterrupted)
{
	const EWaxAttackSlot EndedSlot = CurrentSlot;
	SetPhase(EWaxAttackPhase::Idle);
	CurrentSlot = EWaxAttackSlot::None;
	ActiveMontage = nullptr;
	ActiveMontageInstanceId = INDEX_NONE;
	bCancelWindowOpen = false;
	CancelAllowedSlots.Reset();
	RotationOverrides.Reset();
	LastAttackEndTime = Now();
	RotRecoverStartTime = Now();

	OnAttackEnded.Broadcast(EndedSlot, bInterrupted);
	if (bVerboseLog)
	{
		UE_LOG(LogWaxCombat, Log, TEXT("WaxCombat End slot=%s interrupted=%d t=%.3f"), SlotName(EndedSlot), bInterrupted ? 1 : 0, Now());
	}
}

void UWaxCombatComponent::SetPhase(EWaxAttackPhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}
	Phase = NewPhase;
	OnAttackPhaseChanged.Broadcast(CurrentSlot, Phase);
}

void UWaxCombatComponent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 Serial)
{
	if (Serial != AttackSerial || Phase == EWaxAttackPhase::Idle)
	{
		return;
	}
	EndAttack(bInterrupted);
	TryConsume(EConsumePoint::AttackEnd);
}

bool UWaxCombatComponent::IsCurrentAttackEvent(const USkeletalMeshComponent* MeshComp, const FAnimNotifyEventReference& EventReference) const
{
	if (!OwnerCharacter || MeshComp != OwnerCharacter->GetMesh() || Phase == EWaxAttackPhase::Idle || ActiveMontageInstanceId == INDEX_NONE)
	{
		return false;
	}
	// 공격 노티파이는 현재 몽타주 인스턴스에서 온 것만 인정 (블렌드아웃 중인 이전 공격·몽타주 외 재생 제외)
	const UE::Anim::FAnimNotifyMontageInstanceContext* Context = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
	return Context && Context->MontageInstanceID == ActiveMontageInstanceId;
}

void UWaxCombatComponent::NotifyActiveWindow(USkeletalMeshComponent* MeshComp, const FAnimNotifyEventReference& EventReference, bool bBegin)
{
	if (!IsCurrentAttackEvent(MeshComp, EventReference))
	{
		return;
	}
	if (bBegin)
	{
		bSawActiveWindow = true;
		SetPhase(EWaxAttackPhase::Active);
	}
	else if (Phase == EWaxAttackPhase::Active)
	{
		SetPhase(EWaxAttackPhase::Recovery);
	}
}

void UWaxCombatComponent::NotifyCancelWindow(USkeletalMeshComponent* MeshComp, const FAnimNotifyEventReference& EventReference, bool bBegin, const TArray<EWaxAttackSlot>& AllowedSlots)
{
	if (!IsCurrentAttackEvent(MeshComp, EventReference))
	{
		return;
	}
	bCancelWindowOpen = bBegin;
	CancelAllowedSlots = bBegin ? AllowedSlots : TArray<EWaxAttackSlot>();
	if (bBegin)
	{
		TryConsume(EConsumePoint::CancelWindow);
	}
}

void UWaxCombatComponent::PushRotationOverride(USkeletalMeshComponent* MeshComp, const FAnimNotifyEventReference& EventReference, const UObject* Key, const FWaxRotationProfile& Profile)
{
	if (IsCurrentAttackEvent(MeshComp, EventReference))
	{
		RotationOverrides.Emplace(Key, Profile);
	}
}

void UWaxCombatComponent::PopRotationOverride(USkeletalMeshComponent* MeshComp, const FAnimNotifyEventReference& EventReference, const UObject* Key)
{
	if (!IsCurrentAttackEvent(MeshComp, EventReference))
	{
		return;
	}
	// 구간이 겹쳐도 자기 항목만 제거
	const int32 Index = RotationOverrides.FindLastByPredicate([Key](const TPair<const UObject*, FWaxRotationProfile>& Entry) { return Entry.Key == Key; });
	if (Index != INDEX_NONE)
	{
		RotationOverrides.RemoveAt(Index);
	}
}

FVector UWaxCombatComponent::GetAttackAimDirection() const
{
	const AWaxPpuLabCharacter* WaxCharacter = Cast<AWaxPpuLabCharacter>(OwnerCharacter);
	const float Yaw = OwnerCharacter ? OwnerCharacter->GetActorRotation().Yaw : 0.0f;
	const float Pitch = WaxCharacter ? WaxCharacter->GetAimPitch() : 0.0f;
	return FRotator(Pitch, Yaw, 0.0f).Vector();
}

FVector UWaxCombatComponent::ComputeHitCenter(EWaxHitSource Source, FName BoneName, float ForwardOffset, bool& bOutValid) const
{
	bOutValid = false;
	USkeletalMeshComponent* BodyMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	if (!BodyMesh || BoneName.IsNone() || !BodyMesh->DoesSocketExist(BoneName))
	{
		return FVector::ZeroVector;
	}
	bOutValid = true;
	return BodyMesh->GetSocketLocation(BoneName) + GetAttackAimDirection() * ForwardOffset;
}

void UWaxCombatComponent::HandleHitNotify(USkeletalMeshComponent* MeshComp, const FAnimNotifyEventReference& EventReference, EWaxHitSource Source, FName BoneName, float StrainScale, float RadiusScale)
{
	if (!IsCurrentAttackEvent(MeshComp, EventReference))
	{
		return;
	}

	if (Source == EWaxHitSource::Weapon && !bWarnedWeaponSource)
	{
		bWarnedWeaponSource = true;
		UE_LOG(LogWaxCombat, Warning, TEXT("WaxCombat Hit source=Weapon is not available yet, using Body."));
	}

	bool bValid = false;
	const FVector Center = ComputeHitCenter(Source, BoneName, CurrentSpec.ForwardOffset, bValid);
	if (!bValid)
	{
		UE_LOG(LogWaxCombat, Warning, TEXT("WaxCombat Hit skipped slot=%s reason=missing_socket bone=%s"), SlotName(CurrentSlot), *BoneName.ToString());
		return;
	}

	const FWaxCombatSettings& Settings = GetSettings();
	const float Strain = Settings.BaseImpactStrain * CurrentSpec.StrainScale * FMath::Max(0.1f, StrainScale);
	const float Radius = Settings.BaseImpactRadius * CurrentSpec.RadiusScale * FMath::Max(0.1f, RadiusScale);

	if (bShowImpactMarker)
	{
		SetMarkerVisible(true, Center, Radius);
		MarkerExpireTime = Now() + GetSettings().MarkerDurationSec;
	}
	if (bShowDebugImpact && GetWorld())
	{
		DrawDebugSphere(GetWorld(), Center, Radius, 16, GetSettings().MarkerColor.ToFColor(true), false, GetSettings().MarkerDurationSec, 0, 1.5f);
	}

	FWaxHitPayload Payload;
	Payload.Origin = OwnerCharacter ? OwnerCharacter->GetActorLocation() : Center;
	Payload.Center = Center;
	Payload.AimDirection = GetAttackAimDirection();
	Payload.Radius = Radius;
	Payload.Strain = Strain;
	Payload.KnockbackHorizontal = CurrentSpec.KnockbackHorizontal;
	Payload.KnockbackVertical = CurrentSpec.KnockbackVertical;
	Payload.KnockbackDurationSec = CurrentSpec.KnockbackDurationSec;

	const int32 NumTargets = DeliverHit(Payload);
	const int32 NumDebris = ApplyDebrisImpulse(Center);

	if (!bSawActiveWindow && Phase == EWaxAttackPhase::Windup)
	{
		SetPhase(EWaxAttackPhase::Recovery);
	}

	OnAttackHit.Broadcast(CurrentSlot, Center, NumTargets, Strain, Radius);
	if (bVerboseLog)
	{
		UE_LOG(LogWaxCombat, Log, TEXT("WaxCombat Hit slot=%s bone=%s center=(%.1f,%.1f,%.1f) radius=%.1f strain=%.0f targets=%d knockH=%.0f knockV=%.0f debris=%d t=%.3f"),
			SlotName(CurrentSlot), *BoneName.ToString(), Center.X, Center.Y, Center.Z, Radius, Strain, NumTargets,
			Payload.KnockbackHorizontal, Payload.KnockbackVertical, NumDebris, Now());
	}
}

int32 UWaxCombatComponent::DeliverHit(const FWaxHitPayload& Payload)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WaxAttackSphere), false, GetOwner());
	if (!World->OverlapMultiByChannel(Overlaps, Payload.Center, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Payload.Radius), Params))
	{
		return 0;
	}

	int32 NumTargets = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || HitActorsThisAttack.Contains(HitActor))
		{
			continue;
		}
		HitActorsThisAttack.Add(HitActor);

		// 액터가 피격 인터페이스를 구현하면 내부 분배를 맡기고, 아니면 피격 가능한 컴포넌트에 직접 전달
		bool bHit = false;
		if (HitActor->Implements<UWaxHittable>())
		{
			bHit = IWaxHittable::Execute_ReceiveWaxHit(HitActor, Payload);
		}
		else
		{
			TInlineComponentArray<UActorComponent*> Components(HitActor);
			for (UActorComponent* Component : Components)
			{
				if (Component && Component->Implements<UWaxHittable>())
				{
					bHit |= IWaxHittable::Execute_ReceiveWaxHit(Component, Payload);
				}
			}
		}
		NumTargets += bHit ? 1 : 0;
	}
	return NumTargets;
}

int32 UWaxCombatComponent::ApplyDebrisImpulse(const FVector& Center) const
{
	if (CurrentSpec.DebrisImpulse <= 0.0f && CurrentSpec.DebrisImpulseUpward <= 0.0f)
	{
		return 0;
	}
	const FVector Velocity = GetAttackAimDirection().GetSafeNormal2D() * CurrentSpec.DebrisImpulse + FVector::UpVector * CurrentSpec.DebrisImpulseUpward;
	return UWaxBreakablePartComponent::KickFragmentsInRadius(GetWorld(), Center, Velocity, CurrentSpec.DebrisImpulseRadius, GetOwner());
}

void UWaxCombatComponent::SetMarkerVisible(bool bVisible, const FVector& Center, float Radius)
{
	bMarkerActive = bVisible;
	UWorld* World = GetWorld();
	if (!TargetingMPC || !World)
	{
		return;
	}

	if (UMaterialParameterCollectionInstance* MPCInst = World->GetParameterCollectionInstance(TargetingMPC))
	{
		if (bVisible)
		{
			MPCInst->SetVectorParameterValue(TEXT("TargetSphereCenter"), FLinearColor(Center.X, Center.Y, Center.Z, 1.0f));
			MPCInst->SetScalarParameterValue(TEXT("TargetSphereRadius"), Radius);
			MPCInst->SetVectorParameterValue(TEXT("MarkerColor"), GetSettings().MarkerColor);
			MPCInst->SetScalarParameterValue(TEXT("EmissiveBoost"), GetSettings().MarkerEmissiveBoost);
			MPCInst->SetScalarParameterValue(TEXT("MarkerOpacity"), GetSettings().MarkerOpacity);
			MPCInst->SetScalarParameterValue(TEXT("EdgeSoftness"), GetSettings().MarkerEdgeSoftness);
		}
		else
		{
			MPCInst->SetScalarParameterValue(TEXT("MarkerOpacity"), 0.0f);
		}
	}
}

FWaxRotationProfile UWaxCombatComponent::GetActiveRotationProfile() const
{
	if (Phase == EWaxAttackPhase::Idle)
	{
		return FWaxRotationProfile();
	}

	const FWaxRotationProfile DefaultProfile = RotationOverrides.Num() > 0
		? RotationOverrides.Last().Value
		: CurrentSpec.GetPhaseRotation(Phase);

	AActor* Owner = GetOwner();
	if (Owner && Owner->Implements<UWaxCombatFlowInterface>())
	{
		return IWaxCombatFlowInterface::Execute_ResolveRotationProfile(Owner, CurrentSlot, Phase, DefaultProfile);
	}
	return DefaultProfile;
}

float UWaxCombatComponent::GetPitchRateLimit() const
{
	return IsAttacking() ? GetActiveRotationProfile().MaxPitchRateDegPerSec : 0.0f;
}

bool UWaxCombatComponent::ShouldSuppressHeadFollow() const
{
	return IsAttacking() && CurrentSpec.bSuppressHeadFollow;
}

float UWaxCombatComponent::FilterLookDelta(float Delta, float Offset, float MaxOffset) const
{
	float Scaled = Delta * CurrentSensitivityScale;
	if (MaxOffset <= 0.0f || Delta * Offset < 0.0f)
	{
		// 제한 없음 또는 몸 쪽으로 되돌아가는 방향: 감도 배율만 적용
		return Scaled;
	}

	const float AbsOffset = FMath::Abs(Offset);
	if (AbsOffset >= MaxOffset)
	{
		return 0.0f;
	}

	const float T = AbsOffset / MaxOffset;
	const UCurveFloat* Curve = GetSettings().OffsetFalloffCurve;
	const float Falloff = Curve ? Curve->GetFloatValue(T) : (1.0f - T);
	Scaled *= FMath::Clamp(Falloff, 0.0f, 1.0f);

	// 한 프레임 큰 입력이 허용치를 넘지 않도록 상한
	const float NewOffset = Offset + Scaled;
	if (FMath::Abs(NewOffset) > MaxOffset)
	{
		Scaled = FMath::Sign(NewOffset) * MaxOffset - Offset;
	}
	return Scaled;
}

void UWaxCombatComponent::UpdateRotation(float DeltaTime)
{
	if (!OwnerCharacter)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	if (!PC)
	{
		return;
	}

	const FWaxCombatSettings& Settings = GetSettings();
	const bool bAttacking = IsAttacking();
	const FWaxRotationProfile Profile = GetActiveRotationProfile();
	const FRotator Control = PC->GetControlRotation();
	const float BodyYaw = OwnerCharacter->GetActorRotation().Yaw;

	// 카메라 감도 (소프트 클램프)
	const float TargetScale = bAttacking ? Profile.CameraSensitivityScale : 1.0f;
	CurrentSensitivityScale = (Settings.SensitivityBlendSpeed > KINDA_SMALL_NUMBER)
		? FMath::FInterpTo(CurrentSensitivityScale, TargetScale, DeltaTime, Settings.SensitivityBlendSpeed)
		: TargetScale;
	if (FMath::IsNearlyEqual(CurrentSensitivityScale, TargetScale, 0.001f))
	{
		CurrentSensitivityScale = TargetScale;
	}

	const bool bNeutral = CurrentSensitivityScale >= 0.999f
		&& (!bAttacking || (Profile.MaxBodyCameraYawOffsetDeg <= 0.0f && Profile.MaxBodyCameraPitchOffsetDeg <= 0.0f));
	if (bAttacking)
	{
		RotMinSensitivity = FMath::Min(RotMinSensitivity, CurrentSensitivityScale);
	}

	// 먼저 카메라 입력을 걸러낸 뒤, 몸은 걸러진 시선을 따라감 (몸이 카메라를 앞지르지 않도록)
	FRotator NewControl = Control;
	if (!bNeutral && bHasLastControlRotation)
	{
		const AWaxPpuLabCharacter* WaxCharacter = Cast<AWaxPpuLabCharacter>(OwnerCharacter);
		const float MaxUpperPitch = WaxCharacter ? WaxCharacter->MaxUpperBodyPitchDeg : 45.0f;
		const float AimPitch = WaxCharacter ? WaxCharacter->GetAimPitch() : 0.0f;

		const float DeltaYaw = FRotator::NormalizeAxis(Control.Yaw - LastControlRotation.Yaw);
		const float DeltaPitch = FRotator::NormalizeAxis(Control.Pitch - LastControlRotation.Pitch);
		const float YawOffset = FRotator::NormalizeAxis(LastControlRotation.Yaw - BodyYaw);
		const float PitchOffset = FMath::Clamp(FRotator::NormalizeAxis(LastControlRotation.Pitch), -MaxUpperPitch, MaxUpperPitch) - AimPitch;

		const float MaxYawOffset = bAttacking ? Profile.MaxBodyCameraYawOffsetDeg : 0.0f;
		const float MaxPitchOffset = bAttacking ? Profile.MaxBodyCameraPitchOffsetDeg : 0.0f;

		const float AppliedYaw = FilterLookDelta(DeltaYaw, YawOffset, MaxYawOffset);
		const float AppliedPitch = FilterLookDelta(DeltaPitch, PitchOffset, MaxPitchOffset);
		NewControl = LastControlRotation;
		NewControl.Yaw += AppliedYaw;
		NewControl.Pitch += AppliedPitch;
		NewControl.Roll = Control.Roll;
		NewControl.Normalize();
		PC->SetControlRotation(NewControl);

		if (bRotSessionActive)
		{
			RotMaxPitchOffset = FMath::Max(RotMaxPitchOffset, FMath::Abs(PitchOffset + AppliedPitch));
			// 바깥으로 미는 입력이 경계에서 완전히 막힌 프레임
			const bool bYawBlocked = MaxYawOffset > 0.0f && FMath::Abs(DeltaYaw) > KINDA_SMALL_NUMBER && FMath::Abs(AppliedYaw) <= KINDA_SMALL_NUMBER;
			const bool bPitchBlocked = MaxPitchOffset > 0.0f && FMath::Abs(DeltaPitch) > KINDA_SMALL_NUMBER && FMath::Abs(AppliedPitch) <= KINDA_SMALL_NUMBER;
			if (bYawBlocked || bPitchBlocked)
			{
				++RotBoundaryFrames;
			}
		}
	}
	LastControlRotation = NewControl;
	bHasLastControlRotation = true;

	// 몸 Yaw
	float NewBodyYaw = BodyYaw;
	if (bAttacking && Profile.LimitsYaw())
	{
		if (!bBodyYawDetached)
		{
			OwnerCharacter->bUseControllerRotationYaw = false;
			bBodyYawDetached = true;
		}
		NewBodyYaw = FMath::FixedTurn(BodyYaw, NewControl.Yaw, Profile.MaxYawRateDegPerSec * DeltaTime);
		OwnerCharacter->SetActorRotation(FRotator(0.0f, NewBodyYaw, 0.0f));
	}
	else if (bBodyYawDetached)
	{
		NewBodyYaw = FMath::FixedTurn(BodyYaw, NewControl.Yaw, Settings.RecoverYawRateDegPerSec * DeltaTime);
		OwnerCharacter->SetActorRotation(FRotator(0.0f, NewBodyYaw, 0.0f));
		if (FMath::Abs(FRotator::NormalizeAxis(NewControl.Yaw - NewBodyYaw)) <= Settings.RecoverSnapToleranceDeg)
		{
			OwnerCharacter->bUseControllerRotationYaw = true;
			bBodyYawDetached = false;
		}
	}

	if (bRotSessionActive)
	{
		if (bBodyYawDetached)
		{
			RotMaxYawOffset = FMath::Max(RotMaxYawOffset, FMath::Abs(FRotator::NormalizeAxis(NewControl.Yaw - NewBodyYaw)));
		}
		if (!bAttacking && !bBodyYawDetached && bNeutral)
		{
			FinishRotationSession();
		}
	}
}

bool UWaxCombatComponent::IsJumpAllowed() const
{
	return !IsAttacking() || CurrentSpec.GetPhaseMovement(Phase).bAllowJump;
}

void UWaxCombatComponent::UpdateMovementLimit(float DeltaTime)
{
	UCharacterMovementComponent* Movement = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	if (!Movement || BaseMaxWalkSpeed <= 0.0f)
	{
		return;
	}

	float TargetScale = 1.0f;
	if (IsAttacking())
	{
		// 이동 방향은 몸 정면 기준 (공격 중 몸 Yaw가 시선보다 늦을 수 있음)
		const FVector LocalInput = OwnerCharacter->GetActorRotation().UnrotateVector(Movement->GetLastInputVector());
		const FVector2D Dir = FVector2D(LocalInput.X, LocalInput.Y).GetSafeNormal();
		TargetScale = Dir.IsNearlyZero() ? CurrentMoveScale : CurrentSpec.GetPhaseMovement(Phase).ResolveSpeedScale(Dir.X, Dir.Y);
	}

	const float BlendSpeed = GetSettings().MoveScaleBlendSpeed;
	CurrentMoveScale = BlendSpeed > KINDA_SMALL_NUMBER ? FMath::FInterpTo(CurrentMoveScale, TargetScale, DeltaTime, BlendSpeed) : TargetScale;
	if (FMath::IsNearlyEqual(CurrentMoveScale, TargetScale, 0.001f))
	{
		CurrentMoveScale = TargetScale;
	}
	if (bRotSessionActive)
	{
		RotMinMoveScale = FMath::Min(RotMinMoveScale, CurrentMoveScale);
	}
	Movement->MaxWalkSpeed = BaseMaxWalkSpeed * CurrentMoveScale;
}

void UWaxCombatComponent::FinishRotationSession()
{
	bRotSessionActive = false;
	const float RecoverSec = RotRecoverStartTime >= 0.0 ? static_cast<float>(Now() - RotRecoverStartTime) : 0.0f;
	const bool bStuck = RecoverSec > RotationStuckWarnSec;
	if (bVerboseLog || bStuck)
	{
		UE_LOG(LogWaxCombat, Log, TEXT("WaxRot Session attacks=%d maxYawOff=%.1f maxPitchOff=%.1f minSens=%.2f minMove=%.2f boundaryFrames=%d recover=%.2fs stuck=%d"),
			RotSessionAttacks, RotMaxYawOffset, RotMaxPitchOffset, RotMinSensitivity, RotMinMoveScale, RotBoundaryFrames, RecoverSec, bStuck ? 1 : 0);
	}
}

void UWaxCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CaptureIndex != INDEX_NONE)
	{
		TickCapture();
		return;
	}

	// 소비는 Press/CancelWindow/AttackEnd에서만. Tick은 만료·상태 무효화만 처리
	ExpireBuffer(Now());
	if (BufferedSlot != EWaxAttackSlot::None && IsPermanentlyInvalid(BufferedSlot))
	{
		ClearBuffer(TEXT("invalid_state"));
	}
	if (bHoldStartedGrounded && IsFalling())
	{
		// 홀드 중 점프·낙하하면 착지 후 자동 연타로 이어지지 않음 (새로 눌러야 함)
		bHoldStartedGrounded = false;
	}
	UpdateRotation(DeltaTime);
	UpdateMovementLimit(DeltaTime);

	if (bMarkerActive && (!bShowImpactMarker || Now() >= MarkerExpireTime))
	{
		SetMarkerVisible(false);
	}
}

void UWaxCombatComponent::CollectHitFrames(UAnimMontage* Montage, TArray<TPair<float, FName>>& OutHits) const
{
	if (!Montage)
	{
		return;
	}

	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (const UWaxAttackHitNotify* Hit = Cast<UWaxAttackHitNotify>(Event.Notify))
		{
			OutHits.Emplace(Event.GetTriggerTime(), Hit->ImpactBoneName);
		}
	}

	for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
		{
			const UAnimSequenceBase* Sequence = Segment.GetAnimReference();
			if (!Sequence)
			{
				continue;
			}
			for (const FAnimNotifyEvent& Event : Sequence->Notifies)
			{
				const UWaxAttackHitNotify* Hit = Cast<UWaxAttackHitNotify>(Event.Notify);
				const float T = Event.GetTriggerTime();
				if (!Hit || T < Segment.AnimStartTime || T > Segment.AnimEndTime)
				{
					continue;
				}
				const float Rate = FMath::Max(FMath::Abs(Segment.AnimPlayRate), KINDA_SMALL_NUMBER);
				OutHits.Emplace(Segment.StartPos + (T - Segment.AnimStartTime) / Rate, Hit->ImpactBoneName);
			}
		}
	}
}

bool UWaxCombatComponent::StartCapture(EWaxAttackSlot Slot, bool bQuitWhenDone)
{
	const FWaxAttackSpec* Spec = ResolveSpec(Slot);
	AWaxPpuLabCharacter* WaxCharacter = Cast<AWaxPpuLabCharacter>(OwnerCharacter);
	if (!Spec || Spec->Montages.Num() == 0 || !GetAnimInstance() || !WaxCharacter)
	{
		UE_LOG(LogWaxCombat, Warning, TEXT("WAX_CAPTURE_FAILED slot=%s reason=no_spec"), SlotName(Slot));
		return false;
	}

	CaptureShots.Reset();
	CaptureSpec = *Spec;
	static const float HitOffsets[] = { -0.15f, -0.075f, 0.0f, 0.075f, 0.15f };
	for (int32 MontageIdx = 0; MontageIdx < Spec->Montages.Num(); ++MontageIdx)
	{
		UAnimMontage* Montage = Spec->Montages[MontageIdx];
		if (!Montage)
		{
			continue;
		}
		const float Length = Montage->GetPlayLength();
		TArray<TPair<float, FName>> Hits;
		CollectHitFrames(Montage, Hits);

		auto AddShot = [&](float Position, FName Bone, bool bHit, const TCHAR* Tag)
		{
			FCaptureShot Shot;
			Shot.Montage = Montage;
			Shot.Position = FMath::Clamp(Position, 0.0f, FMath::Max(0.0f, Length - 0.001f));
			Shot.HitBone = Bone;
			Shot.bIsHitFrame = bHit;
			Shot.Label = FString::Printf(TEXT("%s_%02d_t%.3f_%s"), *Montage->GetName(), CaptureShots.Num(), Shot.Position, Tag);
			CaptureShots.Add(Shot);
		};

		AddShot(0.0f, NAME_None, false, TEXT("start"));
		for (const TPair<float, FName>& Hit : Hits)
		{
			for (const float Offset : HitOffsets)
			{
				const bool bIsHit = FMath::IsNearlyZero(Offset);
				AddShot(Hit.Key + Offset, Hit.Value, bIsHit, bIsHit ? TEXT("HIT") : TEXT("near"));
			}
		}
		AddShot(Length, NAME_None, false, TEXT("end"));
	}

	CaptureDir = FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / TEXT("WaxCapture") / FString(SlotName(Slot)) / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	bCaptureQuitWhenDone = bQuitWhenDone;
	bCaptureRestoreThirdPerson = !WaxCharacter->IsThirdPersonPreview();
	WaxCharacter->SetThirdPersonPreview(true);
	bCaptureRestoreControllerYaw = WaxCharacter->bUseControllerRotationYaw;
	WaxCharacter->bUseControllerRotationYaw = false;
	CaptureBodyYaw = WaxCharacter->GetActorRotation().Yaw;
	if (APlayerController* PC = Cast<APlayerController>(WaxCharacter->GetController()))
	{
		PC->SetIgnoreLookInput(true);
		PC->SetIgnoreMoveInput(true);
	}
	ApplyCaptureView();
	CapturePlayingMontage = nullptr;
	CaptureIndex = 0;
	CaptureStage = 0;
	CaptureFrameWait = 45;

	UE_LOG(LogWaxCombat, Display, TEXT("WAX_CAPTURE_BEGIN slot=%s shots=%d dir=%s"), SlotName(Slot), CaptureShots.Num(), *CaptureDir);
	return true;
}

void UWaxCombatComponent::ApplyCaptureView()
{
	if (!OwnerCharacter)
	{
		return;
	}
	OwnerCharacter->SetActorRotation(FRotator(0.0f, CaptureBodyYaw, 0.0f));
	if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
	{
		PC->SetControlRotation(FRotator(CaptureViewPitchDeg, CaptureBodyYaw + CaptureViewYawOffsetDeg, 0.0f));
	}
}

void UWaxCombatComponent::TickCapture()
{
	ApplyCaptureView();
	if (CaptureFrameWait > 0)
	{
		--CaptureFrameWait;
		return;
	}

	UAnimInstance* AnimInst = GetAnimInstance();
	if (!AnimInst || !CaptureShots.IsValidIndex(CaptureIndex))
	{
		FinishCapture();
		return;
	}

	const FCaptureShot& Shot = CaptureShots[CaptureIndex];
	switch (CaptureStage)
	{
	case 0:
		if (CapturePlayingMontage != Shot.Montage)
		{
			AnimInst->Montage_Play(Shot.Montage, 1.0f);
			CapturePlayingMontage = Shot.Montage;
		}
		AnimInst->Montage_Pause(Shot.Montage);
		AnimInst->Montage_SetPosition(Shot.Montage, Shot.Position);
		CaptureStage = 1;
		CaptureFrameWait = 8;
		break;

	case 1:
		if (Shot.bIsHitFrame && GetWorld())
		{
			bool bValid = false;
			const FVector Center = ComputeHitCenter(EWaxHitSource::Body, Shot.HitBone, CaptureSpec.ForwardOffset, bValid);
			if (bValid)
			{
				const float Radius = GetSettings().BaseImpactRadius * CaptureSpec.RadiusScale;
				DrawDebugSphere(GetWorld(), Center, Radius, 20, FColor::Red, false, 0.3f, 0, 1.5f);
			}
		}
		FScreenshotRequest::RequestScreenshot(CaptureDir / (Shot.Label + TEXT(".png")), false, false);
		CaptureStage = 2;
		CaptureFrameWait = 4;
		break;

	default:
		UE_LOG(LogWaxCombat, Display, TEXT("WAX_CAPTURE_SHOT %s"), *(CaptureDir / (Shot.Label + TEXT(".png"))));
		++CaptureIndex;
		CaptureStage = 0;
		if (!CaptureShots.IsValidIndex(CaptureIndex))
		{
			FinishCapture();
		}
		break;
	}
}

void UWaxCombatComponent::FinishCapture()
{
	if (UAnimInstance* AnimInst = GetAnimInstance())
	{
		AnimInst->Montage_Stop(0.1f);
	}
	if (AWaxPpuLabCharacter* WaxCharacter = Cast<AWaxPpuLabCharacter>(OwnerCharacter))
	{
		if (bCaptureRestoreThirdPerson)
		{
			WaxCharacter->SetThirdPersonPreview(false);
		}
		WaxCharacter->bUseControllerRotationYaw = bCaptureRestoreControllerYaw;
		if (APlayerController* PC = Cast<APlayerController>(WaxCharacter->GetController()))
		{
			PC->SetControlRotation(FRotator(0.0f, CaptureBodyYaw, 0.0f));
			PC->SetIgnoreLookInput(false);
			PC->SetIgnoreMoveInput(false);
		}
	}

	bHasLastControlRotation = false;
	UE_LOG(LogWaxCombat, Display, TEXT("WAX_CAPTURE_DONE shots=%d dir=%s"), CaptureShots.Num(), *CaptureDir);
	CaptureIndex = INDEX_NONE;
	CaptureShots.Reset();
	CapturePlayingMontage = nullptr;

	if (bCaptureQuitWhenDone && GEngine)
	{
		GEngine->DeferredCommands.Add(TEXT("quit"));
	}
}
