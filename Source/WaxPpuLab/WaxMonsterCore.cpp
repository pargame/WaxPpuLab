#include "WaxMonsterCore.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundBase.h"
#include "WaxMonsterBase.h"
#include "WaxMonsterDataAsset.h"

AWaxMonsterCore::AWaxMonsterCore()
{
	PrimaryActorTick.bCanEverTick = true;

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	RootComponent = CoreMesh;
	CoreMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	CoreMesh->SetCollisionObjectType(ECC_WorldDynamic);
	CoreMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	CoreMesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	CoreMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CoreMesh->SetNotifyRigidBodyCollision(true);
	CoreMesh->SetReceivesDecals(false);
	CoreMesh->SetLinearDamping(0.35f);
	CoreMesh->SetAngularDamping(0.6f);

	RevivePreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RevivePreviewMesh"));
	RevivePreviewMesh->SetupAttachment(RootComponent);
	RevivePreviewMesh->SetUsingAbsoluteLocation(true);
	RevivePreviewMesh->SetUsingAbsoluteRotation(true);
	RevivePreviewMesh->SetUsingAbsoluteScale(true);
	RevivePreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RevivePreviewMesh->SetReceivesDecals(false);
	RevivePreviewMesh->SetVisibility(false);
}

void AWaxMonsterCore::InitializeCore(TSubclassOf<AWaxMonsterBase> InMonsterClass, UWaxMonsterDataAsset* InDataAsset, float InCapsuleHalfHeight)
{
	MonsterClassToRevive = InMonsterClass;
	MonsterData = InDataAsset;
	MonsterCapsuleHalfHeight = FMath::Max(50.0f, InCapsuleHalfHeight);
	CoreState = EWaxCoreState::Falling;
	StateElapsed = 0.0f;

	if (MonsterData)
	{
		if (MonsterData->CoreMeshAsset)
		{
			CoreMesh->SetStaticMesh(MonsterData->CoreMeshAsset);
		}
		if (MonsterData->CoreMaterial)
		{
			CoreMesh->SetMaterial(0, MonsterData->CoreMaterial);
		}
		if (MonsterData->CorePhysicalMaterial)
		{
			CoreMesh->SetPhysMaterialOverride(MonsterData->CorePhysicalMaterial);
		}
		CoreMesh->SetWorldScale3D(MonsterData->CoreRelativeScale);
		CoreMesh->SetLinearDamping(MonsterData->CoreLinearDamping);
		CoreMesh->SetAngularDamping(MonsterData->CoreAngularDamping);

		if (MonsterData->RevivePreviewMeshAsset)
		{
			RevivePreviewMesh->SetStaticMesh(MonsterData->RevivePreviewMeshAsset);
		}
		if (MonsterData->RevivePreviewMaterial)
		{
			RevivePreviewMesh->SetMaterial(0, MonsterData->RevivePreviewMaterial);
		}
		ReviveTargetScale = MonsterData->RevivePreviewScale;
	}

	CoreMesh->OnComponentHit.RemoveDynamic(this, &AWaxMonsterCore::HandleCoreHit);
	CoreMesh->OnComponentHit.AddDynamic(this, &AWaxMonsterCore::HandleCoreHit);
	CoreMesh->SetSimulatePhysics(true);

	const FVector PopMin = MonsterData ? MonsterData->CorePopVelocityMin : FVector(-90.0f, -90.0f, 110.0f);
	const FVector PopMax = MonsterData ? MonsterData->CorePopVelocityMax : FVector(90.0f, 90.0f, 175.0f);
	const FVector PopVel(
		FMath::FRandRange(PopMin.X, PopMax.X),
		FMath::FRandRange(PopMin.Y, PopMax.Y),
		FMath::FRandRange(PopMin.Z, PopMax.Z));
	CoreMesh->SetPhysicsLinearVelocity(PopVel);
	CoreMesh->SetPhysicsAngularVelocityInDegrees(FVector(
		FMath::FRandRange(-180.0f, 180.0f),
		FMath::FRandRange(-180.0f, 180.0f),
		FMath::FRandRange(-180.0f, 180.0f)));
}

