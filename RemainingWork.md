# 잔여 구현 항목 정리

> 최종 업데이트: 2026-03-14 (Phase 31 완료 — Phase 32부터 진행)

---

## [버그] Bistro 씬 렌더링 버그 수정 (나중에)

Phase 26 구현 후 발견된 Bistro 씬 렌더링 버그들. **현재 코드에 수정이 적용된 상태로 두고, 나중에 정리한다.**

**코드에 적용된 수정 항목 — 분류**

**Bistro 전용 (다른 씬엔 불필요)**

| 수정 내용 | 커밋 | 이유 |
|-----------|------|------|
| PNG→DDS 확장자 fallback | d22737c | Bistro glTF가 PNG 경로를 참조하지만 실제론 DDS 파일만 존재하는 비정상 구조. 정상적인 glTF(DamagedHelmet, Sponza, FlightHelmet 등)에선 발생하지 않음. |
| LoadBistroScene() far plane 명시 고정 | de099a4 | LoadBistroScene()에 max(500, diagonal×10) 하드코딩. 다른 씬은 LoadScene()의 worldBounds 기반 자동 계산으로 충분. |

**과잉 수정 (일반 씬엔 불필요하거나 더 작은 값으로 충분)**

| 수정 내용 | 커밋 | 실제 필요 범위 |
|-----------|------|----------------|
| TRANSIENT_DESCRIPTORS 32768 | 8c62105 | Bistro 551 mesh × 17 = 9,367 때문에 증가. Sponza(~103 mesh)는 2,048 이하면 충분. 증가 자체는 무해하지만 Bistro 전용 스케일링. |
| PERSISTENT_DESCRIPTORS 2048 | 8c62105 | 유사하게 Bistro 텍스처 수 기준 증가. 소형 씬은 512로도 충분. |

**일반적으로 필요한 수정 (모든 씬에 유효)**

| 수정 내용 | 커밋 | 이유 |
|-----------|------|------|
| kMinExtent 0.01→0.5 (AABB 패딩) | 8c62105, d22737c | 어떤 씬이든 Y=0 평면 메시는 존재. Sponza 바닥면에도 동일하게 적용됨. |
| DDS 텍스처 지원 (DirectXTex) | be6be78 | DDS는 DX 표준 포맷. 향후 다른 씬에서도 사용 가능. |
| glTF 카메라 nearPlane/farPlane 적용 | de099a4 | 카메라 노드가 있는 모든 씬에 해당. |
| TextureCache::Clear() persistent index 리셋 | de099a4 | 씬 교체가 있는 모든 경우 필요. |
| STBI_WINDOWS_UTF8 | de099a4 | 한국어 포함 경로 사용하는 모든 PC에서 필요. |
| SceneLoader u8path 유니코드 수정 | be6be78 | 동일. |

**미해결 버그 / 잔여 작업**

1. **Frustum Culling 버그**: 카메라가 -Z 방향으로 이동 시 바닥 타일이 사라짐
   - 조사 완료 파일: `FrustumCuller.cpp`, `Renderer.cpp`, `SceneNode.cpp`, `Camera.cpp`, `SceneGraph.cpp`, `SceneLoader.cpp`, `Engine.cpp`
   - FrustumCuller 수학적 구현은 정상 확인 (LH, 비전치 행렬, invView 변환 올바름)
   - kMinExtent 0.5f AABB 패딩 적용했으나 증상 유지
   - **미확인**: 와이어프레임 + Frustum 경계면 시각화 없이 재현/확인 어려움
   - **다음 단계**: Frustum 시각화 디버그 패스 구현 후 재조사, 또는 Phase 33 Hi-Z 인프라 구축 후 연동

2. **Shadow Map 시각적 튜닝 미완료**
   - 확인 필요: 외부 바닥 → 기둥 접촉면 → 계단/경사 지붕 → 원거리 가로등 → 실내외 개구부
   - SceneSettings.md에 최종 DepthBias/SlopeScaledDepthBias/해상도 값 미기록

---

## 런타임 검증 필요 항목

코드 작업은 모두 완료되었으나 프로그램을 직접 실행하여 확인해야 하는 항목.

