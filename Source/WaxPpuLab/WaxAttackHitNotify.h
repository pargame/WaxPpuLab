#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "WaxCombatTypes.h"
#include "WaxAttackHitNotify.generated.h"

/** 공격 판정 시점. 몸체 메시(또는 무기) 소켓 위치에서 구 오버랩 판정 */
UCLASS(meta = (DisplayName = "Wax Attack Hit"))
class WAXPPULAB_API UWaxAttackHitNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** 타격 원점을 읽을 메시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxAttack")
	EWaxHitSource Source = EWaxHitSource::Body;

	/** 타격 중심 본/소켓 이름 (예: hand_r, hand_l, foot_r, 무기 소켓) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxAttack")
	FName ImpactBoneName = FName(TEXT("hand_r"));

	/** Strain 배율 (최종 = 기본 × Spec × 이 값). 한 몽타주 내 타격별 강약용 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxAttack", meta = (ClampMin = "0.1"))
	float ImpactStrainScale = 1.0f;

	/** 반경 배율 (최종 = 기본 × Spec × 이 값) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxAttack", meta = (ClampMin = "0.1"))
	float ImpactRadiusScale = 1.0f;

	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
