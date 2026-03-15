# 잔여 구현 항목 정리

> 최종 업데이트: 2026-03-14 (Phase 31 완료 — Phase 32부터 진행)

---

## 권장 구현 순서

```
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
