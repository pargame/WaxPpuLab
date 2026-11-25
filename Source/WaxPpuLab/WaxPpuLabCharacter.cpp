// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaxPpuLabCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "WaxDestructibleComponent.h"
#include "WaxMonsterBase.h"
#include "WaxMonsterDataAsset.h"
#include "WaxPpuLab.h"
#include "WaxPpuLabGameMode.h"

AWaxPpuLabCharacter::AWaxPpuLabCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(CameraBaseOffset, FRotator::ZeroRotator);
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;
}

void AWaxPpuLabCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AWaxPpuLabCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AWaxPpuLabCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AWaxPpuLabCharacter::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWaxPpuLabCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AWaxPpuLabCharacter::LookInput);

		// Wax Actions
		if (CrackAction)
		{
			EnhancedInputComponent->BindAction(CrackAction, ETriggerEvent::Triggered, this, &AWaxPpuLabCharacter::RequestPunch);
		}
		if (SpawnTestMonsterAction)
		{
			EnhancedInputComponent->BindAction(SpawnTestMonsterAction, ETriggerEvent::Started, this, &AWaxPpuLabCharacter::SpawnDemoWaxBallInFront);
		}
		if (AudioPresetAction)
		{
			EnhancedInputComponent->BindAction(AudioPresetAction, ETriggerEvent::Started, this, &AWaxPpuLabCharacter::AudioPresetInput);
		}
		if (SwitchMonsterAction)
		{
			EnhancedInputComponent->BindAction(SwitchMonsterAction, ETriggerEvent::Started, this, &AWaxPpuLabCharacter::SwitchMonsterInput);
		}
		if (ToggleThirdPersonAction)
		{
			EnhancedInputComponent->BindAction(ToggleThirdPersonAction, ETriggerEvent::Started, this, &AWaxPpuLabCharacter::ToggleThirdPersonPreview);
		}
	}
	else
	{
		UE_LOG(LogWaxPpuLab, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void AWaxPpuLabCharacter::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void AWaxPpuLabCharacter::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void AWaxPpuLabCharacter::AudioPresetInput(const FInputActionValue& Value)
{
	const int32 PresetIndex = FMath::RoundToInt(Value.Get<float>());
	if (PresetIndex >= 1)
	{
		SetWaxAudioPreset(PresetIndex);
	}
}

void AWaxPpuLabCharacter::SwitchMonsterInput(const FInputActionValue& Value)
{
	const int32 Count = TestMonsterClasses.Num();
	if (Count <= 0)
	{
		return;
	}

	const float Axis = Value.Get<float>();
	const int32 Delta = (Axis >= 0.0f) ? 1 : -1;
	SelectedMonsterIndex = (SelectedMonsterIndex + Delta + Count) % Count;
	TestMonsterClass = TestMonsterClasses[SelectedMonsterIndex];

#if !UE_BUILD_SHIPPING
	if (GEngine && TestMonsterClass)
	{
		GEngine->AddOnScreenDebugMessage(9994, 2.2f, FColor::Cyan,
			FString::Printf(TEXT("[WaxMonster] %d/%d: %s"), SelectedMonsterIndex + 1, Count, *TestMonsterClass->GetName()));
	}
#endif
}

void AWaxPpuLabCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AWaxPpuLabCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void AWaxPpuLabCharacter::DoJumpStart()
{
	// pass Jump to the character
	Jump();
}

void AWaxPpuLabCharacter::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}

void AWaxPpuLabCharacter::UpdateDebrisKick()
{
	if (!bEnableDebrisKick || !GetWorld())
	{
		return;
	}

	const FVector Velocity = GetVelocity();
	const FVector HorizVel(Velocity.X, Velocity.Y, 0.0f);
	if (HorizVel.Size() < DebrisKickMinMoveSpeed)
	{
		return;
	}

	const FVector KickOrigin = GetActorLocation() + FVector(0.0f, 0.0f, -35.0f);
	const FVector KickVelocity = HorizVel * DebrisKickStrength + FVector(0.0f, 0.0f, DebrisKickUpwardBoost);

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WaxDebrisKick), false, this);

	if (GetWorld()->OverlapMultiByObjectType(Overlaps, KickOrigin, FQuat::Identity, ObjParams, FCollisionShape::MakeSphere(DebrisKickRadius), QueryParams))
	{
		TSet<AActor*> CheckedActors;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (!HitActor || CheckedActors.Contains(HitActor))
			{
				continue;
			}
			CheckedActors.Add(HitActor);

			if (UWaxDestructibleComponent* Destructible = HitActor->FindComponentByClass<UWaxDestructibleComponent>())
			{
				if (Destructible->HasBrokenPieces() && (!Destructible->HasCollapsed() || Destructible->bAllowKickAfterCollapse))
				{
					Destructible->ApplyDebrisKick(KickOrigin, KickVelocity, DebrisKickRadius);
				}
			}
		}
	}
}

