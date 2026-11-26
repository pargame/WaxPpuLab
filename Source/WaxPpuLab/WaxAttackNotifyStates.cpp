#include "WaxAttackNotifyStates.h"

#include "Components/SkeletalMeshComponent.h"
#include "WaxCombatComponent.h"

UWaxCombatComponent* UWaxCombatNotifyStateBase::FindCombat(const USkeletalMeshComponent* MeshComp)
{
	const AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	return Owner ? Owner->FindComponentByClass<UWaxCombatComponent>() : nullptr;
}

void UWaxAttackActiveNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (UWaxCombatComponent* Combat = FindCombat(MeshComp))
	{
		Combat->NotifyActiveWindow(MeshComp, EventReference, true);
	}
}

void UWaxAttackActiveNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (UWaxCombatComponent* Combat = FindCombat(MeshComp))
	{
		Combat->NotifyActiveWindow(MeshComp, EventReference, false);
	}
}

FString UWaxCancelWindowNotifyState::GetNotifyName_Implementation() const
{
	FString Names;
	for (const EWaxAttackSlot Slot : AllowedSlots)
	{
		if (!Names.IsEmpty())
		{
			Names += TEXT(",");
		}
		Names += StaticEnum<EWaxAttackSlot>()->GetNameStringByValue(static_cast<int64>(Slot));
	}
	return FString::Printf(TEXT("WaxCancel(%s)"), *Names);
}

void UWaxCancelWindowNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (UWaxCombatComponent* Combat = FindCombat(MeshComp))
	{
		Combat->NotifyCancelWindow(MeshComp, EventReference, true, AllowedSlots);
	}
}

void UWaxCancelWindowNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (UWaxCombatComponent* Combat = FindCombat(MeshComp))
	{
		Combat->NotifyCancelWindow(MeshComp, EventReference, false, AllowedSlots);
	}
}

void UWaxRotationProfileNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (UWaxCombatComponent* Combat = FindCombat(MeshComp))
	{
		Combat->PushRotationOverride(MeshComp, EventReference, this, Profile);
	}
}

void UWaxRotationProfileNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (UWaxCombatComponent* Combat = FindCombat(MeshComp))
	{
		Combat->PopRotationOverride(MeshComp, EventReference, this);
	}
}
