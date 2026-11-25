#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "WaxPunchAnimNotify.generated.h"

UCLASS(meta = (DisplayName = "Wax Punch Impact"))
class WAXPPULAB_API UWaxPunchAnimNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** 타격 중심을 추출할 주먹 본/소켓 이름 (예: hand_r, hand_l) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxPunch")
	FName ImpactBoneName = FName(TEXT("hand_r"));

	/** 캐릭터 기본 WaxImpactStrain에 곱할 충격량 배율. 1보다 크면 강타 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxPunch", meta = (ClampMin = "0.1"))
	float ImpactStrainScale = 1.0f;

	/** 캐릭터 기본 WaxImpactRadius에 곱할 타격 반경 배율. 1보다 크면 광역 타격 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxPunch", meta = (ClampMin = "0.1"))
	float ImpactRadiusScale = 1.0f;

	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
