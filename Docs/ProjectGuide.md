# WaxPpuLab 프로젝트 가이드

범용 언리얼 규칙(툴체인 `ue-on`, Hot Reload·재시작 절차, MCP, 파라미터 외부화)은 전역 `unreal-engine` 스킬을 따른다. 이 문서는 이 프로젝트에만 해당하는 사실과 불변 조건을 담는다. 상세 아키텍처·튜닝 항목은 [README.md](../README.md), 공격 시스템은 [Design/CombatAttackDesign.md](./Design/CombatAttackDesign.md)를 본다.

## 아키텍처 불변 조건

- **몬스터 계층은 1단계**: 모든 몬스터는 `AWaxMonsterBase`(`ACharacter`)의 직계 BP 자식(`BP_WaxBall_Basic`, `BP_WaxButter`)이다. 중간 추상 BP를 추가하지 않는다.
- **`/Game/` 경로 하드코딩 0건**: `Source/WaxPpuLab/`에 에셋 경로를 쓰지 않는다. 에셋은 `UWaxMonsterDataAsset`(`DA_*`), `UWaxAttackSetDataAsset`, BP/컴포넌트 `UPROPERTY`로 주입한다.
- **런타임 복사 필드**는 `VisibleInstanceOnly`로 둬서 편집 대상과 구분한다.
- **고정 눈알**: `EyeLeftGC`/`EyeRightGC`는 `BodyPivot`에 고정되어 몸체와 함께 움직이며 플레이어를 추적 회전하지 않는다.
- **파편 수명**: `RemoveOnMaxSleep` 대신 Dataflow `FRemoveOnBreakDataflowNode`(`bAllowRemovalOnBreak = true`, `bAllowRemovalOnSleep = false`) 고정 수명 제거를 쓴다.

## 저장소·협업

- **Git LFS 미사용**: `.uasset`이 일반 Git으로 추적된다. 같은 `.uasset` 동시 편집을 피하고, 에셋 변경과 코드·문서 변경은 가능하면 커밋을 나눈다.
- `Content/Collections/`, `Content/Developers/`, `SourceArt/`(외부 원본 애니, 재배포 금지)는 `.gitignore` 대상이다.

## 렌더링 예산

- Apple M4 16GB 기준으로 `Config/DefaultEngine.ini`에서 하드웨어 레이 트레이싱·패스 트레이싱 CVar 6종을 껐다. Lumen 소프트웨어 GI/반사, VSM, Nanite, Substrate는 유지한다.

## 개발 도구

- **헤드리스 에셋 검증**: `UnrealEditor-Cmd WaxPpuLab.uproject -run=pythonscript -script=<절대경로>/Scripts/validate_attacks.py -unattended -nullrhi` → `WAXVAL PASS|FAIL` 로그. 스크립트 경로는 절대경로여야 하고, `print` 대신 `unreal.log_warning`을 써야 로그에 남는다.
- **공격 프레임 캡처**: `UnrealEditor.app/Contents/MacOS/UnrealEditor WaxPpuLab.uproject -game -windowed -WaxCapture=<Basic|AerialStrike|Slam|Kick> -WaxCaptureQuit` → `Saved/Screenshots/MacEditor/WaxCapture/`. `python3 Scripts/contact_sheet.py <폴더>`로 콘택트시트 생성.
- `PythonScriptPlugin`, `EditorScriptingUtilities`를 `.uproject`에 에디터 전용으로 활성화해 두었다(엔진 기본 비활성).
