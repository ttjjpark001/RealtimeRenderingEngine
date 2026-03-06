# Scene Rendering Settings Reference

씬별 카메라 / 광원 / 그림자 맵 추천 세팅 모음.
실제 적용 전 참고용 레퍼런스이며, 엔진 코드의 기본값은 별도로 관리된다.

---

## Sponza (glTF)

경로: `assets/test-models/Sponza/glTF/Sponza.gltf`

### 씬 스케일
| 항목 | 값 |
|------|-----|
| 크기 (X×Y×Z) | ~29.8m × 18.3m × 12.4m |
| Scene diagonal | ~37m |
| 중심 | 원점 근처 (0, 0, 0) |
| 삼각형 수 | ~262K |
| glTF root node scale | 0.008 (원본 좌표계 cm → m 변환) |

### 카메라 세팅 (현재 엔진 적용값)
```
Position:  (10.0, 4.5, 4.0)    // 측면 전경, 지상 4.5m (Option B)
LookAt:    (0.0,  0.0, 0.0)    // 씬 중심
FOV:       60°
MoveSpeedScale: sceneDiagonal / 40.0   // ≈ 0.925 (diagonal≈37m)
```

> 참고: 정면 중앙 포지션 — `Position (0.0, 2.0, -10.0)`, LookAt `(0.0, 2.0, 0.0)`

### 광원 추천 세팅

Sponza는 지붕이 열린 중정 구조 → **Directional Light(태양광) 1개**가 핵심.
Point Light 여러 개보다 강한 Directional + Shadow가 훨씬 사실적.

#### Key Light — Directional (태양) ★ 현재 엔진 적용값
```
type:       Directional
direction:  normalize(-0.3, -1.0, 0.5)   // 앙각 ≈ 60°, 오른쪽 전면 상단
color:      (1.0, 0.95, 0.8)             // 따뜻한 태양광
intensity:  10.0
castShadow: true
```

> **L 키 토글** (Sponza! 메뉴 로드 시에만): 태양 방향을 두 프리셋 사이에서 전환
> - 기본: `normalize(-0.3, -1.0, 0.5)` — 앙각 ≈ 60° (전통적 오후 태양)
> - Alt:  `normalize(-0.3, -1.5, 0.3)` — 앙각 ≈ 74° (1층까지 더 깊이 조명, shadow reach 절반)

#### Fill Light — 간접광 모사 (하늘빛) ★ 현재 엔진 적용값
```
type:       Point
position:   (-6.0, 10.0, 0.0)           // 좌측 상단
color:      (0.4, 0.5, 0.7)             // 차가운 하늘빛 (sky ambient)
intensity:  1.75
Kc = 1.0,  Kl = 0.027,  Kq = 0.005
castShadow: false
```

#### 횃불 Point Light (Torch Sconces) ★ 현재 엔진 적용값

glTF 파일 분석 결과, Sponza에는 중정 4개 코너에 벽면 횃불(sconce)이 **총 4개** 존재한다.
각 횃불은 bracket(mat 20) + torch body(mat 21) 2개 프리미티브로 구성.

불꽃 위치는 torch body 바운딩박스 상단 + 10cm 오프셋으로 계산.
좌표는 **Assimp 로딩 후 월드 공간** 기준 (scale 0.008 × aiProcess_ConvertToLeftHanded 적용).

```
type:       Point
color:      (1.0, 0.45, 0.08)   // 불꽃 오렌지
intensity:  8.0
Kc = 1.0,  Kl = 0.7,  Kq = 1.8  // 유효 반경 ~3-4m
castShadow: false

positions:
  { +3.901f, 1.836f, +1.765f }   // 우측-전방
  { -4.954f, 1.836f, +1.765f }   // 좌측-전방
  { -4.954f, 1.836f, -1.154f }   // 좌측-후방
  { +3.901f, 1.836f, -1.154f }   // 우측-후방
```

> **광원 수 합계**: Key + Fill + 횃불 4개 = 6개 → MAX_PBR_LIGHTS(16) 여유 충분

### Shadow Map 추천 세팅

#### 현재 엔진 값 vs 추천값

| 항목 | 현재값 | Sponza 추천값 | 파일 위치 |
|------|--------|--------------|-----------|
| Shadow Map 해상도 | **1024×1024** | **4096×4096** | `D3D12Context.h: SHADOW_MAP_SIZE` |
| Ortho 투영 범위 | **20×20m** | **50×50m** | `Renderer.cpp: XMMatrixOrthographicLH` |
| Ortho Far | 100.0m | 100.0m | `Renderer.cpp` |
| DepthBias | 1000 | 1000~3000 | `D3D12PipelineState.cpp` |
| SlopeScaledDepthBias | 1.0 | 1.0~2.0 | `D3D12PipelineState.cpp` |

> **주의**: Shadow Map 해상도를 4096으로 올리면 VRAM 사용량이 증가한다.
> D32_FLOAT 기준 4096×4096 = 64MB/장 × 최대 8장 = 512MB.
> 실용적으로는 **2048×2048** (16MB/장)로 시작하고, 품질 부족 시 4096으로 올린다.
>
> **Phase 24 이후**: Shadow Map 해상도와 Ortho 범위는 씬 로드 시 `m_sceneDiagonal`을 기준으로
> 자동 결정된다 (`Engine::LoadScene()` → `SetSceneDiagonal()` + `RecreateShadowMaps()`).
> Sponza diagonal ≈ 37m → 해상도 **2048**, orthoSize **55.5m**, far **111m** 자동 적용.

---

## 기타 모델

추후 씬 테스트 결과를 바탕으로 항목 추가.
