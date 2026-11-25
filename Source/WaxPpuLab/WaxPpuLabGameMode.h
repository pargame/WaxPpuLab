// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WaxPpuLabGameMode.generated.h"

/**
 *  Simple GameMode for a first person game
 */
UCLASS(abstract)
class AWaxPpuLabGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWaxPpuLabGameMode();

	int32 GetActiveDevAudioPreset() const { return ActiveDevAudioPreset; }
	void SetActiveDevAudioPreset(int32 InPreset) { ActiveDevAudioPreset = InPreset; }

private:
	/** 현재 월드에 적용 중인 개발용 오디오 프리셋 번호. 0은 기본 프로필, N(≥1)은 DevAudioProfiles[N-1]. */
	int32 ActiveDevAudioPreset = 0;
};



