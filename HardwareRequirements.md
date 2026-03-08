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

## Mac Mini M4 분석

### 사양

| 항목 | 값 |
|------|-----|
| SoC | Apple M4 |
| CPU | 10-core (4 Performance + 6 Efficiency) |
| GPU | 10-core Apple GPU |
| Unified Memory | 16 GB 또는 24 GB (CPU/GPU 공유) |
| GPU 메모리 대역폭 | ~120 GB/s |
| DirectX | ❌ **미지원** (macOS는 Metal API 사용) |
| DXR | ❌ (Metal RT API만 지원) |
| Boot Camp | ❌ Apple Silicon에서 미지원 (Intel Mac 전용) |

### 결론: 현재 코드베이스로 실행 불가

이 프로젝트는 **Win32 API + DirectX 12 + HLSL**로 전면 구성되어 있으며, macOS에는 이 중 어느 것도 존재하지 않습니다.

| 방법 | 가능 여부 | 비고 |
|------|----------|------|
| 네이티브 실행 | ❌ | DirectX 12 / Win32 비존재 |
| Boot Camp | ❌ | Apple Silicon 미지원 |
| Parallels Desktop 20 | ⚠️ 극히 제한적 | DX12 에뮬레이션 지원하나 성능 매우 낮음, DXR·Mesh Shader 불가 |
| Metal 재작성 | 🔧 가능하나 대규모 작업 | RHI 레이어 전체를 Metal로 교체해야 함 |

> **참고**: M4 10-core GPU의 하드웨어 성능 자체는 RTX 3050~3060 수준으로 Phase 01-45 정도를 구동할 역량이 있지만, **운영체제·API 불호환으로 현재 코드베이스 그대로는 아무것도 실행할 수 없습니다.**

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
