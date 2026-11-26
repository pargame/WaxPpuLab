#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WaxCombatTypes.h"
#include "WaxCombatComponent.generated.h"

class ACharacter;
class UAnimInstance;
class UAnimMontage;
class UMaterialParameterCollection;
class USkeletalMeshComponent;
class UWaxAttackSetDataAsset;
struct FAnimNotifyEventReference;

DECLARE_LOG_CATEGORY_EXTERN(LogWaxCombat, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWaxAttackStartedSignature, EWaxAttackSlot, Slot, UAnimMontage*, Montage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWaxAttackPhaseSignature, EWaxAttackSlot, Slot, EWaxAttackPhase, Phase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FWaxAttackHitSignature, EWaxAttackSlot, Slot, FVector, Location, int32, NumTargets, float, Strain, float, Radius);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWaxAttackEndedSignature, EWaxAttackSlot, Slot, bool, bInterrupted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaxBufferConsumedSignature, EWaxAttackSlot, Slot);

/**
 * 공격 세부 논리: 입력 버퍼, 단계 FSM, 히트 판정, 회전 제한.
 * 흐름 결정은 오너의 IWaxCombatFlowInterface, 타이밍은 몽타주 NotifyState, 수치는 UWaxAttackSetDataAsset이 담당.
 */
UCLASS(ClassGroup = (WaxPpu), meta = (BlueprintSpawnableComponent))
class WAXPPULAB_API UWaxCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWaxCombatComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 맨손 공격 세트 및 공통 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WaxCombat")
	TObjectPtr<UWaxAttackSetDataAsset> AttackSet;

	/** 타격 표면 마커 파라미터를 전달할 MPC */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WaxCombat|Marker")
	TObjectPtr<UMaterialParameterCollection> TargetingMPC;

	/** 타격 시 왁스 표면에 원형 마커 표시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxCombat|Marker")
	bool bShowImpactMarker = true;

	/** 타격 판정 구 디버그 와이어프레임 표시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxCombat|Marker")
	bool bShowDebugImpact = false;

	/** 마커·디버그 구 색상 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxCombat|Marker")
	FLinearColor MarkerColor = FLinearColor(1.0f, 0.01f, 0.01f, 1.0f);

	/** 마커 유지 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxCombat|Marker", meta = (ClampMin = "0.05"))
	float MarkerDurationSec = 0.35f;

	/** 마커 발광 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxCombat|Marker", meta = (ClampMin = "0.0"))
	float MarkerEmissiveBoost = 8.0f;

	/** 마커 불투명도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxCombat|Marker", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MarkerOpacity = 1.0f;

	/** 마커 외곽 페더링 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxCombat|Marker", meta = (ClampMin = "0.1"))
	float MarkerEdgeSoftness = 0.45f;

	/** 캡처 시 몸 정면 기준 카메라 Yaw 오프셋 (도). 90 근처면 측면 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxCombat|Debug")
	float CaptureViewYawOffsetDeg = 110.0f;

	/** 캡처 시 카메라 Pitch (도). 음수면 내려다봄 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxCombat|Debug")
	float CaptureViewPitchDeg = -8.0f;

	/** 버퍼 저장·소비·만료와 단계 전이를 로그로 남김 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxCombat|Debug")
	bool bVerboseLog = true;

	/** 공격 종료 후 몸 정렬이 이 시간(초)을 넘기면 회전 고착 경고 로그. 클수록 관대 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaxCombat|Debug", meta = (ClampMin = "0.1"))
	float RotationStuckWarnSec = 1.0f;

	UPROPERTY(BlueprintAssignable, Category = "WaxCombat|Events")
	FWaxAttackStartedSignature OnAttackStarted;

	UPROPERTY(BlueprintAssignable, Category = "WaxCombat|Events")
	FWaxAttackPhaseSignature OnAttackPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "WaxCombat|Events")
	FWaxAttackHitSignature OnAttackHit;

	UPROPERTY(BlueprintAssignable, Category = "WaxCombat|Events")
	FWaxAttackEndedSignature OnAttackEnded;

	UPROPERTY(BlueprintAssignable, Category = "WaxCombat|Events")
	FWaxBufferConsumedSignature OnBufferConsumed;

	/** 명시적 공격 입력(키 누름). 즉시 실행 또는 버퍼 저장 */
	UFUNCTION(BlueprintCallable, Category = "WaxCombat")
	void PressAttackInput(EWaxAttackSlot Slot);

	/** 기본공격 키 홀드 상태. 지상에서 시작한 홀드만 연타 폴백으로 인정 */
	UFUNCTION(BlueprintCallable, Category = "WaxCombat")
	void SetAttackHeld(bool bHeld);

	UFUNCTION(BlueprintPure, Category = "WaxCombat")
	EWaxAttackSlot GetCurrentSlot() const { return CurrentSlot; }

	UFUNCTION(BlueprintPure, Category = "WaxCombat")
	EWaxAttackPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "WaxCombat")
	bool IsAttacking() const { return Phase != EWaxAttackPhase::Idle; }

	/** 현재 적용 중인 회전 제한 (비공격 시 제한 없음) */
	UFUNCTION(BlueprintPure, Category = "WaxCombat")
	FWaxRotationProfile GetActiveRotationProfile() const;

	/** 상반신 Pitch 최대 회전속도 (도/초). 0 이하면 제한 없음 */
	float GetPitchRateLimit() const;

	/** 공격 중 1인칭 머리 추종을 끌지 여부 */
	bool ShouldSuppressHeadFollow() const;

	/** 판정에 쓰는 공격 방향 (몸 Yaw + 상반신 Pitch) */
	FVector GetAttackAimDirection() const;

	/** 공격 판정 몽타주를 노티파이 프레임 전후로 정지 캡처 (3인칭 프리뷰) */
	bool StartCapture(EWaxAttackSlot Slot, bool bQuitWhenDone);

	void HandleHitNotify(USkeletalMeshComponent* MeshComp, const FAnimNotifyEventReference& EventReference, EWaxHitSource Source, FName BoneName, float StrainScale, float RadiusScale);
	void NotifyActiveWindow(USkeletalMeshComponent* MeshComp, const FAnimNotifyEventReference& EventReference, bool bBegin);
	void NotifyCancelWindow(USkeletalMeshComponent* MeshComp, const FAnimNotifyEventReference& EventReference, bool bBegin, const TArray<EWaxAttackSlot>& AllowedSlots);
	void PushRotationOverride(USkeletalMeshComponent* MeshComp, const FAnimNotifyEventReference& EventReference, const UObject* Key, const FWaxRotationProfile& Profile);
	void PopRotationOverride(USkeletalMeshComponent* MeshComp, const FAnimNotifyEventReference& EventReference, const UObject* Key);

