#pragma once

#include "CoreMinimal.h"
#include "Engine/CollisionProfile.h"
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

	/** 이 부위를 받치는 다른 부위(눈 → 껍질)가 깨져 사라지면 함께 통째로 탈락할지 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Support")
	bool bDetachWhenSupportLost = true;

	/** 받침으로 볼 범위 (cm, 이 부위 중심 기준). 이 안에 몸에 붙은 받침 조각이 남아 있는지 검사. 키우면 더 넓게 다 깨져야 탈락 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Support", meta = (ClampMin = "0.0", EditCondition = "bDetachWhenSupportLost"))
	float SupportRadius = 40.0f;

	/** 범위 안 받침 조각이 이 개수 이하로 남으면 탈락. 0이면 하나도 안 남았을 때만, 키우면 더 일찍 탈락 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Support", meta = (ClampMin = "0", EditCondition = "bDetachWhenSupportLost"))
	int32 SupportMinAttachedCount = 0;

	/** 받침 부위가 마지막으로 깨진 뒤 받침 검사를 계속하는 시간 (초). 깨진 조각이 몸에서 풀려나는 데 몇 프레임 걸리므로 그보다 길게 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Support", meta = (ClampMin = "0.1", EditCondition = "bDetachWhenSupportLost"))
	float SupportCheckWindowSec = 1.0f;

	/** 타격 판정 여유 반경 (cm). 타격 구와 부위 중심 거리가 타격 반경+이 값보다 멀면 무시. 0이면 거리 검사 안 함 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (ClampMin = "0.0"))
	float HitAcceptRadius = 0.0f;

	/** 이 부위에 적용되는 최소 타격 반경 (cm). 작은 타격도 부위 전체에 퍼지게 하려면 키움 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (ClampMin = "0.0"))
	float MinImpactRadius = 0.0f;

	/** 타격 가장자리의 Strain 비율 (중심 대비). 낮추면 맞은 곳 중심만 깨짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StrainEdgeRatio = 0.85f;

	/** 떨어져 나간 조각이 그대로 유지되는 시간 범위 (초, 깨진 순간부터). 지나면 축소 시작. 조각마다 범위 안에서 무작위 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	FVector2D FragmentLifeSecRange = FVector2D(1.5f, 2.0f);

	/** 조각이 줄어들어 사라지는 데 걸리는 시간 범위 (초). 키우면 천천히 줄어듦. 0이면 즉시 사라짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	FVector2D FragmentShrinkSecRange = FVector2D(0.35f, 0.45f);

	/** 몸에서 떨어진 덩어리(여러 조각이 붙은 채)가 낱개 조각으로 흩어지기까지 시간 (초). 0이면 바로 흩어져 조각마다 따로 수명이 흐름 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle", meta = (ClampMin = "0.0"))
	float ClusterCrumbleDelaySec = 0.0f;

	/** 부위 전체가 탈락할 때 튀어나가는 속도 (cm/s). X=몸 앞, Y=몸 바깥 옆, Z=위 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lifecycle")
	FVector DetachPopVelocity = FVector(110.0f, 75.0f, 95.0f);

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

	/** 떨어진 조각의 최대 이동 속도 (cm/s). 발차기·몬스터 몸에 밀려 튀는 속도를 이 값에서 자름. 낮추면 덜 날아감, 0이면 제한 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float FragmentMaxLinearSpeed = 1000.0f;

	/** 떨어진 조각의 최대 회전 속도 (도/s). 낮추면 덜 팽이처럼 돌고 덜 굴러감, 0이면 제한 없음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris", meta = (ClampMin = "0.0", Units = "DegreesPerSecond"))
	float FragmentMaxAngularSpeedDeg = 1440.0f;

	/** 떨어진 조각에 적용할 충돌 프로필. 기본 WaxLooseFragment는 캐릭터(Pawn)와 Visibility 트레이스(발 IK·공격 판정)를 무시하고 조각끼리·바닥·벽과만 부딪힘. None이면 부위 컴포넌트 설정 그대로 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debris")
	FCollisionProfileName LooseFragmentCollisionProfile = FCollisionProfileName(TEXT("WaxLooseFragment"));

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
