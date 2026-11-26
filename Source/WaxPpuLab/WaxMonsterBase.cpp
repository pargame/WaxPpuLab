#include "WaxMonsterBase.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Field/FieldSystemObjects.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "WaxDestructibleComponent.h"
#include "WaxLocomotionComponent.h"
#include "WaxMonsterCore.h"
#include "WaxMonsterDataAsset.h"

AWaxMonsterBase::AWaxMonsterBase()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(92.5f, 92.5f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 160.0f;
	bUseControllerRotationYaw = false;

	BodyPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyPivot"));
	BodyPivot->SetupAttachment(GetCapsuleComponent());

	ShellCollection = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("ShellCollection"));
	ShellCollection->SetupAttachment(BodyPivot);
	ShellCollection->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
	ShellCollection->bForceUpdateActiveTransforms = true;
	ShellCollection->SetSimulatePhysics(true);
	ShellCollection->SetEnableDamageFromCollision(false);
	ShellCollection->bNotifyCollisions = true;
	ShellCollection->SetNotifyRigidBodyCollision(true);
	ShellCollection->SetNotifyBreaks(true);
	ShellCollection->SetCollisionProfileName(TEXT("PhysicsActor"));
	ShellCollection->SetCollisionObjectType(ECC_PhysicsBody);
	ShellCollection->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	ShellCollection->bAllowRemovalOnSleep = false;
	ShellCollection->bAllowRemovalOnBreak = true;

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetupAttachment(BodyPivot);
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreMesh->SetReceivesDecals(false);

	auto InitEyeSubobject = [this](const TCHAR* Name) -> UGeometryCollectionComponent*
	{
		UGeometryCollectionComponent* EyeGC = CreateDefaultSubobject<UGeometryCollectionComponent>(Name);
		EyeGC->SetupAttachment(BodyPivot);
		EyeGC->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
		EyeGC->bForceUpdateActiveTransforms = true;
		EyeGC->SetSimulatePhysics(true);
		EyeGC->SetEnableDamageFromCollision(false);
		EyeGC->bNotifyCollisions = true;
		EyeGC->SetNotifyRigidBodyCollision(true);
		EyeGC->SetNotifyBreaks(true);
		EyeGC->SetCollisionProfileName(TEXT("PhysicsActor"));
		EyeGC->SetCollisionObjectType(ECC_PhysicsBody);
		EyeGC->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		EyeGC->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		EyeGC->bAllowRemovalOnSleep = false;
		EyeGC->bAllowRemovalOnBreak = true;
		EyeGC->SetCastShadow(false);
		EyeGC->SetReceivesDecals(false);
		return EyeGC;
	};

	EyeLeftGC = InitEyeSubobject(TEXT("EyeLeftGC"));
	EyeRightGC = InitEyeSubobject(TEXT("EyeRightGC"));

	WaxDestructible = CreateDefaultSubobject<UWaxDestructibleComponent>(TEXT("WaxDestructible"));
}

void AWaxMonsterBase::EnsureEyeFieldNodes()
{
	if (!EyeMaskFalloff)
	{
		EyeMaskFalloff = NewObject<URadialFalloff>(this);
		EyeStrainFalloff = NewObject<URadialFalloff>(this);
		EyeDynamicStateField = NewObject<UUniformInteger>(this);
		EyeLocalDynamicField = NewObject<UCullingField>(this);
		EyeUniformStrainField = NewObject<UUniformScalar>(this);
		EyeVelocityField = NewObject<UUniformVector>(this);
	}
}

void AWaxMonsterBase::EnsureLocomotionComponent()
{
	UClass* DesiredClass = (MonsterData && MonsterData->LocomotionClass)
		? MonsterData->LocomotionClass.Get()
		: UWaxLocomotionComponent::StaticClass();

	if (ActiveLocomotion && ActiveLocomotion->GetClass() != DesiredClass)
	{
		ActiveLocomotion->StopLocomotion();
		ActiveLocomotion->DestroyComponent();
		ActiveLocomotion = nullptr;
	}

	if (!ActiveLocomotion)
	{
		ActiveLocomotion = NewObject<UWaxLocomotionComponent>(this, DesiredClass, TEXT("ActiveLocomotion"));
		if (ActiveLocomotion)
		{
			ActiveLocomotion->RegisterComponent();
		}
	}

	if (ActiveLocomotion)
	{
		ActiveLocomotion->InitializeLocomotion(this, BodyPivot, MonsterData);
	}
}

