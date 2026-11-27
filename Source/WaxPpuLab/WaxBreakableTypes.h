#pragma once

#include "CoreMinimal.h"
#include "WaxBreakableTypes.generated.h"

class USoundBase;

/** 부위가 깨질 때의 처리 규칙 */
UENUM(BlueprintType)
enum class EWaxBreakMode : uint8
{
	/** 깨진 조각만 떨어지고 남은 가장 큰 덩어리는 몸에 계속 붙어 있음 (껍질) */
	ChipAndRetainBody,
	/** 처음 깨지는 순간 부위 전체가 몸에서 탈락해 떨어지고 수명이 지나면 사라짐 (눈) */
	DetachWholeOnBreak
};

/** 깨지는 부위(GeometryCollection) 하나의 튜닝 값. 껍질·눈 등 모든 부위가 같은 구조를 씀 */
USTRUCT(BlueprintType)
struct FWaxBreakablePartSettings
{
	GENERATED_BODY()

	/** 깨질 때 처리 규칙 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	EWaxBreakMode BreakMode = EWaxBreakMode::ChipAndRetainBody;

	/** 깨지기 시작하기까지 필요한 타격 횟수. 키우면 더 단단함 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (ClampMin = "1"))
	int32 HitsToBreak = 1;

	/** 1단계 클러스터 파괴 임계값 (Chaos Strain). 0이면 GC 에셋 값을 그대로 씀 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break", meta = (ClampMin = "0.0"))
	float DamageThreshold = 0.0f;

	/** 2·3단계 클러스터 파괴 임계값 비율 (1단계 대비). 낮추면 한번 금 간 부위가 더 잘게 부서짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Break")
	FVector2D SubLevelThresholdRatios = FVector2D(0.5f, 0.25f);

	/** 타격 판정 여유 반경 (cm). 타격 구와 부위 중심 거리가 타격 반경+이 값보다 멀면 무시. 0이면 거리 검사 안 함 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (ClampMin = "0.0"))
	float HitAcceptRadius = 0.0f;

	/** 이 부위에 적용되는 최소 타격 반경 (cm). 작은 타격도 부위 전체에 퍼지게 하려면 키움 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (ClampMin = "0.0"))
	float MinImpactRadius = 0.0f;

	/** 타격 가장자리의 Strain 비율 (중심 대비). 낮추면 맞은 곳 중심만 깨짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StrainEdgeRatio = 0.85f;

	/** 타격·파괴 후 남은 몸통 덩어리를 몸에 다시 고정하는 검사를 반복하는 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "0.0"))
	float ReanchorWindowSec = 0.15f;

	/** 부위 전체가 탈락할 때 튀어나가는 속도 (cm/s). X=몸 앞, Y=몸 바깥 옆, Z=위 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	FVector DetachPopVelocity = FVector(110.0f, 75.0f, 95.0f);

	/** 탈락한 부위가 사라지기까지 시간 (초). 0이면 사라지지 않음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "0.0"))
	float DetachedLifeSpanSec = 4.0f;

	/** 몸 붕괴로 탈락한 부위가 공중에서 산산조각 나기까지 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "0.0"))
	float DetachShatterDelaySec = 0.55f;

	/** 산산조각 낼 때 Strain 배율 (1단계 임계값 대비) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "1.0"))
	float ShatterStrainScale = 2.5f;

	/** 파편 걷어차기 가장자리 속도 비율 (중심 대비). 낮추면 가장자리 파편이 덜 날아감 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DebrisKickEdgeRatio = 0.25f;

	/** 파편 걷어차기 검사 여유 거리 (cm). 부위 경계+반경 밖이라도 이 거리 안이면 적용 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris", meta = (ClampMin = "0.0"))
	float DebrisKickRangeMargin = 150.0f;

	/** 버티는 타격(아직 안 깨짐)에 재생할 소리. 비우면 무음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> HitSound = nullptr;

	/** 버티는 타격 소리 볼륨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0"))
	float HitSoundVolume = 0.45f;

	/** 버티는 타격 소리 피치 배율 범위 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	FVector2D HitSoundPitchRange = FVector2D(1.45f, 1.7f);

	/** 처음 깨질 때 재생할 소리. 비우면 무음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> BreakSound = nullptr;

	/** 깨짐 소리 볼륨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0"))
	float BreakSoundVolume = 0.85f;

	/** 깨짐 소리 피치 배율 범위 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	FVector2D BreakSoundPitchRange = FVector2D(1.05f, 1.3f);
};