private:
	enum class EConsumePoint : uint8 { Press, CancelWindow, AttackEnd };

	struct FCaptureShot
	{
		TObjectPtr<UAnimMontage> Montage;
		float Position = 0.0f;
		FName HitBone;
		bool bIsHitFrame = false;
		FString Label;
	};

	UAnimInstance* GetAnimInstance() const;
	const FWaxAttackSpec* ResolveSpec(EWaxAttackSlot Slot) const;
	const FWaxCombatSettings& GetSettings() const;
	bool IsFalling() const;
	bool IsPermanentlyInvalid(EWaxAttackSlot Slot) const;
	bool CanExecute(EWaxAttackSlot Slot) const;
	bool IsAllowedNow(EWaxAttackSlot Slot) const;
	bool IsHoldFallbackActive() const;
	bool TryConsume(EConsumePoint Point);
	bool StartAttack(EWaxAttackSlot Slot, bool bFromBuffer);
	void EndAttack(bool bInterrupted);
	void SetPhase(EWaxAttackPhase NewPhase);
	void ExpireBuffer(double Now);
	void ClearBuffer(const TCHAR* Reason);
	bool IsCurrentAttackEvent(const USkeletalMeshComponent* MeshComp, const FAnimNotifyEventReference& EventReference) const;
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int32 Serial);

	FVector ComputeHitCenter(EWaxHitSource Source, FName BoneName, float ForwardOffset, bool& bOutValid) const;
	int32 ApplyImpact(const FVector& Center, float Strain, float Radius);
	void SetMarkerVisible(bool bVisible, const FVector& Center = FVector::ZeroVector, float Radius = 0.0f);

	void UpdateRotation(float DeltaTime);
	float FilterLookDelta(float Delta, float Offset, float MaxOffset) const;

	void CollectHitFrames(UAnimMontage* Montage, TArray<TPair<float, FName>>& OutHits) const;
	void TickCapture();
	void FinishCapture();

	const TCHAR* SlotName(EWaxAttackSlot Slot) const;
	const TCHAR* PhaseName(EWaxAttackPhase InPhase) const;
	double Now() const;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	FWaxAttackSpec CurrentSpec;
	EWaxAttackSlot CurrentSlot = EWaxAttackSlot::None;
	EWaxAttackPhase Phase = EWaxAttackPhase::Idle;
	int32 ActiveMontageInstanceId = INDEX_NONE;
	int32 AttackSerial = 0;
	bool bSawActiveWindow = false;
	int32 ComboIndex = 0;
	EWaxAttackSlot LastStartedSlot = EWaxAttackSlot::None;
	double LastAttackEndTime = -100.0;

	EWaxAttackSlot BufferedSlot = EWaxAttackSlot::None;
	double BufferedTime = 0.0;

	bool bAttackHeld = false;
	bool bHoldStartedGrounded = false;

	bool bCancelWindowOpen = false;
	TArray<EWaxAttackSlot> CancelAllowedSlots;
	TArray<TPair<const UObject*, FWaxRotationProfile>> RotationOverrides;
	TSet<TWeakObjectPtr<AActor>> HitActorsThisAttack;

	bool bBodyYawDetached = false;
	float CurrentSensitivityScale = 1.0f;
	FRotator LastControlRotation = FRotator::ZeroRotator;
	bool bHasLastControlRotation = false;

	bool bRotSessionActive = false;
	int32 RotSessionAttacks = 0;
	float RotMaxYawOffset = 0.0f;
	float RotMaxPitchOffset = 0.0f;
	float RotMinSensitivity = 1.0f;
	int32 RotBoundaryFrames = 0;
	double RotRecoverStartTime = -1.0;
	void FinishRotationSession();

	bool bMarkerActive = false;
	double MarkerExpireTime = -1.0;
	bool bWarnedWeaponSource = false;

	TArray<FCaptureShot> CaptureShots;
	int32 CaptureIndex = INDEX_NONE;
	int32 CaptureFrameWait = 0;
	int32 CaptureStage = 0;
	bool bCaptureQuitWhenDone = false;
	FWaxAttackSpec CaptureSpec;
	bool bCaptureRestoreThirdPerson = false;
	bool bCaptureRestoreControllerYaw = false;
	float CaptureBodyYaw = 0.0f;
	void ApplyCaptureView();
	FString CaptureDir;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CapturePlayingMontage;
};
