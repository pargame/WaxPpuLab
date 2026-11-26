#pragma once

#include "CoreMinimal.h"
#include "WaxCombatTypes.generated.h"

class UAnimMontage;
class UCurveFloat;

/** 입력 슬롯 겸 공격 종류 */
UENUM(BlueprintType)
enum class EWaxAttackSlot : uint8
{
	None,
	Basic,
	AerialStrike,
	Slam,
	Kick
};

/** 공격 진행 단계. Active 구간은 몽타주의 WaxAttackActive NotifyState로 지정 */
UENUM(BlueprintType)
enum class EWaxAttackPhase : uint8
{
	Idle,
	Windup,
	Active,
	Recovery
};

/** 타격 원점을 읽을 메시 */
UENUM(BlueprintType)
enum class EWaxHitSource : uint8
{
	/** 몸체 메시(GetMesh) 본/소켓 */
	Body,
	/** 몸체 메시에 부착된 무기 메시 소켓 (무기 미장착 시 Body로 대체) */
	Weapon
};

/** 공격 단계별 회전 제한. 0 이하 값은 '제한 없음' */
USTRUCT(BlueprintType)
struct FWaxRotationProfile
{
	GENERATED_BODY()

	/** 몸 Yaw가 시선을 따라가는 최대 회전속도 (도/초). 0 이하면 즉시 추종 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	float MaxYawRateDegPerSec = 0.0f;

	/** 상반신 Pitch 최대 회전속도 (도/초). 0 이하면 기본 보간만 적용 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	float MaxPitchRateDegPerSec = 0.0f;

	/** 몸과 카메라의 Yaw 차 허용치 (도). 경계에 가까울수록 카메라 감도가 줄어듦. 0 이하면 제한 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	float MaxBodyCameraYawOffsetDeg = 0.0f;

	/** 상반신과 카메라의 Pitch 차 허용치 (도). 0 이하면 제한 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	float MaxBodyCameraPitchOffsetDeg = 0.0f;

	/** 카메라 회전 감도 배율 (1: 평소와 동일) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CameraSensitivityScale = 1.0f;

	bool LimitsYaw() const { return MaxYawRateDegPerSec > 0.0f; }
};

/** 공격 단계별 이동 제한. 속도 배율은 몸 정면 기준 입력 방향으로 섞임 (1: 평소와 동일) */
USTRUCT(BlueprintType)
struct FWaxMovementProfile
{
	GENERATED_BODY()

	/** 앞으로 이동할 때 속도 배율. 낮출수록 공격 중 전진이 느려짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ForwardSpeedScale = 1.0f;

	/** 옆으로 이동할 때 속도 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StrafeSpeedScale = 1.0f;

	/** 뒤로 이동할 때 속도 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BackwardSpeedScale = 1.0f;

	/** 이 단계에서 점프 허용 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bAllowJump = true;

	/** 몸 기준 입력 방향(로컬 X: 앞, Y: 옆, 정규화)에 대한 속도 배율 */
	float ResolveSpeedScale(float LocalX, float LocalY) const
	{
		const float X2 = LocalX * LocalX;
		const float Y2 = LocalY * LocalY;
		const float Sum = X2 + Y2;
		if (Sum <= KINDA_SMALL_NUMBER)
		{
			return 1.0f;
		}
		const float AxisScale = LocalX >= 0.0f ? ForwardSpeedScale : BackwardSpeedScale;
		return (AxisScale * X2 + StrafeSpeedScale * Y2) / Sum;
	}
};

/** 공격 1종의 튜닝 데이터 */
USTRUCT(BlueprintType)
struct FWaxAttackSpec
{
	GENERATED_BODY()

