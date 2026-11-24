#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WaxMonsterDataAsset.generated.h"

class UGeometryCollection;
class UMaterialInterface;
class UPhysicalMaterial;
class USoundBase;
class UStaticMesh;
class UWaxLocomotionComponent;

USTRUCT(BlueprintType)
struct FWaxCollisionVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MasterIntensity = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.5"))
	float CrackleGain = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.5"))
	float SnapGain = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.5"))
	float BodyGain = 0.14f;
};

USTRUCT(BlueprintType)
struct FWaxAudioProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	FName ProfileName = TEXT("Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> CrackSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TArray<FWaxCollisionVariant> CollisionVariants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MasterIntensity = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.5"))
	float CrackleGain = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.5"))
	float SnapGain = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.5"))
	float BodyGain = 0.14f;
};

USTRUCT(BlueprintType)
struct FWaxHardnessZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness")
	FName ZoneName = TEXT("Core");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness")
	FVector LocalCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness", meta = (ClampMin = "5.0"))
	float LocalRadius = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness", meta = (ClampMin = "1.0"))
	float Durability = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness", meta = (ClampMin = "0.1"))
	float StrainMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness")
	bool bUseZoneAudioProfile = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hardness", meta = (EditCondition = "bUseZoneAudioProfile"))
	FWaxAudioProfile AudioProfile;
};

USTRUCT(BlueprintType)
struct FWaxButterLocomotionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimensions", meta = (ClampMin = "10.0"))
	float HalfX = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimensions", meta = (ClampMin = "10.0"))
	float HalfY = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimensions", meta = (ClampMin = "10.0"))
	float HalfZ = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Charge", meta = (ClampMin = "50.0"))
	float MinChargeStartDistance = 340.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HopAlign", meta = (ClampMin = "0.1"))
	float HopDuration = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HopAlign", meta = (ClampMin = "0.0"))
	float HopPauseDuration = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HopAlign", meta = (ClampMin = "5.0"))
	float HopHeight = 38.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HopAlign", meta = (ClampMin = "5.0", ClampMax = "180.0"))
	float MaxYawPerHopDeg = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HopAlign", meta = (ClampMin = "1.0", ClampMax = "45.0"))
	float AlignToleranceDeg = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LayDown", meta = (ClampMin = "0.1"))
	float LayDownDuration = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LayDown", meta = (ClampMin = "0.1"))
	float StandUpDuration = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "100.0"))
	float EffectiveGravity = 1350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "1.0"))
	float DriveAngularAccel = 13.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "0.5"))
	float InitialStepOmega = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float StepBounceRetention = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "1"))
	int32 MaxTumbleSteps = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float MaxCorrectionYawPerStepDeg = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "10.0", ClampMax = "720.0"))
	float SteerYawRateDeg = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tumble", meta = (ClampMin = "15.0", ClampMax = "180.0"))
	float AbortAngleDeg = 55.0f;
};

UCLASS(BlueprintType)
class WAXPPULAB_API UWaxMonsterDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UGeometryCollection> ShellCollectionAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FVector ShellRelativeOffset = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	TSubclassOf<UWaxLocomotionComponent> LocomotionClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", ToolTip = "Ignored by UWaxButterLocomotionComponent (use ButterLocomotion instead)."))
	float MoveSpeed = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float StopDistanceToPlayer = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "10.0"))
	float RollRadius = 92.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "10.0"))
	float CapsuleRadius = 92.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "10.0", ToolTip = "Ignored by UWaxButterLocomotionComponent (use ButterLocomotion.HalfZ instead)."))
	float CapsuleHalfHeight = 92.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Butter")
	FWaxButterLocomotionSettings ButterLocomotion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float CollapseRemainingRatio = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.0"))
	float CollapseReleaseStrain = 350000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction", meta = (ClampMin = "0.5"))
	float DebrisLifeSpanAfterCollapse = 5.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destruction")
	TArray<FWaxHardnessZone> HardnessZones;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TObjectPtr<UStaticMesh> CoreMeshAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TObjectPtr<UMaterialInterface> CoreMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TObjectPtr<UPhysicalMaterial> CorePhysicalMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	FVector CoreRelativeScale = FVector(1.05f, 1.05f, 1.05f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	FVector CoreRelativeOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0.0"))
	float CoreLinearDamping = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0.0"))
	float CoreAngularDamping = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	FVector CorePopVelocityMin = FVector(-90.0f, -90.0f, 110.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	FVector CorePopVelocityMax = FVector(90.0f, 90.0f, 175.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	bool bEnableCoreRevive = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0.5"))
	float CoreReviveDelaySec = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core", meta = (ClampMin = "0.1"))
	float ReviveGrowDuration = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TObjectPtr<UStaticMesh> RevivePreviewMeshAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TObjectPtr<UMaterialInterface> RevivePreviewMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	FVector RevivePreviewScale = FVector(1.85f, 1.85f, 1.85f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	TObjectPtr<USoundBase> CoreBounceSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes")
	TObjectPtr<UGeometryCollection> EyeCollectionAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes")
	TObjectPtr<UMaterialInterface> EyeMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "1000.0"))
	float EyeDamageThreshold = 180000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "1.0"))
	float EyeDurability = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "10.0"))
	float EyeForwardOffset = 82.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "5.0"))
	float EyeSpacing = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes")
	float EyeHeightOffset = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "0.05"))
	float EyeScale = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "5.0"))
	float EyeBaseHitRadius = 52.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes", meta = (ClampMin = "0.1"))
	float EyeDropTimeoutSec = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyes")
	TObjectPtr<USoundBase> EyeShatterSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	FWaxAudioProfile DefaultAudioProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TArray<FWaxAudioProfile> DevAudioProfiles;
};
