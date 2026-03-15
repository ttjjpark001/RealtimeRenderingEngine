# Hardware Requirements — 실시간 렌더링 엔진

Phase 01 ~ Phase 49 전 구간을 구현·실행하기 위한 하드웨어 요구사항 분석.
작성일: 2026-03-07

---

## 개발자 현재 PC 사양 분석

### 사양

| 항목 | 값 |
|------|-----|
| CPU | Intel Core i7-10700K @ 3.8 GHz (8코어 / 16스레드) |
| RAM | 32 GB |
| GPU | Intel UHD Graphics 630 |
| VRAM | 128 MB 전용 (공유 시스템 메모리 추가 사용 가능, 최대 ~1 GB) |
| GPU 메모리 대역폭 | ~45 GB/s (CPU와 공유) |
| DirectX Feature Level | 12_0 |
| DXR (Ray Tracing) | ❌ 미지원 |
| Mesh Shader (SM 6.5) | ❌ 미지원 |

### CPU / RAM 평가

| 항목 | 평가 |
|------|------|
| i7-10700K 8코어/16스레드 | ✅ Phase 49까지 CPU 작업 충분 (Assimp 파싱, 멀티스레드 로딩, RRScenePreprocessor 등) |
| 32 GB RAM | ✅ Bistro 대형 씬 포함 CPU 메모리에 여유 있음 |

### GPU 문제 — 세 가지 핵심 제약

#### 1. VRAM 부족 (Phase 27 이후 전체에 영향)

1080p 기준 Phase별 VRAM 누적 요구량:

| Phase 범위 | 주요 버퍼 | 누적 VRAM |
|-----------|---------|----------|
| Phase 01-22 | Single RT + Depth | 256 ~ 512 MB |
| Phase 23-36 | Single RT + Shadow Map(최대 8장 × 2048²) + Hi-Z | 1 ~ 2 GB |
| Phase 37-44 | G-Buffer MRT 3장 + Depth + HDR RT + SSAO/Bloom/TAA/DoF/SSR 버퍼 | 4 ~ 6 GB |
| Phase 45 | + DDGI 256 Probe Octahedral Map | 5 ~ 7 GB |
| Phase 46 | + BLAS/TLAS + DXR 레이 페이로드 버퍼 | 6 ~ 8 GB |
| Phase 47-48 | + Meshlet 메타데이터 + FSR/DLSS 업스케일 버퍼 | 7 ~ 9 GB |

> G-Buffer 상세 (Phase 37, 1080p):
> | RT | 포맷 | 크기 |
> |----|------|------|
> | RT0 (Albedo+Metallic) | R8G8B8A8_UNORM_SRGB | 8 MB |
> | RT1 (Normal+Roughness) | R16G16B16A16_FLOAT | 16 MB |
> | RT2 (Emissive+AO) | R8G8B8A8_UNORM | 8 MB |
> | Depth (SRV 겸용) | D32_FLOAT | 8 MB |
> | HDR 출력 | R16G16B16A16_FLOAT | 16 MB |
> | **소계** | | **~56 MB** (버퍼만, 씬 텍스처 제외) |

#### 2. 미지원 GPU 기능 (Phase 46, 47 핵심 기능 구현 불가)

| Phase | 필요 기능 | UHD 630 지원 | 결과 |
|-------|----------|------------|------|
| Phase 33 | Compute Shader (Hi-Z) | ✅ 지원, 매우 느림 | 동작은 하나 실용 성능 안 나옴 |
| Phase 46 | DXR Tier 1.1 | ❌ **미지원** | Fallback(PCF Shadow + SSR)으로만 실행됨 |
| Phase 47 | Mesh Shader (SM 6.5) | ❌ **미지원** | Fallback(DrawIndexedInstanced)으로만 실행됨 |
| Phase 48 | FSR 3 | 이론상 가능 | GPU 자체가 너무 느려 무의미 |
| Phase 48 | DLSS 3 | ❌ NVIDIA RTX 전용 | FSR 3 fallback |

#### 3. Phase별 현실적 실행 가능성 요약