void AWaxMonsterBase::BeginPlay()
{
	Super::BeginPlay();

	if (WaxDestructible)
	{
		WaxDestructible->OnWaxImpactApplied.RemoveDynamic(this, &AWaxMonsterBase::HandleWaxImpactApplied);
		WaxDestructible->OnWaxImpactApplied.AddDynamic(this, &AWaxMonsterBase::HandleWaxImpactApplied);
		WaxDestructible->OnWaxCollapsed.RemoveDynamic(this, &AWaxMonsterBase::HandleWaxCollapsed);
		WaxDestructible->OnWaxCollapsed.AddDynamic(this, &AWaxMonsterBase::HandleWaxCollapsed);
	}

	if (EyeLeftGC)
	{
		EyeLeftGC->OnChaosBreakEvent.RemoveDynamic(this, &AWaxMonsterBase::HandleLeftEyeBreak);
		EyeLeftGC->OnChaosBreakEvent.AddDynamic(this, &AWaxMonsterBase::HandleLeftEyeBreak);
	}
	if (EyeRightGC)
	{
		EyeRightGC->OnChaosBreakEvent.RemoveDynamic(this, &AWaxMonsterBase::HandleRightEyeBreak);
		EyeRightGC->OnChaosBreakEvent.AddDynamic(this, &AWaxMonsterBase::HandleRightEyeBreak);
	}

	InitializeFromData(MonsterData);
}

void AWaxMonsterBase::ConfigureEyeCollection(UGeometryCollectionComponent* EyeGC, const FVector& RelativeLoc)
{
	if (!EyeGC || !MonsterData)
	{
		return;
	}

	EyeGC->SetRelativeLocation(RelativeLoc);
	EyeGC->SetRelativeRotation(FRotator::ZeroRotator);
	EyeGC->SetRelativeScale3D(FVector(MonsterData->EyeScale));

	if (MonsterData->EyeCollectionAsset)
	{
		const float EyeThresh = FMath::Max(1000.0f, MonsterData->EyeDamageThreshold);
		EyeGC->DamageThreshold = { EyeThresh, EyeThresh * 0.5f, EyeThresh * 0.25f };
		EyeGC->SetRestCollection(MonsterData->EyeCollectionAsset, true);
		if (MonsterData->EyeMaterial)
		{
			EyeGC->SetMaterial(0, MonsterData->EyeMaterial);
			EyeGC->SetMaterial(1, MonsterData->EyeMaterial);
		}
		EyeGC->SetVisibility(true);
		EyeGC->SetCollisionProfileName(TEXT("PhysicsActor"));
		EyeGC->SetCollisionObjectType(ECC_PhysicsBody);
		EyeGC->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		EyeGC->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		EyeGC->bAllowRemovalOnSleep = false;
		EyeGC->bAllowRemovalOnBreak = true;
		EyeGC->SetSimulatePhysics(true);
		EyeGC->SetNotifyBreaks(true);

		EnsureEyeFieldNodes();
		EyeDynamicStateField->SetUniformInteger(static_cast<int32>(EObjectStateTypeEnum::Chaos_Object_Kinematic));
		EyeGC->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_DynamicState, nullptr, EyeDynamicStateField);
	}
	else
	{
		EyeGC->SetVisibility(false);
		EyeGC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AWaxMonsterBase::InitializeFromData(UWaxMonsterDataAsset* InData)
{
	MonsterData = InData;
	MonsterState = EWaxMonsterState::Active;
	HitSlowdownRemaining = 0.0f;
	LeftEyeDamage = 0.0f;
	RightEyeDamage = 0.0f;
	bLeftEyeBroken = false;
	bRightEyeBroken = false;

	if (MonsterData)
	{
		StopDistanceToPlayer = MonsterData->StopDistanceToPlayer;
		bChasePlayer = MonsterData->bChasePlayer;
		bEnableHitSlowdown = MonsterData->bEnableHitSlowdown;
		HitSlowdownDuration = MonsterData->HitSlowdownDuration;
		HitSlowdownSpeedMultiplier = MonsterData->HitSlowdownSpeedMultiplier;
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCapsuleSize(FMath::Max(10.0f, MonsterData->CapsuleRadius), FMath::Max(10.0f, MonsterData->CapsuleHalfHeight));
		}
		if (BodyPivot)
		{
			BodyPivot->SetRelativeLocation(FVector::ZeroVector);
			BodyPivot->SetRelativeRotation(FRotator::ZeroRotator);
		}
		if (ShellCollection)
		{
			ShellCollection->SetRelativeLocation(MonsterData->ShellRelativeOffset);
			ShellCollection->SetRelativeRotation(FRotator::ZeroRotator);
		}
		if (CoreMesh)
		{
			if (MonsterData->CoreMeshAsset)
			{
				CoreMesh->SetStaticMesh(MonsterData->CoreMeshAsset);
			}
			if (MonsterData->CoreMaterial)
			{
				CoreMesh->SetMaterial(0, MonsterData->CoreMaterial);
			}
			CoreMesh->SetRelativeLocation(MonsterData->CoreRelativeOffset);
			CoreMesh->SetRelativeScale3D(MonsterData->CoreRelativeScale);
			CoreMesh->SetVisibility(MonsterData->CoreMeshAsset != nullptr);
		}

		ConfigureEyeCollection(EyeLeftGC, FVector(MonsterData->EyeForwardOffset, -MonsterData->EyeSpacing, MonsterData->EyeHeightOffset));
		ConfigureEyeCollection(EyeRightGC, FVector(MonsterData->EyeForwardOffset, MonsterData->EyeSpacing, MonsterData->EyeHeightOffset));
	}

	EnsureLocomotionComponent();

	if (WaxDestructible && ShellCollection)
	{
		WaxDestructible->InitializeWithCollection(ShellCollection, MonsterData);
	}
}

