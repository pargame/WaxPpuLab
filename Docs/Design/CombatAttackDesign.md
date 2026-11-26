# 공격 시스템 설계 (기본공격 · 점프공격 · 밀어내기 발차기)

상태: **P0 완료 · P0.5 완료(회전·이동 제한 체감 튜닝 반영)** · 다음 P1 발차기(H1 애니 다운로드 필요) (설계: Claude–Gemini 2회 협업 검토 반영)

## 1. 확정된 결정

| 항목 | 결정 |
|---|---|
| 공격 3종 | ① 기본공격 ② 점프공격 ③ 밀어내기 발차기 |
| 무기 장착 시 | ①·② 교체, ③은 공통 유지 |
| 발차기 애니 소스 | 외부 다운로드(Mixamo/Fab) → AI가 IK 리타겟·몽타주화 |
| 점프공격 | 하이브리드: 공중 **탭 = 공중 강타**, 공중 **홀드 = 내려찍기 슬램**(착지 광역 파괴) |
| 발차기 효과 | 넉백 + 약한 Strain 파괴 + 주변 파편 임펄스 |
| 공격 중 회전 | 혼합: 몸·타격 방향은 단계별 최대 회전속도(Yaw/Pitch) 제한, 카메라는 타격 구간에서만 감도 배율 하향 |
| 구현 원칙 | 몽타주 기반 단계 표현. **세부 논리는 C++, 전체 흐름·연출은 디자이너가 수정 가능한 BP/에셋으로 노출** |
| 1인칭 연출 | FirstPersonMesh는 SKM_Manny 전신(`ABP_WaxPlayer_FP` CopyPoseFromMesh) → 다리 실제 노출 여부는 H0 실측 후 결정 |

## 2. 실측으로 확인한 현재 구조 (설계 시점 기준, 변경 결과는 §6~7)

| 사실 | 근거 | 설계 영향 |
|---|---|---|
| 몽타주는 몸체 메시(`ABP_WaxPlayer`)에서 재생, FP 메시는 포즈 복사만 | `ABP_WaxPlayer_FP` = `CopyPoseFromMesh` | 노티파이는 몸체 메시에서 1회만 발생(중복 없음) |
| 펀치 몽타주 슬롯 = `DefaultSlot`, ABP에 `LayeredBoneBlend(BranchFilter)` 존재 | `AM_FP_Punch_*`, `ABP_WaxPlayer` 문자열 | DefaultSlot이 상체 마스크 뒤에 있으면 **발차기·슬램 다리가 재생되지 않음** → 슬롯 분리 필요 (§3) |
| 몬스터 이동: 구르기 = 매 틱 `AddMovementInput`, 버터 = FSM이 `AddActorWorldOffset`로 직접 이동 | `WaxLocomotionComponent.cpp:82, 479, 523, 588` | 입력만 막으면 넉백이 상쇄됨 → 로코모션 틱 전체 일시 정지 필요 |
| 플레이어 이동 기준이 액터 전방 | `WaxPpuLabCharacter.cpp:169-170` | 공격 중 몸 Yaw가 지연되면 WASD가 쏠림 → Control Yaw 기준으로 변경 |
| `PythonScriptPlugin`은 기본 비활성이고 `.uproject`에 등록되어 있지 않음 | 엔진 `.uplugin` `EnabledByDefault: false` | 헤드리스 검증·에셋 자동화의 **선행조건** → P0에서 활성화 |
| 타격 원점은 FP 메시 소켓 우선, 없으면 몸 메시 | `WaxPpuLabCharacter.cpp:434-441` | FP 메시는 3인칭 프리뷰에서 숨겨져 소켓 갱신이 멈출 수 있고 종속 메시라 포즈 지연 소지 → **판정은 몸 메시 기준으로 통일**, 무기 소켓 원점 추가 |

## 3. 아키텍처

