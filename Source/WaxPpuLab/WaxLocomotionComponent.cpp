#include "WaxLocomotionComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "WaxMonsterBase.h"
#include "WaxMonsterDataAsset.h"

UWaxLocomotionComponent::UWaxLocomotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UWaxLocomotionComponent::InitializeLocomotion(AWaxMonsterBase* InMonster, USceneComponent* InBodyPivot, UWaxMonsterDataAsset* InData)
{
	OwnerMonster = InMonster;
	BodyPivot = InBodyPivot;
	MonsterData = InData;
	BaseMoveSpeed = MonsterData ? MonsterData->MoveSpeed : 160.0f;
	SpeedMultiplier = 1.0f;
	DesiredMoveDirection = FVector::ZeroVector;
	SuspendRemaining = 0.0f;
	bStopped = false;

	if (OwnerMonster)
	{
		if (UCharacterMovementComponent* MoveComp = OwnerMonster->GetCharacterMovement())
		{
			AddTickPrerequisiteComponent(MoveComp);
			MoveComp->bOrientRotationToMovement = false;
		}
	}

	ApplyEffectiveMoveSpeed();
	SetComponentTickEnabled(true);
}

void UWaxLocomotionComponent::SetDesiredMoveDirection(const FVector& InWorldDirection)
{
	DesiredMoveDirection = InWorldDirection.GetSafeNormal2D();
}

void UWaxLocomotionComponent::SetSpeedMultiplier(float InMultiplier)
{
	SpeedMultiplier = FMath::Max(0.0f, InMultiplier);
	ApplyEffectiveMoveSpeed();
}

void UWaxLocomotionComponent::ApplyEffectiveMoveSpeed()
{
	if (!OwnerMonster)
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = OwnerMonster->GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = BaseMoveSpeed * SpeedMultiplier;
	}
}

void UWaxLocomotionComponent::SuspendFor(float DurationSec)
{
	if (DurationSec > 0.0f)
	{
		SuspendRemaining = FMath::Max(SuspendRemaining, DurationSec);
	}
}

void UWaxLocomotionComponent::StopLocomotion()
{
	bStopped = true;
	DesiredMoveDirection = FVector::ZeroVector;
	SetComponentTickEnabled(false);
}

void UWaxLocomotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bStopped || !OwnerMonster || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (SuspendRemaining > 0.0f)
	{
		SuspendRemaining -= DeltaTime;
		if (SuspendRemaining > 0.0f)
		{
			return;
		}
		SuspendRemaining = 0.0f;
		OnSuspendEnded();
	}

	if (ShouldApplyDefaultMovement() && !DesiredMoveDirection.IsNearlyZero())
	{
		OwnerMonster->AddMovementInput(DesiredMoveDirection, 1.0f);
	}
}

void UWaxRollingLocomotionComponent::InitializeLocomotion(AWaxMonsterBase* InMonster, USceneComponent* InBodyPivot, UWaxMonsterDataAsset* InData)
{
	Super::InitializeLocomotion(InMonster, InBodyPivot, InData);

	RollRadius = (MonsterData && MonsterData->RollRadius > 1.0f) ? MonsterData->RollRadius : 92.5f;
	CurrentAngularVelocity = FVector::ZeroVector;
	GroundDrop = 0.0f;
	LandingBlendRemaining = 0.0f;
	bWasFalling = false;

	if (OwnerMonster)
	{
		PrevActorLocation = OwnerMonster->GetActorLocation();
		bHasPrevLocation = true;
	}
	else
	{
		bHasPrevLocation = false;
	}
}

void UWaxRollingLocomotionComponent::StopLocomotion()
{
	Super::StopLocomotion();
	CurrentAngularVelocity = FVector::ZeroVector;
}

void UWaxRollingLocomotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bStopped || !OwnerMonster || !BodyPivot || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector CurrentLocation = OwnerMonster->GetActorLocation();
	if (!bHasPrevLocation)
	{
		PrevActorLocation = CurrentLocation;
		bHasPrevLocation = true;
		return;
	}

	const FVector DeltaPos = CurrentLocation - PrevActorLocation;
	PrevActorLocation = CurrentLocation;

	UCharacterMovementComponent* MoveComp = OwnerMonster->GetCharacterMovement();
	const bool bIsFalling = MoveComp && MoveComp->IsFalling();

	if (bIsFalling)
	{
		bWasFalling = true;
	}
	else
	{
		if (bWasFalling)
		{
			bWasFalling = false;
			LandingBlendRemaining = (MonsterData ? MonsterData : GetDefault<UWaxMonsterDataAsset>())->RollingLandingBlendSec;
		}

		FVector FloorNormal = FVector::UpVector;
		if (MoveComp && MoveComp->CurrentFloor.IsWalkableFloor())
		{
			FloorNormal = MoveComp->CurrentFloor.HitResult.ImpactNormal.GetSafeNormal();
			if (FloorNormal.IsNearlyZero())
			{
				FloorNormal = FVector::UpVector;
			}
		}

		const FVector PlanarDelta = FVector::VectorPlaneProject(DeltaPos, FloorNormal);
		const float PlanarDist = PlanarDelta.Size();
		FVector TargetAngVel = FVector::ZeroVector;
		if (PlanarDist > KINDA_SMALL_NUMBER)
		{
			const FVector RollAxis = FVector::CrossProduct(FloorNormal, PlanarDelta / PlanarDist).GetSafeNormal();
			const float AngSpeed = PlanarDist / (RollRadius * DeltaTime);
			TargetAngVel = RollAxis * AngSpeed;
		}

		if (LandingBlendRemaining > 0.0f)
		{
			LandingBlendRemaining = FMath::Max(0.0f, LandingBlendRemaining - DeltaTime);
			CurrentAngularVelocity = FMath::VInterpTo(CurrentAngularVelocity, TargetAngVel, DeltaTime, (MonsterData ? MonsterData : GetDefault<UWaxMonsterDataAsset>())->RollingLandingInterpSpeed);
		}
		else
		{
			CurrentAngularVelocity = TargetAngVel;
		}
	}

	const float CurrentAngSpeed = CurrentAngularVelocity.Size();
	if (CurrentAngSpeed > KINDA_SMALL_NUMBER)
	{
		const FVector Axis = CurrentAngularVelocity / CurrentAngSpeed;
		const FQuat DeltaQuat(Axis, CurrentAngSpeed * DeltaTime);
		const FQuat NewWorldQuat = (DeltaQuat * BodyPivot->GetComponentQuat()).GetNormalized();
		BodyPivot->SetWorldRotation(NewWorldQuat);
	}

	UpdateGroundContact(DeltaTime, bIsFalling);
}

void UWaxRollingLocomotionComponent::UpdateGroundContact(float DeltaTime, bool bIsFalling)
{
	// 깨진 껍질의 남은 부분(과 코어) 최저점이 바닥에 닿도록 보이는 몸만 내림. 캡슐·이동은 그대로라 깨진 모양대로 구르며 들썩임
	if (!MonsterData || !MonsterData->bKeepBrokenBodyOnGround)
	{
		return;
	}

	float TargetDrop = GroundDrop;
	if (!bIsFalling)
	{
		TargetDrop = 0.0f;
		float LowestZ = 0.0f;
		const UCapsuleComponent* Capsule = OwnerMonster->GetCapsuleComponent();
		if (Capsule && OwnerMonster->GetAttachedBodyLowestZ(LowestZ))
		{
			const float ActorZ = OwnerMonster->GetActorLocation().Z;
			const float FloorZ = ActorZ - Capsule->GetScaledCapsuleHalfHeight();
			const float LowestRelToPivot = LowestZ - BodyPivot->GetComponentLocation().Z;
			TargetDrop = FMath::Clamp(FloorZ - LowestRelToPivot - ActorZ, -MonsterData->GroundContactMaxDrop, 0.0f);
		}
	}

	GroundDrop = MonsterData->GroundContactInterpSpeed > 0.0f ? FMath::FInterpTo(GroundDrop, TargetDrop, DeltaTime, MonsterData->GroundContactInterpSpeed) : TargetDrop;
	BodyPivot->SetRelativeLocation(FVector(0.0f, 0.0f, GroundDrop));
}