| Phase 범위 | 가능 여부 | 비고 |
|-----------|----------|------|
| Phase 01-11 (기본 엔진) | ✅ 정상 동작 | 간단한 도형, vertex-color |
| Phase 12-24 (PBR + Shadow) | ⚠️ 저해상도에서만 | Sponza 로딩 가능하나 느림. Shadow Map 4096 불가 |
| Phase 25-26 (Bistro) | ⚠️ 매우 느림 | Bistro 2.8M 삼각형 + 텍스처로 VRAM 부족 가능성 높음 |
| Phase 27-32 (최적화) | ⚠️ 구현 가능, 검증 어려움 | VRAM 예산 모니터링이 항상 경고 상태 |
| Phase 33-36 (Hi-Z, CubeMap, 애니) | ⚠️ 동작하나 매우 느림 | Compute/6-pass depth가 극도로 느림 |
| Phase 37-44 (Deferred + 포스트) | ❌ G-Buffer만으로도 VRAM 초과 | 720p 이하에서 간신히 실행 가능할 수도 있음 |
| Phase 45 (DDGI) | ❌ 사실상 불가 | Probe 텍스처 + G-Buffer 동시 불가 |
| Phase 46 (DXR) | ❌ 하드웨어 미지원 | Fallback 코드만 실행됨, DXR 기능 자체 검증 불가 |
| Phase 47 (Mesh Shader) | ❌ 하드웨어 미지원 | Fallback 코드만 실행됨, Mesh Shader 자체 검증 불가 |
| Phase 48 (FSR/DLSS) | ❌ GPU 성능 자체가 부족 | SDK 통합 코드 작성은 가능, 실제 렌더링 검증 불가 |

---

## ASUS Dual GeForce RTX 5060 Ti 16GB 분석 (2026-03-15 추가)

> ASUS Dual GeForce RTX™ 5060 Ti 16GB GDDR7 OC Edition (PCIe 5.0, DLSS 4, HDMI 2.1b, DisplayPort 2.1b)

### 사양

| 항목 | 값 |
|------|-----|
| GPU 아키텍처 | NVIDIA Blackwell (GB206) |
| VRAM | 16 GB GDDR7 |
| GPU 메모리 대역폭 | ~448 GB/s |
| DirectX Feature Level | 12_2 |
| Shader Model | 6.9 |
| DXR (Ray Tracing) | ✅ Tier 1.1 (4세대 RT 코어) |
| Mesh Shader (SM 6.5) | ✅ DirectX 12 Ultimate |
| DLSS | ✅ DLSS 4 (4세대 Tensor 코어, Multi Frame Generation 지원) |
| PCIe | 5.0 |

### Phase 03 전 기능 적합성 평가

| Phase | 핵심 기능 | 요구 사항 | RTX 5060 Ti |
|-------|----------|----------|-------------|
| Phase 32 | RRScenePreprocessor | CPU 작업 주도 | ✅ |
| Phase 33 | Hi-Z GPU Occlusion Culling | Compute Shader (UAV) | ✅ |
| Phase 34 | Cube Map Shadow (Point Light) | TextureCube DSV/SRV | ✅ |
| Phase 35~36 | Skeletal Animation | 일반 D3D12 | ✅ |
| Phase 37 | Deferred Rendering (G-Buffer MRT) | MRT 4장 동시 | ✅ |
| Phase 38 | HDR + Tone Mapping | Compute + R16G16B16A16_FLOAT | ✅ |
| Phase 39 | SSAO | Compute (Hemisphere Kernel) | ✅ |
| Phase 40 | Bloom + Post-Processing | Compute + Ping-Pong Buffer | ✅ |
| Phase 41 | TAA | Motion Vector RT + History Buffer | ✅ |
| Phase 42 | Motion Blur + DoF | Compute (Tile Max Velocity, CoC) | ✅ |
| Phase 43 | SSR + Refraction | Hi-Z Raymarching (Compute) | ✅ |
| Phase 44 | SSSSS | Compute (Separable Gaussian) | ✅ |
| Phase 45 | DDGI (GI Probe) | Compute + Texture2DArray | ✅ |
| **Phase 46** | **DXR Hybrid Ray Tracing** | **DXR Tier 1.1 필수** | ✅ **(4세대 RT 코어)** |
| **Phase 47** | **Nanite-style Mesh Shader** | **SM 6.5 Mesh/Amplification Shader** | ✅ **(DirectX 12 Ultimate)** |
| **Phase 48** | **DLSS 4 + Neural Denoising** | **DLSS: NVIDIA RTX 전용** | ✅ **(DLSS 4 네이티브 지원)** |
| Phase 49 | 최종 벤치마크 (Sponza + Bistro 60fps) | Full Phase 03 파이프라인 | ✅ 여유 있음 |

### VRAM 여유도 (16 GB GDDR7 기준, 1080p)

Phase 03 최대 동시 버퍼 사용량 (Deferred + DXR + DLSS 풀 파이프라인):

| 항목 | 예상 용량 |
|------|---------|
| G-Buffer 3장 + HDR RT (1080p) | ~100~200 MB |
| Shadow Maps (Directional 4096² + Cube Map 4개) | ~300 MB |
| Hi-Z Mip chain, SSAO, Bloom, TAA History, Motion Blur, SSR | ~200 MB |
| DDGI Probe Textures (256 Probe) | ~50~100 MB |
| TLAS/BLAS (DXR 가속 구조, Sponza+Bistro 규모) | ~500 MB~1 GB |
| 씬 메시 + 텍스처 (Sponza 52 MB + Bistro) | ~2~3 GB |
| DLSS 4 업스케일 버퍼, Denoiser History | ~200 MB |
| **총합** | **~3~5 GB** |