```
AWaxPpuLabCharacter
 └─ UWaxCombatComponent          (공격 상태·입력 판정·쿨다운·콤보·버퍼)
      ├─ UnarmedSet : UWaxAttackSetDataAsset   (DA_AttackSet_Unarmed)
      └─ EquippedWeapon : UWaxWeaponDataAsset  (nullable, ①·② 오버라이드 + 무기 메시/소켓)

Montage ─ UWaxAttackHitNotify    (UWaxPunchAnimNotify 일반화, CoreRedirect로 기존 몽타주 호환)
            └─ Combat->HandleHit(NotifyParams)
                  ├─ 원점: Source{Body | Weapon} + Socket
                  ├─ 구 오버랩 → ReceiveWaxImpact / ApplyWaxImpact (공격 1회당 대상 중복 제거)
                  ├─ 넉백 → AWaxMonsterBase::ReceiveKnockback(Velocity, Duration)
                  ├─ 파편 → 기존 UpdateDebrisKick 임펄스 로직 재사용
                  └─ 마커/디버그 구 (기존 MPC 마커 재사용)
```

- **슬롯 해석(C++ 고정)**: `Resolve(Slot) = Weapon && Weapon->Overrides(Slot) ? Weapon->Spec(Slot) : UnarmedSet->Spec(Slot)`. 무기 DA에는 Kick 필드가 아예 없음.
- **`FWaxAttackSpec`**: `Montages[]`(콤보 순환), `PlayRate`, `StrainScale`, `RadiusScale`, `ForwardOffset`, `KnockbackHorizontal/Vertical`, `KnockbackDurationSec`, `DebrisImpulse`, `DebrisImpulseRadius`, `bSuppressHeadFollow`, `CameraShake`(선택).
- **`FWaxJumpAttackSpec`** (Unarmed DA·무기 DA 공유): `AerialStrike`(Spec), `Slam`(Spec), `HoldThresholdSec`, `SlamDownwardSpeed`, `SlamMinHeight`(미만이면 공중 강타로 폴백), `SlamAirControl`, `SlamLandSectionName`(FName).
- **배율 합성**: 최종값 = 캐릭터/DA 기본 × Spec × Notify (곱). Notify 배율은 한 몽타주 내 타격별 강약용.
- **입력 슬롯 enum**: `{Basic, AerialStrike, Slam, Kick}`.
- **애님 슬롯 분리**
  - `UpperBody` 슬롯: 기본공격(하체는 로코모션 유지, 이동 중 펀치 가능).
  - `FullBody` 슬롯: 발차기, 공중 강타, 슬램(LayeredBoneBlend 뒤, 최종 출력 직전).
  - 기존 펀치 몽타주의 `DefaultSlot`은 `UpperBody`로 이전.
- **슬램 섹션**: `Start → Loop → Land`. 캐릭터 `LandedDelegate`를 Combat이 구독해서 `Montage_JumpToSection("Land")`를 호출하고, Land의 HitNotify가 발밑 광역 판정을 냄.
- **몬스터 넉백**: `AWaxMonsterBase::ReceiveKnockback`이 `LaunchCharacter` + `Locomotion->SuspendFor(Duration)` 호출. `SuspendFor` 동안 로코모션 틱 전체 정지, 버터는 재개 시 `HopAlign`부터 재진입.
- **타격 원점**: 항상 몸 메시(`GetMesh()`) 본/소켓 또는 몸 메시에 부착된 무기 메시 소켓. 무기 메시는 FP 메시·몸 메시 양쪽에 부착(가시성용), 판정은 몸 쪽만.
- **[ProjectGuide](../ProjectGuide.md) 준수**: `/Game/` 하드코딩 0건, 수치는 전부 DA/UPROPERTY, 런타임 복사 필드는 `VisibleInstanceOnly`.

### C++ / BP / 에셋 책임 분리

