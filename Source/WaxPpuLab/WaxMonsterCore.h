#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaxMonsterCore.generated.h"

class AWaxMonsterBase;
class UStaticMeshComponent;
class UWaxMonsterDataAsset;

UENUM(BlueprintType)
enum class EWaxCoreState : uint8
{
	Falling,
	Dormant,
	Reviving,
	Dead
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaxCorePermanentlyKilled, AWaxMonsterCore*, KilledCore);

UCLASS()
class WAXPPULAB_API AWaxMonsterCore : public AActor
{
	GENERATED_BODY()

public:
	AWaxMonsterCore();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "WaxCore")
	void InitializeCore(TSubclassOf<AWaxMonsterBase> InMonsterClass, UWaxMonsterDataAsset* InDataAsset, float InCapsuleHalfHeight);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WaxCore")
	void KillCore();
	virtual void KillCore_Implementation();

	UFUNCTION(BlueprintPure, Category = "WaxCore")
	EWaxCoreState GetCoreState() const { return CoreState; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WaxCore")
	TObjectPtr<UStaticMeshComponent> CoreMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WaxCore")
	TObjectPtr<UStaticMeshComponent> RevivePreviewMesh;

	UPROPERTY(BlueprintAssignable, Category = "WaxCore|Events")
	FOnWaxCorePermanentlyKilled OnCorePermanentlyKilled;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "WaxCore|Events")
	void BP_OnCoreReviveStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "WaxCore|Events")
	void BP_OnCoreRevived(AWaxMonsterBase* RevivedMonster);

private:
	UFUNCTION()
	void HandleCoreHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void BeginReviving();
	void FinishReviving();

	UPROPERTY(Transient)
	TSubclassOf<AWaxMonsterBase> MonsterClassToRevive;

	UPROPERTY(Transient)
	TObjectPtr<UWaxMonsterDataAsset> MonsterData;

	EWaxCoreState CoreState = EWaxCoreState::Falling;
	FVector ReviveTargetScale = FVector(1.85f, 1.85f, 1.85f);
	FVector ReviveSpawnLocation = FVector::ZeroVector;
	float MonsterCapsuleHalfHeight = 110.0f;
	float StateElapsed = 0.0f;
	double NextAllowedBounceSoundTime = 0.0;
};