int32 AWaxMonsterBase::GetIntactEyeCount() const
{
	return (bLeftEyeBroken ? 0 : 1) + (bRightEyeBroken ? 0 : 1);
}

void AWaxMonsterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MonsterState != EWaxMonsterState::Active)
	{
		return;
	}

	if (HitSlowdownRemaining > 0.0f)
	{
		HitSlowdownRemaining -= DeltaTime;
		if (HitSlowdownRemaining <= 0.0f)
		{
			HitSlowdownRemaining = 0.0f;
			if (ActiveLocomotion)
			{
				ActiveLocomotion->SetSpeedMultiplier(1.0f);
			}
		}
	}

	if (bChasePlayer)
	{
		UpdateMonsterMovement(DeltaTime);
	}
	else if (ActiveLocomotion)
	{
		ActiveLocomotion->SetDesiredMoveDirection(FVector::ZeroVector);
	}
}

void AWaxMonsterBase::ReceiveWaxImpact(const FVector& WorldImpactPoint, float BaseStrain, float Radius)
{
	ApplyEyeImpact(WorldImpactPoint, BaseStrain, Radius);

	if (WaxDestructible && !WaxDestructible->HasCollapsed())
	{
		WaxDestructible->ApplyWaxImpact(WorldImpactPoint, BaseStrain, Radius);
	}
}

void AWaxMonsterBase::ApplyEyeImpact(const FVector& WorldImpactPoint, float BaseStrain, float Radius)
{
	if (MonsterState != EWaxMonsterState::Active || Radius <= 0.0f)
	{
		return;
	}

	ApplyImpactToSingleEye(EyeLeftGC, bLeftEyeBroken, LeftEyeDamage, WorldImpactPoint, BaseStrain, Radius);
	ApplyImpactToSingleEye(EyeRightGC, bRightEyeBroken, RightEyeDamage, WorldImpactPoint, BaseStrain, Radius);
}