void UWaxButterLocomotionComponent::InitializeLocomotion(AWaxMonsterBase* InMonster, USceneComponent* InBodyPivot, UWaxMonsterDataAsset* InData)
{
	Super::InitializeLocomotion(InMonster, InBodyPivot, InData);

	if (MonsterData)
	{
		const FWaxButterLocomotionSettings& S = MonsterData->ButterLocomotion;
		HalfX = FMath::Max(10.0f, S.HalfX);
		HalfY = FMath::Max(10.0f, S.HalfY);
		HalfZ = FMath::Max(10.0f, S.HalfZ);
		MinChargeStartDistance = FMath::Max(50.0f, S.MinChargeStartDistance);
		HopDuration = FMath::Max(0.05f, S.HopDuration);
		HopPauseDuration = FMath::Max(0.0f, S.HopPauseDuration);
		HopHeight = FMath::Max(5.0f, S.HopHeight);
		MaxYawPerHopDeg = FMath::Clamp(S.MaxYawPerHopDeg, 5.0f, 180.0f);
		AlignToleranceDeg = FMath::Clamp(S.AlignToleranceDeg, 1.0f, 45.0f);
		LayDownDuration = FMath::Max(0.05f, S.LayDownDuration);
		StandUpDuration = FMath::Max(0.05f, S.StandUpDuration);
		EffectiveGravity = FMath::Max(100.0f, S.EffectiveGravity);
		DriveAngularAccel = FMath::Max(1.0f, S.DriveAngularAccel);
		InitialStepOmega = FMath::Max(0.5f, S.InitialStepOmega);
		StepBounceRetention = FMath::Clamp(S.StepBounceRetention, 0.1f, 1.0f);
		MaxTumbleSteps = FMath::Max(1, S.MaxTumbleSteps);
		MaxCorrectionYawPerStepDeg = FMath::Clamp(S.MaxCorrectionYawPerStepDeg, 0.0f, 45.0f);
		SteerYawRateDeg = FMath::Clamp(S.SteerYawRateDeg, 10.0f, 720.0f);
		AbortAngleDeg = FMath::Clamp(S.AbortAngleDeg, 15.0f, 180.0f);
		TargetTrackingRatio = FMath::Clamp(S.TargetTrackingRatio, 0.0f, 1.0f);
	}

	LocoState = EWaxButterLocoState::HopAlign;
	StateElapsed = 0.0f;
	CurrentLayAlpha = 0.0f;
	bInHopAir = false;
	LaySign = 1.0f;
	PrevLayLateral = 0.0f;
	TumbleStepIndex = 0;
	TumbleTheta = 0.0f;
	TumbleOmega = 0.0f;
	PrevStepForward = 0.0f;

	SetCapsuleHalfHeightKeepFloor(HalfZ);
	if (BodyPivot)
	{
		BodyPivot->SetRelativeLocation(FVector::ZeroVector);
		BodyPivot->SetRelativeRotation(FQuat::Identity);
		BodyPivot->SetRelativeScale3D(FVector::OneVector);
	}
}

void UWaxButterLocomotionComponent::StopLocomotion()
{
	Super::StopLocomotion();
	TumbleOmega = 0.0f;
}

void UWaxButterLocomotionComponent::SetCapsuleHalfHeightKeepFloor(float NewHalfHeight)
{
	if (!OwnerMonster)
	{
		return;
	}
	if (UCapsuleComponent* Capsule = OwnerMonster->GetCapsuleComponent())
	{
		const float OldHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		if (!FMath::IsNearlyEqual(OldHalfHeight, NewHalfHeight, 0.25f))
		{
			const float DeltaZ = NewHalfHeight - OldHalfHeight;
			Capsule->SetCapsuleHalfHeight(NewHalfHeight, true);
			OwnerMonster->AddActorWorldOffset(FVector(0.0f, 0.0f, DeltaZ), false);
			if (BodyPivot)
			{
				BodyPivot->AddRelativeLocation(FVector(0.0f, 0.0f, -DeltaZ));
			}
		}
	}
}

