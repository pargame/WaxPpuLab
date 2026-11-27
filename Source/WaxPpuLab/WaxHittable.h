#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WaxHittable.generated.h"

/** 모든 공격이 피격 대상에 전달하는 단일 데이터. 공격 스펙(DA)에서 한 번 계산됨 */
USTRUCT(BlueprintType)
struct FWaxHitPayload
{
	GENERATED_BODY()

	/** 공격자 위치 (넉백 방향 기준) */
	UPROPERTY(BlueprintReadWrite, Category = "WaxHit")
	FVector Origin = FVector::ZeroVector;

	/** 타격 구 중심 */
	UPROPERTY(BlueprintReadWrite, Category = "WaxHit")
	FVector Center = FVector::ZeroVector;

	/** 공격 조준 방향 (공격자와 겹칠 때 넉백 방향 대체) */
	UPROPERTY(BlueprintReadWrite, Category = "WaxHit")
	FVector AimDirection = FVector::ForwardVector;

	/** 타격 반경 (cm) */
	UPROPERTY(BlueprintReadWrite, Category = "WaxHit")
	float Radius = 0.0f;

	/** 파괴 충격량 (Chaos Strain) */
	UPROPERTY(BlueprintReadWrite, Category = "WaxHit")
	float Strain = 0.0f;

	/** 넉백 수평 속도 (cm/s) */
	UPROPERTY(BlueprintReadWrite, Category = "WaxHit")
	float KnockbackHorizontal = 0.0f;

	/** 넉백 수직 속도 (cm/s) */
	UPROPERTY(BlueprintReadWrite, Category = "WaxHit")
	float KnockbackVertical = 0.0f;

	/** 넉백 동안 대상 이동 AI를 멈추는 시간 (초) */
	UPROPERTY(BlueprintReadWrite, Category = "WaxHit")
	float KnockbackDurationSec = 0.0f;

	bool HasKnockback() const { return KnockbackHorizontal > 0.0f || KnockbackVertical > 0.0f; }

	/** 공격자→대상 수평 방향으로 계산한 대상별 넉백 속도. 여러 대상을 맞혀도 방사형으로 밀림 */
	FVector ComputeKnockbackVelocity(const FVector& TargetLocation) const
	{
		FVector Dir = (TargetLocation - Origin).GetSafeNormal2D();
		if (Dir.IsNearlyZero())
		{
			Dir = AimDirection.GetSafeNormal2D();
		}
		return Dir * KnockbackHorizontal + FVector::UpVector * KnockbackVertical;
	}
};

UINTERFACE(MinimalAPI, BlueprintType)
class UWaxHittable : public UInterface
{
	GENERATED_BODY()
};

/** 공격을 받는 모든 대상(몬스터 액터, 파괴 부위 컴포넌트)의 공통 인터페이스 */
class WAXPPULAB_API IWaxHittable
{
	GENERATED_BODY()

public:
	/** 타격 적용. 실제로 영향을 받았으면 true. 호출은 IWaxHittable::Execute_ReceiveHit 사용 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WaxHit")
	bool ReceiveHit(const FWaxHitPayload& Payload);
};