| 계층 | 소유 | 내용 |
|---|---|---|
| C++ (`UWaxCombatComponent`, 캐릭터가 `CreateDefaultSubobject`) | 세부 논리 | 입력 버퍼, `CanExecute` 재검증, 단계 FSM 전이(`Idle→Windup→Active→Recovery`), 히트 판정·대상 중복 제거, 넉백, 회전 제한 계산 |
| C++ → BP 오버라이드 지점 (`BlueprintNativeEvent`, **캐릭터에 선언**) | 흐름 결정 | `SelectAttackMontage(Slot, ResolvedSpec, ComboIndex)`, `CanExecuteAttack(Slot)`(추가 조건), `ResolveRotationProfile(Phase)` — `_Implementation`은 컴포넌트 기본 구현에 위임, 디자이너는 `BP_WaxPlayer`에서만 재정의. 컴포넌트는 오너 캐스트 실패 시 자체 기본 구현으로 폴백. 콤보 인덱스는 선택 직후 C++가 갱신 |
| C++ → BP 이벤트 (`BlueprintAssignable` 델리게이트) | 연출 | `OnAttackStarted`, `OnAttackPhaseChanged`, `OnAttackHit`(위치·대상·강도), `OnAttackEnded`, `OnBufferConsumed` → 카메라 셰이크·VFX·SFX·히트스톱은 BP 이벤트 그래프에서 구성 |
| 몽타주 (AnimNotifyState) | 타이밍 | `Active` 구간 1개(앞 = Windup, 뒤 = Recovery 자동 추론), `CancelWindow(AllowedTypes)`, `RotationProfile`(구간별 오버라이드), `AttackHit`(Notify), 슬램 섹션 |
| DataAsset | 수치 | 공격 Spec, 단계별 기본 회전·이동 프로파일, 전역 Combat 설정(`BufferWindowSec`, `SensitivityBlendSpeed`, `RecoverYawRateDegPerSec`, `RecoverSnapToleranceDeg`, `OffsetFalloffCurve`), 넉백, 반경·강도 |

원칙: C++에는 수치·에셋 경로·연출 분기를 두지 않는다. 디자이너는 C++ 수정 없이 몽타주 타임라인, DA, BP 이벤트만으로 공격 하나의 체감을 끝까지 조정할 수 있어야 한다.

### 공격 중 회전 제한 (혼합)

- **몸(Yaw)**: 공격 동안 `bUseControllerRotationYaw = false`. 몸 Yaw는 컨트롤 Yaw를 향해 `MaxYawRateDegPerSec`로 등속 추종. 공격 종료 후 `RecoverYawRateDegPerSec`로 정렬하고 `RecoverSnapToleranceDeg` 이내면 컨트롤 추종 복귀. 복귀 중 다음 공격이 오면 현재 몸 Yaw에서 연속 진행(스냅 없음).
- **상반신(Pitch)**: 목표 = `Clamp(ControlPitch, ±MaxUpperBodyPitchDeg)`. 공격 중에는 `MaxPitchRateDegPerSec` 상한으로 추종. 타격 Pitch는 실제 상반신 Pitch.
- **카메라 소프트 클램프**(하드 스톱 없음): `DoAim`의 축별 실효 감도
  - 오프셋이 **커지는 방향**: `CameraSensitivityScale × OffsetFalloffCurve(|오프셋| / MaxOffset)` + 1프레임 플릭이 `MaxOffset`을 넘지 않도록 적용 델타 상한 클램프.
  - 오프셋이 **줄어드는 방향**(몸 쪽 복귀): `CameraSensitivityScale`만 적용(경계 고착 방지).
  - 오프셋 정의: Yaw = `ControlYaw - BodyYaw`, Pitch = `Clamp(ControlPitch, ±MaxUpperBodyPitchDeg) - SmoothedAimPitch`(±45° 밖 조준 시 오작동 방지).
  - 폴백: 커브가 null이면 선형(1 - t), `MaxOffset <= 0`이면 오프셋 제한 없음.
  - 감도 변화는 `SensitivityBlendSpeed`로 블렌드.
