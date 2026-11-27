#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WaxHittable.h"
#include "WaxMonsterBase.generated.h"

class AWaxMonsterCore;
class UGeometryCollectionComponent;
class USceneComponent;
class UStaticMeshComponent;
class UWaxBreakablePartComponent;
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
class WAXPPULAB_API AWaxMonsterBase : public ACharacter, public IWaxHittable
{
	GENERATED_BODY()

public:
	AWaxMonsterBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "WaxMonster")
	void InitializeFromData(UWaxMonsterDataAsset* InData);

	/** 몬스터 피격 단일 진입점: 눈 → 껍질 → 넉백 순서로 처리 */
	virtual bool ReceiveHit_Implementation(const FWaxHitPayload& Payload) override;

	UFUNCTION(BlueprintPure, Category = "WaxMonster|Eyes")
	int32 GetIntactEyeCount() const;

	/** 몬스터 튜닝 값의 단일 출처. 데이터 에셋이 없으면 클래스 기본값 */
	const UWaxMonsterDataAsset& GetMonsterData() const;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WaxMonster")
	TObjectPtr<UWaxBreakablePartComponent> EyeLeftPart;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WaxMonster")
	TObjectPtr<UWaxBreakablePartComponent> EyeRightPart;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxMonster")
	TObjectPtr<UWaxLocomotionComponent> ActiveLocomotion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxMonster")
	TObjectPtr<UWaxMonsterDataAsset> MonsterData;

protected:
	virtual void UpdateMonsterMovement(float DeltaTime);

	UFUNCTION(BlueprintImplementableEvent, Category = "WaxMonster|Events")
	void BP_OnWaxCollapsed(AWaxMonsterCore* SpawnedCore);

	UFUNCTION(BlueprintImplementableEvent, Category = "WaxMonster|Events")
	void BP_OnEyeShattered(FVector WorldLocation);

private:
	UFUNCTION()
	void HandleWaxCollapsed(AActor* CollapsedActor);

	UFUNCTION()
	void HandleEyeBroken(UWaxBreakablePartComponent* Part, FVector WorldLocation);

	void EnsureLocomotionComponent();
	void ConfigureEye(UGeometryCollectionComponent* EyeGC, UWaxBreakablePartComponent* EyePart, float LateralSign);
	void StartHitSlowdown();
	void ApplyKnockbackNow(const FVector& Velocity, float DurationSec);

	EWaxMonsterState MonsterState = EWaxMonsterState::Active;
	FTimerHandle EyeTimeoutHandle;
	float HitSlowdownRemaining = 0.0f;
	FVector PendingKnockbackVelocity = FVector::ZeroVector;
	float PendingKnockbackDuration = 0.0f;
	uint64 PendingKnockbackFrame = 0;
	bool bHasPendingKnockback = false;
};