void AWaxPpuLabCharacter::BeginPlay()
{
	Super::BeginPlay();

	bHasCachedHeadRef = false;
	bPunchMarkerActive = false;
	PunchMarkerExpireTime = -1.0;
	SmoothedAimPitch = 0.0f;
	SetPunchMarkerVisible(false);

	if (FirstPersonCameraComponent)
	{
		if (FirstPersonCameraComponent->GetAttachParent() != GetCapsuleComponent() || !FirstPersonCameraComponent->GetAttachSocketName().IsNone())
		{
			FirstPersonCameraComponent->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		}
		FirstPersonCameraComponent->SetRelativeLocationAndRotation(CameraBaseOffset, FRotator::ZeroRotator);
	}

	ApplyCameraViewMode();
}

void AWaxPpuLabCharacter::ToggleThirdPersonPreview()
{
	bThirdPersonPreview = !bThirdPersonPreview;
	ApplyCameraViewMode();

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(9995, 1.8f, FColor::Green,
			bThirdPersonPreview ? TEXT("[Camera] 3rd Person Preview ON") : TEXT("[Camera] 1st Person"));
	}
#endif
}

void AWaxPpuLabCharacter::ApplyCameraViewMode()
{
	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetVisibility(!bThirdPersonPreview, true);
	}
	if (USkeletalMeshComponent* BodyMesh = GetMesh())
	{
		BodyMesh->SetFirstPersonPrimitiveType(bThirdPersonPreview ? EFirstPersonPrimitiveType::None : EFirstPersonPrimitiveType::WorldSpaceRepresentation);
		BodyMesh->SetOwnerNoSee(!bThirdPersonPreview);
	}
}

void AWaxPpuLabCharacter::SetPunchMarkerVisible(bool bVisible, const FVector& ImpactCenter, float Radius)
{
	bPunchMarkerActive = bVisible;
	if (!TargetingMPC || !GetWorld())
	{
		return;
	}

	if (UMaterialParameterCollectionInstance* MPCInst = GetWorld()->GetParameterCollectionInstance(TargetingMPC))
	{
		if (bVisible)
		{
			MPCInst->SetVectorParameterValue(TEXT("TargetSphereCenter"), FLinearColor(ImpactCenter.X, ImpactCenter.Y, ImpactCenter.Z, 1.0f));
			MPCInst->SetScalarParameterValue(TEXT("TargetSphereRadius"), Radius);
			MPCInst->SetVectorParameterValue(TEXT("MarkerColor"), PunchMarkerColor);
			MPCInst->SetScalarParameterValue(TEXT("EmissiveBoost"), PunchMarkerEmissiveBoost);
			MPCInst->SetScalarParameterValue(TEXT("MarkerOpacity"), PunchMarkerOpacity);
			MPCInst->SetScalarParameterValue(TEXT("EdgeSoftness"), PunchMarkerEdgeSoftness);
		}
		else
		{
			MPCInst->SetScalarParameterValue(TEXT("MarkerOpacity"), 0.0f);
		}
	}
}

void AWaxPpuLabCharacter::UpdatePunchMarker(double Now)
{
	if (!bPunchMarkerActive)
	{
		return;
	}

	if (!bShowPunchImpactMarker || Now >= PunchMarkerExpireTime)
	{
		SetPunchMarkerVisible(false);
	}
}

void AWaxPpuLabCharacter::UpdateFirstPersonCamera(float DeltaTime)
{
	if (!FirstPersonCameraComponent)
	{
		return;
	}

	FVector TargetLoc = CameraBaseOffset;
	if (bThirdPersonPreview)
	{
		const FRotator LocalControlRot = (GetControlRotation() - GetActorRotation()).GetNormalized();
		TargetLoc += LocalControlRot.RotateVector(ThirdPersonPreviewOffset);
	}
	else if (HeadFollowAlpha > KINDA_SMALL_NUMBER && FirstPersonMesh && !HeadSocketName.IsNone() && FirstPersonMesh->DoesSocketExist(HeadSocketName))
	{
		const FVector HeadWorld = FirstPersonMesh->GetSocketLocation(HeadSocketName);
		const FVector HeadLocal = GetActorTransform().InverseTransformPosition(HeadWorld);
		if (!bHasCachedHeadRef)
		{
			CachedHeadRefLocal = HeadLocal;
			bHasCachedHeadRef = true;
		}

		FVector HeadDelta = (HeadLocal - CachedHeadRefLocal) * HeadFollowAlpha;
		HeadDelta.X = FMath::Clamp(HeadDelta.X, -MaxHeadForwardOffset, MaxHeadForwardOffset);
		TargetLoc += HeadDelta;
	}

	const FVector CurrentLoc = FirstPersonCameraComponent->GetRelativeLocation();
	const FVector NewLoc = (HeadFollowInterpSpeed > KINDA_SMALL_NUMBER)
		? FMath::VInterpTo(CurrentLoc, TargetLoc, DeltaTime, HeadFollowInterpSpeed)
		: TargetLoc;
	FirstPersonCameraComponent->SetRelativeLocation(NewLoc);
}

