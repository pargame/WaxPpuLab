# WaxPpuLab

Unreal Engine 5.9 (macOS Apple Silicon) Chaos Destruction + 물리 기반 ASMR 오디오 프로토타입(**왁뿌던전 탈출**) 프로젝트입니다.
프로젝트 불변 조건·도구는 [Docs/ProjectGuide.md](./Docs/ProjectGuide.md), 범용 언리얼 규칙은 전역 `unreal-engine` 스킬을 따릅니다.

---

## 빠른 시작 & 조작법 (PIE)

- **기본 시작 맵**: `/Game/Maps/Lvl_WaxPpuDemo`
- **기본 GameMode**: `/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode`

| 입력 | 동작 |
|---|---|
| `WASD` / 마우스 | 이동 / 시점 |
| `J` / 좌클릭 | 기본공격(펀치). 누르고 있으면 연속 콤보(첫 잽 후 R·L 무한 반복), 공격 중 누른 입력은 버퍼로 다음 타에 연결 |
| `K` / 우클릭 | 밀어내기 발차기 (애니 작업 중) |
| `SpaceBar` | 점프 |
| `T` | 3인칭 프리뷰 ↔ 1인칭 전환 (개발 테스트용 기본값 3인칭) |
| `Q` / `E` | 소환할 몬스터 전환 (`BP_WaxBall_Basic` ↔ `BP_WaxButter`) |
| `R` | 현재 선택된 몬스터를 플레이어 전방에 소환 |
| `1` / `2` | 개발용 ASMR 사운드 프리셋 전환 (`1`: WoodPlank + Heavy/BambooClick 랜덤 [기본], `2`: HardBlock + JadeMarble/SoftWaxTap 랜덤) |

- **콘솔 명령 (`UFUNCTION(Exec)`)**:
  - `SetWaxAudioPreset <1..10>` — 현재 월드 몬스터 전체의 오디오 프리셋을 즉시 전환하며, 전역 상태(`GActiveDevAudioPreset`)에 저장되어 이후 `R` 키로 새로 소환되는 몬스터에도 자동 적용됩니다.
  - `WaxAttackCapture <Basic|AerialStrike|Slam|Kick>` — 공격 타격 프레임 전후를 3인칭으로 정지 캡처합니다.

---

## 시스템 아키텍처

### 1. 플레이어 계층 (`Source/WaxPpuLab/`)
- `AWaxPpuLabCharacter` (`BP_WaxPlayer` / `BP_FirstPersonCharacter` 부모):
  - 입력을 `UWaxCombatComponent`로 전달하고, 공격 흐름 훅(`SelectAttackMontage`, `CanExecuteAttack`, `ResolveRotationProfile`)을 `IWaxCombatFlowInterface`로 노출해 BP에서 재정의할 수 있게 합니다.
  - 1인칭 카메라 머리 추종, 상반신 시선 Pitch(척추 3본 분배), 3인칭 프리뷰(`T`), 이동 중 발밑 파편 걷어차기(`UpdateDebrisKick`)
  - 튜닝 수치는 `PlayerData`(`DA_WaxPlayer`)에서 BeginPlay에 복사합니다.
  - 테스트 몬스터 소환(`SpawnDemoWaxBallInFront`) 및 오디오 프리셋 전환(`SetWaxAudioPreset`)
- `UWaxCombatComponent` (캐릭터 기본 서브오브젝트, 수치는 `DA_AttackSet_Unarmed`):
  - 단일 슬롯 입력 버퍼(최신 우선, 만료·재검증), 홀드 연타 상태 폴백, `Idle → Windup → Active → Recovery` 단계 FSM
  - 몸체 메시 본 기준 구 오버랩 타격(공격 1회당 대상 중복 제거), `MPC_WaxTargeting` 빨강 표면 마커
  - 공격 단계별 회전 제한(몸 Yaw 속도, 몸-카메라 오프셋 소프트 클램프, 감도)과 방향별 이동 속도 배율·점프 허용
  - BP 이벤트: `OnAttackStarted`, `OnAttackPhaseChanged`, `OnAttackHit`, `OnAttackEnded`, `OnBufferConsumed`
  - 로그 `LogWaxCombat`(공격·버퍼·타격, `WaxRot Session` 회전·이동 통계)
