#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WaxCombatTypes.h"
#include "WaxAttackSetDataAsset.generated.h"

/** 맨손 공격 세트 (기본공격·점프공격·발차기) 및 공격 공통 설정 */
UCLASS(BlueprintType)
class WAXPPULAB_API UWaxAttackSetDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attacks")
	FWaxAttackSpec Basic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attacks")
	FWaxJumpAttackSpec JumpAttack;

	/** 무기와 무관하게 항상 이 값을 사용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attacks")
	FWaxAttackSpec Kick;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FWaxCombatSettings Settings;

	/** 슬롯에 해당하는 Spec. 없으면 nullptr */
	const FWaxAttackSpec* FindSpec(EWaxAttackSlot Slot) const;
};
