#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WaxPlayerDataAsset.generated.h"

/** 플레이어 체감 튜닝 수치 (카메라·상반신 Pitch·파편 걷어차기). 캐릭터가 BeginPlay에서 런타임 필드로 복사 */
UCLASS(BlueprintType)
class WAXPPULAB_API UWaxPlayerDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 캡슐 기준 1인칭 카메라 기본 상대 위치 (cm, X: 전후, Y: 좌우, Z: 높이) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector CameraBaseOffset = FVector(8.0f, 0.0f, 64.0f);

	/** 1인칭 메시 머리 움직임을 카메라에 반영할 비율 (0: 완전 고정, 1: 100% 추종) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeadFollowAlpha = 0.25f;

	/** 카메라 위치 보간 속도 (1/초). 높일수록 즉각 반응, 0이면 보간 없이 즉시 스냅 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0"))
	float HeadFollowInterpSpeed = 18.0f;

	/** 머리 추종 시 전후(X) 이동 허용 최대치 (cm). 줄이면 펀치·숙임 시 메시에 파묻힘 방지 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0"))
	float MaxHeadForwardOffset = 8.0f;

	/** 카메라 위치 추종 기준으로 참조할 1인칭 메시 본/소켓 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FName HeadSocketName = FName(TEXT("head"));

	/** 3인칭 프리뷰 시 시선 기준 카메라 오프셋 (cm, X: 뒤로(-), Y: 우측(+), Z: 위(+)) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector ThirdPersonPreviewOffset = FVector(-170.0f, 35.0f, 10.0f);

	/** 상반신 시선(Pitch) 추종 활성화 여부 (위·아래 조준 시 팔·상체 함께 기울임) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UpperBody")
	bool bEnableUpperBodyPitch = true;

	/** 상반신 시선 Pitch 보간 속도 (1/초). 높일수록 즉각 반응, 0이면 보간 없이 즉시 반영 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UpperBody", meta = (ClampMin = "0.0", EditCondition = "bEnableUpperBodyPitch"))
	float AimPitchInterpSpeed = 16.0f;

	/** 상반신 Pitch 최대 기울기 각도 (도). 클수록 위·아래 조준 시 상체가 더 많이 기울어짐 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UpperBody", meta = (ClampMin = "0.0", ClampMax = "85.0", EditCondition = "bEnableUpperBodyPitch"))
	float MaxUpperBodyPitchDeg = 45.0f;

	/** spine_01 본에 분배할 상반신 Pitch 비율 (spine_01~03 합 1.0 권장) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UpperBody", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableUpperBodyPitch"))
	float Spine01PitchWeight = 0.25f;

	/** spine_02 본에 분배할 상반신 Pitch 비율 (spine_01~03 합 1.0 권장) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UpperBody", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableUpperBodyPitch"))
	float Spine02PitchWeight = 0.35f;

	/** spine_03 본에 분배할 상반신 Pitch 비율 (spine_01~03 합 1.0 권장) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UpperBody", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableUpperBodyPitch"))
	float Spine03PitchWeight = 0.40f;

	/** 이동 중 발밑 왁스 파편을 걷어차는 물리 반응 활성화 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DebrisKick")
	bool bEnableDebrisKick = true;

	/** 발밑 파편을 감지·충격할 구 반경 (cm). 키우면 더 넓은 범위의 파편을 밀어냄 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DebrisKick", meta = (ClampMin = "10.0"))
	float DebrisKickRadius = 85.0f;

	/** 캐릭터 수평 이동 속도 대비 파편에 전달할 속도 배율. 높일수록 강하게 날아감 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DebrisKick", meta = (ClampMin = "0.0"))
	float DebrisKickStrength = 1.35f;

	/** 파편을 차낼 때 추가로 부여할 상향 속도 (cm/s). 높일수록 높게 튐 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DebrisKick", meta = (ClampMin = "0.0"))
	float DebrisKickUpwardBoost = 160.0f;

	/** 파편 걷어차기가 발동되는 최소 수평 이동 속도 (cm/s). 높이면 미세 움직임 무시 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DebrisKick", meta = (ClampMin = "0.0"))
	float DebrisKickMinMoveSpeed = 25.0f;
};