- **이동**: `DoMove`는 Control Yaw 기준(항상 시선 기준 이동).
- **공격 중 이동 제한**: `FWaxMovementProfile = {ForwardSpeedScale, StrafeSpeedScale, BackwardSpeedScale, bAllowJump}`를 Spec의 단계별(`Windup/Active/RecoveryMovement`) 기본값으로 둔다. 속도 배율은 **몸 정면 기준 입력 방향**으로 섞고(앞·뒤 배율 × X² + 옆 배율 × Y²), `MaxWalkSpeed`에 곱한다. 전환은 `MoveScaleBlendSpeed`로 블렌드. 초기값은 전부 1.0(제한 없음), H3로 튜닝.
- **타격 방향**: 카메라 전방이 아니라 몸 Yaw + 상반신 Pitch로 산출(보이는 팔과 판정 방향 일치).
- **프로파일**: `FWaxRotationProfile = {MaxYawRateDegPerSec, MaxPitchRateDegPerSec, MaxBodyCameraYawOffsetDeg, MaxBodyCameraPitchOffsetDeg, CameraSensitivityScale}`. 해석 순서: `RotationProfile` NotifyState 오버라이드 → DA 단계별 기본값 → 비공격 시 제한 없음. 아래 초기값은 모두 DA 기본값이며 소스 상수가 아님.
- 초기값(H3로 튜닝): 기본공격은 **공격 전체(Windup·Active·Recovery)** 제한 — 몸 Yaw 360°/s, 상반신 Pitch 180°/s, 몸-카메라 오프셋 Yaw 45°/Pitch 25°, 감도 Windup·Recovery 0.7 / Active 0.5. 발차기 Active Yaw 0 + 감도 0.3, 슬램 Loop Yaw 0.

### 입력 및 상태 규칙

| 상황 | 규칙 |
|---|---|
| 기본공격 | `IA_WaxCrack`(좌클릭/Space). **Pulse 트리거 제거**, `Started`/`Completed`/`Canceled`만 바인딩(`Canceled`도 홀드 해제) |
| 점프공격 | 공중에서 **새로 누른(Started) 입력만** 판정 대상. 지상에서 누른 채 점프한 홀드는 무시. 탭/홀드 확정 전 **보류 상태**는 버퍼와 별개(보류 중 버퍼는 비어 있음). 보류 중 착지하면 보류 폐기 |
| 공중 F | 발차기는 지상 전용이라 공중 입력은 버퍼에 넣지 않음 |
| 발차기 | 신규 `IA_WaxKick`(기본 F), 지상 전용 |
| 동시 공격 | 활성 공격은 항상 1개 |
| 캔슬 | `UWaxCancelWindowNotifyState`(허용 타입 `AllowedTypes`)로 몽타주에 지정. 기본공격 HitNotify 이후 구간에 `Basic`·`Kick` 허용, 발차기·슬램에는 창 없음 |
| 이동 | 루트모션 OFF, 이동 허용. 슬램 Loop 동안 AirControl 0 |
| 공중 강타 중 착지 | 몽타주 블렌드아웃, 판정 미발생 시 그대로 종료 |
| 카메라 | `bSuppressHeadFollow` 공격 동안 머리 추종 비활성(슬램 착지 시 카메라 함몰 방지). 회전은 위 "공격 중 회전 제한" 따름 |

### 입력 버퍼 (Claude–Gemini 협업 합의)

- **단일 슬롯, 명시 입력만**: Crack `Started`, Kick 입력만 저장 `{Type, Time}`. 새 입력은 **최신 우선**으로 덮어씀. `BufferWindowSec`(≈0.2s, UPROPERTY) 지나면 만료.
- **소비 시점**: 유휴면 즉시 / `CancelWindow` 진입 시 / 몽타주 블렌드아웃·종료 시.
- **소비 시 재검증** `CanExecute(Type, 상태, 현재 창)`
  - 지금 불가하지만 유효한 항목은 만료까지 유지(예: 창이 `Kick`만 허용할 때 버퍼의 `Basic`).
  - 상태 변화로 영구 불가해진 항목은 즉시 폐기(예: 지상 `Kick` 버퍼 후 낙하). 착지 시 공중 입력도 폐기.
