#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WaxMonsterDataAsset.generated.h"

class UGeometryCollection;
class UMaterialInterface;
class UPhysicalMaterial;
class USoundBase;
class UStaticMesh;
class UWaxLocomotionComponent;

USTRUCT(BlueprintType)
struct FWaxCollisionVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MasterIntensity = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.5"))
	float CrackleGain = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.5"))
	float SnapGain = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.5"))
	float BodyGain = 0.14f;
};

USTRUCT(BlueprintType)
struct FWaxAudioProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	FName ProfileName = TEXT("Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> CrackSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TArray<FWaxCollisionVariant> CollisionVariants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MasterIntensity = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.5"))
	float CrackleGain = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.5"))
	float SnapGain = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.5"))
	float BodyGain = 0.14f;
};

USTRUCT(BlueprintType)
struct FWaxHardnessZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness")
	FName ZoneName = TEXT("Core");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness")
	FVector LocalCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness", meta = (ClampMin = "5.0"))
	float LocalRadius = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness", meta = (ClampMin = "1.0"))
	float Durability = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness", meta = (ClampMin = "0.1"))
	float StrainMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness")
	bool bUseZoneAudioProfile = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness", meta = (EditCondition = "bUseZoneAudioProfile"))
	FWaxAudioProfile AudioProfile;
};

USTRUCT(BlueprintType)
struct FWaxButterLocomotionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimensions", meta = (ClampMin = "10.0"))
	float HalfX = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimensions", meta = (ClampMin = "10.0"))
	float HalfY = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimensions", meta = (ClampMin = "10.0"))
	float HalfZ = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charge", meta = (ClampMin = "50.0"))
	float MinChargeStartDistance = 340.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HopAlign", meta = (ClampMin = "0.1"))
	float HopDuration = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HopAlign", meta = (ClampMin = "0.0"))
	float HopPauseDuration = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HopAlign", meta = (ClampMin = "5.0"))
	float HopHeight = 38.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HopAlign", meta = (ClampMin = "5.0", ClampMax = "180.0"))
	float MaxYawPerHopDeg = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HopAlign", meta = (ClampMin = "1.0", ClampMax = "45.0"))
	float AlignToleranceDeg = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LayDown", meta = (ClampMin = "0.1"))
	float LayDownDuration = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LayDown", meta = (ClampMin = "0.1"))
	float StandUpDuration = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "100.0"))
	float EffectiveGravity = 1350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "1.0"))
	float DriveAngularAccel = 13.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "0.5"))
	float InitialStepOmega = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float StepBounceRetention = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "1"))
	int32 MaxTumbleSteps = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float MaxCorrectionYawPerStepDeg = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "10.0", ClampMax = "720.0"))
	float SteerYawRateDeg = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "15.0", ClampMax = "180.0"))
	float AbortAngleDeg = 55.0f;
};

