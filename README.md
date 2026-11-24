# WaxPpuLab

Unreal Engine 5.9 (macOS Apple Silicon) Chaos Destruction + ASMR 오디오 기반 **왁뿌던전 탈출** 프로토타입 프로젝트입니다.

## 조작법 (PIE)

| 키 | 동작 |
|---|---|
| `WASD` / 마우스 | 이동 / 시점 |
| `SpaceBar` / 좌클릭 | 왁스 및 눈알 깎기(홀드 연타 지원) |
| `Q` / `E` | 소환할 몬스터 전환 (`BP_WaxBall_Basic` ↔ `BP_WaxButter`) |
| `R` | 현재 선택 몬스터 플레이어 앞 소환 |
| `1` ~ `9`, `0` | 개발용 ASMR 파괴 사운드 프리셋 전환 (10종) |

## 몬스터 아키텍처

- **평탄 1단계 상속**: `AWaxMonsterBase` (`ACharacter`) → 직계 블루프린트 자식(`BP_WaxBall_Basic`, `BP_WaxButter`)만 사용합니다.
- **공통 컴포넌트 (`AWaxMonsterBase`)**:
  - `BodyPivot`: 껍질·코어·눈알이 함께 부착되는 중심 피벗(눈알은 몸체 한 곳에 고정).
  - `ShellCollection` (`UGeometryCollectionComponent`): Chaos 파괴 왁스 껍질(붕괴 시 그림자 자동 OFF).
  - `CoreMesh` + `AWaxMonsterCore`: 붕괴 시 탄성 물리(`PM_JellyBounce`)로 튀며 떨어지는 코어 및 부활 시스템.
  - `EyeLeftGC` / `EyeRightGC`: 독립 강도·내구도로 직접 조각낼 수 있는 Chaos 눈알(그림자 OFF).
  - `WaxDestructible` (`UWaxDestructibleComponent`): 국소 크레이터 파괴, 리프 파괴율 추적, 경도 존, 실시간 MetaSound 오디오.
- **전략 로코모션 (`UWaxLocomotionComponent`)**:
  - `UWaxRollingLocomotionComponent`: `BP_WaxBall_Basic`용 실제 변위(`Δp`)·바닥 법선 기반 물리 연동 구르기.
  - `UWaxButterLocomotionComponent`: `BP_WaxButter`용 `HopAlign → LayDown → TumbleCharge → StandUp` 기둥형 큐브 구르기 돌진.

---

## 튜닝값 위치 및 수정 가이드

언리얼 에디터에서 별도 C++ 수정 없이 아래 에셋/컴포넌트의 디테일 패널에서 모든 튜닝값을 수정할 수 있습니다.

> **팁 (여러 몬스터 일괄 비교·수정)**: 콘텐츠 브라우저에서 `DA_WaxBall_Basic`, `DA_WaxButter`를 함께 선택 → 우클릭 → **Asset Actions → Edit Selection in Property Matrix**로 열면 표 형태로 한 번에 수정할 수 있습니다.

### 1. 몬스터별 스펙 — DataAsset (`/Game/Demo/Data/DA_WaxBall_Basic`, `DA_WaxButter`)