- **홀드 연타 = 상태 폴백**: 소비 시점에 버퍼가 비어 있고 `bCrackHeld && bHoldStartedGrounded && !IsFalling()`이면 `Basic`을 이어감. 우선순위가 가장 낮으므로 홀드 중 누른 F가 덮어써지지 않고, 공중에서 시작한 홀드는 착지 후 연타로 이어지지 않음.
- **쿨다운 폐기**: `PunchCooldownSec`·Spec 쿨다운을 없애고 템포는 몽타주 창이 결정. P0 회귀 기준을 맞추기 위해 기존 펀치 몽타주의 `Basic` 창을 기존 템포(실시간 0.34s, PlayRate 1.65 기준 몽타주 시간 약 0.56s)에 배치.

## 4. 검증 체계

### AI가 스스로 하는 검증
| 수단 | 내용 |
|---|---|
| 빌드 | `Build.sh` 증분 빌드, 경고 0 |
| 헤드리스 에셋 검증 | `UnrealEditor-Cmd -run=pythonscript`로 `Scripts/validate_attacks.py` 실행: Spec 몽타주 non-null, 슬롯명(UpperBody/FullBody) 규칙, HitNotify ≥1, 노티파이 소켓 존재, 노티파이 시점 범위, Land 섹션 존재 |
| 타격 프레임 자동 제안 | Python으로 애니의 손/발 본 궤적을 샘플링해서 최대 신전·속도 피크 프레임을 계산하고 HitNotify 초기 위치로 사용 |
| 후보 애니 1차 선별 | 길이, 루트 변위, 발 최고 높이, 리타겟 후 본 오차를 수치로 비교해 사람에게 최대 2개만 제시 |
| 자동 캡처 | 개발용 Exec `WaxAttackCapture <Slot>`: 3인칭 프리뷰에서 `Montage_SetPosition` + Pause로 노티파이 전후 프레임을 고정하고 `HighResShot`. `-game -ExecCmds`로 AI가 직접 실행하고 콘택트시트를 판독(실패 시 사람이 PIE에서 명령 1줄 입력) |
| 구조화 로그 | `LogWaxCombat`: 공격 시작/슬롯/몽타주, 버퍼 저장·덮어쓰기·소비·만료·폐기 사유, 히트 위치·대상 수·넉백 속도, 점프 최고점·슬램 낙하 높이, 탭/홀드 판정값 |

### 사람에게 맡기는 일 (AI가 할 수 없는 것만)

**H0. 1인칭 다리 노출 실측** (2분)
- PIE에서 정지 상태로 발밑을 내려다보고, 걸으면서 한 번 더 확인.
- 회신: `보임/안 보임`, 보이면 `자연스러움/왜곡됨/카메라에 파묻힘` 중 택1. 가능하면 스크린샷 1장.

**H1. 애니 소스 다운로드** (5~10분, 로그인 필요)
- Mixamo 검색어: `Front Kick` 2~3개, `Jump Attack` / `Ground Slam` 2~3개.
- 설정: FBX Binary, 30fps, Keyframe Reduction none, In Place 옵션이 있으면 체크. 첫 파일만 With Skin. 루트 이동은 AI가 제거하므로 In Place가 없어도 무방.
- 저장 위치: `SourceArt/Mixamo/` (gitignore 처리 완료, 원본 재배포 방지).
- 회신: `완료`.

**H2. 최종 후보·타격 프레임 확인** (공격당 1분)
- AI가 보낸 콘택트시트(최대 2개 후보, 노티파이 프레임 강조)를 보고 회신: `A/B` + `OK` 또는 `N프레임 앞으로/뒤로`.