> **16 GB GDDR7** 기준 총합의 3배 이상 여유. Bistro + Sponza 동시 로딩, 4K 해상도 전환에도 VRAM 부족 없음.

### Phase별 현실적 실행 가능성 요약

| Phase 범위 | 가능 여부 | 비고 |
|-----------|----------|------|
| Phase 32-36 (전처리, Hi-Z, CubeMap, 애니) | ✅ 완전 동작 | 4세대 Compute 인프라, 고성능 |
| Phase 37-44 (Deferred + 포스트) | ✅ 완전 동작 | 1080p/1440p 모두 여유 |
| Phase 45 (DDGI) | ✅ 완전 동작 | Probe 텍스처 + G-Buffer 동시에 여유 |
| Phase 46 (DXR) | ✅ **하드웨어 완전 검증 가능** | 4세대 RT 코어, Tier 1.1 네이티브 |
| Phase 47 (Mesh Shader) | ✅ **하드웨어 완전 검증 가능** | DirectX 12 Ultimate (SM 6.9) |
| Phase 48 (DLSS 4 + FSR 3 + NRD) | ✅ **DLSS 4 네이티브, NRD SDK 지원** | RTX Tensor 코어 기반 DLSS 4 풀 지원 |
| Phase 49 (벤치마크: Sponza+Bistro 60fps) | ✅ DLSS 없이도 달성, DLSS 활성 시 여유 | — |

### 기존 권장 사양 대비 위치

| 항목 | 최소 (RTX 2060) | 권장 (RTX 3060 12 GB) | **RTX 5060 Ti 16 GB** |
|------|----------------|----------------------|----------------------|
| DXR | Tier 1.0 | Tier 1.1 | ✅ Tier 1.1 (4세대) |
| Mesh Shader | ✅ | ✅ | ✅ |
| VRAM | 8 GB | 12 GB | **16 GB** |
| DLSS | DLSS 2 | DLSS 3 | **DLSS 4** |
| Phase 46 RT 성능 | 느림 | 보통 | **충분~여유** |
| Phase 49 60fps 목표 | ⚠️ 빠듯 | ✅ 가능 | ✅ **여유** |

### 결론

RTX 5060 Ti 16 GB는 Phase 03(Phase 32~49) **전 기능을 제약 없이 구현·검증**할 수 있는 하드웨어다.
특히 Phase 46(DXR), Phase 47(Mesh Shader), Phase 48(DLSS 4) 세 가지 핵심 고급 기능 모두 네이티브 하드웨어 지원으로 fallback 없이 완전 검증 가능하다.
16 GB GDDR7 VRAM은 Phase 49 풀 파이프라인 기준 3배 이상 여유가 있어, 4K 해상도나 Bistro+Sponza 동시 로딩 등 극단적인 시나리오에서도 VRAM 부족이 발생하지 않는다.

---

## GeForce RTX 5070 12GB 분석 (2026-03-15 추가)

### 사양

| 항목 | 값 |
|------|-----|
| GPU 아키텍처 | NVIDIA Blackwell (GB205) |
| VRAM | 12 GB GDDR7 |
| 메모리 버스 폭 | 192-bit |
| GPU 메모리 대역폭 | ~672 GB/s |
| DirectX Feature Level | 12_2 |
| Shader Model | 6.9 |
| DXR (Ray Tracing) | ✅ Tier 1.1 (4세대 RT 코어) |
| Mesh Shader (SM 6.5) | ✅ DirectX 12 Ultimate |
| DLSS | ✅ DLSS 4 (5세대 Tensor 코어, Multi Frame Generation 지원) |
| PCIe | 5.0 |

### Phase 03 전 기능 적합성 평가