- 몽타주 노티파이: `UWaxAttackHitNotify`(타격, 구 `UWaxPunchAnimNotify`는 CoreRedirect로 호환), `Active`·`CancelWindow`·`RotationProfile` NotifyState
- `AWaxPpuLabPlayerController`, `AWaxPpuLabCameraManager`, `AWaxPpuLabGameMode`: 1인칭 입력 매핑, 카메라 피치/시점, 게임모드 기본 틀 제공.

### 2. 몬스터 계층 (`AWaxMonsterBase`)
- **평탄 1단계 상속**: `AWaxMonsterBase`(`ACharacter`) → 직계 블루프린트 자식(`BP_WaxBall_Basic`, `BP_WaxButter`)만 사용합니다.
- **공통 컴포넌트**:
  - `BodyPivot`: 껍질·코어·눈알이 함께 부착되는 중심 피벗(눈알은 몸체에 고정).
  - `ShellCollection` (`UGeometryCollectionComponent`): Chaos 파괴 왁스 껍질(붕괴 시 그림자 자동 OFF).
  - `CoreMesh` + `AWaxMonsterCore`: 붕괴 시 탄성 물리(`PM_JellyBounce`)로 튀며 방출되는 코어 및 부활 루프.
  - `EyeLeftGC` / `EyeRightGC`: 껍질과 독립된 강도·내구도로 개별 파쇄 가능한 Chaos 눈알.
  - `WaxDestructible` (`UWaxDestructibleComponent`): 2단계 국소 파괴 필드(`Chaos_Object_Dynamic` 전환 → `Chaos_ExternalClusterStrain` 인가), 리프 잔존율 추적 및 임계 붕괴, 경도 존, 물리 기반 MetaSound 오디오.
- **전략 로코모션 (`UWaxLocomotionComponent`)**:
  - `UWaxRollingLocomotionComponent`: `BP_WaxBall_Basic`용 실제 변위(`Δp`)·바닥 법선 외적 기반 물리 연동 구르기.
  - `UWaxButterLocomotionComponent`: `BP_WaxButter`용 `HopAlign → LayDown → TumbleCharge → StandUp` 4단계 FSM 및 전·후면 최단각 양방향 텀블링(`TumbleDirectionSign`). 굴림 목표는 눕는 순간의 플레이어 위치와 실시간 위치를 `TargetTrackingRatio`로 섞어 정합니다.

### 3. 핵심 서브시스템 설계
- **고정 수명 파편 제거 (`RemoveOnBreak`)**: `DF_WaxOrb`, `DF_WaxButter`, `DF_MonsterEye` Dataflow에 `FRemoveOnBreakDataflowNode`(`PostBreakTimer 1.5~2.0s`, `RemovalTimer 0.35~0.45s`, `ScaleOnRemoval=True`)를 삽입하고 `bAllowRemovalOnBreak=true`, `bAllowRemovalOnSleep=false`로 고정해 바닥 잔류 파편·소음을 차단합니다.
- **물리 기반 하이브리드 ASMR 오디오**: `MS_WaxCrackASMR` 프리셋과 `FWaxCollisionVariant` 구조체로 파괴음과 파편 충돌음을 이원화합니다. 파괴 시 다중 그레인 클리핑 방지를 위해 `QueueDensityScale`을 적용하고, 충돌음은 `AccumulatedImpulse`·`Mass`·`NormalSpeed`에서 산출한 음향 압력($\sqrt{E}$)에 Weber-Fechner 로그 라우드니스 압축과 지수 분포 쿨다운을 적용합니다.
- **Subsurface 밀랍 셰이딩 & 순수 발광 타게팅**:
  - `M_WaxButter`: `Subsurface` 셰이딩(따뜻한 노란 밀랍 + `Roughness 0.18` / `Specular 0.60`). `MPC_WaxTargeting` 구체 마스크 내부에서는 `BaseColor`와 `SubsurfaceColor`를 `0`으로 차단하고 `MarkerColor * EmissiveBoost`만 다이렉트 출력해 바탕 왁스 색과 무관하게 선명한 조준 표시를 유지합니다.
  - `M_ButterCore` / `M_JellyCore`: 노란 겉 왁스와 대비되는 핑크·로즈 러프 무광 질감(`Roughness 0.84`, `Specular 0.12`)으로 통일했습니다.

---

## 콘텐츠 & 에셋 맵

