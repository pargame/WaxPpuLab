#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WaxCombatTypes.h"
#include "WaxCombatFlowInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UWaxCombatFlowInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 공격 흐름 결정 지점. 캐릭터가 구현하고 BP(BP_WaxPlayer)에서 재정의한다.
 * 세부 논리(버퍼·FSM·판정)는 UWaxCombatComponent가 담당한다.
 */
class WAXPPULAB_API IWaxCombatFlowInterface
{
	GENERATED_BODY()

public:
	/** 재생할 몽타주 인덱스 선택. 기본: 콤보 순서 그대로 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WaxCombat|Flow")
	int32 SelectAttackMontage(EWaxAttackSlot Slot, const FWaxAttackSpec& Spec, int32 ComboIndex);

	/** C++ 기본 조건(상태·데이터) 통과 후 추가로 검사할 조건. 기본: 항상 허용 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WaxCombat|Flow")
	bool CanExecuteAttack(EWaxAttackSlot Slot);

	/** 현재 단계의 회전 제한 결정. DefaultProfile = NotifyState 오버라이드 또는 DA 단계 기본값 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WaxCombat|Flow")
	FWaxRotationProfile ResolveRotationProfile(EWaxAttackSlot Slot, EWaxAttackPhase Phase, const FWaxRotationProfile& DefaultProfile);
};