void AWaxPpuLabCharacter::UpdateUpperBodyAimPitch(float DeltaTime)
{
	const float RawPitch = FRotator::NormalizeAxis(GetControlRotation().Pitch);
	const float TargetPitch = bEnableUpperBodyPitch
		? FMath::Clamp(RawPitch, -MaxUpperBodyPitchDeg, MaxUpperBodyPitchDeg)
		: 0.0f;

	SmoothedAimPitch = (AimPitchInterpSpeed > KINDA_SMALL_NUMBER)
		? FMath::FInterpTo(SmoothedAimPitch, TargetPitch, DeltaTime, AimPitchInterpSpeed)
		: TargetPitch;
}

void AWaxPpuLabCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateFirstPersonCamera(DeltaTime);
	UpdateUpperBodyAimPitch(DeltaTime);

	if (GetWorld())
	{
		UpdatePunchMarker(GetWorld()->GetTimeSeconds());
	}

	UpdateDebrisKick();
}

void AWaxPpuLabCharacter::RequestPunch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (PunchMontages.Num() == 0)
	{
		UE_LOG(LogWaxPpuLab, Warning, TEXT("'%s' RequestPunch skipped: PunchMontages is empty."), *GetNameSafe(this));
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (Now - LastPunchStartTime < static_cast<double>(PunchCooldownSec))
	{
		return;
	}

	USkeletalMeshComponent* AnimMesh = GetMesh();
	UAnimInstance* AnimInst = AnimMesh ? AnimMesh->GetAnimInstance() : nullptr;
	if (!AnimInst)
	{
		UE_LOG(LogWaxPpuLab, Warning, TEXT("'%s' RequestPunch skipped: AnimInstance is null."), *GetNameSafe(this));
		return;
	}

	UAnimMontage* MontageToPlay = PunchMontages[NextPunchIndex % PunchMontages.Num()];
	NextPunchIndex = (NextPunchIndex + 1) % PunchMontages.Num();
	if (!MontageToPlay)
	{
		UE_LOG(LogWaxPpuLab, Warning, TEXT("'%s' RequestPunch skipped: selected PunchMontage is null."), *GetNameSafe(this));
		return;
	}

	if (AnimInst->Montage_Play(MontageToPlay, PunchPlayRate) <= 0.0f)
	{
		UE_LOG(LogWaxPpuLab, Warning, TEXT("'%s' RequestPunch skipped: Montage_Play failed for '%s'."), *GetNameSafe(this), *GetNameSafe(MontageToPlay));
		return;
	}

	LastPunchStartTime = Now;
}

void AWaxPpuLabCharacter::HandlePunchImpactNotify(FName ImpactBoneName, float StrainScale, float RadiusScale)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	USkeletalMeshComponent* SourceMesh = nullptr;
	if (!ImpactBoneName.IsNone())
	{
		if (FirstPersonMesh && FirstPersonMesh->DoesSocketExist(ImpactBoneName))
		{
			SourceMesh = FirstPersonMesh;
		}
		else if (GetMesh() && GetMesh()->DoesSocketExist(ImpactBoneName))
		{
			SourceMesh = GetMesh();
		}
	}

	if (!SourceMesh)
	{
		UE_LOG(LogWaxPpuLab, Warning, TEXT("'%s' HandlePunchImpactNotify skipped: socket/bone '%s' not found."), *GetNameSafe(this), *ImpactBoneName.ToString());
		return;
	}

	const FVector ForwardDir = FirstPersonCameraComponent
		? FirstPersonCameraComponent->GetForwardVector()
		: GetActorForwardVector();
	const FVector ImpactCenter = SourceMesh->GetSocketLocation(ImpactBoneName) + ForwardDir * PunchBoneForwardOffset;

	const float FinalStrain = WaxImpactStrain * FMath::Max(0.1f, StrainScale);
	const float FinalRadius = WaxImpactRadius * FMath::Max(0.1f, RadiusScale);

	if (bShowPunchImpactMarker)
	{
		SetPunchMarkerVisible(true, ImpactCenter, FinalRadius);
		PunchMarkerExpireTime = World->GetTimeSeconds() + static_cast<double>(PunchMarkerDurationSec);
	}
	else if (bPunchMarkerActive)
	{
		SetPunchMarkerVisible(false);
	}

	if (bShowDebugPunchImpact)
	{
		DrawDebugSphere(World, ImpactCenter, FinalRadius, 16, PunchMarkerColor.ToFColor(true), false, PunchMarkerDurationSec, 0, 1.5f);
	}

	ExecuteWaxImpactAtLocation(ImpactCenter, FinalStrain, FinalRadius);
}

