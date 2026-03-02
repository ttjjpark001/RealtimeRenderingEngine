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

### 카메라 추천 세팅
```
Position:  (0.0, 2.0, -10.0)   // 중정 한쪽 끝, 지상 2m
LookAt:    (0.0, 2.0,   0.0)   // 중앙 기둥열 방향
FOV:       60°                   // 넓은 내부 공간에 적합 (45°보다 60° 권장)
Near:      0.1m
Far:       200.0m                // 씬 대각선 37m의 여유분 포함
```

> 대안: 측면 전경 포지션 `(10.0, 4.5, 4.0)` → 중심 방향 (twinpeekz 참조 구현 기준, Y-up 변환)

### 광원 추천 세팅

Sponza는 지붕이 열린 중정 구조 → **Directional Light(태양광) 1개**가 핵심.
Point Light 여러 개보다 강한 Directional + Shadow가 훨씬 사실적.

#### Key Light — Directional (태양)
```
type:       Directional
direction:  normalize(-0.3, -1.0, 0.5)   // 오른쪽 상단 전면에서 내리쬐는 태양
color:      (1.0, 0.95, 0.8)             // 따뜻한 태양광
intensity:  6.0 ~ 8.0                    // HDR 범위 (>1.0)
castShadow: true
```

#### Fill Light — 간접광 모사 (하늘빛)
```
type:       Point (또는 Directional)
position:   (-8.0, 12.0, 6.0)            // 반대편 상단
color:      (0.4, 0.5, 0.7)             // 차가운 하늘빛 (sky ambient)
intensity:  1.5 ~ 2.0
castShadow: false
```

#### 현재 엔진 4-광원 체계에 맞춘 배치 (Point Light 근사)
| 역할 | position | color | intensity |
|------|----------|-------|-----------|
| Key (태양 근사) | (8, 18, -6) | (1.0, 0.95, 0.8) warm | 10 |
| Fill (하늘빛) | (-8, 12, 6) | (0.8, 0.85, 1.0) cool | 3 |
| Back (rim) | (0, 20, 8) | (1.0, 1.0, 1.0) neutral | 4 |
| Orbit | 현재 구현 그대로 | — | 6 |

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

#### 수정 포인트 요약
1. `src/RHI/D3D12/D3D12Context.h`
   ```cpp
   // 변경 전
   static constexpr uint32 SHADOW_MAP_SIZE = 1024;
   // 변경 후 (Sponza 권장)
   static constexpr uint32 SHADOW_MAP_SIZE = 2048;  // 또는 4096
   ```

2. `src/Renderer/Renderer.cpp` — Directional Light LVP 생성부
   ```cpp
   // 변경 전
   XMMatrixOrthographicLH(20.0f, 20.0f, 0.1f, 100.0f)
   // 변경 후 (Sponza 권장)
   XMMatrixOrthographicLH(50.0f, 50.0f, 0.1f, 100.0f)
   ```

---

## FlightHelmet (glTF)

경로: `assets/test-models/FlightHelmet.gltf`

> 소형 오브젝트 — 기본 Fit to Scene 카메라 배치로 충분.
> 추천 세팅 미작성 (추후 추가 예정).

---

## 기타 모델

추후 씬 테스트 결과를 바탕으로 항목 추가.
