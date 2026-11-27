#include "WaxMonsterBase.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "WaxBreakablePartComponent.h"
#include "WaxDestructibleComponent.h"
#include "WaxLocomotionComponent.h"
#include "WaxMonsterCore.h"
#include "WaxMonsterDataAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogWaxMonster, Log, All);

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

	// 모든 파괴 부위 GC의 공통 기본 설정 (ObjectType은 Dynamic, 몸 고정은 UWaxBreakablePartComponent가 담당)
	auto CreateBreakableCollection = [this](const TCHAR* Name, bool bCastShadow) -> UGeometryCollectionComponent*
	{
		UGeometryCollectionComponent* GC = CreateDefaultSubobject<UGeometryCollectionComponent>(Name);
		GC->SetupAttachment(BodyPivot);
		GC->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
		GC->bForceUpdateActiveTransforms = true;
		GC->SetSimulatePhysics(true);
		GC->SetEnableDamageFromCollision(false);
		GC->bNotifyCollisions = true;
		GC->SetNotifyRigidBodyCollision(true);
		GC->SetNotifyBreaks(true);
		GC->SetCollisionProfileName(TEXT("PhysicsActor"));
		GC->SetCollisionObjectType(ECC_PhysicsBody);
		GC->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		GC->bAllowRemovalOnSleep = false;
		GC->bAllowRemovalOnBreak = true;
		GC->SetCastShadow(bCastShadow);
		return GC;
	};

	ShellCollection = CreateBreakableCollection(TEXT("ShellCollection"), true);

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetupAttachment(BodyPivot);
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreMesh->SetReceivesDecals(false);

	EyeLeftGC = CreateBreakableCollection(TEXT("EyeLeftGC"), false);
	EyeRightGC = CreateBreakableCollection(TEXT("EyeRightGC"), false);
	for (UGeometryCollectionComponent* EyeGC : { EyeLeftGC.Get(), EyeRightGC.Get() })
	{
		EyeGC->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		EyeGC->SetReceivesDecals(false);
	}

	WaxDestructible = CreateDefaultSubobject<UWaxDestructibleComponent>(TEXT("WaxDestructible"));
	EyeLeftPart = CreateDefaultSubobject<UWaxBreakablePartComponent>(TEXT("EyeLeftPart"));
	EyeRightPart = CreateDefaultSubobject<UWaxBreakablePartComponent>(TEXT("EyeRightPart"));
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
		WaxDestructible->OnWaxCollapsed.RemoveDynamic(this, &AWaxMonsterBase::HandleWaxCollapsed);
		WaxDestructible->OnWaxCollapsed.AddDynamic(this, &AWaxMonsterBase::HandleWaxCollapsed);
	}

	for (UWaxBreakablePartComponent* EyePart : { EyeLeftPart.Get(), EyeRightPart.Get() })
	{
		if (EyePart)
		{
			EyePart->OnPartBroken.RemoveDynamic(this, &AWaxMonsterBase::HandleEyeBroken);
			EyePart->OnPartBroken.AddDynamic(this, &AWaxMonsterBase::HandleEyeBroken);
		}
	}

	InitializeFromData(MonsterData);
}

void AWaxMonsterBase::ConfigureEye(UGeometryCollectionComponent* EyeGC, UWaxBreakablePartComponent* EyePart, float LateralSign)
{
	if (!EyeGC || !EyePart || !MonsterData)
	{
		return;
	}

	EyeGC->SetRelativeLocation(FVector(MonsterData->EyeForwardOffset, LateralSign * MonsterData->EyeSpacing, MonsterData->EyeHeightOffset));
	EyeGC->SetRelativeRotation(FRotator::ZeroRotator);
	EyeGC->SetRelativeScale3D(FVector(MonsterData->EyeScale));

	if (!MonsterData->EyeCollectionAsset)
	{
		EyeGC->SetVisibility(false);
		EyeGC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EyePart->InitializePart(nullptr, nullptr, nullptr, nullptr);
		return;
	}

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

	EyePart->InitializePart(EyeGC, MonsterData->EyeCollectionAsset, &MonsterData->EyePart, MonsterData, LateralSign);
}

