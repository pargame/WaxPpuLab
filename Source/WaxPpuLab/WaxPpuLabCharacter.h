// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "WaxCombatFlowInterface.h"
#include "WaxPpuLabCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UWaxCombatComponent;
class AWaxMonsterBase;
class UWaxPlayerDataAsset;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class AWaxPpuLabCharacter : public ACharacter, public IWaxCombatFlowInterface
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** 공격 세부 논리 (입력 버퍼·단계 FSM·판정·회전 제한) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWaxCombatComponent> CombatComponent;

protected:

	/** 점프 입력 액션 */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** 이동 입력 액션 (WASD/스틱) */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** 시점 회전 입력 액션 (컨트롤러) */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** 마우스 시점 회전 입력 액션 */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	/** 기본공격 입력 액션. Started=누름, Completed/Canceled=뗌 (홀드 연타는 Combat 컴포넌트가 상태로 처리) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CrackAction;

	/** 밀어내기 발차기 입력 액션 (지상 전용, Started만 사용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> KickAction;

	/** 테스트 몬스터 스폰 입력 액션 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SpawnTestMonsterAction;

	/** 개발용 오디오 프리셋 전환 입력 액션 (1~9, 0 키 → 1~10번 프리셋) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AudioPresetAction;

	/** 테스트 몬스터 종류 전환 입력 액션 (Q: 이전, E: 다음) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SwitchMonsterAction;

	/** 임시 3인칭 프리뷰 카메라 토글 입력 액션 (T 키) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleThirdPersonAction;
	
public:
	AWaxPpuLabCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="WaxDemo")
	void SpawnDemoWaxBallInFront();

	UFUNCTION(BlueprintCallable, Category="WaxDemo|Camera")
	void ToggleThirdPersonPreview();

	UFUNCTION(BlueprintCallable, Category="WaxDemo|Camera")
	void SetThirdPersonPreview(bool bEnable);

	UFUNCTION(BlueprintPure, Category="WaxDemo|Camera")
	bool IsThirdPersonPreview() const { return bThirdPersonPreview; }

	UFUNCTION(BlueprintPure, Category="WaxDemo|UpperBody")
	float GetAimPitch() const { return SmoothedAimPitch; }

	UFUNCTION(BlueprintPure, Category="WaxDemo|UpperBody")
	FRotator GetSpine01PitchRotation() const { return FRotator(0.0f, 0.0f, -SmoothedAimPitch * Spine01PitchWeight); }

	UFUNCTION(BlueprintPure, Category="WaxDemo|UpperBody")
	FRotator GetSpine02PitchRotation() const { return FRotator(0.0f, 0.0f, -SmoothedAimPitch * Spine02PitchWeight); }

	UFUNCTION(BlueprintPure, Category="WaxDemo|UpperBody")
	FRotator GetSpine03PitchRotation() const { return FRotator(0.0f, 0.0f, -SmoothedAimPitch * Spine03PitchWeight); }

	UFUNCTION(Exec, Category="WaxDemo|Audio")
	void SetWaxAudioPreset(int32 PresetIndex);

	/** 개발용: 슬롯(Basic/AerialStrike/Slam/Kick)의 타격 프레임 전후를 3인칭으로 정지 캡처. 커맨드라인 -WaxCapture=<Slot> [-WaxCaptureQuit]도 지원 */
	UFUNCTION(Exec, Category="WaxDemo|Combat")
	void WaxAttackCapture(const FString& SlotName, int32 bQuitWhenDone = 0);

	UWaxCombatComponent* GetCombatComponent() const { return CombatComponent; }

	virtual int32 SelectAttackMontage_Implementation(EWaxAttackSlot Slot, const FWaxAttackSpec& Spec, int32 ComboIndex) override;
	virtual bool CanExecuteAttack_Implementation(EWaxAttackSlot Slot) override;
	virtual FWaxRotationProfile ResolveRotationProfile_Implementation(EWaxAttackSlot Slot, EWaxAttackPhase Phase, const FWaxRotationProfile& DefaultProfile) override;

	/** 플레이어 튜닝 수치 에셋. 아래 VisibleInstanceOnly 필드는 BeginPlay에서 이 에셋 값으로 덮어쓰는 런타임 복사본 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WaxDemo")
	TObjectPtr<UWaxPlayerDataAsset> PlayerData;

	/** 캡슐 기준 1인칭 카메라 기본 상대 위치 (cm, X: 전후, Y: 좌우, Z: 높이) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|Camera")
	FVector CameraBaseOffset = FVector(8.0f, 0.0f, 64.0f);

	/** 1인칭 메시 머리 움직임을 카메라에 반영할 비율 (0: 완전 고정, 1: 100% 추종) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|Camera")
	float HeadFollowAlpha = 0.25f;

	/** 카메라 위치 보간 속도 (1/초). 높일수록 즉각 반응, 0이면 보간 없이 즉시 스냅 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|Camera")
	float HeadFollowInterpSpeed = 18.0f;

	/** 머리 추종 시 전후(X) 이동 허용 최대치 (cm). 줄이면 펀치·숙임 시 메시에 파묻힘 방지 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|Camera")
	float MaxHeadForwardOffset = 8.0f;

	/** 카메라 위치 추종 기준으로 참조할 1인칭 메시 본/소켓 이름 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|Camera")
	FName HeadSocketName = FName(TEXT("head"));

	/** 3인칭 프리뷰 카메라 활성화 여부 (개발 테스트용 기본값 켜짐, T 키로 1인칭 전환) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Camera")
	bool bThirdPersonPreview = true;

	/** 3인칭 프리뷰 시 시선 기준 카메라 오프셋 (cm, X: 뒤로(-), Y: 우측(+), Z: 위(+)) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|Camera")
	FVector ThirdPersonPreviewOffset = FVector(-170.0f, 35.0f, 10.0f);

	/** 상반신 시선(Pitch) 추종 활성화 여부 (위·아래 조준 시 팔·상체 함께 기울임) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|UpperBody")
	bool bEnableUpperBodyPitch = true;

	/** 상반신 시선 Pitch 보간 속도 (1/초). 높일수록 즉각 반응, 0이면 보간 없이 즉시 반영 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|UpperBody")
	float AimPitchInterpSpeed = 16.0f;

	/** 상반신 Pitch 최대 기울기 각도 (도). 클수록 위·아래 조준 시 상체가 더 많이 기울어짐 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|UpperBody")
	float MaxUpperBodyPitchDeg = 45.0f;

	/** spine_01 본에 분배할 상반신 Pitch 비율 (spine_01~03 합 1.0 권장) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|UpperBody")
	float Spine01PitchWeight = 0.25f;

	/** spine_02 본에 분배할 상반신 Pitch 비율 (spine_01~03 합 1.0 권장) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|UpperBody")
	float Spine02PitchWeight = 0.35f;

	/** spine_03 본에 분배할 상반신 Pitch 비율 (spine_01~03 합 1.0 권장) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|UpperBody")
	float Spine03PitchWeight = 0.40f;

	/** 이동 중 발밑 왁스 파편을 걷어차는 물리 반응 활성화 여부 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|DebrisKick")
	bool bEnableDebrisKick = true;

	/** 발밑 파편을 감지·충격할 구 반경 (cm). 키우면 더 넓은 범위의 파편을 밀어냄 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|DebrisKick")
	float DebrisKickRadius = 85.0f;

	/** 캐릭터 수평 이동 속도 대비 파편에 전달할 속도 배율. 높일수록 강하게 날아감 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|DebrisKick")
	float DebrisKickStrength = 1.35f;

	/** 파편을 차낼 때 추가로 부여할 상향 속도 (cm/s). 높일수록 높게 튐 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|DebrisKick")
	float DebrisKickUpwardBoost = 160.0f;

	/** 파편 걷어차기가 발동되는 최소 수평 이동 속도 (cm/s). 높이면 미세 움직임 무시 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WaxDemo|DebrisKick")
	float DebrisKickMinMoveSpeed = 25.0f;

	/** 현재 선택되어 스폰될 테스트 몬스터 블루프린트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WaxDemo|Spawn")
	TSubclassOf<AWaxMonsterBase> TestMonsterClass;

	/** Q/E 키로 순환 전환할 테스트 몬스터 클래스 목록 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WaxDemo|Spawn")
	TArray<TSubclassOf<AWaxMonsterBase>> TestMonsterClasses;

	/** 캐릭터 전방 테스트 몬스터 스폰 거리 (cm). 키우면 더 멀리 생성됨 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Spawn", meta = (ClampMin = "80.0"))
	float WaxSpawnDistance = 260.0f;

private:
	void OnAttackInputStarted();
	void OnAttackInputReleased();
	void OnKickInputStarted();
	void TryStartCommandLineCapture();
	void ApplyCameraViewMode();
	void ApplyPlayerData();
	void ApplyViewPitchLimits();
	virtual void NotifyControllerChanged() override;
	void UpdateFirstPersonCamera(float DeltaTime);
	void UpdateUpperBodyAimPitch(float DeltaTime);
	void UpdateDebrisKick();

	int32 SelectedMonsterIndex = 0;
	float SmoothedAimPitch = 0.0f;
	FVector CachedHeadRefLocal = FVector::ZeroVector;
	bool bHasCachedHeadRef = false;

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Called from Input Actions for dev audio preset keys 1..9, 0 */
	void AudioPresetInput(const FInputActionValue& Value);

	/** Called from Input Actions for Q/E monster switching */
	void SwitchMonsterInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

