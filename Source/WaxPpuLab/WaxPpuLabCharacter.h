// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "WaxPpuLabCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UMaterialParameterCollection;
class UInputAction;
class UAnimMontage;
class AWaxMonsterBase;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class AWaxPpuLabCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

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

	/** 펀치·파괴 입력 액션 (길게 누르기 연타는 IA 에셋의 Pulse 트리거로 제어) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CrackAction;

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

	UFUNCTION(BlueprintCallable, Category="WaxDemo|Punch")
	void RequestPunch();

	UFUNCTION(BlueprintCallable, Category="WaxDemo|Punch")
	void HandlePunchImpactNotify(FName ImpactBoneName, float StrainScale = 1.0f, float RadiusScale = 1.0f);

	UFUNCTION(BlueprintCallable, Category="WaxDemo")
	void SpawnDemoWaxBallInFront();

	UFUNCTION(BlueprintCallable, Category="WaxDemo|Camera")
	void ToggleThirdPersonPreview();

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

	/** 기본 타격 구 반경 (cm). 키우면 한 번에 파괴되는 왁스 범위가 넓어짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Impact", meta = (ClampMin = "5.0"))
	float WaxImpactRadius = 75.0f;

	/** 기본 파괴 충격량 (Chaos Strain). 높일수록 단단한 왁스도 쉽게 부서짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Impact", meta = (ClampMin = "1000.0"))
	float WaxImpactStrain = 320000.0f;

	/** 순환 재생할 펀치 애님 몽타주 목록 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WaxDemo|Punch")
	TArray<TObjectPtr<UAnimMontage>> PunchMontages;

	/** 펀치 몽타주 재생 속도 배율. 높일수록 스윙이 빨라짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Punch", meta = (ClampMin = "0.1"))
	float PunchPlayRate = 1.65f;

	/** 다음 펀치 입력까지 최소 대기 시간 (초). 줄이면 연타 간격이 짧아짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Punch", meta = (ClampMin = "0.05"))
	float PunchCooldownSec = 0.34f;

	/** 주먹 본 위치에서 시선 전방으로 밀어낼 타격 중심 오프셋 (cm). 키우면 리치가 길어짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Punch", meta = (ClampMin = "0.0"))
	float PunchBoneForwardOffset = 18.0f;

	/** 펀치 타격 표면 마커 파라미터를 전달할 Material Parameter Collection 에셋 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WaxDemo|PunchMarker")
	TObjectPtr<UMaterialParameterCollection> TargetingMPC;

	/** 펀치 타격 시 왁스 표면에 빨강 원형 마커(MPC) 표시 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|PunchMarker")
	bool bShowPunchImpactMarker = true;

	/** 펀치 타격 시 실제 판정 구 디버그 와이어프레임 표시 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|PunchMarker")
	bool bShowDebugPunchImpact = false;

	/** 펀치 타격 마커 및 디버그 구 색상 (Linear RGB) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|PunchMarker", meta = (EditCondition = "bShowPunchImpactMarker || bShowDebugPunchImpact"))
	FLinearColor PunchMarkerColor = FLinearColor(1.0f, 0.01f, 0.01f, 1.0f);

	/** 펀치 타격 마커 유지 시간 (초). 키우면 타격 범위가 더 오래 남음 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|PunchMarker", meta = (ClampMin = "0.05", EditCondition = "bShowPunchImpactMarker || bShowDebugPunchImpact"))
	float PunchMarkerDurationSec = 0.35f;

	/** 펀치 표면 마커 발광 배율. 높일수록 어두운 배경에서 더 선명하게 빛남 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|PunchMarker", meta = (ClampMin = "0.0", EditCondition = "bShowPunchImpactMarker"))
	float PunchMarkerEmissiveBoost = 8.0f;

	/** 펀치 표면 마커 불투명도 (0~1). 낮출수록 반투명해짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|PunchMarker", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bShowPunchImpactMarker"))
	float PunchMarkerOpacity = 1.0f;

	/** 펀치 표면 마커 외곽 페더링 강도. 클수록 경계가 부드럽게 번짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|PunchMarker", meta = (ClampMin = "0.1", EditCondition = "bShowPunchImpactMarker"))
	float PunchMarkerEdgeSoftness = 0.45f;

	/** 캡슐 기준 1인칭 카메라 기본 상대 위치 (cm, X: 전후, Y: 좌우, Z: 높이) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Camera")
	FVector CameraBaseOffset = FVector(8.0f, 0.0f, 64.0f);

	/** 1인칭 메시 머리 움직임을 카메라에 반영할 비율 (0: 완전 고정, 1: 100% 추종) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Camera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeadFollowAlpha = 0.25f;

	/** 카메라 위치 보간 속도 (1/초). 높일수록 즉각 반응, 0이면 보간 없이 즉시 스냅 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Camera", meta = (ClampMin = "0.0"))
	float HeadFollowInterpSpeed = 18.0f;

	/** 머리 추종 시 전후(X) 이동 허용 최대치 (cm). 줄이면 펀치·숙임 시 메시에 파묻힘 방지 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Camera", meta = (ClampMin = "0.0"))
	float MaxHeadForwardOffset = 8.0f;

	/** 카메라 위치 추종 기준으로 참조할 1인칭 메시 본/소켓 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Camera")
	FName HeadSocketName = FName(TEXT("head"));

	/** 임시 3인칭 프리뷰 카메라 활성화 여부 (T 키로 토글, 상반신 모션 확인용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Camera")
	bool bThirdPersonPreview = false;

	/** 3인칭 프리뷰 시 시선 기준 카메라 오프셋 (cm, X: 뒤로(-), Y: 우측(+), Z: 위(+)) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Camera")
	FVector ThirdPersonPreviewOffset = FVector(-170.0f, 35.0f, 10.0f);

	/** 상반신 시선(Pitch) 추종 활성화 여부 (위·아래 조준 시 팔·상체 함께 기울임) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|UpperBody")
	bool bEnableUpperBodyPitch = true;

	/** 상반신 시선 Pitch 보간 속도 (1/초). 높일수록 즉각 반응, 0이면 보간 없이 즉시 반영 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|UpperBody", meta = (ClampMin = "0.0", EditCondition = "bEnableUpperBodyPitch"))
	float AimPitchInterpSpeed = 16.0f;

	/** 상반신 Pitch 최대 기울기 각도 (도). 클수록 위·아래 조준 시 상체가 더 많이 기울어짐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|UpperBody", meta = (ClampMin = "0.0", ClampMax = "85.0", EditCondition = "bEnableUpperBodyPitch"))
	float MaxUpperBodyPitchDeg = 45.0f;

	/** spine_01 본에 분배할 상반신 Pitch 비율 (spine_01~03 합 1.0 권장) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|UpperBody", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableUpperBodyPitch"))
	float Spine01PitchWeight = 0.25f;

	/** spine_02 본에 분배할 상반신 Pitch 비율 (spine_01~03 합 1.0 권장) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|UpperBody", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableUpperBodyPitch"))
	float Spine02PitchWeight = 0.35f;

	/** spine_03 본에 분배할 상반신 Pitch 비율 (spine_01~03 합 1.0 권장) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|UpperBody", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableUpperBodyPitch"))
	float Spine03PitchWeight = 0.40f;

	/** 이동 중 발밑 왁스 파편을 걷어차는 물리 반응 활성화 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|DebrisKick")
	bool bEnableDebrisKick = true;

	/** 발밑 파편을 감지·충격할 구 반경 (cm). 키우면 더 넓은 범위의 파편을 밀어냄 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|DebrisKick", meta = (ClampMin = "10.0"))
	float DebrisKickRadius = 85.0f;

	/** 캐릭터 수평 이동 속도 대비 파편에 전달할 속도 배율. 높일수록 강하게 날아감 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|DebrisKick", meta = (ClampMin = "0.0"))
	float DebrisKickStrength = 1.35f;

	/** 파편을 차낼 때 추가로 부여할 상향 속도 (cm/s). 높일수록 높게 튐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|DebrisKick", meta = (ClampMin = "0.0"))
	float DebrisKickUpwardBoost = 160.0f;

	/** 파편 걷어차기가 발동되는 최소 수평 이동 속도 (cm/s). 높이면 미세 움직임 무시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|DebrisKick", meta = (ClampMin = "0.0"))
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
	void ExecuteWaxImpactAtLocation(const FVector& ImpactCenter, float Strain, float Radius);
	void SetPunchMarkerVisible(bool bVisible, const FVector& ImpactCenter = FVector::ZeroVector, float Radius = 0.0f);
	void UpdatePunchMarker(double Now);
	void ApplyCameraViewMode();
	void UpdateFirstPersonCamera(float DeltaTime);
	void UpdateUpperBodyAimPitch(float DeltaTime);
	void UpdateDebrisKick();

	int32 SelectedMonsterIndex = 0;
	int32 NextPunchIndex = 0;
	double LastPunchStartTime = -10.0;
	bool bPunchMarkerActive = false;
	double PunchMarkerExpireTime = -1.0;
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