void AWaxMonsterBase::InitializeFromData(UWaxMonsterDataAsset* InData)
{
	MonsterData = InData;
	MonsterState = EWaxMonsterState::Active;
	HitSlowdownRemaining = 0.0f;

	if (MonsterData)
	{
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

		ConfigureEye(EyeLeftGC, EyeLeftPart, -1.0f);
		ConfigureEye(EyeRightGC, EyeRightPart, 1.0f);
	}

	EnsureLocomotionComponent();

	if (WaxDestructible && ShellCollection)
	{
		WaxDestructible->InitializeWithCollection(ShellCollection, MonsterData);
	}
}

int32 AWaxMonsterBase::GetIntactEyeCount() const
{
	int32 Count = 0;
	for (const UWaxBreakablePartComponent* EyePart : { EyeLeftPart.Get(), EyeRightPart.Get() })
	{
		if (EyePart && EyePart->GetCollection() && !EyePart->IsBroken() && !EyePart->IsDetached())
		{
			++Count;
		}
	}
	return Count;
}

const UWaxMonsterDataAsset& AWaxMonsterBase::GetMonsterData() const
{
	return MonsterData ? *MonsterData : *GetDefault<UWaxMonsterDataAsset>();
}

void AWaxMonsterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DrawDebugStatus();

	if (MonsterState != EWaxMonsterState::Active)
	{
		return;
	}

	UpdateEyeSupport();

	if (bHasPendingKnockback && GFrameCounter >= PendingKnockbackFrame)
	{
		bHasPendingKnockback = false;
		ApplyKnockbackNow(PendingKnockbackVelocity, PendingKnockbackDuration);
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

	if (GetMonsterData().bChasePlayer)
	{
		UpdateMonsterMovement(DeltaTime);
	}
	else if (ActiveLocomotion)
	{
		ActiveLocomotion->SetDesiredMoveDirection(FVector::ZeroVector);
	}
}

bool AWaxMonsterBase::ReceiveWaxHit_Implementation(const FWaxHitPayload& Payload)
{
	if (MonsterState != EWaxMonsterState::Active)
	{
		return false;
	}

	if (bShowDebugStatus)
	{
		UE_LOG(LogWaxMonster, Log, TEXT("WaxMonster Hit actor=%s distToCenter=%.0f radius=%.0f strain=%.0f knock=(%.0f,%.0f)"),
			*GetName(), FVector::Dist(Payload.Center, GetActorLocation()), Payload.Radius, Payload.Strain, Payload.KnockbackHorizontal, Payload.KnockbackVertical);
	}

	for (UWaxBreakablePartComponent* EyePart : { EyeLeftPart.Get(), EyeRightPart.Get() })
	{
		if (EyePart)
		{
			IWaxHittable::Execute_ReceiveWaxHit(EyePart, Payload);
		}
	}

	if (WaxDestructible && !WaxDestructible->HasCollapsed())
	{
		IWaxHittable::Execute_ReceiveWaxHit(WaxDestructible, Payload);
	}

	StartHitSlowdown();

	// 막 깨져 나온 조각이 Dynamic으로 바뀌기 전(1~2프레임)에 몸이 밀리면 조각이 몸을 따라 끌려가므로 넉백을 몇 프레임 늦춤
	if (Payload.HasKnockback() && MonsterState == EWaxMonsterState::Active)
	{
		const FVector Velocity = Payload.ComputeKnockbackVelocity(GetActorLocation());
		const int32 DelayFrames = GetMonsterData().KnockbackDelayFrames;
		if (DelayFrames <= 0)
		{
			ApplyKnockbackNow(Velocity, Payload.KnockbackDurationSec);
		}
		else
		{
			PendingKnockbackVelocity = Velocity;
			PendingKnockbackDuration = Payload.KnockbackDurationSec;
			PendingKnockbackFrame = GFrameCounter + DelayFrames;
			bHasPendingKnockback = true;
		}
	}
	return true;
}