bool UWaxButterLocomotionComponent::GetTumbleTarget(FVector& OutDir2D, float& OutDist2D) const
{
	FVector DirToPlayer;
	float DistToPlayer = 0.0f;
	const bool bHasPlayer = GetDirectionToPlayer(DirToPlayer, DistToPlayer);
	if (!bHasLockedTarget || !OwnerMonster)
	{
		OutDir2D = DirToPlayer;
		OutDist2D = DistToPlayer;
		return bHasPlayer;
	}

	const FVector Here = OwnerMonster->GetActorLocation();
	const FVector Target = bHasPlayer
		? FMath::Lerp(LockedTargetLocation, Here + DirToPlayer * DistToPlayer, TargetTrackingRatio)
		: LockedTargetLocation;
	FVector ToTarget = Target - Here;
	ToTarget.Z = 0.0f;
	OutDist2D = ToTarget.Size();
	OutDir2D = OutDist2D > KINDA_SMALL_NUMBER ? ToTarget / OutDist2D : OwnerMonster->GetActorForwardVector().GetSafeNormal2D();
	return true;
}

bool UWaxButterLocomotionComponent::GetDirectionToPlayer(FVector& OutDir2D, float& OutDist2D) const
{
	if (!OwnerMonster)
	{
		return false;
	}
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(OwnerMonster, 0);
	if (!PlayerPawn)
	{
		return false;
	}

	FVector ToPlayer = PlayerPawn->GetActorLocation() - OwnerMonster->GetActorLocation();
	ToPlayer.Z = 0.0f;
	OutDist2D = ToPlayer.Size();
	if (OutDist2D <= KINDA_SMALL_NUMBER)
	{
		OutDir2D = OwnerMonster->GetActorForwardVector().GetSafeNormal2D();
		return true;
	}
	OutDir2D = ToPlayer / OutDist2D;
	return true;
}

FQuat UWaxButterLocomotionComponent::ComputeLaidRelQuat(float LayAlpha, float TumbleAngleRad) const
{
	const FQuat LayQuat(FVector::ForwardVector, LaySign * (PI * 0.5f) * LayAlpha);
	const FQuat TumbleQuat(FVector::RightVector, TumbleAngleRad);
	return (TumbleQuat * LayQuat).GetNormalized();
}

void UWaxButterLocomotionComponent::ChooseLayDirection()
{
	if (!OwnerMonster || !GetWorld())
	{
		LaySign = FMath::RandBool() ? 1.0f : -1.0f;
		return;
	}

	const FVector Origin = OwnerMonster->GetActorLocation();
	const FVector Right = OwnerMonster->GetActorRightVector().GetSafeNormal2D();
	const float SweepDist = HalfY + HalfZ;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WaxButterLaySweep), false, OwnerMonster);
	const FCollisionShape Shape = FCollisionShape::MakeSphere(FMath::Min(HalfX, HalfY) * 0.8f);

	// 파괴 조각(PhysicsBody)은 몬스터 이동을 막지 않으므로 눕는 방향 판단에서도 무시. 다른 몬스터는 캡슐(Pawn)로 감지됨
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Ignore);

	FHitResult HitLeft, HitRight;
	// LaySign = +1 falls toward -Right (left), LaySign = -1 falls toward +Right (right)
	const bool bLeftBlocked = GetWorld()->SweepSingleByChannel(HitLeft, Origin, Origin - Right * SweepDist, FQuat::Identity, ECC_WorldDynamic, Shape, Params, ResponseParams);
	const bool bRightBlocked = GetWorld()->SweepSingleByChannel(HitRight, Origin, Origin + Right * SweepDist, FQuat::Identity, ECC_WorldDynamic, Shape, Params, ResponseParams);

	if (bLeftBlocked && !bRightBlocked)
	{
		LaySign = -1.0f;
	}
	else if (bRightBlocked && !bLeftBlocked)
	{
		LaySign = 1.0f;
	}
	else
	{
		LaySign = FMath::RandBool() ? 1.0f : -1.0f;
	}
}

