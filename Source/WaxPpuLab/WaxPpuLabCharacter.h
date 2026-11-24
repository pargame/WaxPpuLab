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

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;

	/** Wax Crack Input Action (use Pulse trigger in IA asset for hold-repeat) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> CrackAction;

	/** Spawn Test Monster Input Action */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SpawnTestMonsterAction;

	/** Dev Audio Preset Input Action (Axis1D: keys 1..9, 0 -> 1..10 via Scalar modifier) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AudioPresetAction;

	/** Switch Test Monster Input Action (Axis1D: Q = -1, E = +1) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SwitchMonsterAction;
	
public:
	AWaxPpuLabCharacter();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="WaxDemo")
	void DoWaxCrackTrace();

	UFUNCTION(BlueprintCallable, Category="WaxDemo")
	void SpawnDemoWaxBallInFront();

	UFUNCTION(Exec, Category="WaxDemo|Audio")
	void SetWaxAudioPreset(int32 PresetIndex);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WaxDemo|Targeting")
	TObjectPtr<UMaterialParameterCollection> TargetingMPC;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Targeting", meta = (ClampMin = "30.0"))
	float WaxTargetForwardDistance = 185.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Targeting", meta = (ClampMin = "5.0"))
	float WaxImpactRadius = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Targeting")
	bool bShowDebugTargetSphere = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Targeting")
	FLinearColor TargetDecalColor = FLinearColor(1.0f, 0.01f, 0.01f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Targeting", meta = (ClampMin = "0.0"))
	float TargetDecalEmissiveBoost = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Targeting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetDecalOpacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Targeting", meta = (ClampMin = "0.1"))
	float TargetDecalEdgeSoftness = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Impact", meta = (ClampMin = "1000.0"))
	float WaxImpactStrain = 320000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|DebrisKick")
	bool bEnableDebrisKick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|DebrisKick", meta = (ClampMin = "10.0"))
	float DebrisKickRadius = 85.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|DebrisKick", meta = (ClampMin = "0.0"))
	float DebrisKickStrength = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|DebrisKick", meta = (ClampMin = "0.0"))
	float DebrisKickUpwardBoost = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|DebrisKick", meta = (ClampMin = "0.0"))
	float DebrisKickMinMoveSpeed = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WaxDemo|Spawn")
	TSubclassOf<AWaxMonsterBase> TestMonsterClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WaxDemo|Spawn")
	TArray<TSubclassOf<AWaxMonsterBase>> TestMonsterClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WaxDemo|Spawn", meta = (ClampMin = "80.0"))
	float WaxSpawnDistance = 260.0f;

private:
	FVector GetFloatingTargetSphereCenter() const;
	void UpdateDebrisKick();

	int32 SelectedMonsterIndex = 0;

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

