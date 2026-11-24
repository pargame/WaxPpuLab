#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Chaos/ChaosGameplayEventDispatcher.h"
#include "WaxMonsterBase.generated.h"

class AWaxMonsterCore;
class UCullingField;
class UGeometryCollectionComponent;
class URadialFalloff;
class USceneComponent;
class UStaticMeshComponent;
class UUniformInteger;
class UUniformScalar;
class UUniformVector;
class UWaxDestructibleComponent;
class UWaxLocomotionComponent;
class UWaxMonsterDataAsset;

UENUM(BlueprintType)
enum class EWaxMonsterState : uint8
{
	Active,
	Collapsed
};

UCLASS()
class WAXPPULAB_API AWaxMonsterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AWaxMonsterBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "WaxMonster")
	void InitializeFromData(UWaxMonsterDataAsset* InData);

	UFUNCTION(BlueprintCallable, Category = "WaxMonster|Eyes")
	void ApplyEyeImpact(const FVector& WorldImpactPoint, float BaseStrain, float Radius);

	UFUNCTION(BlueprintPure, Category = "WaxMonster|Eyes")
	int32 GetIntactEyeCount() const;

	UFUNCTION(BlueprintPure, Category = "WaxMonster")
	EWaxMonsterState GetMonsterState() const { return MonsterState; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WaxMonster")
	TObjectPtr<USceneComponent> BodyPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WaxMonster")
	TObjectPtr<UGeometryCollectionComponent> ShellCollection;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WaxMonster")
	TObjectPtr<UStaticMeshComponent> CoreMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WaxMonster")
	TObjectPtr<UGeometryCollectionComponent> EyeLeftGC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WaxMonster")
	TObjectPtr<UGeometryCollectionComponent> EyeRightGC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WaxMonster")
	TObjectPtr<UWaxDestructibleComponent> WaxDestructible;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxMonster")
	TObjectPtr<UWaxLocomotionComponent> ActiveLocomotion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxMonster")
	TObjectPtr<UWaxMonsterDataAsset> MonsterData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxMonster|Movement")
	bool bChasePlayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxMonster|Movement", meta = (ClampMin = "0.0"))
	float StopDistanceToPlayer = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxMonster|HitStop")
	bool bEnableHitSlowdown = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxMonster|HitStop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HitSlowdownDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxMonster|HitStop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HitSlowdownSpeedMultiplier = 0.30f;

protected:
	virtual void UpdateMonsterMovement(float DeltaTime);

	UFUNCTION(BlueprintImplementableEvent, Category = "WaxMonster|Events")
	void BP_OnWaxCollapsed(AWaxMonsterCore* SpawnedCore);

	UFUNCTION(BlueprintImplementableEvent, Category = "WaxMonster|Events")
	void BP_OnEyeShattered(FVector WorldLocation);

private:
	UFUNCTION()
	void HandleWaxImpactApplied(FVector WorldImpactPoint);

	UFUNCTION()
	void HandleWaxCollapsed(AActor* CollapsedActor);

	UFUNCTION()
	void HandleLeftEyeBreak(const FChaosBreakEvent& BreakEvent);

	UFUNCTION()
	void HandleRightEyeBreak(const FChaosBreakEvent& BreakEvent);

	void EnsureEyeFieldNodes();
	void EnsureLocomotionComponent();
	void ConfigureEyeCollection(UGeometryCollectionComponent* EyeGC, const FVector& RelativeLoc);
	void ApplyImpactToSingleEye(UGeometryCollectionComponent* EyeGC, bool& bBrokenFlag, float& AccumulatedDamage, const FVector& WorldImpactPoint, float BaseStrain, float Radius);
	void DropEyeWithChaos(UGeometryCollectionComponent* EyeGC, bool bAlreadyBroken, const FVector& PopVelocity);
	void ShatterRemainingEye(UGeometryCollectionComponent* EyeGC, bool& bBrokenFlag);
	void NotifyEyeBroken(bool& bBrokenFlag, const FVector& WorldLocation);

	UPROPERTY(Transient)
	TObjectPtr<URadialFalloff> EyeMaskFalloff = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URadialFalloff> EyeStrainFalloff = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUniformInteger> EyeDynamicStateField = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCullingField> EyeLocalDynamicField = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUniformScalar> EyeUniformStrainField = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUniformVector> EyeVelocityField = nullptr;

	EWaxMonsterState MonsterState = EWaxMonsterState::Active;
	FTimerHandle EyeTimeoutHandle;
	float HitSlowdownRemaining = 0.0f;
	float LeftEyeDamage = 0.0f;
	float RightEyeDamage = 0.0f;
	bool bLeftEyeBroken = false;
	bool bRightEyeBroken = false;
};