void AWaxMonsterBase::ApplyImpactToSingleEye(UGeometryCollectionComponent* EyeGC, bool& bBrokenFlag, float& AccumulatedDamage, const FVector& WorldImpactPoint, float BaseStrain, float Radius)
{
	if (!EyeGC || !EyeGC->IsVisible() || !MonsterData || !MonsterData->EyeCollectionAsset)
	{
		return;
	}

	const float EyeRadius = MonsterData->EyeBaseHitRadius * MonsterData->EyeScale;
	const float Dist = FVector::Dist(WorldImpactPoint, EyeGC->GetComponentLocation());
	if (Dist > (Radius + EyeRadius))
	{
		return;
	}

	if (!bBrokenFlag)
	{
		const float Durability = FMath::Max(1.0f, MonsterData->EyeDurability);
		AccumulatedDamage += 1.0f;
		if (AccumulatedDamage < Durability)
		{
			if (MonsterData->EyeShatterSound && GetWorld())
			{
				UGameplayStatics::PlaySoundAtLocation(this, MonsterData->EyeShatterSound, EyeGC->GetComponentLocation(), 0.45f, FMath::FRandRange(1.45f, 1.7f));
			}
			return;
		}
	}

	EnsureEyeFieldNodes();

	const float EffectiveRadius = FMath::Max(Radius, EyeRadius * 2.5f);
	EyeMaskFalloff->SetRadialFalloff(1.0f, 0.0f, 1.0f, 0.0f, EffectiveRadius, WorldImpactPoint, EFieldFalloffType::Field_FallOff_None);
	EyeDynamicStateField->SetUniformInteger(static_cast<int32>(EObjectStateTypeEnum::Chaos_Object_Dynamic));
	EyeLocalDynamicField->SetCullingField(EyeMaskFalloff, EyeDynamicStateField, EFieldCullingOperationType::Field_Culling_Outside);
	EyeGC->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_DynamicState, nullptr, EyeLocalDynamicField);

	EyeStrainFalloff->SetRadialFalloff(BaseStrain, 0.85f, 1.0f, 0.0f, EffectiveRadius, WorldImpactPoint, EFieldFalloffType::Field_Falloff_Linear);
	EyeGC->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain, nullptr, EyeStrainFalloff);
}

void AWaxMonsterBase::HandleLeftEyeBreak(const FChaosBreakEvent& BreakEvent)
{
	NotifyEyeBroken(bLeftEyeBroken, BreakEvent.Location);
}

void AWaxMonsterBase::HandleRightEyeBreak(const FChaosBreakEvent& BreakEvent)
{
	NotifyEyeBroken(bRightEyeBroken, BreakEvent.Location);
}

void AWaxMonsterBase::NotifyEyeBroken(bool& bBrokenFlag, const FVector& WorldLocation)
{
	if (bBrokenFlag)
	{
		return;
	}
	bBrokenFlag = true;

	if (MonsterData && MonsterData->EyeShatterSound && GetWorld())
	{
		UGameplayStatics::PlaySoundAtLocation(this, MonsterData->EyeShatterSound, WorldLocation, 0.85f, FMath::FRandRange(1.05f, 1.3f));
	}

	BP_OnEyeShattered(WorldLocation);
}

void AWaxMonsterBase::HandleWaxImpactApplied(FVector WorldImpactPoint)
{
	if (MonsterState != EWaxMonsterState::Active || !bEnableHitSlowdown || HitSlowdownDuration <= 0.0f)
	{
		return;
	}

	HitSlowdownRemaining = HitSlowdownDuration;
	if (ActiveLocomotion)
	{
		ActiveLocomotion->SetSpeedMultiplier(HitSlowdownSpeedMultiplier);
	}
}

void AWaxMonsterBase::UpdateMonsterMovement(float DeltaTime)
{
	if (!ActiveLocomotion)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		ActiveLocomotion->SetDesiredMoveDirection(FVector::ZeroVector);
		return;
	}

	FVector ToPlayer = PlayerPawn->GetActorLocation() - GetActorLocation();
	ToPlayer.Z = 0.0f;
	const float Dist = ToPlayer.Size();
	if (Dist > StopDistanceToPlayer && Dist > KINDA_SMALL_NUMBER)
	{
		ActiveLocomotion->SetDesiredMoveDirection(ToPlayer / Dist);
	}
	else
	{
		ActiveLocomotion->SetDesiredMoveDirection(FVector::ZeroVector);
	}
}