void AWaxMonsterBase::ApplyKnockbackNow(const FVector& Velocity, float DurationSec)
{
	if (ActiveLocomotion)
	{
		ActiveLocomotion->SuspendFor(DurationSec);
	}
	LaunchCharacter(Velocity, true, true);
}

void AWaxMonsterBase::HandleEyeBroken(UWaxBreakablePartComponent* Part, FVector WorldLocation)
{
	BP_OnEyeShattered(WorldLocation);
}

void AWaxMonsterBase::StartHitSlowdown()
{
	const UWaxMonsterDataAsset& Data = GetMonsterData();
	if (MonsterState != EWaxMonsterState::Active || !Data.bEnableHitSlowdown || Data.HitSlowdownDuration <= 0.0f)
	{
		return;
	}

	HitSlowdownRemaining = Data.HitSlowdownDuration;
	if (ActiveLocomotion)
	{
		ActiveLocomotion->SetSpeedMultiplier(Data.HitSlowdownSpeedMultiplier);
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
	if (Dist > GetMonsterData().StopDistanceToPlayer && Dist > KINDA_SMALL_NUMBER)
	{
		ActiveLocomotion->SetDesiredMoveDirection(ToPlayer / Dist);
	}
	else
	{
		ActiveLocomotion->SetDesiredMoveDirection(FVector::ZeroVector);
	}
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

	// 안 깨진 눈은 떼어 낸 뒤 부위 컴포넌트가 DetachShatterDelaySec 뒤 산산조각 냄
	for (UWaxBreakablePartComponent* EyePart : { EyeLeftPart.Get(), EyeRightPart.Get() })
	{
		if (EyePart)
		{
			EyePart->DetachWhole(EyePart->ComputeDetachPopVelocity(HorizontalFwd));
		}
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

	SetLifeSpan(GetMonsterData().DebrisLifeSpanAfterCollapse);
}

void AWaxMonsterBase::DrawDebugStatus()
{
#if !UE_BUILD_SHIPPING
	if (!bShowDebugStatus || !GetWorld())
	{
		return;
	}

	const FVector Loc = GetActorLocation();
	const FVector Vel = GetVelocity();
	const float ShellOffset = ShellCollection ? FVector::Dist(ShellCollection->Bounds.Origin, Loc) : 0.0f;
	const float Remaining = WaxDestructible ? WaxDestructible->GetRemainingRatio() : 1.0f;

	// 좌표·속도·껍질 위치가 비정상이면 한 번만 로그 (밀려남인지 소멸인지 구분용)
	if (!bReportedAbnormal && (Loc.Size() > 1.0e6f || Vel.Size() > 2.0e4f || ShellOffset > 1000.0f || Loc.ContainsNaN()))
	{
		bReportedAbnormal = true;
		UE_LOG(LogWaxMonster, Warning, TEXT("WaxMonster Abnormal actor=%s loc=%s vel=%.0f shellOffset=%.0f state=%d"),
			*GetName(), *Loc.ToString(), Vel.Size(), ShellOffset, static_cast<int32>(MonsterState));
	}

	if (!GEngine)
	{
		return;
	}

	int32 ActiveCount = 0;
	int32 TotalCount = 0;
	for (TActorIterator<AWaxMonsterBase> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			++TotalCount;
			ActiveCount += It->MonsterState == EWaxMonsterState::Active ? 1 : 0;
		}
	}
	GEngine->AddOnScreenDebugMessage(9980, 0.0f, FColor::Green, FString::Printf(TEXT("[WaxMonsters] alive=%d total=%d"), ActiveCount, TotalCount));

	const FColor LineColor = (ShellOffset > 150.0f || Vel.Size() > 3000.0f) ? FColor::Red : FColor::White;
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(9000000 + GetUniqueID()), 0.0f, LineColor,
		FString::Printf(TEXT("  %s pos=(%.0f, %.0f, %.0f) speed=%.0f shellOffset=%.0f remain=%.0f%% shellVisible=%d"),
			*GetName(), Loc.X, Loc.Y, Loc.Z, Vel.Size(), ShellOffset, Remaining * 100.0f, ShellCollection && ShellCollection->IsVisible() ? 1 : 0));
#endif
}

void AWaxMonsterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bShowDebugStatus)
	{
		UE_LOG(LogWaxMonster, Log, TEXT("WaxMonster EndPlay actor=%s reason=%d state=%d loc=%s"),
			*GetName(), static_cast<int32>(EndPlayReason), static_cast<int32>(MonsterState), *GetActorLocation().ToString());
	}
	Super::EndPlay(EndPlayReason);
}

void AWaxMonsterBase::FellOutOfWorld(const UDamageType& DmgType)
{
	UE_LOG(LogWaxMonster, Warning, TEXT("WaxMonster FellOutOfWorld actor=%s loc=%s vel=%.0f"), *GetName(), *GetActorLocation().ToString(), GetVelocity().Size());
	Super::FellOutOfWorld(DmgType);
}

void AWaxMonsterBase::OutsideWorldBounds()
{
	UE_LOG(LogWaxMonster, Warning, TEXT("WaxMonster OutsideWorldBounds actor=%s loc=%s vel=%.0f"), *GetName(), *GetActorLocation().ToString(), GetVelocity().Size());
	Super::OutsideWorldBounds();
}

bool AWaxMonsterBase::GetAttachedBodyLowestZ(float& OutLowestZ) const
{
	float ShellLowest = 0.0f;
	if (!WaxDestructible || !WaxDestructible->ComputeAttachedLowestZ(ShellLowest))
	{
		return false;
	}

	float Lowest = ShellLowest;
	if (CoreMesh && CoreMesh->IsVisible() && CoreMesh->GetStaticMesh())
	{
		// 코어는 둥글어서 회전과 무관하게 중심 - 반지름이 최저점
		const float CoreRadius = CoreMesh->GetStaticMesh()->GetBounds().BoxExtent.Z * CoreMesh->GetComponentScale().Z;
		Lowest = FMath::Min(Lowest, static_cast<float>(CoreMesh->GetComponentLocation().Z) - CoreRadius);
	}
	OutLowestZ = Lowest;
	return true;
}

void AWaxMonsterBase::UpdateEyeSupport()
{
	// 눈을 받치는 껍질 조각이 모두 떨어져 나가면 눈도 통째로 탈락 (껍질이 깨진 뒤에만 검사)
	if (!WaxDestructible || !WaxDestructible->IsBroken() || !GetWorld())
	{
		return;
	}
	const float SinceShellBreak = GetWorld()->GetTimeSeconds() - WaxDestructible->GetLastBreakTime();
	for (UWaxBreakablePartComponent* EyePart : { EyeLeftPart.Get(), EyeRightPart.Get() })
	{
		if (!EyePart || !EyePart->GetCollection() || EyePart->IsDetached() || EyePart->IsReleased() || !EyePart->GetCollection()->IsVisible())
		{
			continue;
		}
		// 껍질이 마지막으로 깨진 뒤 일정 시간 안에서만 검사 (그 밖에서는 받침 상태가 바뀌지 않음)
		const FWaxBreakablePartSettings& S = EyePart->GetPartSettings();
		if (!S.bDetachWhenSupportLost || SinceShellBreak > S.SupportCheckWindowSec)
		{
			continue;
		}
		const FVector EyeLocation = EyePart->GetCollection()->GetComponentLocation();
		const int32 Attached = WaxDestructible->CountAttachedLeavesNear(EyeLocation, S.SupportRadius);
		if (Attached != INDEX_NONE && Attached <= S.SupportMinAttachedCount)
		{
			// 몸이 구르며 액터 앞 방향과 눈 방향이 달라지므로 눈의 바깥 방향으로 튀어나감
			FVector Outward = (EyeLocation - GetActorLocation()).GetSafeNormal2D();
			if (Outward.IsNearlyZero())
			{
				Outward = GetActorForwardVector();
			}
			EyePart->DetachWhole(EyePart->ComputeDetachPopVelocity(Outward));
		}
	}
}
