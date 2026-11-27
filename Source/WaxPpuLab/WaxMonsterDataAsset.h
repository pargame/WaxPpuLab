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

	/** 눕는 동작의 가속 곡선 지수. 1이면 일정한 속도, 키우면 처음엔 천천히 기울다 무게로 털썩 넘어감 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LayDown", meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float LayDownEaseExponent = 1.65f;

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

	/** 맞아서 느려질 때 텀블 속도 배율의 하한. 낮추면 맞았을 때 더 많이 멈칫함 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float MinSpeedScale = 0.25f;

	/** 모서리를 넘어간 뒤(정점 이후) 구동 가속 비율 (정점 전 대비). 높이면 넘어가는 동작이 빨라짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PostApexDriveRatio = 0.25f;

	/** 텀블 최소 각속도 (rad/s, 속도 배율 적용 전). 높이면 모서리에서 머뭇거리지 않음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "0.05"))
	float MinTumbleOmega = 0.55f;

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

	/** 공중에서 착지한 직후 구르기 회전을 실제 이동에 맞춰 섞는 시간 (초). 늘리면 착지 후 회전이 부드럽게 따라붙음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Rolling", meta = (ClampMin = "0.0"))
	float RollingLandingBlendSec = 0.2f;

	/** 착지 블렌드 동안 회전 속도를 따라가는 빠르기. 낮추면 착지 후 미끄러지듯 구름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Rolling", meta = (ClampMin = "0.1"))
	float RollingLandingInterpSpeed = 14.0f;

	/** 껍질이 깨져 아랫부분이 비면 남은 몸의 가장 낮은 점이 바닥에 닿도록 보이는 몸을 내림 (구르는 볼 전용). 끄면 깨져도 원래 높이 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|GroundContact")
	bool bKeepBrokenBodyOnGround = true;

	/** 바닥 맞춤 높이를 따라가는 빠르기. 높이면 깨진 모양대로 즉각 들썩이고, 낮추면 부드럽게 출렁임 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|GroundContact", meta = (ClampMin = "0.0"))
	float GroundContactInterpSpeed = 14.0f;

	/** 몸을 바닥 쪽으로 최대 얼마나 내릴지 (cm). 너무 많이 파묻히거나 튀면 줄임 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|GroundContact", meta = (ClampMin = "0.0"))
	float GroundContactMaxDrop = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "10.0"))
	float CapsuleRadius = 92.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "10.0", ToolTip = "Ignored by UWaxButterLocomotionComponent (use ButterLocomotion.HalfZ instead)."))
	float CapsuleHalfHeight = 92.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Butter")
	FWaxButterLocomotionSettings ButterLocomotion;

	/** 껍질 부위 파괴 규칙 (기본: 깨진 조각만 떨어지고 남은 몸통은 몸에 붙어 있음) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction")
	FWaxBreakablePartSettings ShellPart;

	/** 넉백을 적용하기 전 대기 프레임 수. 막 깨진 조각이 몸에서 풀려난 뒤 밀리도록 하는 값이라 1~3 권장, 0이면 즉시 */
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

	/** 코어가 튀어나올 때 무작위 회전 속도의 최대치 (도/s). 높이면 더 팽글팽글 돔 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0.0"))
	float CorePopMaxAngularSpeedDeg = 180.0f;

	/** 바운스 소리가 나는 최소 속도 (cm/s). 높이면 약한 굴림에는 소리 안 남 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|BounceAudio", meta = (ClampMin = "0.0"))
	float CoreBounceMinSpeed = 45.0f;

	/** 바운스 소리 볼륨이 최대가 되는 기준 속도 (cm/s). 낮추면 작은 충돌도 크게 들림 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|BounceAudio", meta = (ClampMin = "1.0"))
	float CoreBounceVolumeSpeedRef = 320.0f;

	/** 바운스 소리 볼륨 범위 (최소, 최대) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|BounceAudio")
	FVector2D CoreBounceVolumeRange = FVector2D(0.2f, 0.85f);

	/** 바운스 소리 피치 무작위 범위 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|BounceAudio")
	FVector2D CoreBouncePitchRange = FVector2D(0.95f, 1.18f);

	/** 바운스 소리 최소 간격 (초). 늘리면 연속 충돌음이 줄어듦 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|BounceAudio", meta = (ClampMin = "0.0"))
	float CoreBounceCooldownSec = 0.14f;

	/** 떨어진 코어가 멈춘 것으로 보기까지 최대 시간 (초). 지나면 속도와 무관하게 휴면 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Settle", meta = (ClampMin = "0.0"))
	float CoreSettleMaxSec = 1.2f;

	/** 이 시간이 지난 뒤부터 속도로 정착 판정 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Settle", meta = (ClampMin = "0.0"))
	float CoreSettleMinSec = 0.35f;

	/** 이 속도 아래면 정착한 것으로 봄 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Settle", meta = (ClampMin = "0.0"))
	float CoreSettleSpeedThreshold = 25.0f;

	/** 부활 미리보기가 자라기 시작하는 크기 (최종 대비 비율) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Revive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReviveStartScale = 0.08f;

	/** 부활 성장 감속 곡선 지수. 키우면 처음에 빨리 커지고 끝에서 천천히 멈춤 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Revive", meta = (ClampMin = "1.0"))
	float ReviveGrowEaseExp = 2.2f;

	/** 부활 중 맥동 진폭 (크기 비율). 0이면 맥동 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Revive", meta = (ClampMin = "0.0"))
	float RevivePulseAmplitude = 0.05f;

	/** 부활 동안 맥동 횟수 (반주기 단위). 키우면 더 자주 꿈틀거림 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core|Revive", meta = (ClampMin = "0.0"))
	float RevivePulseCycles = 5.0f;

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

	/** 눈 부위 파괴 규칙 (기본: 처음 깨지면 통째로 탈락, 이후 조각마다 수명 뒤 줄어들어 사라짐) */
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