void UWaxButterLocomotionComponent::EnterState(EWaxButterLocoState NewState)
{
	LocoState = NewState;
	StateElapsed = 0.0f;

	if (NewState == EWaxButterLocoState::HopAlign)
	{
		bInHopAir = false;
		SetCapsuleHalfHeightKeepFloor(HalfZ);
		if (BodyPivot)
		{
			BodyPivot->SetRelativeLocation(FVector::ZeroVector);
			BodyPivot->SetRelativeRotation(FQuat::Identity);
			BodyPivot->SetRelativeScale3D(FVector::OneVector);
		}
	}
	else if (NewState == EWaxButterLocoState::LayDown)
	{
		ChooseLayDirection();
		PrevLayLateral = 0.0f;
		CurrentLayAlpha = 0.0f;

		// 눕는 순간의 플레이어 위치를 기록 (추적 비율에 따라 현재 위치와 섞어 굴림 목표로 사용)
		bHasLockedTarget = false;
		FVector DirToPlayer;
		float DistToPlayer = 0.0f;
		if (GetDirectionToPlayer(DirToPlayer, DistToPlayer))
		{
			LockedTargetLocation = OwnerMonster->GetActorLocation() + DirToPlayer * DistToPlayer;
			bHasLockedTarget = true;
		}
		if (BodyPivot)
		{
			BodyPivot->SetRelativeScale3D(FVector::OneVector);
		}
	}
	else if (NewState == EWaxButterLocoState::TumbleCharge)
	{
		SetCapsuleHalfHeightKeepFloor(HalfY);
		CurrentLayAlpha = 1.0f;
		TumbleStepIndex = 0;
		TumbleTheta = 0.0f;
		TumbleOmega = InitialStepOmega * FMath::Max(ButterSettings().MinSpeedScale, SpeedMultiplier);
		PrevStepForward = 0.0f;
		PendingSteerYaw = 0.0f;
	}
	else if (NewState == EWaxButterLocoState::StandUp)
	{
		StandStartHeight = (TumbleStepIndex % 2 == 0) ? HalfY : HalfX;
		StandStartTheta = TumbleTheta;
		StandStartLayAlpha = CurrentLayAlpha;
		TumbleTheta = 0.0f;
		PendingSteerYaw = 0.0f;
		const float StartAngle = StandStartLayAlpha * (PI * 0.5f);
		PrevLayLateral = StandStartHeight * (1.0f - FMath::Cos(StartAngle)) + HalfZ * FMath::Sin(StartAngle);
		SetCapsuleHalfHeightKeepFloor(HalfZ);
	}
}

void UWaxButterLocomotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bStopped || !OwnerMonster || !BodyPivot || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (IsSuspended())
	{
		return;
	}

	switch (LocoState)
	{
	case EWaxButterLocoState::HopAlign:
		TickHopAlign(DeltaTime);
		break;
	case EWaxButterLocoState::LayDown:
		TickLayDown(DeltaTime);
		break;
	case EWaxButterLocoState::TumbleCharge:
		TickTumbleCharge(DeltaTime);
		break;
	case EWaxButterLocoState::StandUp:
		TickStandUp(DeltaTime);
		break;
	}
}

void UWaxButterLocomotionComponent::OnSuspendEnded()
{
	// 누운 상태에서 밀려났으면 현재 자세에서 일어난 뒤 HopAlign으로, 서 있었으면 바로 HopAlign 재진입
	if (LocoState == EWaxButterLocoState::LayDown)
	{
		TumbleStepIndex = 0;
		TumbleTheta = 0.0f;
		EnterState(EWaxButterLocoState::StandUp);
	}
	else if (LocoState == EWaxButterLocoState::TumbleCharge)
	{
		EnterState(EWaxButterLocoState::StandUp);
	}
	else if (LocoState == EWaxButterLocoState::HopAlign)
	{
		EnterState(EWaxButterLocoState::HopAlign);
	}
}

