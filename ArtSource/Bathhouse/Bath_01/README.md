# Bath 01 — 청록 타일 공동 욕탕

Blender에서 제작한 독립형 공용목욕탕 욕탕입니다. StylArts의 Stylized House Interior 공개 미리보기를 시각 참고로 사용해 청록·크림색, 둥근 테두리, 타일 색 편차, 작은 유약 마모를 적용했습니다. 원본 에셋 메시나 텍스처를 추출하거나 재사용하지 않았습니다.

참고: https://www.fab.com/listings/ab92e5d3-6db6-4cf3-bff5-c2c98ae8db5b

## 구성

- `Bath_01.blend`: 텍스처를 포함한 Blender 5.2 원본, 카메라·조명과 렌더 장면.
- `Bath_01.glb`: 몸체·손잡이·장식·수면을 포함한 조립 상태의 모델. 조명과 배경은 제외.
- `SM_Bath_01_Body.fbx`: 욕탕 몸체, 외부·내부 계단, 벤치, 급수대와 12개의 UCX 충돌 조각.
- `SM_Bath_01_Hardware.fbx`: 금속 부품 메시와 온도판/배수구 장식 메시.
- `SM_Bath_01_Water.fbx`: 수면과 작은 잔물결. 게임의 수면 시스템으로 교체 가능.
- `textures/`: 생성한 PNG Base Color 이미지. 단색 재질의 색·거칠기·금속성은 Blender 재질에 저장.
- `Bath_01_Hero.png`, `Bath_01_Empty.png`: 수면 포함 및 물을 뺀 내부 확인용 렌더.
- `asset_manifest.json`, `validation_report.json`: 메시 통계와 저장/재수입 검증(52개 항목 통과).
- `materials.json`: Unreal 재질 복원용 텍스처 경로·기본색·거칠기·금속성 수치.

## 크기와 배치

- 욕탕 테두리 기준 약 4.56 × 3.56 m, 테두리 높이 약 1.13 m.
- 외부 계단 포함 깊이는 약 4.43 m. 손잡이 최고점은 약 1.8 m.
- 수면 높이 0.865 m, 바닥 타일 높이 약 0.325 m.
- 모든 구성 메시의 원점은 욕탕 중심의 바닥 높이 `(0, 0, 0)`.
- Blender 장면은 m 단위. FBX에는 단위 변환 정보를 저장했으며 재수입 크기를 검증합니다.

## Unreal 가져오기

1. Body FBX를 Static Mesh로 가져옵니다. 제공된 UCX를 사용하고 자동 충돌 생성은 끕니다.
2. Hardware FBX의 두 메시를 가져옵니다. 수면은 별도 Static Mesh로 가져옵니다.
3. 구성 메시를 같은 위치·회전, Scale 1에 배치합니다. 실제 테두리 폭이 약 456 cm인지 확인합니다.
4. 제공 PNG를 각 재질 슬롯에 연결합니다. 금속 재질은 Metallic 약 0.77~0.86, Roughness 약 0.23~0.30을 참고합니다.
5. 수면은 Unreal용 물 재질을 별도로 지정하고 충돌을 끕니다. Blender의 투과/코팅 설정은 FBX로 동일하게 전송되지 않습니다.
6. UV0은 재질용, UV1_Lightmap은 별도로 복사한 비중첩 UV입니다. 정적 조명 사용 시 Unreal에서 라이트맵 해상도와 패딩을 확인합니다.

에디터 임포트, 게임 내 이동/충돌, 프로젝트 조명에서의 최종 스타일 일치는 아직 검증하지 않았습니다. 이번 결과물은 Blender 모델링 및 외부 에셋 패키지 범위입니다.

## 재현

설치된 Blender Lab MCP 애드온의 로컬 실행 브리지(127.0.0.1:9876)로 `build_bath.py`를 실행했습니다. `bridge_client.py`는 그 애드온의 JSON/NULL 프로토콜을 사용합니다. 현재 Codex 도구 목록에는 Blender MCP가 없으므로 애드온 브리지에 직접 연결했습니다.

`build_bath.py`는 작업 전용 빈 Blender 세션에서 사용합니다. 해당 세션의 장면을 초기화합니다. 사용자가 작업 중인 Blender 세션에는 실행하지 마세요. `validate_bath.py`는 저장 원본을 다시 열고 별도 임시 장면에서 FBX를 재수입합니다.
