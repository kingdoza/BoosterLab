# Bath 01 Painterly — 별도 스타일 수정본

기존 `../Bath_01/`의 완성본을 보존한 별도 버전입니다. StylArts Stylized House Interior의 공개 이미지에서 관찰한 빛바랜 청록색, 넓은 붓 자국, 군데군데 벗겨진 도장과 닳은 모서리에 더 가깝게 표면을 수정했습니다.

참고: https://www.fab.com/listings/ab92e5d3-6db6-4cf3-bff5-c2c98ae8db5b

## 변경 내용

- 타일의 면별 UV0을 다시 구성해 한 장마다 붓 자국과 유약 농담이 보이도록 수정.
- 큰 색 얼룩, 길게 끊어진 붓 자국, 불규칙한 가장자리 마모를 새 텍스처로 제작.
- 타일과 테두리에 1024 px Base Color와 별도 Roughness 맵 적용.
- 금속 손잡이와 황동 부품의 거칠기를 높여 새것 같은 광택을 완화.
- 기존과 같은 카메라·조명·노출로 렌더해 재질 변화 자체를 비교 가능.
- 원래 형태, 크기, 분리 수면과 12개 UCX 충돌 조각 유지.

## 파일

- `Bath_01_Painterly.blend`: 텍스처가 포함된 Blender 원본.
- `Bath_01_Painterly.glb`: 조립된 모델. 배경과 조명은 제외.
- `SM_Bath_01_Painterly_Body.fbx`: 몸체와 UCX 충돌 메시.
- `SM_Bath_01_Painterly_Hardware.fbx`: 손잡이, 급수구와 장식.
- `SM_Bath_01_Painterly_Water.fbx`: 분리 수면.
- `Bath_01_Painterly_Hero.png`: 기존과 동일한 카메라/조명의 수정본 렌더.
- `Bath_01_Painterly_Empty.png`: 수면을 숨긴 내부 확인용 렌더.
- `textures/`, `materials.json`: 색과 거칠기 텍스처 및 재질 수치.
- `validation_report.json`: 저장된 원본과 FBX 재수입 검증.
- `original_preservation.json`: 기존 완성본 폴더의 파일 해시 보존 확인.

모델과 재질에 별도 이름(`SM_Bath_01_Painterly_*`, `M_PT_*`)을 사용합니다. Unreal에 가져올 때도 원래 버전과 별도 폴더에 가져오면 비교하기 쉽습니다.

## Unreal 참고

욕탕 테두리는 약 456 × 356 cm이며 외부 계단 포함 깊이는 약 443 cm입니다. 모든 구성 메시를 같은 위치·회전, Scale 1에 배치합니다. Body는 제공 UCX를 사용하며 수면 충돌은 끕니다.

Base Color는 sRGB, Roughness는 sRGB를 끈 데이터 텍스처로 연결합니다. Roughness가 FBX 임포트에서 자동 연결되지 않으면 `materials.json`의 경로를 따라 수동 연결합니다. UV0은 반복되는 면별 재질 UV로 겹침을 의도했으며, UV1_Lightmap은 별도로 패킹했습니다. Unreal에서 라이트맵 해상도와 패딩을 확인해야 합니다.

Blender 물 재질은 Unreal에 동일하게 전송되지 않으므로 별도 물 재질을 지정합니다. Unreal Editor 임포트와 게임 내 이동/충돌 검증은 이번 수정 범위에서 실행하지 않았습니다.

## 재현

작업 전용 빈 Blender 세션에서 `build_bath_painterly.py`를 실행합니다. `painterly_materials.py`를 같은 폴더에 둡니다. 빌더는 작업 세션을 초기화하므로 사용자가 편집 중인 장면에서 실행하지 마세요. `prepare_variant.py`는 기존 빌더를 읽기 전용으로 참고해 수정본 빌더를 생성한 기록입니다.