**H3. 플레이 체감 체크리스트** (공격당 3분, PIE)
| 문항 | 회신 |
|---|---|
| 입력 후 동작 시작이 즉각적인가 | 1~5 |
| 맞는 순간과 파괴 순간이 일치하는가 | 1~5 |
| 무게감/타격감 | 1~5 |
| 공격 중 시점 회전이 답답하거나 미끄러운가 | 답답/적당/미끄러움 |
| 연타·연계 시 끊김이나 씹힘 | 예/아니오 |
| 한 줄 코멘트 | 자유 |
- 테스트 후 에디터만 종료하면 AI가 `LogWaxCombat`을 읽어 체감 점수와 수치를 대조.

에디터 종료·재실행은 `unreal-engine` 스킬의 재시작 절차에 따라 AI가 직접 수행하며 사람에게 요청하지 않는다.

## 5. 단계 계획 (단계별 통과 시 커밋, 코드/에셋 커밋 분리)

| 단계 | 내용 | AI 검증 | 사람 | 통과 기준 |
|---|---|---|---|---|
| P0 기반 | Python·EditorScriptingUtilities 활성화(에디터 전용), Combat 컴포넌트(입력 버퍼·단계 FSM·회전 제한 구조·이벤트), 캐릭터 흐름 훅, AttackSet DA, `AttackHit`/`Active`/`CancelWindow`/`RotationProfile` Notify, IA_WaxCrack Pulse 제거, 펀치 이전, 판정 원점 몸 메시 통일, DoMove Control Yaw 기준, 캡처 Exec | 빌드, 헤드리스 검증 스크립트 동작, 펀치 노티파이 호환, 판정 위치 디버그 구 비교, 캡처 성공 | H0, H3(펀치 회귀만) | 펀치 체감 변화 없음, 판정 위치 동일, 검증 스크립트 PASS (회전 제한 기본값 = 무제한) |
| P0.5 회전 제한 | 단계별 회전 프로파일 활성화, 소프트 클램프 튜닝 | 로그(몸·카메라 오프셋 최대치, 실효 감도) | H3(회전 문항 포함) | 회전 문항 '적당', 경계 고착 0회 |
| P1 발차기 | IA_WaxKick, 리타겟, ABP 슬롯 분리(UpperBody/FullBody, 펀치 몽타주 UpperBody 이전, 검증 스크립트 슬롯명 규칙 추가), AM_Kick(FullBody), 몬스터 넉백 API(`ReceiveKnockback`·`SuspendFor`), 파편 임펄스 | 후보 선별, 프레임 제안, 캡처, 로그(넉백 속도·대상 수) | H1, H2, H3 | H3 평균 ≥4, 씹힘 없음, 넉백이 로코모션에 상쇄되지 않음 |
| P2 점프공격 | 탭/홀드 판정, 공중 강타, 슬램 섹션, 높이 폴백 | 점프 최고점 실측 후 `SlamMinHeight` 결정, 캡처, 로그 | H2, H3 | 탭/홀드 오판 0회(10회 시도), 지상 홀드 후 점프 시 슬램 미발동 |
| P3 무기 스왑 골격 | `UWaxWeaponDataAsset`(①·`FWaxJumpAttackSpec` 오버라이드), 더미 무기 DA(엔진 기본 메시 + 소켓, ①·② 수치만 변경), FP·몸 메시 양쪽 부착, 장착 토글 개발키 | 슬롯 해석 로그, 무기 소켓 원점 디버그 구 | H3(스왑 전후 비교) | ①·②만 바뀌고 ③ 동일, 타격 원점이 무기 소켓 |

실제 무기 메시·전용 애니는 P3 이후 별도 트랙.

## 6. 구현 메모 (P0)

