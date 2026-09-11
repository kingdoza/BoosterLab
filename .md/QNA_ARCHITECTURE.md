# QNA — 설비 배치 Authoring 단순화 기술 설계

각 질문의 `답변:` 뒤에 선택지를 하나 적어 주세요. 하나의 질문은 하나의 기술 결정만 다룹니다.

## 확인된 현재 구조

- 설비 아이템의 공통 소지 위치는 현재 `APlaceableFacilityItemActor.HeldTransform`에 reflected 값으로 남아 있습니다.
- Definition은 `PreviewActorClass`와 `FootprintCellsX/Y`를 별도로 authoring합니다.
- Zone 후보 Z에 `ZoneBounds` 반높이를 더한 뒤 footprint 반높이를 다시 더해 48/50/60cm 부양이 발생합니다.
- 공통 `PlacementNavModifier`가 `Placed` 전환과 함께 Navigation relevancy를 켭니다.
- 락커는 Authority보다 먼저 `BeginPlay`하면 한도 0으로 등록 실패한 뒤 Authority 변경 delegate에서 재시도합니다.

## 확정 전제

- `UFacilityPlacementSettings`의 공통 `HeldTransform`만 모든 설비 아이템이 사용하며 Scale은 항상 단위값으로 정규화합니다.
- cell 수는 scaled `PlacementFootprint` 전체 X/Y 크기와 전역 `GridSizeCm`에서 계산합니다.
- Zone의 명시적 바닥 plane과 footprint 바닥면으로 Actor transform을 한 번만 역산합니다.
- 실제 몸체 collision primitive만 Navigation geometry가 됩니다.
- `PlacementFootprint`, 상호작용 Box, 슬롯, Action/Approach Point는 Navigation에 영향을 주지 않습니다.
- runtime raw geometry 생성·제거를 위해 Recast Runtime Generation은 `Dynamic`을 사용합니다.
- 범용 네이티브 프리뷰는 collision, physics, Navigation과 domain 등록을 모두 비활성화합니다.

## Q1. 기존 reflected `HeldTransform`은 어떻게 폐기할까요?

- A: 기존 설비 아이템·legacy 설비 컴포넌트 필드를 한 migration cycle 동안 deprecated·숨김 상태로 보존하되 runtime에서는 무시
- B: 기존 필드를 즉시 삭제하고 모든 asset을 같은 변경에서 resave
- C: 기존 값이 단위 Transform이 아니면 공통 설정보다 우선하는 fallback으로 유지
- 권장안: A — serialized asset 호환성을 지키면서 공통 Project Settings를 즉시 단일 정본으로 만들 수 있습니다.
- 답변: B

## Q2. 기존 serialized `FootprintCellsX/Y`는 어떻게 폐기할까요?

- A: 한 migration cycle 동안 deprecated·숨김 상태로 보존하되 validation과 runtime 계산에서는 무시
- B: 즉시 삭제하고 기존 Definition asset을 모두 resave
- C: 계산된 cell 수를 기존 필드에 계속 자동 기록
- 권장안: A — 이중 authoring은 즉시 제거하면서 기존 asset 로딩 위험을 줄입니다.
- 답변: B

## Q3. 기존 Definition의 `PreviewActorClass`는 어떻게 폐기할까요?

- A: 한 migration cycle 동안 deprecated·숨김 상태로 보존하되 runtime에서는 공통 네이티브 preview class만 사용
- B: 즉시 삭제하고 기존 Definition asset을 모두 resave
- C: 값이 지정된 Definition만 기존 class를 우선 사용
- 권장안: A — 설비별 preview 계약은 즉시 끊으면서 serialized 참조는 안전하게 정리할 수 있습니다.
- 답변: B

## Q4. PlacementZone의 명시적 바닥 plane은 무엇으로 표현할까요?

- A: `ZoneBounds`와 분리된 `USceneComponent`를 두고 그 component의 XY plane을 사용
- B: Zone Actor root의 local XY plane을 사용
- C: Zone마다 숫자형 Z offset을 별도로 입력
- 권장안: A — Bounds 두께와 독립적이고 Editor에서 위치·회전을 직접 확인할 수 있습니다.
- 답변: A

## Q5. 설비의 footprint 바닥 authoring 계약은 무엇으로 고정할까요?

- A: Actor local 설치 바닥을 Z=0으로 두고 `PlacementFootprint`의 바닥면도 정확히 Z=0에 맞춤
- B: `PlacementFootprint` 중심을 Actor local Z=0으로 유지하고 별도 설치 높이 값을 입력
- C: 설비마다 별도 `InstallationPivot` component를 추가해 footprint 바닥과 연결
- 권장안: A — 설비별 보정값이나 추가 pivot 없이 footprint 상대 Z와 높이만으로 일관되게 검증할 수 있습니다.
- 답변: A

