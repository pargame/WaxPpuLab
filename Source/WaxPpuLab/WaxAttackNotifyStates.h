#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "WaxCombatTypes.h"
#include "WaxAttackNotifyStates.generated.h"

class UWaxCombatComponent;

/** 공용 베이스: 오너의 Combat 컴포넌트 탐색 */
UCLASS(Abstract)
class WAXPPULAB_API UWaxCombatNotifyStateBase : public UAnimNotifyState
{
	GENERATED_BODY()

protected:
	static UWaxCombatComponent* FindCombat(const USkeletalMeshComponent* MeshComp);
};

/** 타격(Active) 구간. 앞은 Windup, 뒤는 Recovery로 자동 추론 */
UCLASS(meta = (DisplayName = "Wax Attack Active"))
class WAXPPULAB_API UWaxAttackActiveNotifyState : public UWaxCombatNotifyStateBase
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("WaxActive"); }
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

/** 이 구간 동안 버퍼의 다음 공격(허용 슬롯만)으로 캔슬·연계 가능 */
UCLASS(meta = (DisplayName = "Wax Cancel Window"))
class WAXPPULAB_API UWaxCancelWindowNotifyState : public UWaxCombatNotifyStateBase
{
	GENERATED_BODY()

public:
	/** 이 구간에서 연계를 허용할 공격 슬롯 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxAttack")
	TArray<EWaxAttackSlot> AllowedSlots = { EWaxAttackSlot::Basic, EWaxAttackSlot::Kick };

	virtual FString GetNotifyName_Implementation() const override;
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

/** 이 구간 동안 DA 단계별 회전 제한을 덮어씀 */
UCLASS(meta = (DisplayName = "Wax Rotation Profile"))
class WAXPPULAB_API UWaxRotationProfileNotifyState : public UWaxCombatNotifyStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxAttack")
	FWaxRotationProfile Profile;

	virtual FString GetNotifyName_Implementation() const override { return TEXT("WaxRotation"); }
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
