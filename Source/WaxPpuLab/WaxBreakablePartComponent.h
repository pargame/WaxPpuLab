#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chaos/ChaosGameplayEventDispatcher.h"
#include "WaxBreakableTypes.h"
#include "WaxHittable.h"
#include "WaxBreakablePartComponent.generated.h"

class UGeometryCollection;
class UGeometryCollectionComponent;
class UCullingField;
class URadialFalloff;
class UUniformInteger;
class UUniformScalar;
class UUniformVector;
class UWaxBreakablePartComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaxImpactApplied, FVector, WorldImpactPoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWaxPartBroken, UWaxBreakablePartComponent*, Part, FVector, WorldLocation);

/**
 * GeometryCollection 부위 하나의 파괴 물리를 담당하는 공통 계층.
 * 껍질·눈 등 모든 부위가 같은 규칙을 공유한다.
 * 타격은 Strain만 가하고, Kinematic(몸에 붙음)은 남은 본체 하나에만 허용하며 나머지 조각은 모두 Dynamic으로 떨어뜨린다.
 * 떨어진 조각은 엔진 RemoveOnBreak(축소 제거)로 조각마다 수명 후 줄어들어 사라지며, 그 시간 값은 부위 설정(DA)이 GC 동적 속성에 덮어쓴다.
 */
UCLASS(ClassGroup = (WaxDemo), meta = (BlueprintSpawnableComponent))
class WAXPPULAB_API UWaxBreakablePartComponent : public UActorComponent, public IWaxHittable
{
	GENERATED_BODY()

public:
	UWaxBreakablePartComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 부위 초기화. RestAsset 지정 전에 파괴 임계값을 적용함. Settings는 데이터 에셋 안의 구조체를 직접 가리킴(복사하지 않음). LateralSign은 몸 기준 바깥 옆 방향(-1 왼쪽, +1 오른쪽) */
	void InitializePart(UGeometryCollectionComponent* InCollection, UGeometryCollection* RestAsset, const FWaxBreakablePartSettings* InSettings, UObject* InSettingsOwner, float InLateralSign = 0.0f);

	virtual bool ReceiveWaxHit_Implementation(const FWaxHitPayload& Payload) override;

	/** 타격 구로 부위를 깸. 실제로 영향을 받았으면 true */
	virtual bool ApplyImpact(const FVector& WorldImpactPoint, float Strain, float Radius);

	/** 이미 깨진 조각에 속도를 줌 */
	virtual void ApplyDebrisVelocity(const FVector& Origin, const FVector& Velocity, float Radius);

	/** 반경 안 모든 부위의 깨진 조각에 속도를 줌. 영향받은 부위 수 반환 */
	static int32 KickFragmentsInRadius(UWorld* World, const FVector& Origin, const FVector& Velocity, float Radius, const AActor* IgnoreActor);

	/** 부위 전체를 몸에서 떼어 냄 (DetachWholeOnBreak 탈락, 몸 붕괴 시 사용). 이후 조각은 일반 수명 규칙으로 사라짐 */
	void DetachWhole(const FVector& WorldPopVelocity);

	/** 몸 앞 방향 기준 탈락 속도 계산 (설정의 DetachPopVelocity 사용) */
	FVector ComputeDetachPopVelocity(const FVector& BodyForward) const;

	/** 아직 안 깨진 부위를 한 번에 산산조각 냄 */
	void ShatterWhole();

	/** 몸 부착을 모두 풀고 남은 덩어리를 전부 무너뜨림 (몸 전체 붕괴) */
	void ReleaseAll(float ReleaseStrain);

	UFUNCTION(BlueprintPure, Category = "WaxBreakable")
	bool IsBroken() const { return bBroken; }

	UFUNCTION(BlueprintPure, Category = "WaxBreakable")
	bool IsDetached() const { return bDetached; }

	UFUNCTION(BlueprintPure, Category = "WaxBreakable")
	bool HasBrokenPieces() const { return BrokenLeafIndices.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "WaxBreakable")
	float GetRemainingRatio() const;

	bool IsReleased() const { return bReleased; }
	UGeometryCollectionComponent* GetCollection() const { return Collection; }
	const FWaxBreakablePartSettings& GetPartSettings() const;

	/** 타격이 이 부위에 적용될 때마다 (버틴 타격 포함) */
	UPROPERTY(BlueprintAssignable, Category = "WaxBreakable|Events")
	FOnWaxImpactApplied OnWaxImpactApplied;

	/** 부위가 처음 깨질 때 */
	UPROPERTY(BlueprintAssignable, Category = "WaxBreakable|Events")
	FOnWaxPartBroken OnPartBroken;

protected:
	/** 하위 계층 훅: 깨짐 이벤트마다 (조각 단위) */
	virtual void OnFragmentBroken(const FChaosBreakEvent& BreakEvent) {}

	void EnsureFieldNodes();
	void RefreshTickEnabled();
	virtual bool NeedsTick() const;
	float GetNow() const;

	UPROPERTY(Transient)
	TObjectPtr<UGeometryCollectionComponent> Collection;

	TSet<int32> LeafNodeIndices;
	TSet<int32> BrokenLeafIndices;

private:
	UFUNCTION()
	void HandleChaosBreakEvent(const FChaosBreakEvent& BreakEvent);

	void BuildLeafCache();
	void ApplyStrain(const FVector& WorldImpactPoint, float Strain, float Radius);
	bool NeedsAnchorRule() const;
	void ExtendLooseRule();
	void EnforceAnchorRule();
	void SyncFragmentRemovalTimers();
	void ApplyLooseFragmentProfile();
	void PlayPartSound(USoundBase* Sound, const FVector& Location, float Volume, const FVector2D& PitchRange) const;

	UPROPERTY(Transient)
	TObjectPtr<UObject> SettingsOwner;

	const FWaxBreakablePartSettings* Settings = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URadialFalloff> MaskFalloff;

	UPROPERTY(Transient)
	TObjectPtr<URadialFalloff> StrainFalloff;

	UPROPERTY(Transient)
	TObjectPtr<UUniformInteger> StateValue;

	UPROPERTY(Transient)
	TObjectPtr<UUniformScalar> UniformStrain;

	UPROPERTY(Transient)
	TObjectPtr<UUniformVector> UniformVelocity;

	UPROPERTY(Transient)
	TObjectPtr<UCullingField> LocalVelocity;

	/** 떼어 냄·전체 붕괴 이후 Kinematic 정리 규칙을 계속 적용할 마감 시각 (모든 조각 수명이 끝날 때까지) */
	float LooseRuleUntil = -1.0f;
	int32 RemovalSeed = 0;
	/** 떨어진 조각 충돌 프로필을 적용한 조각 수 (새 조각이 생겼을 때만 다시 적용) */
	int32 LooseProfileAppliedCount = 0;
	bool bWarnedMissingLooseProfile = false;
	float LateralSign = 0.0f;
	int32 HitCount = 0;
	bool bBroken = false;
	bool bDetached = false;
	bool bReleased = false;
};
