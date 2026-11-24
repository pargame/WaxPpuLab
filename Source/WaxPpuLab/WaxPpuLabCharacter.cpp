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
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "WaxDestructibleComponent.h"
#include "WaxLocomotionComponent.h"
#include "WaxMonsterBase.h"
#include "WaxMonsterDataAsset.h"
#include "WaxPpuLab.h"

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
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
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
			EnhancedInputComponent->BindAction(CrackAction, ETriggerEvent::Triggered, this, &AWaxPpuLabCharacter::DoWaxCrackTrace);
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

FVector AWaxPpuLabCharacter::GetFloatingTargetSphereCenter() const
{
	if (!FirstPersonCameraComponent)
	{
		return GetActorLocation() + GetActorForwardVector() * WaxTargetForwardDistance;
	}
	return FirstPersonCameraComponent->GetComponentLocation() + FirstPersonCameraComponent->GetForwardVector() * WaxTargetForwardDistance;
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

void AWaxPpuLabCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (FirstPersonCameraComponent && GetWorld())
	{
		const FVector SphereCenter = GetFloatingTargetSphereCenter();

		if (TargetingMPC)
		{
			if (UMaterialParameterCollectionInstance* MPCInst = GetWorld()->GetParameterCollectionInstance(TargetingMPC))
			{
				MPCInst->SetVectorParameterValue(TEXT("TargetSphereCenter"), FLinearColor(SphereCenter.X, SphereCenter.Y, SphereCenter.Z, 1.0f));
				MPCInst->SetScalarParameterValue(TEXT("TargetSphereRadius"), WaxImpactRadius);
				MPCInst->SetVectorParameterValue(TEXT("MarkerColor"), TargetDecalColor);
				MPCInst->SetScalarParameterValue(TEXT("EmissiveBoost"), TargetDecalEmissiveBoost);
				MPCInst->SetScalarParameterValue(TEXT("MarkerOpacity"), TargetDecalOpacity);
				MPCInst->SetScalarParameterValue(TEXT("EdgeSoftness"), TargetDecalEdgeSoftness);
			}
		}

		if (bShowDebugTargetSphere)
		{
			DrawDebugSphere(GetWorld(), SphereCenter, WaxImpactRadius, 20, FColor::Red, false, 0.0f, 0, 1.0f);
		}
	}

	UpdateDebrisKick();
}

void AWaxPpuLabCharacter::DoWaxCrackTrace()
{
	if (!FirstPersonCameraComponent || !GetWorld())
	{
		return;
	}

	const FVector SphereCenter = GetFloatingTargetSphereCenter();
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WaxCrackSphere), false, this);
	if (GetWorld()->OverlapMultiByChannel(Overlaps, SphereCenter, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(WaxImpactRadius), Params))
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
				Monster->ApplyEyeImpact(SphereCenter, WaxImpactStrain, WaxImpactRadius);
			}

			if (UWaxDestructibleComponent* Destructible = HitActor->FindComponentByClass<UWaxDestructibleComponent>())
			{
				if (!Destructible->HasCollapsed())
				{
					Destructible->ApplyWaxImpact(SphereCenter, WaxImpactStrain, WaxImpactRadius);
				}
			}
		}
	}
}

namespace
{
	int32 GActiveDevAudioPreset = 0;
}

void AWaxPpuLabCharacter::SetWaxAudioPreset(int32 PresetIndex)
{
	GActiveDevAudioPreset = PresetIndex;
	if (!GetWorld())
	{
		return;
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
			if (Data->LocomotionClass && Data->LocomotionClass->IsChildOf(UWaxButterLocomotionComponent::StaticClass()))
			{
				HalfHeight = FMath::Max(10.0f, Data->ButterLocomotion.HalfZ) + 4.0f;
			}
			else
			{
				HalfHeight = FMath::Max(10.0f, Data->CapsuleHalfHeight) + 4.0f;
			}
		}
	}
	const FVector SpawnLoc(XYPos.X, XYPos.Y, GroundZ + HalfHeight);
	const FRotator FacingPlayerRot(0.0f, GetControlRotation().Yaw + 180.0f, 0.0f);
	const FTransform SpawnTransform(FacingPlayerRot, SpawnLoc, FVector::OneVector);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (AWaxMonsterBase* Spawned = GetWorld()->SpawnActor<AWaxMonsterBase>(TestMonsterClass, SpawnTransform, SpawnParams))
	{
		if (GActiveDevAudioPreset >= 1 && Spawned->WaxDestructible)
		{
			Spawned->WaxDestructible->SelectDevAudioProfile(GActiveDevAudioPreset - 1);
		}
	}
}