| Phase | 핵심 기능 | 요구 사항 | RTX 5070 12 GB |
|-------|----------|----------|----------------|
| Phase 32 | RRScenePreprocessor | CPU 작업 주도 | ✅ |
| Phase 33 | Hi-Z GPU Occlusion Culling | Compute Shader (UAV) | ✅ |
| Phase 34 | Cube Map Shadow (Point Light) | TextureCube DSV/SRV | ✅ |
| Phase 35~36 | Skeletal Animation | 일반 D3D12 | ✅ |
| Phase 37 | Deferred Rendering (G-Buffer MRT) | MRT 4장 동시 | ✅ |
| Phase 38 | HDR + Tone Mapping | Compute + R16G16B16A16_FLOAT | ✅ |
| Phase 39 | SSAO | Compute (Hemisphere Kernel) | ✅ |
| Phase 40 | Bloom + Post-Processing | Compute + Ping-Pong Buffer | ✅ |
| Phase 41 | TAA | Motion Vector RT + History Buffer | ✅ |
| Phase 42 | Motion Blur + DoF | Compute (Tile Max Velocity, CoC) | ✅ |
| Phase 43 | SSR + Refraction | Hi-Z Raymarching (Compute) | ✅ |
| Phase 44 | SSSSS | Compute (Separable Gaussian) | ✅ |
| Phase 45 | DDGI (GI Probe) | Compute + Texture2DArray | ✅ |
| **Phase 46** | **DXR Hybrid Ray Tracing** | **DXR Tier 1.1 필수** | ✅ **(4세대 RT 코어, 5060 Ti 대비 ~60% 빠름)** |
| **Phase 47** | **Nanite-style Mesh Shader** | **SM 6.5 Mesh/Amplification Shader** | ✅ **(DirectX 12 Ultimate)** |
| **Phase 48** | **DLSS 4 + Neural Denoising** | **DLSS: NVIDIA RTX 전용** | ✅ **(DLSS 4 네이티브 지원)** |
| Phase 49 | 최종 벤치마크 (Sponza + Bistro 60fps) | Full Phase 03 파이프라인 | ✅ **DLSS 없이도 1440p 60fps 달성 가능** |

### VRAM 여유도 (12 GB GDDR7 기준)

Phase 03 최대 동시 버퍼 사용량 (Deferred + DXR + DLSS 풀 파이프라인):

| 항목 | 1080p | 1440p | 4K |
|------|-------|-------|-----|
| G-Buffer 3장 + HDR RT | ~100~200 MB | ~200~370 MB | ~400~800 MB |
| Shadow Maps (Directional 4096² + Cube Map 4개) | ~300 MB | ~300 MB | ~300 MB |
| Hi-Z, SSAO, Bloom, TAA, Motion Blur, SSR | ~200 MB | ~350 MB | ~700 MB |
| DDGI Probe Textures (256 Probe) | ~50~100 MB | ~50~100 MB | ~50~100 MB |
| TLAS/BLAS (DXR, Sponza+Bistro) | ~500 MB~1 GB | ~500 MB~1 GB | ~500 MB~1 GB |
| 씬 메시 + 텍스처 (Sponza + Bistro) | ~2~3 GB | ~2~3 GB | ~2~3 GB |
| DLSS 4 업스케일 버퍼, Denoiser History | ~200 MB | ~300 MB | ~500 MB |
| **총합** | **~3~5 GB** | **~4~6 GB** | **~5~7 GB** |

> **12 GB GDDR7** 기준: 1080p/1440p는 50%~67% 이상 여유. 4K에서도 총합이 12 GB 한계 이내이나, Bistro+Sponza 동시 로딩 + 4K 조합에서 최대 7~8 GB까지 올라갈 수 있어 RTX 5060 Ti 16 GB보다 여유 폭이 좁다.

### Phase별 현실적 실행 가능성 요약

| Phase 범위 | 가능 여부 | 비고 |
|-----------|----------|------|
| Phase 32-36 (전처리, Hi-Z, CubeMap, 애니) | ✅ 완전 동작 | 672 GB/s 대역폭, 고성능 |
| Phase 37-44 (Deferred + 포스트) | ✅ 완전 동작 | 1080p/1440p/4K 모두 여유 |
| Phase 45 (DDGI) | ✅ 완전 동작 | Probe 텍스처 + G-Buffer 동시에 여유 |
| Phase 46 (DXR) | ✅ **하드웨어 완전 검증 가능** | 4세대 RT 코어, Tier 1.1 네이티브, 5060 Ti보다 RT 성능 우위 |
| Phase 47 (Mesh Shader) | ✅ **하드웨어 완전 검증 가능** | DirectX 12 Ultimate (SM 6.9) |
| Phase 48 (DLSS 4 + FSR 3 + NRD) | ✅ **DLSS 4 네이티브, NRD SDK 지원** | 5세대 Tensor 코어 기반 DLSS 4 풀 지원 |
| Phase 49 (벤치마크: Sponza+Bistro 60fps) | ✅ **DLSS 없이 1440p 60fps 달성 가능** | DLSS 활성 시 4K도 여유 |

### 결론

RTX 5070 12 GB는 Phase 03(Phase 32~49) **전 기능을 제약 없이 구현·검증**할 수 있는 하드웨어다.
RTX 5060 Ti 16 GB와 동일하게 Phase 46(DXR), Phase 47(Mesh Shader), Phase 48(DLSS 4) 모두 네이티브 지원이며, GPU 셰이더/RT/대역폭 성능이 5060 Ti를 크게 상회한다.
유일한 상대적 단점은 VRAM이 12 GB로 5060 Ti(16 GB)보다 4 GB 적다는 것이며, 4K + Bistro+Sponza 동시 로딩 극단적 시나리오에서 VRAM 여유가 좁아질 수 있다. 그러나 Phase 49 기준 풀 파이프라인에서는 12 GB로 충분하다.

