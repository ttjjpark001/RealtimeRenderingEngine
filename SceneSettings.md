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

## Bistro (niagara_bistro glTF 변환본)

출처: `github.com/zeux/niagara_bistro` (MIT 라이선스, DDS→PNG 변환, glTF 카메라 내장)
경로: `assets/test-models/Bistro/bistro.gltf`

> **Amazon Lumberyard Bistro** 씬의 glTF 변환본. Vulkan niagara 렌더러로 검증된 버전.
> `bistro.gltf` + `bistro.bin` + `textures/` (PNG) 단일 통합 파일 구성.
> `bistrox.gltf`는 별도 변형 버전. Exterior + Interior가 하나의 파일로 통합됨.

### bistro.gltf vs bistrox.gltf 비교

두 파일은 **동일한 씬(Exterior + Interior 통합)** 을 담고 있으며, 지오메트리·머티리얼이 완전히 같다.
별도 Exterior-only / Interior-only 파일은 존재하지 않는다.

| 항목 | bistro.gltf | bistrox.gltf |
|------|-------------|--------------|
| **생성 도구** | FBX2glTF v0.9.7 (직접 변환) | glTF-Transform v4.0.8 (후처리 정제) |
| 삼각형 수 | 1,753,630 | 1,753,630 (동일) |
| 메시 수 | 551 | 551 (동일) |
| 머티리얼 수 | 254 | 254 (동일) |
| 텍스처 수 | 343 | 331 (-12, 중복 제거) |
| 이미지 파일 수 | 686 | 679 (-7, 중복 제거) |
| 노드 수 | 5,910 | 5,927 (+17 orphaned 노드) |
| `MSFT_texture_dds` 확장 | **있음** (DDS 참조 잔존) | **없음** |
| `KHR_materials_ior` | 없음 | 선언만 있음 (실제 사용 0개) |
| `KHR_lights_punctual` (Sun) | intensity=6830, color=[1,1,1] | intensity=6830, color=null |
| 카메라 노드 | 동일 | 동일 |

**orphaned 노드(bistrox 전용)**: `Vespa`, `Vespa.001` 계층 총 17개가 씬 루트에 미포함된 채로 존재.
씬에 참조되지 않으므로 렌더링에 영향 없음.

**권장 로딩 파일: `bistro.gltf`**
- 원본 변환본으로 레퍼런스 명확
- `MSFT_texture_dds`는 Assimp에서 무시되므로 실제 로딩 오류 없음
- bistrox의 orphaned 노드 문제 없음

### 씬 스케일 (bistro.gltf 실측값)

| 항목 | 값 |
|------|-----|
| 크기 (X×Y×Z) | ~111m × 32m × 119m (Exterior + Interior 통합 씬) |
| Scene diagonal | ~166m |
| 중심 (world) | (3.1, 11.2, 12.7) — glTF RH 기준 |
| 삼각형 수 | ~1,753,630 (niagara_bistro 최적화 버전) |
| 메시 수 | 551 |
| 머티리얼 수 | 254 |
| 텍스처 수 | 343 |
| glTF root node scale | 100 (내부 cm 좌표 × 100 → 월드 m 변환 내장) |

> **원본 NVIDIA ORCA vs niagara_bistro**: ORCA 원본 Exterior 2,832,120 / Interior 1,046,609 삼각형.
> niagara_bistro는 최적화된 glTF 변환본으로 삼각형 수가 줄어들었으며, Exterior + Interior가 통합되어 있다.

### 카메라 세팅 (glTF 내장 카메라 기준)

```
// glTF 카메라 노드 (aiProcess_ConvertToLeftHanded 적용 후 엔진 좌표)
Position:  (-26.4, 3.2, -11.2)   // 씬 서쪽 보도, 지상 3.2m
LookAt:    (-7.0,  2.6, -17.0)   // 거리 동쪽 방향, 약 20m 전방
FOV:       60°                    // glTF yfov=0.628 rad ≈ 36° VFOV → 60° HFOV (16:9)
MoveSpeedScale: sceneDiagonal / 40.0   // ≈ 4.15 (diagonal≈166m)
```

> glTF 내장 카메라 노드 기준. 엔진 LoadBistroScene()에서 직접 적용.
> 참고: 건물 내부 탐색 — `Position (-5.0, 2.0, 0.0)`, LookAt `(0.0, 2.0, 5.0)`

### 광원 추천 세팅

Bistro는 Exterior(야외 거리 + 건물 외관) + Interior(레스토랑 내부) 통합 구조.
야외 구역은 **Directional Light(태양광) 1개**가 핵심, 내부는 Point Light 추가.

#### Key Light — Directional (태양) ★ 추천값

```
type:       Directional
direction:  normalize(-0.5, -1.0, 0.3)   // 앙각 ≈ 60°, 좌측 전면 상단
color:      (1.0, 0.95, 0.8)             // 따뜻한 태양광
intensity:  8.0
castShadow: true
```

> glTF Sun 노드 위치: (-12.3, -0.9, 15.4) — 씬 내 배치된 태양 방향 참조.

#### Fill Light — 간접광 모사 (하늘빛) ★ 추천값

```
type:       Point
position:   (3.0, 30.0, -13.0)           // 씬 중앙 상단 (LH 엔진 좌표)
color:      (0.4, 0.5, 0.7)             // 차가운 하늘빛
intensity:  2.0
Kc = 1.0,  Kl = 0.007,  Kq = 0.0002   // 유효 반경 ~50m (대형 씬 대응)
castShadow: false
```