| 카테고리 | 주요 파라미터 | 설명 |
|---|---|---|
| **Visual** | `ShellCollectionAsset`, `ShellRelativeOffset` | Chaos 껍질 에셋 및 `BodyPivot` 기준 오프셋 |
| **Movement** | `LocomotionClass`, `MoveSpeed`, `StopDistanceToPlayer`, `RollRadius`, `CapsuleRadius`, `CapsuleHalfHeight` | 로코모션 클래스, 이동 속도, 공격 대기 거리(이 거리 이내에서는 이동 중단), 구르기 반경, 캡슐 크기 |
| **Movement \| Butter** | `ButterLocomotion` (`HalfX/Y/Z`, `MinChargeStartDistance`, `HopDuration/Pause/Height`, `MaxYawPerHopDeg`, `AlignToleranceDeg`, `LayDownDuration`, `StandUpDuration`, `EffectiveGravity`, `DriveAngularAccel`, `InitialStepOmega`, `StepBounceRetention`, `MaxTumbleSteps`, `MaxCorrectionYawPerStepDeg`, `SteerYawRateDeg`, `AbortAngleDeg`) | **왁스버터 전용 튜닝값 전부** (`UWaxButterLocomotionComponent`의 런타임 필드는 읽기 전용이며, 반드시 `DA_WaxButter.ButterLocomotion`에서 수정) |
| **Destruction** | `CollapseRemainingRatio`, `CollapseReleaseStrain`, `DebrisLifeSpanAfterCollapse`, `HardnessZones` | 붕괴 임계 잔여율(기본 `0.30`), 붕괴 시 방출 스트레인, 파편 수명, 부위별 경도/오디오 존 |
| **Core** | `CoreMeshAsset`, `CoreMaterial`, `CorePhysicalMaterial`, `CoreRelativeScale/Offset`, `CoreLinearDamping`, `CoreAngularDamping`, `CorePopVelocityMin/Max`, `bEnableCoreRevive`, `CoreReviveDelaySec`, `ReviveGrowDuration`, `RevivePreviewMeshAsset/Material/Scale`, `CoreBounceSound` | 코어 외형·탄성·팝 속도·감쇠, 자동 부활 ON/OFF 및 재생 프리뷰 설정 |
| **Eyes** | `EyeCollectionAsset`, `EyeMaterial`, `EyeDamageThreshold`, `EyeDurability`, `EyeForwardOffset`, `EyeSpacing`, `EyeHeightOffset`, `EyeScale`, `EyeBaseHitRadius`, `EyeDropTimeoutSec`, `EyeShatterSound` | 눈알 Chaos 에셋, 파괴 임계치·타격 내구도, 배치 오프셋·크기, 붕괴 시 낙하 파쇄 타임아웃 |
| **Audio** | `DefaultAudioProfile`, `DevAudioProfiles` | 기본 파괴 사운드 프로파일 및 개발용 1~0번 프리셋 목록 |

> **왁스버터(`DA_WaxButter`) 주의사항**
> - `UWaxButterLocomotionComponent`를 사용할 때는 **`MoveSpeed`와 `CapsuleHalfHeight`가 무시**됩니다.
> - 버터의 구르기 속도·기둥 높이는 반드시 **`ButterLocomotion`** (`EffectiveGravity`, `DriveAngularAccel`, `InitialStepOmega`, `HalfX/Y/Z`)에서 수정하세요.
> - `StopDistanceToPlayer`는 버터에서도 구르기 종료 및 최소 돌진 기준(`Max(StopDistanceToPlayer, MinChargeStartDistance)`)에 정상 반영됩니다.

### 2. 공통 몬스터 BP — `/Game/Demo/Blueprints/BP_WaxBall_Basic`, `BP_WaxButter`

- **루트(`AWaxMonsterBase`) 디테일**:
  - `MonsterData`: 참조할 DataAsset 연결
  - `bChasePlayer`: 플레이어 추적 ON/OFF (레벨 배치 테스트용)
  - `bEnableHitSlowdown`, `HitSlowdownDuration`, `HitSlowdownSpeedMultiplier`: 피격 시 일시 감속 연출
- **`WaxDestructible` 컴포넌트 디테일**:
  - `WaxDestructible | Audio`: `BreakGrainSpreadMin/MaxSec`, `MaxQueuedGrains`, `CollapseGrainSpreadMaxSec`, `CollisionSoundSpeedThreshold`, `MinCollisionAudioInterval`, `CollisionSpatialCellSize/CooldownSec`, `CollisionCrackle/Snap/BodyScale`
  - `WaxDestructible | Collapse`: `bAllowKickAfterCollapse`, `bMuteCollisionAudioAfterCollapse`, `CollapseCollisionMaxGraceSec`
  - `WaxDestructible | Debug`: `bShowDebugCounters` (화면 디버그 카운터 ON/OFF)

### 3. 플레이어 공격·타게팅 — `/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter`

- `WaxDemo | Targeting`: `TargetingMPC`, `WaxTargetForwardDistance`, `WaxImpactRadius`, `bShowDebugTargetSphere`, `TargetDecalColor/EmissiveBoost/Opacity/EdgeSoftness`
- `WaxDemo | Impact`: `WaxImpactStrain` (기본 `320,000`)
- `WaxDemo | DebrisKick`: `bEnableDebrisKick`, `DebrisKickRadius`, `DebrisKickStrength`, `DebrisKickUpwardBoost`, `DebrisKickMinMoveSpeed`
- `WaxDemo | Spawn`: `TestMonsterClass`, `TestMonsterClasses`, `WaxSpawnDistance`

## 빌드 (macOS)

```bash
source ~/.zshrc && ue-on
"/Users/Shared/Epic Games/UE_5.9/Engine/Build/BatchFiles/Mac/Build.sh" \
  WaxPpuLabEditor Mac Development \
  -Project="$(pwd)/WaxPpuLab.uproject" -WaitMutex -FromMsBuild
```
