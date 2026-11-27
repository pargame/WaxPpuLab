// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaxPpuLabCharacter.h"
#include "Animation/AnimInstance.h"
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
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "WaxCombatComponent.h"
#include "WaxBreakablePartComponent.h"
#include "WaxDestructibleComponent.h"
#include "WaxMonsterBase.h"
#include "WaxMonsterDataAsset.h"
#include "WaxPlayerDataAsset.h"
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

	CombatComponent = CreateDefaultSubobject<UWaxCombatComponent>(TEXT("CombatComponent"));

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
	// 1인칭에서 안 보여도 판정 소켓·FP 포즈 복사를 위해 본 갱신 유지
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
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
			EnhancedInputComponent->BindAction(CrackAction, ETriggerEvent::Started, this, &AWaxPpuLabCharacter::OnAttackInputStarted);
			EnhancedInputComponent->BindAction(CrackAction, ETriggerEvent::Completed, this, &AWaxPpuLabCharacter::OnAttackInputReleased);
			EnhancedInputComponent->BindAction(CrackAction, ETriggerEvent::Canceled, this, &AWaxPpuLabCharacter::OnAttackInputReleased);
		}
		if (KickAction)
		{
			EnhancedInputComponent->BindAction(KickAction, ETriggerEvent::Started, this, &AWaxPpuLabCharacter::OnKickInputStarted);
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
		// 공격 중 몸 Yaw가 시선보다 늦을 수 있으므로 이동은 항상 시선(Control Yaw) 기준
		const FRotator YawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Right);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Forward);
	}
}

void AWaxPpuLabCharacter::DoJumpStart()
{
	// 공격 단계의 이동 제한이 점프를 막으면 무시
	if (CombatComponent && !CombatComponent->IsJumpAllowed())
	{
		return;
	}
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

	const float KickHeight = (PlayerData ? PlayerData : GetDefault<UWaxPlayerDataAsset>())->DebrisKickHeightOffset;
	const FVector KickOrigin = GetActorLocation() + FVector(0.0f, 0.0f, KickHeight);
	const FVector KickVelocity = HorizVel * DebrisKickStrength + FVector(0.0f, 0.0f, DebrisKickUpwardBoost);

	UWaxBreakablePartComponent::KickFragmentsInRadius(GetWorld(), KickOrigin, KickVelocity, DebrisKickRadius, this);
}

void AWaxPpuLabCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyPlayerData();
	bHasCachedHeadRef = false;
	SmoothedAimPitch = 0.0f;

	if (FirstPersonCameraComponent)
	{
		if (FirstPersonCameraComponent->GetAttachParent() != GetCapsuleComponent() || !FirstPersonCameraComponent->GetAttachSocketName().IsNone())
		{
			FirstPersonCameraComponent->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		}
		FirstPersonCameraComponent->SetRelativeLocationAndRotation(CameraBaseOffset, FRotator::ZeroRotator);
	}

	ApplyCameraViewMode();
	TryStartCommandLineCapture();
}

void AWaxPpuLabCharacter::ApplyPlayerData()
{
	if (!PlayerData)
	{
		return;
	}
	CameraBaseOffset = PlayerData->CameraBaseOffset;
	HeadFollowAlpha = PlayerData->HeadFollowAlpha;
	HeadFollowInterpSpeed = PlayerData->HeadFollowInterpSpeed;
	MaxHeadForwardOffset = PlayerData->MaxHeadForwardOffset;
	HeadSocketName = PlayerData->HeadSocketName;
	ThirdPersonPreviewOffset = PlayerData->ThirdPersonPreviewOffset;
	bEnableUpperBodyPitch = PlayerData->bEnableUpperBodyPitch;
	AimPitchInterpSpeed = PlayerData->AimPitchInterpSpeed;
	MaxUpperBodyPitchDeg = PlayerData->MaxUpperBodyPitchDeg;
	Spine01PitchWeight = PlayerData->Spine01PitchWeight;
	Spine02PitchWeight = PlayerData->Spine02PitchWeight;
	Spine03PitchWeight = PlayerData->Spine03PitchWeight;
	bEnableDebrisKick = PlayerData->bEnableDebrisKick;
	DebrisKickRadius = PlayerData->DebrisKickRadius;
	DebrisKickStrength = PlayerData->DebrisKickStrength;
	DebrisKickUpwardBoost = PlayerData->DebrisKickUpwardBoost;
	DebrisKickMinMoveSpeed = PlayerData->DebrisKickMinMoveSpeed;

	if (FirstPersonCameraComponent)
	{
		FirstPersonCameraComponent->FirstPersonFieldOfView = PlayerData->FirstPersonFieldOfView;
		FirstPersonCameraComponent->FirstPersonScale = PlayerData->FirstPersonScale;
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->AirControl = PlayerData->AirControl;
		Move->BrakingDecelerationFalling = PlayerData->BrakingDecelerationFalling;
	}
	ApplyViewPitchLimits();
}