| 경로 | 주요 에셋 |
|---|---|
| `/Game/WaxPpu/Audio/Foley/` | CC0 충격음 기반 `SW_*` 사운드 웨이브 |
| `/Game/WaxPpu/Audio/MetaSounds/` | `MS_WaxCrackASMR` 및 프리셋(`WoodPlank`, `HardBlock`, `Heavy`, `BambooClick`, `JadeMarble`, `SoftWaxTap`), `SC_WaxCrack` |
| `/Game/WaxPpu/Blueprints/` | `BP_WaxBall_Basic`, `BP_WaxButter` |
| `/Game/WaxPpu/Data/` | `DA_WaxBall_Basic`, `DA_WaxButter` (`UWaxMonsterDataAsset`), `DA_WaxPlayer` (`UWaxPlayerDataAsset`), `DA_AttackSet_Unarmed` (`UWaxAttackSetDataAsset`) |
| `/Game/WaxPpu/Destruction/Dataflow/` | `DF_WaxOrb`, `DF_WaxButter`, `DF_MonsterEye` |
| `/Game/WaxPpu/Destruction/GeometryCollections/` | `GC_WaxOrb`, `GC_WaxButter`, `GC_MonsterEye` |
| `/Game/WaxPpu/Materials/` | `M_WaxButter`, `M_ButterCore`, `M_JellyCore`, `M_MonsterEye`, `MPC_WaxTargeting` |
| `/Game/WaxPpu/Physics/` | `PM_JellyBounce` |
| `/Game/Maps/` | `Lvl_WaxPpuDemo` |
| `/Game/WaxPpu/Player/` | `BP_WaxPlayer`, `ABP_WaxPlayer`(몸체), `ABP_WaxPlayer_FP`(1인칭 포즈 복사) |
| `/Game/WaxPpu/Animations/` | `AM_FP_Punch_L/R`(펀치 몽타주), `AS_FP_Punch_L/R` |
| `/Game/FirstPerson/` | `Blueprints/BP_FirstPersonCharacter`, `BP_FirstPersonGameMode`, `Anims/ABP_FP_Copy` |
| `/Game/Input/` | `IMC_Default`, `Actions/IA_*` |

---

## 튜닝값 위치 및 수정 가이드

체감·튜닝 수치는 **DataAsset이 유일한 원본**입니다. 캐릭터·몬스터·컴포넌트에 같은 이름으로 보이는 필드는 BeginPlay/초기화 때 DA에서 복사된 읽기 전용(`VisibleInstanceOnly`) 런타임 값이므로, 수정은 항상 아래 DA에서 합니다. BP에는 입력 액션·컴포넌트·에셋 참조와 디버그 토글만 둡니다.

> **팁 (일괄 비교·수정)**: 콘텐츠 브라우저에서 `DA_WaxBall_Basic`, `DA_WaxButter`를 함께 선택 → 우클릭 → **Asset Actions → Edit Selection in Property Matrix**로 표 형태 일괄 편집이 가능합니다.

### 1. 몬스터별 스펙 — DataAsset (`/Game/WaxPpu/Data/DA_WaxBall_Basic`, `DA_WaxButter`)

