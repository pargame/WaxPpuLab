#include "WaxAttackHitNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "WaxCombatComponent.h"

FString UWaxAttackHitNotify::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("WaxAttackHit(%s)"), *ImpactBoneName.ToString());
}

void UWaxAttackHitNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetOwner())
	{
		return;
	}

	if (UWaxCombatComponent* Combat = MeshComp->GetOwner()->FindComponentByClass<UWaxCombatComponent>())
	{
		Combat->HandleHitNotify(MeshComp, EventReference, Source, ImpactBoneName, ImpactStrainScale, ImpactRadiusScale);
	}
}
