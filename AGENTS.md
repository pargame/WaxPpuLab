# WaxPpuLab Agent & Contributor Guardrails

이 프로젝트에 새로 합류하는 협업자 및 AI 에이전트가 반드시 지켜야 할 불변 설계 원칙, 빌드 절차, UE 5.9 Preview 2 작업 주의사항입니다. 상세 아키텍처와 튜닝 가이드는 [README.md](./README.md)를 참고합니다.

---

## 1. 불변 아키텍처 규칙

- **Flat 1-Level 몬스터 계층**: 모든 몬스터는 `AWaxMonsterBase`(`ACharacter`)를 직접 상속받는 직계 블루프린트 자식(`BP_WaxBall_Basic`, `BP_WaxButter`)으로만 구성한다. 중간 추상 BP 계층을 추가하지 않는다.
- **Zero Hardcoded `/Game/` Paths**: `Source/WaxPpuLab/` 내부에 `/Game/` 에셋 경로를 정적으로 하드코딩하지 않는다(현재 0건 유지). 에셋 참조는 `UWaxMonsterDataAsset`(`DA_*`) 또는 BP/컴포넌트 `UPROPERTY`로 주입한다.
- **튜닝 파라미터 외부화**: 밸런싱·물리·오디오·셰이더 수치는 `UWaxMonsterDataAsset` 또는 컴포넌트 `UPROPERTY(EditAnywhere)`로 노출한다. 런타임 복사 전용 필드는 `VisibleInstanceOnly`로 유지해 편집 혼선을 방지한다.
- **고정 눈알(`BodyPivot`)**: 양쪽 눈알(`EyeLeftGC`, `EyeRightGC`)은 `BodyPivot`에 고정되어 몸체와 함께 이동·회전하며, 플레이어를 추적 회전하지 않는다.
- **파편 수명 정책**: 파편 정리는 `RemoveOnMaxSleep` 대신 Dataflow `FRemoveOnBreakDataflowNode`(`bAllowRemovalOnBreak = true`, `bAllowRemovalOnSleep = false`) 기반 고정 수명 제거를 사용한다.

---

## 2. 빌드 및 에디터 재시작 절차 (macOS UE 5.9)

- **Live Coding 미지원**: UE 5.9의 Live Coding은 Windows 전용(`WITH_LIVE_CODING=0`)이므로 macOS에서는 비모놀리식 에디터 **Hot Reload**를 사용한다.
- **`.cpp` 본문만 수정한 경우**: 에디터를 켠 상태에서 `Build.sh`를 실행하면 `Binaries/Mac/libUnrealEditor-WaxPpuLab-000N.dylib`가 생성되고 에디터가 `DoHotReloadFromIDE`로 자동 리로드한다(`Saved/Logs`의 `LogHotReload` 확인).
- **`.h` / 리플렉션(`UPROPERTY`·`UFUNCTION`·`UCLASS`) / `.Build.cs` / `.uproject` 변경 시**:
  1. 에디터 더티 패키지 전체 저장 (`EditorLoadingAndSavingUtils.save_dirty_packages(True, True)`)
  2. 에디터 정상 종료 및 `pgrep -x UnrealEditor`로 완전 종료 확인 (프로세스 잔존 시 빌드 금지)
  3. 잔류 핫리로드 파일 삭제: `find Binaries/Mac -name '*-000*.dylib' -delete`
  4. `Build.sh WaxPpuLabEditor Mac Development -Project="$(pwd)/WaxPpuLab.uproject" -WaitMutex` 증분 빌드
  5. `Binaries/Mac/UnrealEditor.modules`가 번호 없는 기본 `libUnrealEditor-WaxPpuLab.dylib`를 가리키는지 확인 후 에디터 재실행

---

## 3. UE 5.9 Preview 2 에셋 작업 주의사항

- **머티리얼 표현식 일괄 삭제 금지**: `MaterialEditingLibrary.delete_all_material_expressions(mat)` 호출 시 `Assertion failed: !IsRooted()`(`SIGSEGV`)로 에디터가 즉시 크래시한다. 그래프를 정리할 때는 노드를 새로 연결한 뒤 `DeleteUnusedExpressions`를 사용하거나 에셋을 삭제 후 재생성한다.
- **에셋 이동/리네이밍**: 개별 `EditorAssetLibrary.rename_asset` 후 폴더를 삭제하면 리다이렉터 참조가 깨질 수 있다. 반드시 모든 참조 에셋을 사전 로드한 뒤 `AssetTools.rename_assets`로 일괄 이동하고, BP 재컴파일·강제 저장·AssetRegistry 재스캔으로 깨진 참조 0건을 확인한 뒤 원본 폴더를 삭제한다.
- **Enhanced Input Python API**: `UInputMappingContext.mappings`는 deprecated이므로 `default_key_mappings`(`FDefaultContextKeyMappings`)를 함께 갱신한다.

---

## 4. Git 및 협업 주의사항

- **Git LFS 미사용**: `.gitattributes` 및 Git LFS가 설정되어 있지 않으며, `.uasset` 바이너리가 일반 Git으로 추적된다. 동일 `.uasset`의 동시 편집을 피하고, 에셋 변경과 문서/코드 변경은 가급적 원자적 커밋으로 분리한다.
- **로컬 제외 폴더**: `Content/Collections/`, `Content/Developers/`는 `.gitignore`로 제외되어 있으므로 공용 에셋을 해당 경로에 두지 않는다.
- **렌더링 예산 유지**: Apple M4 16GB 통합 메모리 환경을 기준으로 `Config/DefaultEngine.ini`의 하드웨어 레이 트레이싱·패스 트레이싱 CVar 6종이 비활성화되어 있다. Lumen 소프트웨어 GI/반사, VSM, Nanite, Substrate는 유지한다.