| 카테고리 | 주요 파라미터 | 설명 |
|---|---|---|
| **Visual** | `ShellCollectionAsset`, `ShellRelativeOffset` | Chaos 껍질 에셋 및 `BodyPivot` 기준 오프셋 |
| **Movement** | `LocomotionClass`, `MoveSpeed`, `StopDistanceToPlayer`, `bChasePlayer`, `RollRadius`, `CapsuleRadius`, `CapsuleHalfHeight` | 로코모션 클래스, 이동 속도, 공격 대기 거리, 추적 여부, 구르기 반경, 캡슐 크기 |
| **HitStop** | `bEnableHitSlowdown`, `HitSlowdownDuration`, `HitSlowdownSpeedMultiplier` | 피격 시 잠깐 느려지는 반응 |
| **Movement \| Butter** | `ButterLocomotion` (`HalfX/Y/Z`, `MinChargeStartDistance`, `HopDuration/Pause/Height`, `MaxYawPerHopDeg`, `AlignToleranceDeg`, `LayDownDuration`, `StandUpDuration`, `EffectiveGravity`, `DriveAngularAccel`, `InitialStepOmega`, `StepBounceRetention`, `MaxTumbleSteps`, `MaxCorrectionYawPerStepDeg`, `SteerYawRateDeg`, `AbortAngleDeg`, `TargetTrackingRatio`) | **왁스버터 전용 튜닝값 전부** (`UWaxButterLocomotionComponent` 런타임 필드는 읽기 전용이며 반드시 `DA_WaxButter.ButterLocomotion`에서 수정) |
| **Destruction** | `CollapseRemainingRatio`, `CollapseReleaseStrain`, `DebrisLifeSpanAfterCollapse`, `HardnessZones` | 붕괴 임계 잔여율(기본 `0.30`), 붕괴 방출 스트레인, 파편 수명, 부위별 경도/오디오 존 |
| **Core** | `CoreMeshAsset`, `CoreMaterial`, `CorePhysicalMaterial`, `CoreRelativeScale/Offset`, `CoreLinearDamping`, `CoreAngularDamping`, `CorePopVelocityMin/Max`, `bEnableCoreRevive`, `CoreReviveDelaySec`, `ReviveGrowDuration`, `RevivePreviewMeshAsset/Material/Scale`, `CoreBounceSound` | 코어 외형·탄성·팝 속도·감쇠, 자동 부활 ON/OFF 및 재생 프리뷰 설정 |
| **Eyes** | `EyeCollectionAsset`, `EyeMaterial`, `EyeDamageThreshold`, `EyeDurability`, `EyeForwardOffset`, `EyeSpacing`, `EyeHeightOffset`, `EyeScale`, `EyeBaseHitRadius`, `EyeDropTimeoutSec`, `EyeShatterSound` | 눈알 Chaos 에셋, 파괴 임계치·타격 내구도, 배치 오프셋·크기, 붕괴 시 낙하 파쇄 타임아웃 |
| **Audio** | `DefaultAudioProfile`, `DevAudioProfiles` | 기본 파괴·충돌 하이브리드 프로파일 및 개발용 프리셋 목록 (입력 슬롯은 `1..9, 0` 최대 10개 지원, 현재 `1`·`2`번 2개 할당) |
| **DestructionAudio** | `BreakGrainSpreadMin/MaxSec`, `MaxQueuedGrains`, `CollapseGrainSpreadMaxSec`, `CollapseMaxQueuedGrains`, `CollisionSoundSpeedThreshold`, `MinCollisionAudioInterval`, `CollisionSpatialCellSize/CooldownSec`, `CollisionCrackle/Snap/BodyScale` | 깨짐 그레인 분산·충돌음 빈도·레이어 볼륨 |
| **Collapse** | `bAllowKickAfterCollapse`, `bMuteCollisionAudioAfterCollapse`, `CollapseCollisionMaxGraceSec` | 붕괴 후 파편 차기·충돌음 처리 |

> **왁스버터(`DA_WaxButter`) 주의사항**
> - `UWaxButterLocomotionComponent` 사용 시 **`MoveSpeed`와 `CapsuleHalfHeight`는 무시**됩니다.
> - 버터 구르기 속도·기둥 높이는 반드시 **`ButterLocomotion`** (`EffectiveGravity`, `DriveAngularAccel`, `InitialStepOmega`, `HalfX/Y/Z`)에서 수정하세요.
> - `TargetTrackingRatio`(0~1, 기본 0): 굴림 목표를 눕는 순간의 플레이어 위치와 현재 플레이어 위치 사이에서 섞습니다. 0이면 눕는 순간의 지점으로만 가고, 1이면 매 굴림마다 실시간 위치를 따라갑니다(이전 방식). 목표의 `StopDistanceToPlayer` 이내 도착, 이탈 각도(`AbortAngleDeg`) 초과, 막힘, `MaxTumbleSteps` 도달 시 일어섭니다.
> - `StopDistanceToPlayer`는 버터에서도 구르기 종료 및 최소 돌진 기준(`Max(StopDistanceToPlayer, MinChargeStartDistance)`)에 정상 반영됩니다.

### 2. 플레이어 — DataAsset (`/Game/WaxPpu/Data/DA_WaxPlayer`)

| 카테고리 | 주요 파라미터 |
|---|---|
| **Camera** | `CameraBaseOffset`, `HeadFollowAlpha`, `HeadFollowInterpSpeed`, `MaxHeadForwardOffset`, `HeadSocketName`, `ThirdPersonPreviewOffset` |
| **UpperBody** | `bEnableUpperBodyPitch`, `AimPitchInterpSpeed`, `MaxUpperBodyPitchDeg`, `Spine01/02/03PitchWeight` |
| **DebrisKick** | `bEnableDebrisKick`, `DebrisKickRadius`, `DebrisKickStrength`, `DebrisKickUpwardBoost`, `DebrisKickMinMoveSpeed` |

