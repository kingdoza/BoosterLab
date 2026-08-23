# Unreal Prompt — Mass-Independent Authored Impulses

## 작업 상태

- Content 변경 불필요
- Source build 또는 Live Coding 반영 확인만 필요
- 사용자 visible UE 5.8 Editor를 재사용하고 별도 background Editor를 실행하지 않는다.

## 변경하지 않는 에셋

- `/Game/Bathhouse/Blueprints/Combat/BP_MonkeyWrench`
- `/Game/Bathhouse/Blueprints/Customer/BP_BathhouseCustomer`
- `/Game/Bathhouse/Physics/PA_BathhouseCustomer`
- `/Game/Bathhouse/Curves/Equipment/CV_MonkeyWrench_Rotation`
- `/Game/Maps/DefaultMap`

Blueprint property 값, PhysicsAsset 질량/damping/constraint와 Curve key를 수정하거나 저장하지 않는다.

## Native Contract

- `MeleeAttack.ImpulseStrength`와 `VerticalImpulse`의 합성값은 customer root body에 mass-independent velocity change로 적용된다.
- wet mop/towel basket/monkey wrench의 `ThrowImpulseStrength`도 기존과 같이 mass-independent velocity change다.
- 수치 기본값과 reflected property 계약은 변경되지 않았다.

## Editor Verification

1. 사용자가 연 UE 5.8 Editor에서 Live Coding을 실행하거나 Editor를 정상 재시작한다.
2. compile error가 없는지 확인한다.
3. PIE에서 질량이 다른 ragdoll body가 같은 authored 공격 Strength에 대해 동일한 초기 속도 변화 경향을 보이는지 확인한다.
4. 몽키스패너 G 드랍과 wet mop/towel basket G 드랍이 기존과 같이 동작하는지 확인한다.
5. Content/Map package를 저장하지 않는다.

## 수용 기준

- C++ compile 성공.
- customer knockdown과 common equipment drop의 Strength 경로가 모두 질량 독립적이다.
- root-body validation, soft interruption, 4초 recovery와 G drop sweep에 회귀가 없다.
- 예상 밖 Content/map dirty package가 없다.

## 미적용 범위

몽키스패너 rotation curve는 이번 코드 수정 대상이 아니다. 추천 key는 완료 보고에서 제공하고 사용자가 별도 요청할 때만 Editor에서 변경한다.