	/** 콤보 순서대로 순환 재생할 몽타주 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	TArray<TObjectPtr<UAnimMontage>> Montages;

	/** 몽타주 재생 속도 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.1"))
	float PlayRate = 1.0f;

	/** 기본 Strain에 곱할 배율 (최종 = 기본 × Spec × Notify) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
	float StrainScale = 1.0f;

	/** 기본 반경에 곱할 배율 (최종 = 기본 × Spec × Notify) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.1"))
	float RadiusScale = 1.0f;

	/** 타격 본 위치에서 공격 방향으로 밀어낼 판정 중심 오프셋 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
	float ForwardOffset = 18.0f;

	/** 몬스터 수평 넉백 속도 (cm/s). 0이면 넉백 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0.0"))
	float KnockbackHorizontal = 0.0f;

	/** 몬스터 수직 넉백 속도 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0.0"))
	float KnockbackVertical = 0.0f;

	/** 넉백 동안 몬스터 이동을 멈출 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0.0"))
	float KnockbackDurationSec = 0.0f;

	/** 주변 파편에 줄 속도 (cm/s). 0이면 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0.0"))
	float DebrisImpulse = 0.0f;

	/** 파편에 더할 위쪽 속도 (cm/s). 클수록 파편이 떠오름 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0.0"))
	float DebrisImpulseUpward = 0.0f;

	/** 파편 임펄스 반경 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0.0"))
	float DebrisImpulseRadius = 0.0f;

	/** 공격 동안 1인칭 카메라의 머리 추종을 끔 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bSuppressHeadFollow = false;

	/** 선딜(Windup) 회전 제한 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FWaxRotationProfile WindupRotation;

	/** 타격(Active) 회전 제한 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FWaxRotationProfile ActiveRotation;

	/** 후딜(Recovery) 회전 제한 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FWaxRotationProfile RecoveryRotation;

	/** 선딜(Windup) 이동 제한 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FWaxMovementProfile WindupMovement;

	/** 타격(Active) 이동 제한 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FWaxMovementProfile ActiveMovement;

	/** 후딜(Recovery) 이동 제한 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FWaxMovementProfile RecoveryMovement;

	const FWaxRotationProfile& GetPhaseRotation(EWaxAttackPhase Phase) const
	{
		switch (Phase)
		{
		case EWaxAttackPhase::Active: return ActiveRotation;
		case EWaxAttackPhase::Recovery: return RecoveryRotation;
		default: return WindupRotation;
		}
	}

	const FWaxMovementProfile& GetPhaseMovement(EWaxAttackPhase Phase) const
	{
		switch (Phase)
		{
		case EWaxAttackPhase::Active: return ActiveMovement;
		case EWaxAttackPhase::Recovery: return RecoveryMovement;
		default: return WindupMovement;
		}
	}
};

/** 점프공격(공중 탭 = 강타, 공중 홀드 = 슬램). 무기 DA와 공유 */
USTRUCT(BlueprintType)
struct FWaxJumpAttackSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpAttack")
	FWaxAttackSpec AerialStrike;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpAttack")
	FWaxAttackSpec Slam;

	/** 이 시간 이상 누르면 슬램 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpAttack", meta = (ClampMin = "0.05"))
	float HoldThresholdSec = 0.2f;

	/** 슬램 낙하 속도 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpAttack", meta = (ClampMin = "0.0"))
	float SlamDownwardSpeed = 2200.0f;

	/** 지면까지 이 높이 미만이면 슬램 대신 공중 강타 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpAttack", meta = (ClampMin = "0.0"))
	float SlamMinHeight = 120.0f;

	/** 슬램 낙하 중 공중 제어 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpAttack", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlamAirControl = 0.0f;

	/** 착지 시 점프할 몽타주 섹션 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JumpAttack")
	FName SlamLandSectionName = FName(TEXT("Land"));
};

/** 공격 공통 설정 */
USTRUCT(BlueprintType)
struct FWaxCombatSettings
{
	GENERATED_BODY()

	/** 기본 타격 반경 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "5.0"))
	float BaseImpactRadius = 75.0f;

	/** 기본 파괴 충격량 (Chaos Strain) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "1000.0"))
	float BaseImpactStrain = 320000.0f;

	/** 입력 버퍼 유지 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.0"))
	float BufferWindowSec = 0.2f;

	/** 이 시간 동안 공격이 없으면 콤보를 1타로 되돌림 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.0"))
	float ComboResetSec = 1.0f;

	/** 카메라 감도 배율 전환 속도 (1/초). 0이면 즉시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation", meta = (ClampMin = "0.0"))
	float SensitivityBlendSpeed = 20.0f;

	/** 공격 중 이동 속도 배율 전환 속도 (1/초). 0이면 즉시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float MoveScaleBlendSpeed = 15.0f;

	/** 공격 종료 후 몸을 시선 방향으로 되돌리는 속도 (도/초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation", meta = (ClampMin = "1.0"))
	float RecoverYawRateDegPerSec = 720.0f;

	/** 이 각도 이내로 정렬되면 시선 추종 복귀 (도) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation", meta = (ClampMin = "0.0"))
	float RecoverSnapToleranceDeg = 1.0f;

	/** 몸-카메라 각도 차 비율(0~1) → 감도 배율. 비어 있으면 선형(1 - t) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	TObjectPtr<UCurveFloat> OffsetFalloffCurve;

	/** 타격 마커·디버그 구 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker")
	FLinearColor MarkerColor = FLinearColor(1.0f, 0.01f, 0.01f, 1.0f);

	/** 마커 유지 시간 (초). 늘리면 오래 남음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker", meta = (ClampMin = "0.05"))
	float MarkerDurationSec = 0.35f;

	/** 마커 발광 배율. 높일수록 밝음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker", meta = (ClampMin = "0.0"))
	float MarkerEmissiveBoost = 8.0f;

	/** 마커 불투명도 (0~1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MarkerOpacity = 1.0f;

	/** 마커 외곽 번짐 정도. 클수록 가장자리가 부드러움 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker", meta = (ClampMin = "0.1"))
	float MarkerEdgeSoftness = 0.45f;
};