void AWaxPpuLabCharacter::ApplyViewPitchLimits()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (PlayerData && PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->ViewPitchMin = PlayerData->ViewPitchMin;
		PC->PlayerCameraManager->ViewPitchMax = PlayerData->ViewPitchMax;
	}
}

void AWaxPpuLabCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	ApplyViewPitchLimits();
}

void AWaxPpuLabCharacter::SetThirdPersonPreview(bool bEnable)
{
	if (bThirdPersonPreview != bEnable)
	{
		bThirdPersonPreview = bEnable;
		ApplyCameraViewMode();
	}
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
	else if (HeadFollowAlpha * (CombatComponent ? CombatComponent->GetHeadFollowScale() : 1.0f) > KINDA_SMALL_NUMBER && FirstPersonMesh && !HeadSocketName.IsNone() && FirstPersonMesh->DoesSocketExist(HeadSocketName))
	{
		const FVector HeadWorld = FirstPersonMesh->GetSocketLocation(HeadSocketName);
		const FVector HeadLocal = GetActorTransform().InverseTransformPosition(HeadWorld);
		if (!bHasCachedHeadRef)
		{
			CachedHeadRefLocal = HeadLocal;
			bHasCachedHeadRef = true;
		}

		FVector HeadDelta = (HeadLocal - CachedHeadRefLocal) * HeadFollowAlpha * (CombatComponent ? CombatComponent->GetHeadFollowScale() : 1.0f);
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

	float NewPitch = (AimPitchInterpSpeed > KINDA_SMALL_NUMBER)
		? FMath::FInterpTo(SmoothedAimPitch, TargetPitch, DeltaTime, AimPitchInterpSpeed)
		: TargetPitch;

	const float PitchRateLimit = CombatComponent ? CombatComponent->GetPitchRateLimit() : 0.0f;
	if (PitchRateLimit > 0.0f)
	{
		const float MaxStep = PitchRateLimit * DeltaTime;
		NewPitch = SmoothedAimPitch + FMath::Clamp(NewPitch - SmoothedAimPitch, -MaxStep, MaxStep);
	}
	SmoothedAimPitch = NewPitch;
}

void AWaxPpuLabCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateFirstPersonCamera(DeltaTime);
	UpdateUpperBodyAimPitch(DeltaTime);

	UpdateDebrisKick();
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

void AWaxPpuLabCharacter::OnAttackInputStarted()
{
	if (CombatComponent)
	{
		CombatComponent->SetAttackHeld(true);
		CombatComponent->PressAttackInput(EWaxAttackSlot::Basic);
	}
}

void AWaxPpuLabCharacter::OnKickInputStarted()
{
	if (CombatComponent)
	{
		CombatComponent->PressAttackInput(EWaxAttackSlot::Kick);
	}
}

void AWaxPpuLabCharacter::OnAttackInputReleased()
{
	if (CombatComponent)
	{
		CombatComponent->SetAttackHeld(false);
	}
}

int32 AWaxPpuLabCharacter::SelectAttackMontage_Implementation(EWaxAttackSlot Slot, const FWaxAttackSpec& Spec, int32 ComboIndex)
{
	return ComboIndex;
}

bool AWaxPpuLabCharacter::CanExecuteAttack_Implementation(EWaxAttackSlot Slot)
{
	return true;
}

FWaxRotationProfile AWaxPpuLabCharacter::ResolveRotationProfile_Implementation(EWaxAttackSlot Slot, EWaxAttackPhase Phase, const FWaxRotationProfile& DefaultProfile)
{
	return DefaultProfile;
}

static bool ParseAttackSlot(const FString& Name, EWaxAttackSlot& OutSlot)
{
	const UEnum* SlotEnum = StaticEnum<EWaxAttackSlot>();
	const int64 Value = SlotEnum ? SlotEnum->GetValueByNameString(Name) : INDEX_NONE;
	if (Value == INDEX_NONE || Value == static_cast<int64>(EWaxAttackSlot::None))
	{
		return false;
	}
	OutSlot = static_cast<EWaxAttackSlot>(Value);
	return true;
}

void AWaxPpuLabCharacter::WaxAttackCapture(const FString& SlotName, int32 bQuitWhenDone)
{
	EWaxAttackSlot Slot = EWaxAttackSlot::None;
	if (!CombatComponent || !ParseAttackSlot(SlotName, Slot))
	{
		UE_LOG(LogWaxPpuLab, Warning, TEXT("WaxAttackCapture: unknown slot '%s' (Basic/AerialStrike/Slam/Kick)"), *SlotName);
		return;
	}
	CombatComponent->StartCapture(Slot, bQuitWhenDone != 0);
}

void AWaxPpuLabCharacter::TryStartCommandLineCapture()
{
	FString SlotName;
	if (!FParse::Value(FCommandLine::Get(), TEXT("WaxCapture="), SlotName) || !GetWorld())
	{
		return;
	}
	const bool bQuit = FParse::Param(FCommandLine::Get(), TEXT("WaxCaptureQuit"));
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, SlotName, bQuit]()
	{
		WaxAttackCapture(SlotName, bQuit ? 1 : 0);
	}), 1.5f, false);
}