void UWaxButterLocomotionComponent::TickHopAlign(float DeltaTime)
{
	FVector DirToPlayer;
	float DistToPlayer = 0.0f;
	if (!GetDirectionToPlayer(DirToPlayer, DistToPlayer))
	{
		return;
	}

	const float CurrentYaw = OwnerMonster->GetActorRotation().Yaw;
	const float DesiredYaw = DirToPlayer.Rotation().Yaw;
	const float DeltaFwd = FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredYaw);
	const float DeltaBack = FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredYaw + 180.0f);

	const bool bUseBackFace = FMath::Abs(DeltaBack) < FMath::Abs(DeltaFwd);
	const float DeltaYaw = bUseBackFace ? DeltaBack : DeltaFwd;

	StateElapsed += DeltaTime;

	if (!bInHopAir)
	{
		BodyPivot->SetRelativeLocation(FVector::ZeroVector);

		if (StateElapsed >= HopPauseDuration)
		{
			const float ChargeThreshold = FMath::Max(OwnerMonster->GetMonsterData().StopDistanceToPlayer, MinChargeStartDistance);
			if (FMath::Abs(DeltaYaw) <= AlignToleranceDeg && DistToPlayer > ChargeThreshold)
			{
				TumbleDirectionSign = bUseBackFace ? -1.0f : 1.0f;
				EnterState(EWaxButterLocoState::LayDown);
				return;
			}

			if (FMath::Abs(DeltaYaw) > AlignToleranceDeg)
			{
				bInHopAir = true;
				StateElapsed = 0.0f;
				HopStartYaw = CurrentYaw;
				const float StepYaw = FMath::Clamp(DeltaYaw, -MaxYawPerHopDeg, MaxYawPerHopDeg);
				HopTargetYaw = CurrentYaw + StepYaw;
			}
			else
			{
				StateElapsed = HopPauseDuration;
			}
		}
		return;
	}

	const float T = FMath::Clamp(StateElapsed / FMath::Max(0.05f, HopDuration), 0.0f, 1.0f);
	const float ParabolicZ = 4.0f * HopHeight * T * (1.0f - T);
	const float NewYaw = FMath::Lerp(HopStartYaw, HopTargetYaw, FMath::SmoothStep(0.0f, 1.0f, T));

	OwnerMonster->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
	BodyPivot->SetRelativeLocation(FVector(0.0f, 0.0f, ParabolicZ));

	if (T >= 1.0f)
	{
		bInHopAir = false;
		StateElapsed = 0.0f;
		BodyPivot->SetRelativeLocation(FVector::ZeroVector);
	}
}

void UWaxButterLocomotionComponent::TickLayDown(float DeltaTime)
{
	StateElapsed += DeltaTime;
	const float RawT = FMath::Clamp(StateElapsed / FMath::Max(0.05f, LayDownDuration), 0.0f, 1.0f);
	// Gravity-weighted ease-in so the block tips over naturally with weight
	const float Alpha = FMath::Pow(RawT, ButterSettings().LayDownEaseExponent);
	CurrentLayAlpha = Alpha;
	const float AngleRad = Alpha * (PI * 0.5f);

	const float CenterZ = HalfY * FMath::Sin(AngleRad) + HalfZ * FMath::Cos(AngleRad);
	const float Lateral = HalfY * (1.0f - FMath::Cos(AngleRad)) + HalfZ * FMath::Sin(AngleRad);
	const float DeltaLateral = Lateral - PrevLayLateral;
	PrevLayLateral = Lateral;

	const FVector LayMoveDir = -LaySign * OwnerMonster->GetActorRightVector().GetSafeNormal2D();
	if (FMath::Abs(DeltaLateral) > KINDA_SMALL_NUMBER)
	{
		OwnerMonster->AddActorWorldOffset(LayMoveDir * DeltaLateral, true);
	}

	const float CapHalfH = OwnerMonster->GetCapsuleComponent() ? OwnerMonster->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : HalfZ;
	BodyPivot->SetRelativeLocation(FVector(0.0f, 0.0f, CenterZ - CapHalfH));
	BodyPivot->SetRelativeRotation(ComputeLaidRelQuat(Alpha, 0.0f));

	if (RawT >= 1.0f)
	{
		EnterState(EWaxButterLocoState::TumbleCharge);
	}
}