void AWaxPpuLabCharacter::ExecuteWaxImpactAtLocation(const FVector& ImpactCenter, float Strain, float Radius)
{
	if (!GetWorld())
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WaxCrackSphere), false, this);
	if (GetWorld()->OverlapMultiByChannel(Overlaps, ImpactCenter, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Params))
	{
		TSet<AActor*> CheckedActors;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (!HitActor || CheckedActors.Contains(HitActor))
			{
				continue;
			}
			CheckedActors.Add(HitActor);

			if (AWaxMonsterBase* Monster = Cast<AWaxMonsterBase>(HitActor))
			{
				Monster->ReceiveWaxImpact(ImpactCenter, Strain, Radius);
			}
			else if (UWaxDestructibleComponent* Destructible = HitActor->FindComponentByClass<UWaxDestructibleComponent>())
			{
				if (!Destructible->HasCollapsed())
				{
					Destructible->ApplyWaxImpact(ImpactCenter, Strain, Radius);
				}
			}
		}
	}
}

void AWaxPpuLabCharacter::SetWaxAudioPreset(int32 PresetIndex)
{
	if (!GetWorld())
	{
		return;
	}
	if (AWaxPpuLabGameMode* GameMode = GetWorld()->GetAuthGameMode<AWaxPpuLabGameMode>())
	{
		GameMode->SetActiveDevAudioPreset(PresetIndex);
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (UWaxDestructibleComponent* Destructible = It->FindComponentByClass<UWaxDestructibleComponent>())
		{
			Destructible->SelectDevAudioProfile(PresetIndex - 1);
		}
	}
}

void AWaxPpuLabCharacter::SpawnDemoWaxBallInFront()
{
	if (!FirstPersonCameraComponent || !GetWorld() || !TestMonsterClass)
	{
		return;
	}

	const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
	const FVector ForwardFlat = YawOnly.Vector();
	const FVector XYPos = GetActorLocation() + ForwardFlat * WaxSpawnDistance;

	const FVector TraceStart(XYPos.X, XYPos.Y, GetActorLocation().Z + 300.0f);
	const FVector TraceEnd(XYPos.X, XYPos.Y, GetActorLocation().Z - 600.0f);
	FHitResult FloorHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WaxFloorSnap), false, this);
	float GroundZ = GetActorLocation().Z - 96.0f;
	if (GetWorld()->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		GroundZ = FloorHit.ImpactPoint.Z;
	}

	float HalfHeight = 100.0f;
	if (const AWaxMonsterBase* DefaultMonster = TestMonsterClass->GetDefaultObject<AWaxMonsterBase>())
	{
		if (const UWaxMonsterDataAsset* Data = DefaultMonster->MonsterData)
		{
			HalfHeight = Data->GetSpawnHalfHeight() + 4.0f;
		}
	}
	const FVector SpawnLoc(XYPos.X, XYPos.Y, GroundZ + HalfHeight);
	const FRotator FacingPlayerRot(0.0f, GetControlRotation().Yaw + 180.0f, 0.0f);
	const FTransform SpawnTransform(FacingPlayerRot, SpawnLoc, FVector::OneVector);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (AWaxMonsterBase* Spawned = GetWorld()->SpawnActor<AWaxMonsterBase>(TestMonsterClass, SpawnTransform, SpawnParams))
	{
		const AWaxPpuLabGameMode* GameMode = GetWorld()->GetAuthGameMode<AWaxPpuLabGameMode>();
		const int32 ActivePreset = GameMode ? GameMode->GetActiveDevAudioPreset() : 0;
		if (ActivePreset >= 1 && Spawned->WaxDestructible)
		{
			Spawned->WaxDestructible->SelectDevAudioProfile(ActivePreset - 1);
		}
	}
}