---

## RTX 5070 12 GB vs RTX 5060 Ti 16 GB 비교

두 카드 모두 Phase 32~49 전 기능을 제약 없이 구현·검증할 수 있는 하드웨어다. 선택 기준은 GPU 성능(속도)과 VRAM(용량) 중 무엇을 우선시하느냐다.

### 하드웨어 사양 비교

| 항목 | RTX 5070 12 GB | RTX 5060 Ti 16 GB |
|------|---------------|------------------|
| GPU 칩 | GB205 | GB206 |
| VRAM | **12 GB** GDDR7 | **16 GB** GDDR7 |
| 메모리 버스 | **192-bit** | 128-bit |
| 메모리 대역폭 | **~672 GB/s** (+50%) | ~448 GB/s |
| GPU 셰이더 성능 (FP32) | **~+30% 우위** | 기준 |
| RT 코어 세대 | 4세대 Blackwell | 4세대 Blackwell |
| RT 코어 수 | **더 많음** (SM 비례) | 더 적음 |
| RT 성능 | **~50~60% 우위** | 기준 |
| Tensor 코어 세대 | 5세대 Blackwell | 5세대 Blackwell |
| DLSS 4 | ✅ | ✅ |
| DXR Tier | 1.1 | 1.1 |
| Mesh Shader | SM 6.9 | SM 6.9 |
| DirectX FL | 12_2 | 12_2 |
| PCIe | 5.0 | 5.0 |
| TDP | ~150 W | ~180 W |
| 2026년 기준 가격 | ~80~90만원 | ~60~70만원 |

### 프로젝트 Phase별 실질 차이

| 항목 | RTX 5070 12 GB | RTX 5060 Ti 16 GB |
|------|---------------|------------------|
| Phase 32-36 (전처리, Hi-Z, 애니) | 차이 미미 (CPU 주도 또는 경량 Compute) | 차이 미미 |
| Phase 37-44 (Deferred + 포스트, 1080p) | 두 카드 모두 여유 → 차이 미미 | 두 카드 모두 여유 |
| Phase 37-44 (1440p) | ✅ 여유 | ✅ 여유 |
| Phase 37-44 (4K) | ✅ 가능, VRAM 여유 있음 | ✅ VRAM 여유 더 큼 |
| Phase 45 (DDGI Probe) | ✅ | ✅ |
| **Phase 46 (DXR RT)** | **✅ RT 성능 ~50~60% 우위** — RT Shadow/Reflection 품질 검증에 실질적 이점 | ✅ 가능하나 5070 대비 느림 |
| Phase 47 (Mesh Shader) | ✅ (Amplification Shader 처리량 우위) | ✅ |
| Phase 48 (DLSS 4 + NRD) | ✅ Tensor 성능 우위 → Denoiser 속도 빠름 | ✅ |
| **Phase 49 (60fps 벤치마크, 1080p)** | ✅ **DLSS 없이도 달성** | ✅ DLSS 없이 달성 |
| **Phase 49 (60fps 벤치마크, 1440p)** | ✅ **DLSS 없이도 달성** | ⚠️ DLSS 권장 |
| **Phase 49 (Bistro+Sponza 4K)** | ⚠️ VRAM 빠듯할 수 있음 (7~8 GB) | ✅ VRAM 여유 |

### 결정 기준 요약

| 우선순위 | 추천 카드 | 이유 |
|---------|----------|------|
| **GPU 성능 / RT 품질 검증** | **RTX 5070 12 GB** | Phase 46 DXR RT 성능 ~50~60% 우위, Phase 49 고해상도 목표 달성 용이 |
| **메모리 대역폭** | **RTX 5070 12 GB** | 672 vs 448 GB/s — Deferred G-Buffer 읽기, DDGI Probe 업데이트, SSR Raymarching에 유리 |
| **VRAM 여유 (대형 씬, 4K)** | **RTX 5060 Ti 16 GB** | 4K + Bistro+Sponza 동시 로딩 등 극단적 VRAM 시나리오에서 안정적 |
| **가성비** | **RTX 5060 Ti 16 GB** | ~15~20만원 저렴, Phase 49까지 전 기능 검증 가능 |
| **전력 효율** | **RTX 5070 12 GB** | TDP 150 W vs 180 W — 동일 성능 대비 전력 유리 |

> **결론**: 두 카드 모두 Phase 49까지 전 기능을 제약 없이 구현·검증할 수 있다. **RTX 5070 12 GB**는 GPU/RT/대역폭 성능이 우위이며 Phase 46 DXR과 Phase 49 고해상도 목표에서 실질적 이점이 있다. **RTX 5060 Ti 16 GB**는 VRAM이 4 GB 많아 4K + 대형 씬 극단적 시나리오에서 더 안정적이고 가격도 저렴하다. 이 프로젝트 범위(1080p/1440p Phase 49 벤치마크 기준)에서는 **RTX 5070 12 GB가 개발 경험 면에서 유리**하나, 예산이 제약된다면 RTX 5060 Ti 16 GB로도 완전한 구현·검증이 가능하다.