## Q6. 범용 프리뷰가 복제할 visual component는 어떻게 찾을까요?

- A: `PlacedFacilityClass`의 class-default hierarchy에서 표시 가능한 Static Mesh component를 자동 수집하고 helper·숨김·editor-only component는 제외
- B: 이름이 정확히 `VisualMesh`인 component 하나만 사용
- C: Definition마다 preview용 component reference 배열을 별도로 authoring
- 권장안: A — 복합 설비 외형을 지원하면서 preview용 중복 authoring을 만들지 않습니다.
- 답변: A

## Q7. Navigation geometry를 배치 시스템이 별도로 식별할까요?

- A: 별도 수집 없이 Unreal 기본 Navigation relevancy와 기존 메시 collision에 맡기고 배치 transaction은 Actor collision 상태만 전환
- B: `UFacilityPlacementComponent`가 몸체 primitive reference 배열을 명시적으로 소유
- C: 공통 `NavModifier` 또는 component tag로 별도 Navigation 형상을 지정
- 권장안: A — `RecastNavMesh = Dynamic`이 실제 collision 변경을 자동 반영하므로 중복 형상·배열 authoring이 필요 없습니다.
- 답변: A. 실제 몸체 메시의 기존 collision과 Unreal 기본 Navigation relevancy를 정본으로 사용한다. staged placement와 recovery unregistration에서는 Actor collision을 끄고, commit 또는 rollback에서는 transaction이 캡처한 이전 Actor collision 상태를 복원한다. `PlacementNavModifier`는 사용하지 않으며 `PlacementFootprint`, 상호작용 Box, 슬롯과 Action/Approach Point는 `Can Ever Affect Navigation = false`를 강제한다.

## Q8. 기존 `PlacementNavModifier` component는 어떻게 폐기할까요?

- A: 한 migration cycle 동안 deprecated component로 보존하되 항상 navigation 비활성화하고 runtime에서는 무시
- B: 네이티브 component를 즉시 삭제하고 관련 Blueprint를 같은 변경에서 전부 resave
- C: 실제 몸체 primitive가 없을 때만 fallback으로 계속 사용
- 권장안: A — `FailsafeExtent` fallback은 즉시 차단하면서 Blueprint 상속 자산의 migration 위험을 줄입니다.
- 답변: B

## Q9. Authority 준비 전 pre-placed 락커의 pending 등록은 누가 소유할까요?

- A: Facility Subsystem이 Authority readiness와 pending actor를 소유하고 Locker Capacity Subsystem이 용량 검증·등록을 담당
- B: Locker Capacity Subsystem이 readiness, pending actor와 facility 등록 조율까지 모두 소유
- C: 각 락커 Actor가 pending 상태와 재등록 시도를 개별 소유
- 권장안: A — 기존 Authority owner와 capacity owner 경계를 유지하면서 Actor별 재시도를 제거할 수 있습니다.
- 답변: A

## Q10. reconciliation 성공 publication은 어떤 단위로 발행할까요?

- A: 등록된 락커마다 facility와 capacity 변경을 각각 한 번씩 발행
- B: 모든 pending 락커의 내부 등록을 끝낸 뒤 facility 변경 한 번과 capacity 변경 한 번만 일괄 발행
- C: 초기 pre-placed 등록에서는 publication을 생략
- 권장안: B — BeginPlay 순서와 락커 수에 따른 중간 상태 노출 없이 최종 상태를 한 번만 공개합니다.
- 답변: B

## Q11. QNA_FEATURE_SPEC Q2에서 A를 선택할 경우 락커의 고정 순서는 무엇으로 정할까요?

- A: 락커 instance에 저장되는 runtime persistent `RegistrationId` GUID 순서
- B: runtime Actor 이름·경로 문자열 순서
- C: 별도 `RegistrationPriority` 값을 추가
- 권장안: A — Editor가 ID를 자동 생성하게 하면 rename과 BeginPlay 순서에 영향받지 않고 cooked build에서도 같은 결과를 유지합니다.
- 답변: A. UE의 `AActor::ActorGuid`는 Editor-only이므로 사용하지 않고, 락커 instance에 자동 생성·저장되는 runtime `FGuid RegistrationId`를 사용한다.

## 설계 전 읽기 전용 Unreal 확인 범위

- 배치 대상 Blueprint CDO의 visual component hierarchy와 `PlacementFootprint` 상대 transform
- 현재 설비별 preview Blueprint 및 material 연결
- PlacementZone instance의 Bounds와 바닥 기준 authoring 상태
- 각 설비의 실제 몸체 collision primitive와 Navigation relevancy
- Level의 RecastNavMesh Runtime Generation과 모든 Facility/Queue Approach Point 투영 상태