UCLASS(BlueprintType)
class WAXPPULAB_API UWaxMonsterDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 스폰 시 지면 기준 반높이(cm). Butter 이동이면 ButterLocomotion.HalfZ, 그 외에는 CapsuleHalfHeight를 사용한다. */
	float GetSpawnHalfHeight() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UGeometryCollection> ShellCollectionAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FVector ShellRelativeOffset = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	TSubclassOf<UWaxLocomotionComponent> LocomotionClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", ToolTip = "Ignored by UWaxButterLocomotionComponent (use ButterLocomotion instead)."))
	float MoveSpeed = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float StopDistanceToPlayer = 150.0f;

	/** 플레이어를 추적 이동할지 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bChasePlayer = true;

	/** 맞았을 때 잠깐 느려지는 반응 사용 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitStop")
	bool bEnableHitSlowdown = true;

	/** 맞았을 때 느려지는 시간 (초). 늘리면 경직이 길어짐 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitStop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HitSlowdownDuration = 0.15f;

	/** 느려지는 동안 이동 속도 배율 (0~1). 낮출수록 더 많이 멈칫함 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitStop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HitSlowdownSpeedMultiplier = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "10.0"))
	float RollRadius = 92.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "10.0"))
	float CapsuleRadius = 92.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "10.0", ToolTip = "Ignored by UWaxButterLocomotionComponent (use ButterLocomotion.HalfZ instead)."))
	float CapsuleHalfHeight = 92.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Butter")
	FWaxButterLocomotionSettings ButterLocomotion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float CollapseRemainingRatio = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.0"))
	float CollapseReleaseStrain = 350000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.5"))
	float DebrisLifeSpanAfterCollapse = 5.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction")
	TArray<FWaxHardnessZone> HardnessZones;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TObjectPtr<UStaticMesh> CoreMeshAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TObjectPtr<UMaterialInterface> CoreMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TObjectPtr<UPhysicalMaterial> CorePhysicalMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	FVector CoreRelativeScale = FVector(1.05f, 1.05f, 1.05f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	FVector CoreRelativeOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0.0"))
	float CoreLinearDamping = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0.0"))
	float CoreAngularDamping = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	FVector CorePopVelocityMin = FVector(-90.0f, -90.0f, 110.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	FVector CorePopVelocityMax = FVector(90.0f, 90.0f, 175.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	bool bEnableCoreRevive = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0.5"))
	float CoreReviveDelaySec = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0.1"))
	float ReviveGrowDuration = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TObjectPtr<UStaticMesh> RevivePreviewMeshAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TObjectPtr<UMaterialInterface> RevivePreviewMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	FVector RevivePreviewScale = FVector(1.85f, 1.85f, 1.85f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TObjectPtr<USoundBase> CoreBounceSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes")
	TObjectPtr<UGeometryCollection> EyeCollectionAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes")
	TObjectPtr<UMaterialInterface> EyeMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "1000.0"))
	float EyeDamageThreshold = 180000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "1.0"))
	float EyeDurability = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "10.0"))
	float EyeForwardOffset = 82.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "5.0"))
	float EyeSpacing = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes")
	float EyeHeightOffset = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "0.05"))
	float EyeScale = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "5.0"))
	float EyeBaseHitRadius = 52.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "0.1"))
	float EyeDropTimeoutSec = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes")
	TObjectPtr<USoundBase> EyeShatterSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	FWaxAudioProfile DefaultAudioProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TArray<FWaxAudioProfile> DevAudioProfiles;

	/** 파괴 시 깨짐 소리 조각(그레인)을 흩뿌리는 최소 지연 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio", meta = (ClampMin = "0.0"))
	float BreakGrainSpreadMinSec = 0.012f;

	/** 파괴 시 그레인을 흩뿌리는 최대 지연 (초). 늘리면 소리가 길게 퍼짐 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio", meta = (ClampMin = "0.0"))
	float BreakGrainSpreadMaxSec = 0.085f;

	/** 동시에 대기할 수 있는 최대 그레인 수. 늘리면 소리가 빽빽해짐 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio", meta = (ClampMin = "1"))
	int32 MaxQueuedGrains = 16;

	/** 전체 붕괴 시 그레인을 흩뿌리는 최대 지연 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio", meta = (ClampMin = "0.05"))
	float CollapseGrainSpreadMaxSec = 0.45f;

	/** 전체 붕괴 시 최대 그레인 수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio", meta = (ClampMin = "4"))
	int32 CollapseMaxQueuedGrains = 24;

	/** 파편 충돌음이 나는 최소 충돌 속도 (cm/s). 높이면 약한 충돌은 조용함 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio", meta = (ClampMin = "0.0"))
	float CollisionSoundSpeedThreshold = 65.0f;

	/** 충돌음 사이 최소 간격 (초). 늘리면 충돌음 빈도 감소 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio", meta = (ClampMin = "0.0"))
	float MinCollisionAudioInterval = 0.020f;

	/** 같은 자리 반복 충돌음을 묶는 공간 칸 크기 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio", meta = (ClampMin = "2.0"))
	float CollisionSpatialCellSize = 15.0f;

	/** 같은 칸에서 충돌음을 다시 내기까지 대기 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio", meta = (ClampMin = "0.01"))
	float CollisionSpatialCooldownSec = 0.08f;

	/** 충돌음 잔금(Crackle) 레이어 볼륨 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CollisionCrackleScale = 1.00f;

	/** 충돌음 딱(Snap) 레이어 볼륨 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CollisionSnapScale = 1.00f;

	/** 충돌음 몸통(Body) 레이어 볼륨 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CollisionBodyScale = 1.00f;

	/** 전체 붕괴 후에도 파편 걷어차기 허용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collapse")
	bool bAllowKickAfterCollapse = false;

	/** 전체 붕괴 후 유예 시간이 지나면 충돌음 끄기 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collapse")
	bool bMuteCollisionAudioAfterCollapse = true;

	/** 붕괴 후 충돌음을 허용하는 유예 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collapse", meta = (ClampMin = "0.0"))
	float CollapseCollisionMaxGraceSec = 3.0f;
};