> **광원 수 합계**: Key + Fill = 2개 → MAX_PBR_LIGHTS(16) 여유 충분
> Interior 탐색 시 Point Light 추가(식탁 조명, 천장 조명 등) 권장

### Shadow Map 자동 설정 (현재 엔진 기준)

Shadow Map 해상도와 Ortho 범위는 `m_sceneDiagonal`을 기준으로 자동 결정된다
(`Engine::LoadScene()` → `SetSceneDiagonal()` + `RecreateShadowMaps()`).

**Bistro diagonal ≈ 166m 기준 자동 적용값:**

| 항목 | 자동 결정값 | 계산식 |
|------|------------|--------|
| Shadow Map 해상도 | **4096×4096** | diagonal > 100m → 4096 |
| Ortho 투영 범위 (width/height) | **249m × 249m** | diagonal × 1.5f |
| Ortho Far | **498m** | diagonal × 3.0f |
| Ortho Near | **83m** | diagonal × 0.5f |
| Shadow Normal Bias (world-space) | **≈ 0.122m** | (249f / 4096) × 2.0f |

| 항목 | 추천 튜닝값 |
|------|------------|
| DepthBias | 1000~3000 |
| SlopeScaledDepthBias | 1.0~2.0 |

> **주의**: Bistro는 씬 규모(diagonal ~166m)와 복잡한 건물 구조(처마, 기둥 등)로
> Shadow Acne가 발생하기 쉽다. DepthBias와 SlopeScaledDepthBias를 씬 로드 후 시각적으로 튜닝하는 것을 권장한다.
> Shadow ortho 249m 범위는 전체 씬 커버용이며, 탐색 구역에 따라 수동 축소 가능.

### Shadow Map VRAM 분석

#### Shadow Map 해상도별 VRAM 소비 (D32_FLOAT, 장당)

| 해상도 | 크기 계산 | VRAM |
|--------|----------|------|
| 1024×1024 | 1024 × 1024 × 4 B | **4 MB** |
| 2048×2048 | 2048 × 2048 × 4 B | **16 MB** |
| **4096×4096** | 4096 × 4096 × 4 B | **64 MB** |

> Bistro 기준 shadow-casting 광원: Key Light (Directional) 1개 → 4096×4096 1장 = 64 MB.

#### 현재 개발 PC (Intel UHD 630) VRAM 예산 분석

UHD 630은 전용 VRAM 128 MB, 시스템 공유 포함 최대 ~1 GB를 사용할 수 있다.
Bistro 씬 로딩 시 GPU 메모리 소비 예측:

| 항목 | 추정 VRAM | 비고 |
|------|----------|------|
| OS + 드라이버 오버헤드 | ~200 MB | |
| 백버퍼 × 2 (960×540 RGBA8) | ~4 MB | 기본 창 크기 기준 |
| Depth Buffer (960×540 D24S8) | ~2 MB | |
| Vertex Buffer (Bistro, ~2M 정점 × 40B) | ~80 MB | |
| Index Buffer (~5.3M 인덱스 × 4B) | ~21 MB | |
| 텍스처 (343장, 평균 512×512 RGBA8 + MIP) | **~460 MB** | MIP 포함 ×4/3 |
| **Shadow Map 4096×4096 D32** | **64 MB** | 자동 결정값 |
| **Shadow Map 2048×2048 D32** | **16 MB** | 수동 강제 시 |
| **합계 (4096 기준)** | **~831 MB** | 1 GB 한계에 근접 |
| **합계 (2048 기준)** | **~783 MB** | 1 GB 내 수용 |

> 텍스처 추정: 343장 × 평균 512×512 RGBA8 (= 1 MB/장) × MIP 배율(4/3) ≈ 457 MB.
> 실제 텍스처 중 일부는 1024×1024(4 MB/장)이므로 실측 시 이보다 클 수 있음.

#### 결론: 4096×4096 Shadow Map 적합성 평가

| GPU | VRAM | 4096 Shadow (64 MB) | 판정 | 권장 해상도 |
|-----|------|--------------------|----|------------|
| **Intel UHD 630 (현재 PC)** | ~1 GB 공유 | 831 MB 예상 → 한계 초과 가능성 | ⚠️ **비권장** | **2048** |
| GTX 1060 6 GB | 6 GB | 64 MB는 전체의 1% | ✅ 여유 | 4096 가능 |
| RTX 3060 12 GB | 12 GB | 64 MB는 전체의 0.5% | ✅ 여유 | 4096 권장 |
| Mac Mini M4 (16 GB Unified) | 16 GB | 64 MB는 전체의 0.4% | ✅ 여유 | 4096 권장 |

**UHD 630에서 Bistro 실행 시 실용 권장값:**
```
Shadow Map 해상도: 2048×2048  (D3D12Context.h: SHADOW_MAP_SIZE = 2048)
orthoSize:        249m         (엔진 자동 결정값 유지)
// 4096은 텍스처 메모리와 합산 시 ~1 GB 초과 위험
// → 페이징(시스템 RAM 스왑) 발생 → 프레임 타임 급등
```

**Shadow 텍셀 크기 (ortho=249m 기준):**
| 해상도 | 텍셀 크기 | 품질 |
|--------|----------|------|
| 4096 | 0.061 m/texel | 고품질 |
| **2048** | **0.122 m/texel** | 야외 씬에서 충분 |
| 1024 | 0.243 m/texel | Shadow Acne 심화 |

---

## 기타 모델

추후 씬 테스트 결과를 바탕으로 항목 추가.