void AWaxMonsterCore::HandleCoreHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (CoreState == EWaxCoreState::Dead || !GetWorld() || !MonsterData || !MonsterData->CoreBounceSound)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextAllowedBounceSoundTime)
	{
		return;
	}

	const float Speed = CoreMesh ? CoreMesh->GetPhysicsLinearVelocity().Size() : 0.0f;
	if (Speed < 45.0f)
	{
		return;
	}

	const float Vol = FMath::Clamp(Speed / 320.0f, 0.2f, 0.85f);
	UGameplayStatics::PlaySoundAtLocation(this, MonsterData->CoreBounceSound, Hit.ImpactPoint, Vol, FMath::FRandRange(0.95f, 1.18f));
	NextAllowedBounceSoundTime = Now + 0.14;
}

void AWaxMonsterCore::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CoreState == EWaxCoreState::Dead)
	{
		return;
	}

	StateElapsed += DeltaTime;

	if (CoreState == EWaxCoreState::Falling)
	{
		const float Speed = CoreMesh ? CoreMesh->GetPhysicsLinearVelocity().Size() : 0.0f;
		if (StateElapsed >= 1.2f || (StateElapsed >= 0.35f && Speed < 25.0f))
		{
			CoreState = EWaxCoreState::Dormant;
			StateElapsed = 0.0f;
		}
	}
	else if (CoreState == EWaxCoreState::Dormant)
	{
		const bool bCanRevive = MonsterData && MonsterData->bEnableCoreRevive && MonsterClassToRevive != nullptr;
		const float ReviveDelay = MonsterData ? MonsterData->CoreReviveDelaySec : 5.0f;
		if (bCanRevive && StateElapsed >= ReviveDelay)
		{
			BeginReviving();
		}
	}
	else if (CoreState == EWaxCoreState::Reviving)
	{
		const float GrowDur = MonsterData ? FMath::Max(0.1f, MonsterData->ReviveGrowDuration) : 0.85f;
		const float Alpha = FMath::Clamp(StateElapsed / GrowDur, 0.0f, 1.0f);
		const float Smooth = FMath::InterpEaseOut(0.08f, 1.0f, Alpha, 2.2f);
		const float Pulse = 1.0f + 0.05f * FMath::Sin(Alpha * PI * 5.0f) * (1.0f - Alpha);

		RevivePreviewMesh->SetWorldScale3D(ReviveTargetScale * Smooth * Pulse);

		if (Alpha >= 1.0f)
		{
			FinishReviving();
		}
	}
}

void AWaxMonsterCore::BeginReviving()
{
	CoreState = EWaxCoreState::Reviving;
	StateElapsed = 0.0f;

	FVector CoreLoc = CoreMesh->GetComponentLocation();
	float GroundZ = CoreLoc.Z;
	if (GetWorld())
	{
		FHitResult FloorHit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(WaxCoreReviveFloor), false, this);
		if (GetWorld()->LineTraceSingleByChannel(FloorHit, CoreLoc + FVector(0.0f, 0.0f, 150.0f), CoreLoc - FVector(0.0f, 0.0f, 400.0f), ECC_WorldStatic, Params))
		{
			GroundZ = FloorHit.ImpactPoint.Z;
		}
	}

	ReviveSpawnLocation = FVector(CoreLoc.X, CoreLoc.Y, GroundZ + MonsterCapsuleHalfHeight + 2.0f);

	CoreMesh->SetSimulatePhysics(false);
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RevivePreviewMesh->SetWorldLocation(ReviveSpawnLocation);
	RevivePreviewMesh->SetWorldRotation(FRotator::ZeroRotator);
	RevivePreviewMesh->SetWorldScale3D(ReviveTargetScale * 0.08f);
	RevivePreviewMesh->SetVisibility(true);

	BP_OnCoreReviveStarted();
}

void AWaxMonsterCore::FinishReviving()
{
	if (!GetWorld() || !MonsterClassToRevive)
	{
		Destroy();
		return;
	}

	const FTransform SpawnTransform(FRotator::ZeroRotator, ReviveSpawnLocation, FVector::OneVector);
	AWaxMonsterBase* Revived = GetWorld()->SpawnActorDeferred<AWaxMonsterBase>(
		MonsterClassToRevive,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (Revived)
	{
		Revived->MonsterData = MonsterData;
		Revived->FinishSpawning(SpawnTransform);
		BP_OnCoreRevived(Revived);
	}

	CoreState = EWaxCoreState::Dead;
	Destroy();
}

void AWaxMonsterCore::KillCore_Implementation()
{
	if (CoreState == EWaxCoreState::Dead)
	{
		return;
	}

	CoreState = EWaxCoreState::Dead;
	OnCorePermanentlyKilled.Broadcast(this);
	Destroy();
}
