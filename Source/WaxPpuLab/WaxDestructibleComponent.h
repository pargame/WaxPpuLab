#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chaos/ChaosGameplayEventDispatcher.h"
#include "Chaos/ChaosNotifyHandlerInterface.h"
#include "WaxMonsterDataAsset.h"
#include "WaxDestructibleComponent.generated.h"

class UGeometryCollectionComponent;
class URadialFalloff;
class UUniformInteger;
class UUniformScalar;
class UUniformVector;
class UCullingField;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWaxZoneDamaged, FName, ZoneName, FVector, WorldHitPoint, float, RemainingDurability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaxImpactApplied, FVector, WorldImpactPoint);
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

UCLASS(ClassGroup = (WaxDemo), meta = (BlueprintSpawnableComponent))
class WAXPPULAB_API UWaxDestructibleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWaxDestructibleComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "WaxDestructible")
	void InitializeWithCollection(UGeometryCollectionComponent* InShellCollection, UWaxMonsterDataAsset* InDataAsset);

	UFUNCTION(BlueprintCallable, Category = "WaxDestructible")
	void ApplyWaxImpact(const FVector& WorldImpactPoint, float BaseStrain, float Radius);

	UFUNCTION(BlueprintCallable, Category = "WaxDestructible")
	void ApplyDebrisKick(const FVector& KickOrigin, const FVector& KickVelocity, float Radius);

	UFUNCTION(BlueprintCallable, Category = "WaxDestructible")
	void TriggerRemainingCollapse();

	UFUNCTION(BlueprintCallable, Category = "WaxDestructible|Audio")
	void SelectDevAudioProfile(int32 ProfileIndex);

	UFUNCTION(BlueprintPure, Category = "WaxDestructible")
	bool HasBrokenPieces() const { return BrokenLeafIndices.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "WaxDestructible")
	bool HasCollapsed() const { return bHasCollapsed; }

	UFUNCTION(BlueprintPure, Category = "WaxDestructible")
	float GetRemainingRatio() const;

	UPROPERTY(BlueprintAssignable, Category = "WaxDestructible|Events")
	FOnWaxZoneDamaged OnWaxZoneDamaged;

	UPROPERTY(BlueprintAssignable, Category = "WaxDestructible|Events")
	FOnWaxImpactApplied OnWaxImpactApplied;

	UPROPERTY(BlueprintAssignable, Category = "WaxDestructible|Events")
	FOnWaxCollapsed OnWaxCollapsed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Audio")
	float BreakGrainSpreadMinSec = 0.012f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Audio")
	float BreakGrainSpreadMaxSec = 0.085f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Audio")
	int32 MaxQueuedGrains = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Audio", meta = (ClampMin = "0.05"))
	float CollapseGrainSpreadMaxSec = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Audio", meta = (ClampMin = "4"))
	int32 CollapseMaxQueuedGrains = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Audio")
	float CollisionSoundSpeedThreshold = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Audio")
	float MinCollisionAudioInterval = 0.020f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Audio", meta = (ClampMin = "2.0"))
	float CollisionSpatialCellSize = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Audio", meta = (ClampMin = "0.01"))
	float CollisionSpatialCooldownSec = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CollisionCrackleScale = 1.00f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CollisionSnapScale = 1.00f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CollisionBodyScale = 1.00f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Collapse")
	bool bAllowKickAfterCollapse = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Collapse")
	bool bMuteCollisionAudioAfterCollapse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Collapse", meta = (ClampMin = "0.0"))
	float CollapseCollisionMaxGraceSec = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxDestructible|Debug")
	bool bShowDebugCounters = true;

private:
	UFUNCTION()
	void HandleChaosBreakEvent(const FChaosBreakEvent& BreakEvent);

	UFUNCTION()
	void HandleChaosCollisionEvent(const FChaosPhysicsCollisionInfo& CollisionInfo);

	void BuildLeafHierarchyCache();
	void EnsureFieldNodes();
	int32 FindMatchingZoneIndex(const FVector& WorldPoint) const;
	const FWaxAudioProfile& ResolveAudioProfileAt(const FVector& WorldPoint) const;
	void PlayOneShotCrack(const FVector& Location, float Intensity, float CutoffHz, float DecaySec, float PitchOffset, const FWaxAudioProfile& Profile);
	void RefreshTickEnabled();

	UPROPERTY(Transient)
	TObjectPtr<UGeometryCollectionComponent> ShellCollection;

	UPROPERTY(Transient)
	TObjectPtr<UWaxMonsterDataAsset> DataAsset;

	UPROPERTY(Transient)
	TObjectPtr<URadialFalloff> ImpactMaskFalloff;

	UPROPERTY(Transient)
	TObjectPtr<URadialFalloff> ImpactStrainFalloff;

	UPROPERTY(Transient)
	TObjectPtr<UUniformInteger> ImpactDynamicState;

	UPROPERTY(Transient)
	TObjectPtr<UCullingField> ImpactLocalDynamic;

	UPROPERTY(Transient)
	TObjectPtr<URadialFalloff> KickMaskFalloff;

	UPROPERTY(Transient)
	TObjectPtr<UUniformVector> KickUniformVel;

	UPROPERTY(Transient)
	TObjectPtr<UCullingField> KickLocalVel;

	UPROPERTY(Transient)
	TObjectPtr<UUniformInteger> CollapseDynamicState;

	UPROPERTY(Transient)
	TObjectPtr<UUniformScalar> CollapseUniformStrain;

	TSet<int32> LeafNodeIndices;
	TSet<int32> BrokenLeafIndices;
	TArray<float> ZoneAccumulatedDamage;
	TArray<FWaxDestructibleGrain> PendingGrains;
	TMap<FIntVector, double> CollisionCellNextTime;
	TArray<double> RecentCollisionTimes;

	FWaxAudioProfile ActiveAudioProfile;
	double NextAllowedCollisionSoundTime = 0.0;
	double CollapseTimestamp = -1.0;
	int32 TotalLeafCount = 0;
	int32 TotalBreakEvents = 0;
	int32 TotalCollisionEvents = 0;
	bool bHasCollapsed = false;
	bool bDevAudioOverride = false;
};