void UWaxButterLocomotionComponent::TickTumbleCharge(float DeltaTime)
{
	if (!FMath::IsNearlyZero(PendingSteerYaw, 0.05f))
	{
		const float MaxStepYaw = SteerYawRateDeg * DeltaTime;
		const float ApplyYaw = FMath::Clamp(PendingSteerYaw, -MaxStepYaw, MaxStepYaw);
		PendingSteerYaw -= ApplyYaw;
		OwnerMonster->AddActorWorldRotation(FRotator(0.0f, ApplyYaw, 0.0f));
	}

	const bool bEvenStep = (TumbleStepIndex % 2 == 0);
	const float W0 = bEvenStep ? HalfX : HalfY;
	const float H0 = bEvenStep ? HalfY : HalfX;
	const float CornerR = FMath::Sqrt(W0 * W0 + H0 * H0);
	const float ApexPhi = FMath::Atan2(W0, H0);

	const FWaxButterLocomotionSettings& BS = ButterSettings();
	const float SpeedScale = FMath::Max(BS.MinSpeedScale, SpeedMultiplier);
	const float GravTerm = (0.75f * EffectiveGravity * SpeedScale / FMath::Max(1.0f, CornerR)) * FMath::Sin(TumbleTheta - ApexPhi);
	const float DriveTerm = (TumbleTheta < ApexPhi) ? (DriveAngularAccel * SpeedScale) : (DriveAngularAccel * BS.PostApexDriveRatio * SpeedScale);
	const float AlphaAccel = GravTerm + DriveTerm;

	TumbleOmega = FMath::Max(BS.MinTumbleOmega * SpeedScale, TumbleOmega + AlphaAccel * DeltaTime);
	const float NextTheta = FMath::Min(PI * 0.5f, TumbleTheta + TumbleOmega * DeltaTime);

	const float NextForward = W0 * (1.0f - FMath::Cos(NextTheta)) + H0 * FMath::Sin(NextTheta);
	const float DeltaForward = NextForward - PrevStepForward;

	const FVector ChargeDir = OwnerMonster->GetActorForwardVector().GetSafeNormal2D() * TumbleDirectionSign;
	const FVector BeforeLoc = OwnerMonster->GetActorLocation();
	if (DeltaForward > KINDA_SMALL_NUMBER)
	{
		OwnerMonster->AddActorWorldOffset(ChargeDir * DeltaForward, true);
	}
	const float ActualForward = FVector::DotProduct(OwnerMonster->GetActorLocation() - BeforeLoc, ChargeDir);

	if (DeltaForward > 2.0f && ActualForward < DeltaForward * 0.45f)
	{
		EnterState(EWaxButterLocoState::StandUp);
		return;
	}

	TumbleTheta = NextTheta;
	PrevStepForward = NextForward;

	const float CenterZ = W0 * FMath::Sin(TumbleTheta) + H0 * FMath::Cos(TumbleTheta);
	const float CapHalfH = OwnerMonster->GetCapsuleComponent() ? OwnerMonster->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : HalfY;
	const float TotalTumbleAngle = TumbleDirectionSign * (static_cast<float>(TumbleStepIndex) * (PI * 0.5f) + TumbleTheta);

	BodyPivot->SetRelativeLocation(FVector(0.0f, 0.0f, CenterZ - CapHalfH));
	BodyPivot->SetRelativeRotation(ComputeLaidRelQuat(1.0f, TotalTumbleAngle));

	if (TumbleTheta >= (PI * 0.5f) - KINDA_SMALL_NUMBER)
	{
		TumbleStepIndex++;
		TumbleTheta = 0.0f;
		PrevStepForward = 0.0f;
		TumbleOmega = FMath::Max(InitialStepOmega * SpeedScale, TumbleOmega * StepBounceRetention);

		FVector DirToPlayer;
		float DistToPlayer = 0.0f;
		const bool bHasPlayer = GetTumbleTarget(DirToPlayer, DistToPlayer);
		if (!bHasPlayer || DistToPlayer <= OwnerMonster->GetMonsterData().StopDistanceToPlayer || TumbleStepIndex >= MaxTumbleSteps)
		{
			EnterState(EWaxButterLocoState::StandUp);
			return;
		}

		const float ChargeYaw = ChargeDir.Rotation().Yaw;
		const float DesiredYaw = DirToPlayer.Rotation().Yaw;
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(ChargeYaw, DesiredYaw);
		if (FMath::Abs(DeltaYaw) > AbortAngleDeg)
		{
			EnterState(EWaxButterLocoState::StandUp);
			return;
		}

		PendingSteerYaw = FMath::Clamp(DeltaYaw, -MaxCorrectionYawPerStepDeg, MaxCorrectionYawPerStepDeg);
	}
}