| 항목 | 확인 방법 |
|------|----------|
| 대형 씬 벤치마크 | Sponza/Bistro 로딩 → Full PBR+Shadows + 모든 최적화 ON → 60fps 목표 확인 |
| 5단계 렌더링 모드 전체 동작 | Render 메뉴: Wireframe → Solid → Base Color → Full PBR → Full PBR+Shadows 순서로 육안 검증 |
| PIX / 타임스탬프 쿼리 프로파일링 | PIX for Windows 또는 D3D12 Timestamp Query로 Shadow Pass·Main Pass 병목 측정 |
| D3D12 Debug Layer 경고 0건 확인 | Debug 빌드 실행 → Output 창에서 D3D12 WARNING/ERROR 없음 확인 |
| 메모리 누수 점검 | Shutdown 후 D3D12 Live Object 리포트에서 미해제 리소스 없음 확인 |
| 윈도우 리사이즈 / 모드 전환 안정성 | 800×450 → 드래그 리사이즈 → Full Screen → Esc 복귀 사이클 반복 확인 |

---

## 권장 구현 순서

```
[버그] Bistro 씬 미해결 버그 (언제든 진행 가능)
    │           Frustum Culling 버그 재조사 (Frustum 시각화 도구 구현 후)
    │           Shadow Map 시각적 튜닝 + SceneSettings.md 기록
    │
[검증] 런타임 검증 필요 항목 (언제든 진행 가능)
    │           Sponza/Bistro 60fps 벤치마크, 5단계 렌더링 모드 육안 검증
    │           D3D12 Debug Layer 경고 0건, 메모리 누수 점검
    │
Phase 32    RRScenePreprocessor (.rrscene 오프라인 전처리 도구)
    │           Assimp 파싱·이미지 디코딩·LOD·Mip chain 오프라인 처리
    │           렌더러 이중 로딩 경로 (고속/.rrscene + 표준/Assimp)
    │           → Sponza/Bistro 로딩 시간 ~90% 단축
    │
Phase 33    Occlusion Culling (Hi-Z GPU)
    │           CPU Readback 단계 없이 바로 Hi-Z GPU 구현
    │           Compute Shader 파이프라인 + Optimization 메뉴 항목 포함
    │
Phase 34    Point Light Cube Map Shadowing
    │           Omnidirectional Shadow Map (TextureCube, 6-pass depth)
    │
Phase 35    Skeletal Animation
    │           Part A: Node Transform Animation (TRS 키프레임)
    │           Part B: Skeletal Animation (본/스킨, GPU Skinning)
    │
Phase 36    RRScenePreprocessor 확장 (Skeletal Animation 지원)
    │           .rrscene v2: Skeleton/Skin/Animation 섹션 추가
    │
    │   ── Phase 03 시작 ──
    │
Phase 37    Deferred Rendering
Phase 38    HDR Pipeline + Tone Mapping (ACES/Reinhard + Auto-Exposure)
Phase 39    SSAO (Hemisphere Kernel + Bilateral Blur)
Phase 40    Bloom + Post-Processing 파이프라인 (Ping-Pong Buffer)
Phase 41    TAA (Halton Jitter + Variance Clipping + Velocity Buffer)
Phase 42    Motion Blur (Tile-based) + Depth of Field (Bokeh CoC)
Phase 43    SSR (Hi-Z Raymarching) + Refraction (IOR)
Phase 44    Screen Space Subsurface Scattering (Separable 6-weight Gaussian)
Phase 45    Global Illumination — DDGI (Irradiance Probe 3D Grid)
Phase 46    DXR Hybrid Ray Tracing (BLAS/TLAS + RT Shadow + RT Reflection)
Phase 47    Nanite-style Virtual Geometry (Meshlet + Mesh Shader + GPU-Driven)
Phase 48    Neural Upscaling (FSR 3 / DLSS 3) + Neural Denoising (NRD SDK)
    │
Phase 49    Phase 03 코드 리뷰 + 최적화 + 버그 수정 + ARCHITECTURE.md 완성
            Sponza/Bistro Full Phase 03 벤치마크 (60fps 목표)
```

---

## 기타 메모

- **Occlusion Culling Optimization 메뉴 항목**: Phase 33 구현 시 `ID_OPTIM_OCCLUSION_CULL = 8005` 추가 (`8004`는 MipMap 토글에서 사용 중).