### 3. 공격 — DataAsset (`/Game/WaxPpu/Data/DA_AttackSet_Unarmed`)

| 항목 | 주요 파라미터 |
|---|---|
| **Basic / Kick** (`FWaxAttackSpec`) | `Montages`, `PlayRate`, `StrainScale`, `RadiusScale`, `ForwardOffset`, 넉백·파편 임펄스, `Windup/Active/RecoveryRotation`(몸 회전 속도·몸-카메라 오프셋·감도), `Windup/Active/RecoveryMovement`(앞·옆·뒤 이동 속도 배율, 점프 허용) |
| **JumpAttack** | `AerialStrike`, `Slam`, `HoldThresholdSec`, `SlamMinHeight` 등 |
| **Settings** | `BaseImpactRadius`, `BaseImpactStrain`, `BufferWindowSec`, `ComboResetSec`, 회전 복귀·감도 블렌드, `MoveScaleBlendSpeed`, `OffsetFalloffCurve`, `Marker*`(색·지속·발광·불투명도·번짐) |

공격 타이밍(타격·Active·CancelWindow·RotationProfile)은 펀치 몽타주의 노티파이로 조정합니다. 상세는 [공격 시스템 설계](Docs/Design/CombatAttackDesign.md).

### 4. BP에 남는 항목
- **플레이어 BP** (`BP_WaxPlayer`, `BP_FirstPersonCharacter`): `PlayerData`, 입력 액션, `bThirdPersonPreview`(개발용 기본 3인칭, T 토글), `TestMonsterClass(es)`, `WaxSpawnDistance`, Combat 컴포넌트의 `AttackSet`·`TargetingMPC`·`bShowImpactMarker`·`bShowDebugImpact`·로그 토글
- **몬스터 BP** (`BP_WaxBall_Basic`, `BP_WaxButter`): `MonsterData`, `WaxDestructible.bShowDebugCounters`

---

## 렌더링 예산 & 빌드 (macOS)

### M4 16GB 통합 메모리 렌더링 설정 (`Config/DefaultEngine.ini`)
- **유지**: Lumen 소프트웨어 GI(`r.DynamicGlobalIlluminationMethod=1`), Lumen 소프트웨어 반사(`r.ReflectionMethod=1`), Virtual Shadow Maps(`r.Shadow.Virtual.Enable=1`), Nanite(`r.Nanite.ProjectEnabled=True`), Substrate.
- **비활성화 (6개 CVar)**:
  - `r.Lumen.HardwareRayTracing=False`
  - `r.Lumen.Reflections.HardwareRayTracing.Translucent.Refraction.EnableForProject=False`
  - `r.RayTracing.Shadows=False`
  - `r.RayTracing=False`
  - `r.RayTracing.RayTracingProxies.ProjectEnabled=False`
  - `r.PathTracing=False`

### macOS 증분 빌드
Xcode가 여러 버전 설치된 환경이라면 먼저 `DEVELOPER_DIR`을 UE 5.9 호환 Xcode로 맞춥니다(로컬 머신의 `ue-on` 셸 함수는 선택 사항).

```bash
# (선택) Xcode 툴체인 지정 예시
export DEVELOPER_DIR="/Applications/Xcode-26.1.1.app/Contents/Developer"

"/Users/Shared/Epic Games/UE_5.9/Engine/Build/BatchFiles/Mac/Build.sh" \
  WaxPpuLabEditor Mac Development \
  -Project="$(pwd)/WaxPpuLab.uproject" -WaitMutex
```

> **Git 협업 주의**: 이 저장소는 Git LFS 없이 `.uasset` 바이너리를 직접 추적합니다. 동일 `.uasset`의 동시 편집을 피하세요. `Content/Collections/`, `Content/Developers/`는 `.gitignore`로 제외되어 있습니다.

## 라이선스

이 저장소의 코드와 직접 제작한 에셋은 [MIT License](LICENSE)를 따릅니다. Unreal Engine 엔진과 Epic Games가 제공한 콘텐츠는 이 라이선스에 포함되지 않으며 [Unreal Engine EULA](https://www.unrealengine.com/eula)를 따릅니다.
