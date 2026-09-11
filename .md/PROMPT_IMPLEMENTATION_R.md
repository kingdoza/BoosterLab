# 구현 재작업 프롬프트 — Placement Definition Opt-Out Data Validation

## 통합 리뷰 결론

설비 배치 Content migration은 완료 상태가 아니다. 다음 두 Definition은 승인된 conversion opt-out이라 `PlacedFacilityClass`, `RecoveryItemClass`, `RecoveryItemMesh`가 모두 `None`이어야 하지만, 현재 `UFacilityPlacementDefinition::IsDataValid()`가 활성 conversion 규칙을 적용해 각각 세 오류를 만든다.

- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_CleanTowelStack`
- `/Game/Bathhouse/Data/Placement/DA_FacilityPlacement_UsedTowelBin`

Content에 class를 채워 우회하지 않는다. 두 Actor가 placement/recovery prompt 또는 Actor conversion 대상이 되면 승인 계약을 위반한다.

## 대상 코드

- `Source/BathhouseSim/Private/Placement/FacilityPlacementDefinition.cpp`
- `Source/BathhouseSim/Private/Tests/FacilityPlacementAutomationTests.cpp`
- 테스트 fixture가 필요할 때만 `FacilityPlacementAutomationTestProbe.h/.cpp`

## 필수 수정

1. `IsDataValid()`에서 완전한 conversion opt-out과 활성/불완전한 conversion 설정을 구조적으로 구분한다.
2. `PlacedFacilityClass`, `RecoveryItemClass`, `RecoveryItemMesh`가 모두 비어 있는 완전한 opt-out은 공통 필드(`StableId`, `FacilityTags`, 음수가 아닌 `LockerSlotCount`)만 검사하고 conversion class, footprint, Navigation, recovery mesh 검사를 건너뛴다.
3. 일부 conversion 필드만 비어 있거나 서로 모순되는 Definition은 기존처럼 명시적으로 Invalid 처리한다.
4. 활성 Definition은 `IPlaceableFacility`, `SupportsFacilityActorConversion()`, 파생 footprint, Navigation 계약, `APlaceableFacilityItemActor` 파생 recovery class와 recovery mesh/fallback 검사를 그대로 유지한다.
5. opt-out에는 native Cube fallback 경고를 내지 않는다. 실제 recovery item이 없으므로 fallback도 사용하지 않는다.
6. `ValidateRuntime()`는 placement/recovery 실행용 계약이므로 완전한 opt-out을 성공시키지 말고 계속 fail-closed한다.

## 자동화 수용 기준

- 완전한 opt-out Definition의 `IsDataValid()`가 conversion 관련 오류·경고 없이 Valid다.
- `PlacedFacilityClass`만 있거나 `RecoveryItemClass`만 있는 부분 설정은 Invalid다.
- conversion을 지원하지 않는 Clean Towel Stack/Used Towel Bin class를 억지로 지정한 경우도 Invalid다.
- 활성 native recovery class와 Blueprint 파생 recovery class Definition은 계속 Valid다.
- 활성 Definition의 non-multiple footprint, helper Navigation 위반과 잘못된 recovery class 회귀가 계속 Invalid다.
- opt-out Definition의 `ValidateRuntime()`는 실패하고 placement/recovery prompt 및 Actor conversion 경로가 활성화되지 않는다.

## 재검증과 인계

- `git diff --check`
- UE 5.8 정책의 `Build.bat BathhouseSimEditor Win64 Development` 전체 빌드
- `BathhouseSim.Placement` 및 전체 `BathhouseSim` 자동화 재실행
- 코드 리뷰에서 위 opt-out/부분 설정/활성 설정 분기를 명시적으로 승인받는다.
- 새 DLL로 Editor를 재시작한 뒤 Definition 9개를 Data Validation하고 결과를 `.md/PROMPT_INTEGRATION_REVIEW.md`에 갱신한다.
- `PROMPT_REVIEW.md`와 `PROMPT_UNREAL.md`는 이 재작업의 정확한 변경 범위와 남은 FP-AS 시나리오를 서로 일치하게 인계한다.

## 변경 금지

- Clean Towel Stack/Used Towel Bin Definition에 placement/recovery class를 채우지 않는다.
- 두 towel endpoint를 `SupportsFacilityActorConversion=true`로 바꾸지 않는다.
- opt-out을 위해 asset path나 concrete towel class 이름을 Definition validator에 하드코딩하지 않는다.
- preview, navigation, transaction, towel token ownership과 공통 facility item scale 계약을 변경하지 않는다.
