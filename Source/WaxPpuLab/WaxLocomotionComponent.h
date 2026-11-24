#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WaxLocomotionComponent.generated.h"

class AWaxMonsterBase;
class USceneComponent;
class UWaxMonsterDataAsset;

UCLASS(ClassGroup = (WaxMonster), Blueprintable, meta = (BlueprintSpawnableComponent))
class WAXPPULAB_API UWaxLocomotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWaxLocomotionComponent();

	virtual void InitializeLocomotion(AWaxMonsterBase* InMonster, USceneComponent* InBodyPivot, UWaxMonsterDataAsset* InData);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "WaxLocomotion")
	void SetDesiredMoveDirection(const FVector& InWorldDirection);

	UFUNCTION(BlueprintCallable, Category = "WaxLocomotion")
	void SetSpeedMultiplier(float InMultiplier);

	UFUNCTION(BlueprintCallable, Category = "WaxLocomotion")
	virtual void StopLocomotion();

protected:
	virtual bool ShouldApplyDefaultMovement() const { return true; }
	void ApplyEffectiveMoveSpeed();

	UPROPERTY(Transient)
	TObjectPtr<AWaxMonsterBase> OwnerMonster = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> BodyPivot = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWaxMonsterDataAsset> MonsterData = nullptr;

	FVector DesiredMoveDirection = FVector::ZeroVector;
	float BaseMoveSpeed = 160.0f;
	float SpeedMultiplier = 1.0f;
	bool bStopped = false;
};

UCLASS(ClassGroup = (WaxMonster), Blueprintable, meta = (BlueprintSpawnableComponent))
class WAXPPULAB_API UWaxRollingLocomotionComponent : public UWaxLocomotionComponent
{
	GENERATED_BODY()

public:
	virtual void InitializeLocomotion(AWaxMonsterBase* InMonster, USceneComponent* InBodyPivot, UWaxMonsterDataAsset* InData) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void StopLocomotion() override;

private:
	FVector PrevActorLocation = FVector::ZeroVector;
	FVector CurrentAngularVelocity = FVector::ZeroVector;
	float RollRadius = 92.5f;
	float LandingBlendRemaining = 0.0f;
	bool bHasPrevLocation = false;
	bool bWasFalling = false;
};

UENUM(BlueprintType)
enum class EWaxButterLocoState : uint8
{
	HopAlign,
	LayDown,
	TumbleCharge,
	StandUp
};

UCLASS(ClassGroup = (WaxMonster), Blueprintable, meta = (BlueprintSpawnableComponent))
class WAXPPULAB_API UWaxButterLocomotionComponent : public UWaxLocomotionComponent
{
	GENERATED_BODY()

public:
	virtual void InitializeLocomotion(AWaxMonsterBase* InMonster, USceneComponent* InBodyPivot, UWaxMonsterDataAsset* InData) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void StopLocomotion() override;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float HalfX = 55.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float HalfY = 75.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float HalfZ = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float MinChargeStartDistance = 340.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float HopDuration = 0.28f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float HopPauseDuration = 0.12f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float HopHeight = 38.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float MaxYawPerHopDeg = 35.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float AlignToleranceDeg = 12.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float LayDownDuration = 0.42f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float StandUpDuration = 0.45f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float EffectiveGravity = 2600.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float DriveAngularAccel = 26.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float InitialStepOmega = 4.2f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float StepBounceRetention = 0.84f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	int32 MaxTumbleSteps = 10;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float MaxCorrectionYawPerStepDeg = 15.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float SteerYawRateDeg = 90.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "WaxButter|Runtime")
	float AbortAngleDeg = 55.0f;

protected:
	virtual bool ShouldApplyDefaultMovement() const override { return false; }

private:
	void EnterState(EWaxButterLocoState NewState);
	void TickHopAlign(float DeltaTime);
	void TickLayDown(float DeltaTime);
	void TickTumbleCharge(float DeltaTime);
	void TickStandUp(float DeltaTime);

	void SetCapsuleHalfHeightKeepFloor(float NewHalfHeight);
	void ChooseLayDirection();
	FQuat ComputeLaidRelQuat(float LayAlpha, float TumbleAngleRad) const;
	bool GetDirectionToPlayer(FVector& OutDir2D, float& OutDist2D) const;

	EWaxButterLocoState LocoState = EWaxButterLocoState::HopAlign;
	float StateElapsed = 0.0f;

	float HopStartYaw = 0.0f;
	float HopTargetYaw = 0.0f;
	bool bInHopAir = false;

	float LaySign = 1.0f;
	float PrevLayLateral = 0.0f;

	int32 TumbleStepIndex = 0;
	float TumbleTheta = 0.0f;
	float TumbleOmega = 0.0f;
	float PrevStepForward = 0.0f;
	float PendingSteerYaw = 0.0f;
	float StandStartHeight = 75.0f;
	float StandStartTheta = 0.0f;
};