---

## Mac Mini M4 분석 (Metal 재작성 시나리오)

> **전제**: 현재 코드베이스(Win32 + DirectX 12 + HLSL)를 macOS(AppKit + Metal + MSL)로 전면 재작성하는 경우의 하드웨어 가능성 분석.
> 현재 코드베이스 그대로는 macOS에서 실행 불가 (Win32 / DirectX 12 / HLSL 미존재, Boot Camp도 Apple Silicon 미지원).

### 사양

| 항목 | Mac Mini M4 (기본형) | Mac Mini M4 Pro |
|------|---------------------|-----------------|
| SoC | Apple M4 | Apple M4 Pro |
| CPU | 10-core (4P + 6E) | 14-core (10P + 4E) |
| GPU | 10-core Apple GPU | 20-core Apple GPU |
| Unified Memory | 16 GB 또는 24 GB | 24 GB 또는 64 GB |
| GPU 메모리 대역폭 | ~120 GB/s | ~273 GB/s |
| Metal Ray Tracing | ✅ 하드웨어 가속 | ✅ 하드웨어 가속 |
| Metal Mesh Shaders | ✅ (Metal 3, M3 세대부터) | ✅ |
| MetalFX Upscaling | ✅ (FSR 2 기반 Spatial + Temporal) | ✅ |
| DLSS 3 | ❌ NVIDIA 전용 | ❌ NVIDIA 전용 |
| GPU 성능 (게임 래스터) | ≈ **GTX 1650** 수준 | ≈ **RTX 3060** 수준 |

