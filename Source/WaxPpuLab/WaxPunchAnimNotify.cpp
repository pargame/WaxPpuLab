#include "WaxPunchAnimNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "WaxPpuLabCharacter.h"

FString UWaxPunchAnimNotify::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("WaxPunchImpact(%s)"), *ImpactBoneName.ToString());
}

void UWaxPunchAnimNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (AWaxPpuLabCharacter* Character = Cast<AWaxPpuLabCharacter>(MeshComp->GetOwner()))
	{
		Character->HandlePunchImpactNotify(ImpactBoneName, ImpactStrainScale, ImpactRadiusScale);
	}
}