void UWaxButterLocomotionComponent::TickStandUp(float DeltaTime)
{
	StateElapsed += DeltaTime;
	const float RawT = FMath::Clamp(StateElapsed / FMath::Max(0.05f, StandUpDuration), 0.0f, 1.0f);
	const float SmoothT = FMath::SmoothStep(0.0f, 1.0f, RawT);
	const float LayAlpha = StandStartLayAlpha * (1.0f - SmoothT);
	const float AngleRad = LayAlpha * (PI * 0.5f);

	const float CenterZ = StandStartHeight * FMath::Sin(AngleRad) + HalfZ * FMath::Cos(AngleRad);
	const float Lateral = StandStartHeight * (1.0f - FMath::Cos(AngleRad)) + HalfZ * FMath::Sin(AngleRad);
	const float DeltaBack = PrevLayLateral - Lateral;
	PrevLayLateral = Lateral;

	const FVector StandMoveDir = LaySign * OwnerMonster->GetActorRightVector().GetSafeNormal2D();
	if (FMath::Abs(DeltaBack) > KINDA_SMALL_NUMBER)
	{
		OwnerMonster->AddActorWorldOffset(StandMoveDir * DeltaBack, true);
	}

	const float CapHalfH = OwnerMonster->GetCapsuleComponent() ? OwnerMonster->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : HalfZ;
	const float UnwoundTheta = FMath::Lerp(StandStartTheta, 0.0f, SmoothT);
	const float TotalTumbleAngle = TumbleDirectionSign * (static_cast<float>(TumbleStepIndex) * (PI * 0.5f) + UnwoundTheta);

	BodyPivot->SetRelativeLocation(FVector(0.0f, 0.0f, CenterZ - CapHalfH));
	const FQuat UnwindQuat(FVector::ForwardVector, -LaySign * (PI * 0.5f) * StandStartLayAlpha * SmoothT);
	const FQuat LaidBaseQuat = ComputeLaidRelQuat(StandStartLayAlpha, TotalTumbleAngle);
	BodyPivot->SetRelativeRotation((UnwindQuat * LaidBaseQuat).GetNormalized());

	if (RawT >= 1.0f)
	{
		// Absorb remaining k*90 deg yaw into ActorYaw so BodyPivot resets cleanly to Identity without popping
		const float ResidualYaw = BodyPivot->GetRelativeRotation().Yaw;
		const float NewActorYaw = OwnerMonster->GetActorRotation().Yaw + ResidualYaw;
		OwnerMonster->SetActorRotation(FRotator(0.0f, NewActorYaw, 0.0f));
		BodyPivot->SetRelativeRotation(FQuat::Identity);
		BodyPivot->SetRelativeLocation(FVector::ZeroVector);

		EnterState(EWaxButterLocoState::HopAlign);
	}
}

const FWaxButterLocomotionSettings& UWaxButterLocomotionComponent::ButterSettings() const
{
	return (MonsterData ? MonsterData : GetDefault<UWaxMonsterDataAsset>())->ButterLocomotion;
}