> M4 기본형 GPU 성능 참고: [NotebookCheck M4 GPU 벤치마크](https://www.notebookcheck.net/Apple-M4-10-core-GPU-Benchmarks-and-Specs.835807.0.html)

### DirectX → Metal API 대응표

Metal 재작성 시 각 DirectX/Win32 컴포넌트를 macOS 대응 API로 교체해야 한다.

| DirectX / Win32 | macOS / Metal 대체 | 비고 |
|-----------------|-------------------|------|
| Win32 API (HWND, WndProc, WM_*) | AppKit (NSWindow, NSView, NSApplication) | 플랫폼 레이어 전면 교체 |
| DirectX 12 (ID3D12Device 등) | Metal (MTLDevice, MTLCommandQueue 등) | RHI D3D12 → RHI Metal |
| HLSL (.hlsl → .cso) | MSL (Metal Shading Language, .metal) | 셰이더 전부 재작성 |
| DXR (BLAS/TLAS, ID3D12StateObject) | Metal Acceleration Structure (MTLAccelerationStructure) | Ray Tracing API 재작성 |
| Mesh Shaders (DirectX) | Metal Mesh Shaders (Metal 3) | API는 다르지만 동일 기능 |
| D3D11On12 + D2D1 + DirectWrite (HUD) | Core Text + Core Graphics | HUD 텍스트 렌더링 교체 |
| DXGI_FORMAT (픽셀 포맷) | MTLPixelFormat | 포맷 매핑 필요 |
| NVIDIA Streamline SDK (DLSS) | MetalFX Upscaling | MetalFX는 FSR 2 기반 |
| Assimp (glTF 파싱) | Assimp ✅ macOS 지원 | 교체 불필요 |
| DirectXMath | simd (Apple SIMD 라이브러리) 또는 GLM | 수학 라이브러리 교체 |

### Phase별 구현 가능성 (Metal 재작성 기준)

#### M4 기본형 (10-core GPU ≈ GTX 1650)

| Phase 범위 | 구현 가능 | 성능 | 비고 |
|-----------|----------|------|------|
| Phase 01-22 (기본 엔진) | ✅ 완전 구현 | 충분 | Metal compute, MSL 셰이더 |
| Phase 23-26 (최적화 + Bistro) | ✅ 완전 구현 | 충분 | Unified 16 GB로 Bistro VRAM 여유 |
| Phase 27-36 (Hi-Z, CubeMap, 애니) | ✅ 완전 구현 | 보통 | Metal Compute Kernel, TextureCube 지원 |
| Phase 37-44 (Deferred + 포스트) | ✅ 완전 구현 | ⚠️ 1080p 30~60 fps | G-Buffer ~56 MB — Unified 16 GB로 여유 |
| Phase 45 (DDGI) | ✅ 완전 구현 | ⚠️ 보통 | Texture2DArray / 3D Texture Metal 지원 |
| Phase 46 (Ray Tracing) | ✅ 하드웨어 검증 가능 | ⚠️ 느림 | Metal Acceleration Structure로 구현. RT 전용 코어 없어 RTX 대비 느림 |
| Phase 47 (Mesh Shader) | ✅ 하드웨어 검증 가능 | 충분 | Metal 3 Mesh Shaders 하드웨어 지원 |
| Phase 48 (Upscaling) | ⚠️ 부분 구현 | 보통 | MetalFX(FSR 2 기반) 사용. **DLSS 3 불가** — MetalFX로 대체 |
| Phase 49 (코드 리뷰) | ✅ | — | — |

> **M4 기본형 한계**: Phase 46 레이 트레이싱은 하드웨어가 지원하나, 전용 RT 코어가 없어 RTX 대비 성능이 낮다. 1080p Full RT는 실용적이지 않을 수 있다. Phase 37-44 Deferred Shading은 1080p에서 30~60 fps 수준.

#### M4 Pro (20-core GPU ≈ RTX 3060)

| Phase 범위 | 구현 가능 | 성능 | 비고 |
|-----------|----------|------|------|
| Phase 01-45 | ✅ 완전 구현 | 충분~여유 | 273 GB/s 대역폭, 24~64 GB Unified Memory |
| Phase 46 (Ray Tracing) | ✅ 완전 검증 | 보통~충분 | RT 성능이 M4 기본형 대비 2배, 실용적 RT 품질 달성 가능 |
| Phase 47 (Mesh Shader) | ✅ 완전 검증 | 충분 | Metal 3 Mesh Shaders |
| Phase 48 (Upscaling) | ⚠️ 부분 구현 | 충분 | MetalFX(FSR 2 기반). DLSS 3 불가 |
| Phase 49 | ✅ | 충분 | — |

### Phase 48 업스케일링 대체 방안

DLSS 3은 NVIDIA RTX 전용으로 macOS에서 사용 불가. 대체 방안:

| 기능 | Windows (DirectX) | macOS (Metal) |
|------|------------------|---------------|
| 공간 업스케일 | FSR 3 Spatial | MetalFX Spatial Upscaling ✅ |
| 시간적 업스케일 | FSR 3 / DLSS | MetalFX Temporal Upscaling ✅ (FSR 2 기반) |
| 프레임 생성 | FSR 3 Frame Gen / DLSS 3 | ❌ MetalFX 미지원 (FSR 3 오픈소스화로 향후 가능성 있음) |
| 신경망 디노이징 | NRD SDK (NVIDIA) | ❌ NRD 미지원 — 대안 디노이저 직접 구현 필요 |

> MetalFX는 AMD FSR 기술 기반으로 구현되었다. ([Tom's Hardware 기사](https://www.tomshardware.com/pc-components/gpus/amd-fsr-is-the-building-block-for-apples-metalfx-upscaling-tech-the-apps-legal-info-references-the-usage-of-amd-fsr))
> AMD가 FSR 3를 오픈소스(MIT)로 공개했으므로, 향후 MetalFX에 Frame Generation이 통합될 가능성이 있다.

### VRAM(Unified Memory) 여유도

Unified Memory는 CPU와 GPU가 공유하지만 16 GB / 24 GB 전체를 GPU가 사용할 수 있어, 전용 VRAM 기준 PC GPU 대비 여유가 크다.

| Phase 범위 | 필요 GPU 메모리 | M4 16 GB | M4 24 GB |
|-----------|----------------|----------|----------|
| Phase 01-36 | 1 ~ 2 GB | ✅ 여유 | ✅ 여유 |
| Phase 37-44 (G-Buffer 포함) | 4 ~ 6 GB | ✅ 여유 | ✅ 여유 |
| Phase 45 (DDGI) | 5 ~ 7 GB | ✅ 여유 | ✅ 여유 |
| Phase 46 (Ray Tracing AS) | 6 ~ 8 GB | ⚠️ 빠듯함 | ✅ 여유 |
| Phase 47-48 (Meshlet + 업스케일) | 7 ~ 9 GB | ⚠️ 빠듯함 | ✅ 여유 |

> M4 기본형(16 GB)은 Phase 46-48에서 메모리가 빠듯할 수 있다. OS 시스템 점유분(~4~6 GB)을 고려하면 실질 GPU 가용 메모리는 10~12 GB 수준. **24 GB 이상 권장.**

### 결론

| 항목 | M4 기본형 (10-core GPU) | M4 Pro (20-core GPU) |
|------|------------------------|----------------------|
| Phase 01-45 전 기능 구현 | ✅ 가능 | ✅ 가능 |
| Phase 46 Ray Tracing 검증 | ⚠️ 가능하나 성능 제한 | ✅ 실용적 성능 |
| Phase 47 Mesh Shader 검증 | ✅ 가능 | ✅ 가능 |
| Phase 48 DLSS 3 | ❌ 불가 (MetalFX 대체) | ❌ 불가 (MetalFX 대체) |
| Phase 49 | ✅ | ✅ |
| 권장 메모리 | 24 GB | 24 GB 이상 |
| 재작성 규모 | **대규모** (플랫폼 + RHI + 셰이더 전면 교체) | 동일 |

> **결론**: Metal로 재작성하면 M4 기본형도 Phase 47까지 전 기능을 구현하고 하드웨어 검증할 수 있다.
> Phase 48에서 DLSS 3만 MetalFX로 대체하면 실질적으로 Phase 49 전 기능 구현 가능.
> 단, **재작성 규모가 매우 크다** — 플랫폼 레이어(Win32→AppKit), RHI 전체(D3D12→Metal), 셰이더 전부(HLSL→MSL), HUD 텍스트 레이어(D2D1→Core Text) 교체가 필요하다.
> 성능 측면에서 Phase 46 RT는 M4 Pro 이상을 권장한다.

---

## 권장 사양

### Phase 01-36 (Phase 02 전체) 구현 및 실행

| 항목 | 최소 | 권장 |
|------|------|------|
| GPU | GTX 1060 6 GB | RTX 3060 12 GB |
| VRAM | 6 GB | 12 GB |
| DirectX Feature Level | 12_0 | 12_1 이상 |
| Shader Model | 5.0 | 6.0+ |
| 예상 가격 (2026년 기준) | 중고 GTX 1070 ~15만원 | RTX 3060 신품 ~30만원 |

### Phase 37-49 (Phase 03, 고급 렌더링) 전체 구현 및 실행

| 항목 | 최소 | 권장 |
|------|------|------|
| GPU | RTX 2060 6 GB | **RTX 3060 12 GB** |
| VRAM | 8 GB (1080p 간신히) | **12 GB 이상** |
| DXR | Tier 1.0 (RTX 2000~) | Tier 1.1 (RTX 3000~) |
| Mesh Shader | RDNA2 / RTX 3000~ | RTX 4000~ / RDNA3 |
| DLSS 3 | RTX 4000 전용 | RTX 4070 이상 (없으면 FSR 3로 대체) |
| Shader Model | 6.5 (Mesh Shader 필수) | 6.6+ |
| 예상 가격 (2026년 기준) | 중고 RTX 2060 Super ~20만원 | RTX 3060 신품 ~30만원 |

### 가성비 최선 선택: RTX 3060 12 GB

Phase 49까지 전 기능을 1080p에서 여유 있게 구현·검증할 수 있는 최소 비용 선택:

| 기능 | 지원 여부 |
|------|----------|
| DXR Tier 1.1 | ✅ (Phase 46 핵심 기능 모두 구현 가능) |
| Mesh Shader (SM 6.5) | ✅ (Phase 47 핵심 기능 모두 구현 가능) |
| VRAM 12 GB | ✅ (Phase 49까지 1080p에서 여유 있음) |
| FSR 3 | ✅ |
| DLSS 3 | ❌ (RTX 4000 시리즈 전용 — FSR 3으로 대체) |
| DirectX Feature Level | 12_2 |

> 현재 i7-10700K + 32 GB RAM은 전혀 교체할 필요 없음.
> **GPU만 RTX 3060 12 GB로 업그레이드하면 Phase 49까지 전부 구현 가능.**

---

## GPU 기능 요구사항 상세 (Phase별)

| Phase | 필요 GPU 기능 | 비고 |
|-------|-------------|------|
| Phase 01-22 | DirectX 12 Feature Level 11.0, Shader Model 5.0 | 기본 렌더링 |
| Phase 23-26 | + Compute Shader 5.0 | Hi-Z, LightCuller |
| Phase 27-32 | + 대용량 Descriptor Heap | CBPool, Instancing |
| Phase 33 | + Compute Shader (UAV read/write) | Hi-Z Occlusion Culling |
| Phase 34 | + TextureCube DSV/SRV | Cube Map Shadow |
| Phase 37 | + MRT (4 Render Targets 동시) | G-Buffer |
| Phase 41 | + R16G16_FLOAT Motion Vector RT | TAA |
| Phase 45 | + 3D Texture 또는 Texture2DArray | DDGI Probe |
| Phase 46 | **+ DXR Tier 1.1** | Ray Tracing (fallback 있음) |
| Phase 47 | **+ Mesh Shader (SM 6.5)** | Virtual Geometry (fallback 있음) |
| Phase 48 | + FidelityFX SDK (FSR 3) / Streamline SDK (DLSS) | Neural Upscaling |

> Phase 46과 47은 각각 PCF Shadow+SSR / DrawIndexedInstanced fallback이 구현되어 있으므로,
> 해당 GPU 기능이 없어도 코드를 작성하고 fallback 경로로 렌더링 결과를 볼 수는 있다.
> 단, DXR·Mesh Shader 핵심 기능 자체의 렌더링 검증은 해당 하드웨어에서만 가능하다.