- 펀치 템포 기준값: 몽타주 1.0s, 타격 R 0.414 / L 0.483, PlayRate 1.65. 기존 홀드 연타 실측 간격 0.40s(Pulse 0.08s × 쿨다운 0.34s 정렬) → CancelWindow 시작 0.66(몽타주 시간)으로 일치.
- 소비 시점은 Press / CancelWindow / AttackEnd만. Tick은 버퍼 만료·상태 무효화·낙하 시 홀드 무효화만 수행.
- 이전 공격 이벤트 차단: 노티파이는 `FAnimNotifyMontageInstanceContext`의 인스턴스 ID 일치만 인정, 블렌드아웃 델리게이트는 `AttackSerial` 페이로드로 거름.
- 몸 메시 `VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones`(1인칭에서 숨겨져도 판정 소켓 갱신).
- 회전 보정은 현재 Tick에서 ControlRotation 델타를 재스케일하는 방식. P0.5 착수 시 `DoAim` 입력 단계 필터로 옮길지 재검토(입력 축 스케일 부호 확인 필요).
- 도구: `Scripts/validate_attacks.py`(헤드리스 검증), `-game -WaxCapture=<Slot> -WaxCaptureQuit`(측면 고정 시점 정지 캡처, `CaptureViewYawOffsetDeg`/`CaptureViewPitchDeg`), `Scripts/contact_sheet.py`.

## 7. 구현 메모 (P0.5)

- 회전 갱신 순서: 감도 블렌드 → 카메라 입력 필터(몸 Yaw 기준 오프셋) → 몸 Yaw가 **필터된** 시선을 추종. 보정 전 시선을 추종하면 몸이 카메라를 앞질러 오프셋이 허용치를 넘음(강한 플릭에서 59° 실측 후 수정). 회전 보정은 Tick 재스케일 방식 유지.
- 로그 `WaxRot Session`: 연속 공격 묶음(첫 공격 ~ 몸 정렬 완료) 단위로 `maxYawOff`·`maxPitchOff`·`minSens`·`boundaryFrames`(바깥 입력이 경계에서 완전히 막힌 프레임)·`recover`·`stuck` 기록. `recover > RotationStuckWarnSec`(1.0s)이면 고착.
- 자동 검증(PIE, Python으로 공격 홀드 + 시점 입력 주입): 약한 회전 maxYawOff 14.7 / 강한 플릭 39.1(≤45), maxPitchOff ≤25, recover 0.2s, stuck 0.
- 자동 검증 한계: 에디터 창이 포커스를 잃으면 백그라운드 절전으로 PIE가 3~4fps로 떨어져 회전 수치가 의미를 잃음(몸이 매 프레임 따라잡아 오프셋 0). 회전·이동 체감 측정은 사람이 포그라운드에서 수행(H3).
- 공격 중 이동 제한(`FWaxMovementProfile`)은 `UpdateMovementLimit`에서 BeginPlay 때 읽은 `MaxWalkSpeed`를 기준으로 배율 적용. 세션 로그 `minMove`로 확인.
- 체감 확인(H3): 회전 '적당', 경계 고착 0회. 최종 회전·이동 수치는 사람이 `DA_AttackSet_Unarmed`에서 직접 튜닝(PIE 68연타 maxYawOff 31.6, minMove 0.20, stuck 0).

## 8. 리스크

- **FirstPersonScale 0.6**: 다리가 보이면 카메라 쪽으로 축소·왜곡될 수 있음 → H0 결과에 따라 하체 HideBone 또는 FP 스케일 예외 처리 검토.
- **Mixamo 리타겟 품질**: 손가락·루트 이슈 가능 → 루트 트랙 제거, 루트모션 OFF, 본 오차 수치로 후보 선별.
- **ABP 슬롯 이전**: `ABP_WaxPlayer`의 노드 연결 순서는 바이너리라 문자열만으로는 확정할 수 없음 → P0에서 Python으로 그래프 확인 후 수정. 실패하면 사람이 슬롯 노드 1개를 추가하는 작업 카드로 전환.
- **UCLASS 리네이밍**: `[CoreRedirects]` 추가 후 기존 몽타주 재저장으로 정착.
