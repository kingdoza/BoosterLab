# BathhouseSim 사람 개발자 작업 순서

이 문서는 사람이 각 전문 에이전트를 어떤 순서로 실행하고 어디서 직접 승인해야 하는지 요약한다. 세부 규칙은 [AGENTS.md](AGENTS.md)와 [.md/AGENT_WORKFLOW.md](.md/AGENT_WORKFLOW.md)를 따른다.

## 기본 작업 순서

1. **기능 요청 작성**
   - 구현 방법보다 플레이어가 무엇을 하고 무엇을 봐야 하는지 적는다.
   - 유지할 기존 동작, 실패·취소 결과와 범위 밖 항목을 함께 적는다.

2. **기능 명세 에이전트 실행**
   - `.md/AGENT_FEATURE_SPEC.md`에 따라 `PROMPT_ARCHITECTURE.md`를 작성시킨다.
   - Blueprint, Level, StateTree, UI 또는 실제 PIE 상태가 명세에 영향을 주면 사전 MCP 조사를 요청한다.

3. **필요할 때 Unreal MCP 사전 조사**
   - 읽기 전용으로 현재 CDO, component, binding, override, collision, transform과 설정을 확인한다.
   - 결과를 `REPORT_UNREAL_DISCOVERY.md`에 작성한 뒤 기능 명세 에이전트로 돌아간다.
   - MCP가 지원하지 않는 조작은 Computer Use로 자동 전환하지 않는다.

4. **사람이 기능 명세 승인**
   - 입력, 프롬프트, 완료 순간, 성공 결과, 실패·취소·복구와 Editor 설정 위치를 확인한다.
   - 예상과 다르면 이 단계에서 수정하고, 승인 전에는 설계·구현을 시작하지 않는다.

5. **설계 에이전트 실행**
   - 승인된 `PROMPT_ARCHITECTURE.md`를 기술 구조로 변환한다.
   - 고위험·다중 시스템 작업은 대표 대상 하나의 수직 구현만 먼저 설계한다.

6. **구현 에이전트 실행**
   - 현재 `PROMPT_IMPLEMENTATION.md` 범위의 C++·승인된 Config만 구현한다.
   - `PROMPT_REVIEW.md`와 `PROMPT_UNREAL.md`가 생성됐는지 확인한다.

7. **코드 리뷰 에이전트 실행**
   - 코드 안전성뿐 아니라 기능 명세, 수직 구현 범위와 Editor 계약 일치를 검토한다.
   - 재작업이면 구현으로, 구조 문제면 설계로, 동작 문제면 기능 명세로 돌아간다.

8. **Unreal MCP Editor 작업 실행**
   - MCP로 가능한 allowlist asset만 수정·Compile·Save·재로드·PIE 검증한다.
   - 저장된 현재 구조를 `.md/Unreal/*System.md`에 갱신한다.
   - MCP 미지원 작업은 한국어 `.md/USER_UNREAL.md`에 남긴다.

9. **남은 Editor 작업 처리**
   - `USER_UNREAL.md`가 있으면 사람이 직접 수행하거나 Computer Use 작업을 별도로 명시한다.
   - 완료 후 저장 상태와 Unreal 문서를 확인하고 해결된 항목을 제거한다.

10. **통합 리뷰 에이전트 실행**
    - 기능 명세, C++, Blueprint/Level, Unreal 문서와 PIE 결과를 함께 검토한다.
    - 필수 수동 작업이나 미검증 시나리오가 남으면 승인하지 않는다.

11. **수직 구현이면 사람이 플레이 승인**
    - 대표 흐름이 예상과 같을 때만 설계 단계부터 전체 대상으로 확장한다.
    - 전체 확장은 `설계 → 구현 → 코드 리뷰 → MCP → 통합 리뷰`를 다시 수행한다.

12. **최종 diff·플레이 확인 후 Git commit**
    - 예상 밖 Source/Content/Level 변경, dirty package와 미완료 `USER_UNREAL.md`가 없는지 확인한다.

## 에이전트별 모델·추론 수준

아래는 이 프로젝트의 권장값이다. OpenAI는 Astra를 가장 어려운 end-to-end 작업용 모델, Sol을 복잡한 전문 작업용 flagship, Terra를 지능과 비용의 균형 모델, Luna를 비용 민감형 모델로 설명한다. 모델을 바꿔도 MCP에 없는 Editor 기능이 생기지는 않는다.

| 단계 | 기본 모델 / 추론 | 상향 조건 |
|---|---|---|
| 기능 명세 | `gpt-6-astra` / `high` | 복잡한 상태·실패 시나리오면 `xhigh` |
| MCP 사전 조사 | `gpt-5.6-terra` / `medium` | StateTree·다수 asset 상관분석이면 `high` |
| 아키텍처 설계 | `gpt-6-astra` / `high` | lifecycle·serialization·Navigation·전역 설정이 얽히면 `xhigh` |
| C++ 구현 | `gpt-5.6-sol` / `high` | 단순하고 경계가 확정된 수정은 Terra `high` |
| 코드 리뷰 | `gpt-6-astra` / `high` | 대형 transaction·비동기·GC·rollback 검토는 `xhigh` |
| Unreal MCP 작업 | `gpt-5.6-terra` / `medium` | 여러 asset과 PIE 로그를 함께 판단하면 `high` |
| Computer Use | `gpt-6-astra` / `high` | 사용자가 명시적으로 요청한 경우에만 실행 |
| 통합 리뷰 | `gpt-6-astra` / `high` | 다중 시스템 회귀와 원인 분리가 어려우면 `xhigh` |
| 단순 문서 정리 | `gpt-5.6-luna` / `medium` | 계약 판단이 포함되면 Terra `medium` 이상 |

`max/ultra`는 기본값으로 쓰지 않는다. `high`에서 판단이 불안정하거나 대안 비교가 반복해서 실패한 고난도 작업에만 제한적으로 올린다.

모델 특성 근거: [GPT-6 Astra](https://developers.openai.com/api/docs/models/gpt-6-astra), [GPT-5.6 Sol](https://developers.openai.com/api/docs/models/gpt-5.6-sol), [GPT-5.6 Terra](https://developers.openai.com/api/docs/models/gpt-5.6-terra), [GPT-5.6 Luna](https://developers.openai.com/api/docs/models/gpt-5.6-luna).

## 빠른 선택

- 결과가 틀리면 재작업 비용이 큼: **Astra High**
- 범위가 확정된 장시간 C++ 구현: **Sol High**
- 반복적인 MCP 조회·에셋 설정: **Terra Medium**
- 단순 문서·목록 정리: **Luna Medium**
- 잘 모르겠으면 **Astra High**로 시작하고, 반복 작업만 Terra/Sol로 낮춘다.
