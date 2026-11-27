#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosNotifyHandlerInterface.h"
#include "WaxBreakablePartComponent.h"
#include "WaxMonsterDataAsset.h"
#include "WaxDestructibleComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWaxZoneDamaged, FName, ZoneName, FVector, WorldHitPoint, float, RemainingDurability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaxCollapsed, AActor*, CollapsedActor);

struct FWaxDestructibleGrain
{
	double FireTime = 0.0;
	FVector Location = FVector::ZeroVector;
	float Intensity = 0.6f;
	float CutoffHz = 10000.0f;
	float DecaySec = 0.12f;
	float PitchOffset = 0.0f;
	FWaxAudioProfile Profile;
};

/**
 * 왁스 껍질 계층: 부위 공통 파괴 규칙 위에 경도 구역, 깨짐·충돌 소리 합성, 남은 비율 기반 전체 붕괴를 더한다.
 * 모든 튜닝 값은 UWaxMonsterDataAsset에서 직접 읽는다.
 */
UCLASS(ClassGroup = (WaxDemo), meta = (BlueprintSpawnableComponent))
class WAXPPULAB_API UWaxDestructibleComponent : public UWaxBreakablePartComponent
{
	GENERATED_BODY()

public:
	UWaxDestructibleComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "WaxDestructible")
	void InitializeWithCollection(UGeometryCollectionComponent* InShellCollection, UWaxMonsterDataAsset* InDataAsset);

	virtual bool ApplyImpact(const FVector& WorldImpactPoint, float Strain, float Radius) override;
	virtual void ApplyDebrisVelocity(const FVector& Origin, const FVector& Velocity, float Radius) override;

	UFUNCTION(BlueprintCallable, Category = "WaxDestructible")
	void TriggerRemainingCollapse();

	UFUNCTION(BlueprintCallable, Category = "WaxDestructible|Audio")
	void SelectDevAudioProfile(int32 ProfileIndex);

	UFUNCTION(BlueprintPure, Category = "WaxDestructible")
	bool HasCollapsed() const { return bHasCollapsed; }

	UPROPERTY(BlueprintAssignable, Category = "WaxDestructible|Events")
	FOnWaxZoneDamaged OnWaxZoneDamaged;

	UPROPERTY(BlueprintAssignable, Category = "WaxDestructible|Events")
	FOnWaxCollapsed OnWaxCollapsed;

	/** 화면에 파괴·충돌 카운터 디버그 표시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Debug")
	bool bShowDebugCounters = true;

protected:
	virtual void OnFragmentBroken(const FChaosBreakEvent& BreakEvent) override;
	virtual bool NeedsTick() const override;

private:
	UFUNCTION()
	void HandleChaosCollisionEvent(const FChaosPhysicsCollisionInfo& CollisionInfo);

	const UWaxMonsterDataAsset& Data() const;
	int32 FindMatchingZoneIndex(const FVector& WorldPoint) const;
	const FWaxAudioProfile& ResolveAudioProfileAt(const FVector& WorldPoint) const;
	void PlayOneShotCrack(const FVector& Location, float Intensity, float CutoffHz, float DecaySec, float PitchOffset, const FWaxAudioProfile& Profile);

	UPROPERTY(Transient)
	TObjectPtr<UWaxMonsterDataAsset> DataAsset;

	TArray<float> ZoneAccumulatedDamage;
	TArray<FWaxDestructibleGrain> PendingGrains;
	TMap<FIntVector, double> CollisionCellNextTime;
	TArray<double> RecentCollisionTimes;

	FWaxAudioProfile ActiveAudioProfile;
	double NextAllowedCollisionSoundTime = 0.0;
	double CollapseTimestamp = -1.0;
	int32 TotalBreakEvents = 0;
	int32 TotalCollisionEvents = 0;
	bool bHasCollapsed = false;
	bool bDevAudioOverride = false;
};
