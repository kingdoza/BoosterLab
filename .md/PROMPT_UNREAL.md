# Unreal 후속 검증 — 설비 설치 Transform 이중 보정 수정

## Editor 작업 여부

Blueprint, DataAsset, Level과 Project Settings 수정은 필요하지 않다. 이번 변경은 실제 설치 transaction이 프리뷰의 최종 Transform을 그대로 사용하도록 하는 C++ 버그 수정이다.

## 선행 조건

UE 5.8 `BathhouseSimEditor Win64 Development` 전체 빌드와 `BathhouseSim.Placement` 및 전체 `BathhouseSim` 자동화는 성공했다. 새 DLL로 Editor를 실행한다.

## PIE 검증

같은 `PlacementZone.PlacementFloor`에 다음 설비를 각각 프리뷰하고 설치한다.

- `/Game/Bathhouse/Blueprints/Facility/BP_Bath`
- `/Game/Bathhouse/Blueprints/Facility/BP_ClothesLocker`
- `/Game/Bathhouse/Blueprints/Towel/BP_Washer`
- `/Game/Bathhouse/Blueprints/Towel/BP_Dryer`

각 설비에서 다음을 확인한다.

1. 프리뷰의 footprint bottom이 `PlacementFloor`에 붙는다.
2. LMB 설치 직후 실제 설비의 footprint bottom이 프리뷰와 같은 높이에 있다.
3. Actor Z가 footprint 반높이만큼 두 번째로 상승하지 않는다.
4. 설치 뒤 body collision과 Dynamic NavMesh가 기존 계약대로 복원된다.
5. 회수 후 재설치해도 같은 결과가 반복된다.

## 저장 정책

이번 검증에서 Content 변경은 필요하지 않으며 `Save All`을 사용하지 않는다. 실제 asset 값이 의도치 않게 dirty가 되면 저장하지 않고 변경 원인을 먼저 확인한다.
