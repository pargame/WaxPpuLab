#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WaxBreakableTypes.h"
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
struct FWaxCrackSynthTuning
{
	GENERATED_BODY()

	/** 질량 정규화 기준 (kg). 이 질량 이상이면 가장 무거운 소리(낮은 컷오프·긴 감쇠·낮은 피치) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common", meta = (ClampMin = "0.1"))
	float MassReferenceKg = 8.0f;

	/** 가벼움→무거움 컷오프 주파수 (Hz). X=가벼운 조각, Y=무거운 조각 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common")
	FVector2D CutoffHzLightToHeavy = FVector2D(13500.0f, 4200.0f);

	/** 컷오프 주파수 무작위 흔들림 폭 (±Hz) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common", meta = (ClampMin = "0.0"))
	float CutoffJitterHz = 900.0f;

	/** 가벼움→무거움 감쇠 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common")
	FVector2D DecaySecLightToHeavy = FVector2D(0.075f, 0.22f);

	/** 감쇠 시간 무작위 배율 범위 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common")
	FVector2D DecayJitterScale = FVector2D(0.85f, 1.15f);

	/** 가벼움→무거움 피치 오프셋 (반음) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common")
	FVector2D PitchLightToHeavy = FVector2D(2.8f, -2.4f);

	/** 피치 무작위 흔들림 폭 (±반음) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common", meta = (ClampMin = "0.0"))
	float PitchJitter = 0.9f;

	/** 깨짐 속도 정규화 기준 (cm/s). 이 속도 이상이면 최대 세기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (ClampMin = "1.0"))
	float BreakSpeedReference = 450.0f;

	/** 깨짐 속도 계수 하한. 키우면 느리게 깨져도 소리가 커짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BreakSpeedAlphaMin = 0.2f;

	/** 깨짐 소리 기본 세기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (ClampMin = "0.0"))
	float BreakIntensityBase = 0.35f;

	/** 질량·속도에 따라 더해지는 깨짐 소리 세기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (ClampMin = "0.0"))
	float BreakIntensityGain = 0.5f;

	/** 깨짐 소리 세기 허용 범위 (최소, 최대) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	FVector2D BreakIntensityClamp = FVector2D(0.25f, 0.9f);

	/** 깨짐 소리 세기 무작위 배율 범위 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	FVector2D BreakIntensityJitter = FVector2D(0.85f, 1.08f);

	/** 붕괴 시 가장 늦게 나는 그레인의 세기 배율. 낮추면 붕괴 꼬리가 더 작아짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CollapseTailIntensityScale = 0.38f;

	/** 대기 중 그레인 수에 따른 세기 감쇠 지수. 키우면 동시에 많이 깨질 때 더 작아짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (ClampMin = "0.0"))
	float QueueDensityExponent = 0.22f;

	/** 충돌음 밀도 계산 창 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.01"))
	float CollisionDensityWindowSec = 0.12f;

	/** 최근 충돌 수에 따른 세기 감쇠 지수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.0"))
	float CollisionDensityExponent = 0.35f;

	/** 충돌 밀도 감쇠의 하한 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CollisionDensityMinGain = 0.65f;

	/** 충돌 계산에 쓰는 최소 질량 (kg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.01"))
	float CollisionMinMassKg = 0.2f;

	/** 이 압력 이하 충돌은 무음 처리되는 기준값. 키우면 약한 충돌이 조용해짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.0"))
	float CollisionPressureOffset = 40.0f;

	/** 압력 정규화 범위. 키우면 최대 세기에 도달하기 어려워짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "1.0"))
	float CollisionPressureRange = 700.0f;

	/** 압력→세기 로그 곡선 경사. 키우면 약한 충돌도 빨리 커짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.1"))
	float CollisionLoudnessCurve = 5.0f;

	/** 충돌음 세기 범위 (약함, 강함) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	FVector2D CollisionIntensityRange = FVector2D(0.06f, 0.36f);

	/** 충돌음 세기 무작위 배율 범위 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	FVector2D CollisionIntensityJitter = FVector2D(0.88f, 1.10f);

	/** 경도 구역이 아직 버틸 때 나는 짧은 균열음 세기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZoneHit", meta = (ClampMin = "0.0"))
	float ZoneHitIntensity = 0.5f;

	/** 경도 구역 균열음 컷오프 (Hz) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZoneHit", meta = (ClampMin = "100.0"))
	float ZoneHitCutoffHz = 6500.0f;

	/** 경도 구역 균열음 감쇠 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZoneHit", meta = (ClampMin = "0.01"))
	float ZoneHitDecaySec = 0.06f;

	/** 경도 구역 균열음 피치 오프셋 (반음) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZoneHit")
	float ZoneHitPitchOffset = 1.0f;
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

	/** 굴림 목표의 실시간 추적 비율. 0: 눕는 순간 플레이어가 있던 지점으로만 감, 1: 매 굴림마다 현재 플레이어 위치를 따라감, 사이 값은 두 지점 사이를 노림 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetTrackingRatio = 0.0f;
};

UCLASS(BlueprintType)
class WAXPPULAB_API UWaxMonsterDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWaxMonsterDataAsset();

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

	/** 껍질 부위 파괴 규칙 (기본: 깨진 조각만 떨어지고 남은 몸통은 몸에 붙어 있음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction")
	FWaxBreakablePartSettings ShellPart;

	/** 넉백을 적용하기 전 대기 프레임 수. 깨진 껍질이 몸에 다시 고정된 뒤 밀리도록 하는 값이라 1~3 권장, 0이면 즉시 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0", ClampMax = "10"))
	int32 KnockbackDelayFrames = 2;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "10.0"))
	float EyeForwardOffset = 82.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "5.0"))
	float EyeSpacing = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes")
	float EyeHeightOffset = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "0.05"))
	float EyeScale = 0.30f;

	/** 눈 부위 파괴 규칙 (기본: 처음 깨지면 통째로 탈락 후 수명 뒤 사라짐) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes")
	FWaxBreakablePartSettings EyePart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	FWaxAudioProfile DefaultAudioProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TArray<FWaxAudioProfile> DevAudioProfiles;

	/** 깨짐·충돌 소리 합성 파라미터 매핑 (질량·속도·압력 → 세기·컷오프·감쇠·피치) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestructionAudio")
	FWaxCrackSynthTuning CrackSynth;

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