void AWaxMonsterBase::DropEyeWithChaos(UGeometryCollectionComponent* EyeGC, bool bAlreadyBroken, const FVector& PopVelocity)
{
	if (!EyeGC || !EyeGC->IsVisible())
	{
		return;
	}

	EnsureEyeFieldNodes();

	EyeGC->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	EyeGC->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	EyeDynamicStateField->SetUniformInteger(static_cast<int32>(EObjectStateTypeEnum::Chaos_Object_Dynamic));
	EyeGC->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_DynamicState, nullptr, EyeDynamicStateField);

	if (!bAlreadyBroken && !PopVelocity.IsNearlyZero())
	{
		EyeVelocityField->SetUniformVector(PopVelocity.Size(), PopVelocity.GetSafeNormal());
		EyeGC->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_LinearVelocity, nullptr, EyeVelocityField);
	}
}

void AWaxMonsterBase::ShatterRemainingEye(UGeometryCollectionComponent* EyeGC, bool& bBrokenFlag)
{
	if (!EyeGC || !EyeGC->IsVisible() || bBrokenFlag)
	{
		return;
	}

	EnsureEyeFieldNodes();
	const float ShatterStrain = (MonsterData ? MonsterData->EyeDamageThreshold : 180000.0f) * 2.5f;
	EyeUniformStrainField->SetUniformScalar(ShatterStrain);
	EyeGC->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain, nullptr, EyeUniformStrainField);
}

void AWaxMonsterBase::HandleWaxCollapsed(AActor* CollapsedActor)
{
	if (MonsterState == EWaxMonsterState::Collapsed)
	{
		return;
	}

	MonsterState = EWaxMonsterState::Collapsed;

	if (ActiveLocomotion)
	{
		ActiveLocomotion->StopLocomotion();
	}

	if (ShellCollection)
	{
		ShellCollection->SetCastShadow(false);
	}

	FVector HorizontalFwd = GetVelocity().GetSafeNormal2D();
	if (HorizontalFwd.IsNearlyZero())
	{
		if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			HorizontalFwd = (PlayerPawn->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		}
	}
	if (HorizontalFwd.IsNearlyZero())
	{
		HorizontalFwd = GetActorForwardVector().GetSafeNormal2D();
	}
	const FVector HorizontalRight = FVector::CrossProduct(FVector::UpVector, HorizontalFwd).GetSafeNormal();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
		MoveComp->SetComponentTickEnabled(false);
	}

	const float CapsuleHalfH = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 92.5f;
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (AController* Ctrl = GetController())
	{
		Ctrl->UnPossess();
		Ctrl->Destroy();
	}

	SetActorTickEnabled(false);

	DropEyeWithChaos(EyeLeftGC, bLeftEyeBroken, HorizontalFwd * 110.0f - HorizontalRight * 75.0f + FVector(0.0f, 0.0f, 95.0f));
	DropEyeWithChaos(EyeRightGC, bRightEyeBroken, HorizontalFwd * 110.0f + HorizontalRight * 75.0f + FVector(0.0f, 0.0f, 95.0f));

	if (GetWorld() && (!bLeftEyeBroken || !bRightEyeBroken))
	{
		const float EyeTimeout = MonsterData ? MonsterData->EyeDropTimeoutSec : 0.55f;
		GetWorld()->GetTimerManager().SetTimer(EyeTimeoutHandle, [this]()
		{
			ShatterRemainingEye(EyeLeftGC, bLeftEyeBroken);
			ShatterRemainingEye(EyeRightGC, bRightEyeBroken);
		}, EyeTimeout, false);
	}

	AWaxMonsterCore* SpawnedCore = nullptr;
	if (CoreMesh && GetWorld() && MonsterData && MonsterData->CoreMeshAsset)
	{
		const FTransform CoreXform(FRotator::ZeroRotator, CoreMesh->GetComponentLocation(), FVector::OneVector);
		CoreMesh->SetVisibility(false);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnedCore = GetWorld()->SpawnActor<AWaxMonsterCore>(AWaxMonsterCore::StaticClass(), CoreXform, SpawnParams);
		if (SpawnedCore)
		{
			SpawnedCore->InitializeCore(GetClass(), MonsterData, CapsuleHalfH);
		}
	}

	BP_OnWaxCollapsed(SpawnedCore);

	const float LifeSpan = MonsterData ? MonsterData->DebrisLifeSpanAfterCollapse : 5.5f;
	SetLifeSpan(LifeSpan);
}
