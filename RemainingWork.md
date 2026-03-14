# 잔여 구현 항목 정리

> 최종 업데이트: 2026-03-13 (Phase 29 완료 — GPU 메모리 최적화)

---

## 잔여 구현 항목

### [버그] Bistro 씬 렌더링 버그 수정 (나중에)

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

**임시 방편 (Phase 32 교체 대상)**

| 수정 내용 | 커밋 | 비고 |
|-----------|------|------|
| 전역 CullMode=NONE + SV_IsFrontFace | be6be78 | Phase 32에서 Material.doubleSided 기반 PSO 분기로 교체 예정 (RM-11) |

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

3. **doubleSided PSO 분기** (Phase 32): 전역 CullMode=NONE → Material.doubleSided 기반 PSO 분기로 교체 (RM-11)

---

### Phase 30 — 통합 & 벤치마크

Phase 27~29 완료 후 전체 파이프라인 연결 및 검증.

| 작업 | 설명 |
|------|------|
| 12단계 파이프라인 통합 | Scene Graph 순회 → Frustum → Occlusion → LOD → Light Cull → Instance Batching → Texture Streaming → CB 갱신 → Material 정렬 → Front-to-Back → Shadow Pass → Main Pass |
| DebugHUD 전체 항목 | VRAM 사용량, 스트리밍 리소스 수, 드로우콜 수 추가 |
| 대형 씬 벤치마크 | Sponza, Bistro 로딩 + Full PBR + Shadows + 모든 최적화 활성 상태에서 60fps 목표 |
| 5단계 렌더링 모드 전체 확인 | Wireframe / Solid / Base Color / Full PBR / Full PBR+Shadows |
| 전체 테스트 통과 확인 | 유닛 + 스모크 통과 |

---

### Phase 31 — RRScenePreprocessor (오프라인 전처리 도구 + 백그라운드 자동 생성)

glTF/GLB/FBX 씬을 처리하여 엔진 전용 바이너리(`.rrscene`)로 저장하는 파이프라인.

| 작업 | 설명 |
|------|------|
| `ScenePreprocessor.h/.cpp` | 전처리 파이프라인 공용 클래스 (`src/Asset/`). `Generate()`(동기) + `GenerateAsync()`(비동기, `std::future<bool>`) |
| VS 프로젝트 추가 | `RRScenePreprocessor` (Console Application) — `ScenePreprocessor::Generate()` 호출하는 얇은 CLI 래퍼 |
| `.rrscene` 포맷 정의 | `src/Asset/RRSceneFormat.h` (공용): Header + Scene/Mesh/Material/Texture/Light 섹션 |
| 전처리 파이프라인 | Assimp 파싱 → Vertex 변환 + Tangent → 프리미티브 분리 → AABB → Auto-LOD → 이미지 디코딩 → Mip chain → 씬 직렬화, 원자적 파일 쓰기(.tmp→rename) |
| 이중 로딩 경로 | `SceneLoader::LoadScene()`: `.rrscene` 존재 + 해시 일치 시 고속 경로, 없으면 Assimp 폴백 |
| 해시 기반 변경 감지 | 원본 파일 크기^수정시각 → sourceHash, 불일치 시 폴백 + 재전처리 시작 |
| 백그라운드 자동 생성 | `Engine::LoadScene()` 표준 경로 완료 후 `GenerateAsync()` 호출. `m_preprocessFuture` 저장, 매 프레임 폴링, 완료 시 로그 출력 |
| DebugHUD 표시 | "Fast (.rrscene)" / "Standard (Assimp)" 로딩 경로 + "Preprocessing scene..." 진행 상태 |

**절감 효과**: 첫 로딩은 표준 경로 + 백그라운드 전처리, 두 번째부터 ~90% 단축 (10~40초 → 1~3초)

---

### Phase 32 — 코드 리뷰 & 문서화

| 작업 | 설명 |
|------|------|
| 전체 코드 리뷰 | Dead code 제거, include 정리, 네이밍 일관성 검증 |
| PBR.hlsl CalcShadow X4000 경고 | FXC 컴파일러 한계 (비교 샘플러 + 동적 cbuffer 인덱스). Texture2DArray로의 리팩터링 또는 FXC 업데이트로 재검토 |
| glTF doubleSided PSO 분기 (RM-11) | 전역 CullMode=NONE → Material.doubleSided 기반 PSO 선택: `false`→CullMode=BACK, `true`→CullMode=NONE |
| GPU 리소스 해제 누락 검사 | Fence 대기 후 해제 보장, ComPtr 사용 일관성 |
| Shadow Map SRV 누수 정리 | RecreateShadowMaps() 시 이전 SRV 8개 누수 — persistent heap 관리 정리 |
| PIX / 타임스탬프 쿼리 프로파일링 | 병목 구간 식별 및 최적화 |
| `ARCHITECTURE.md` 작성 | 전체 엔진 구조, 모듈 간 의존성, 렌더 파이프라인 다이어그램 |

---

### Phase 33 — Occlusion Culling (Hi-Z GPU)

> **Phase 33 시작 전**: Phase 02 Backup 생성 (1회)
> - `Phase 02 Backup/` 폴더에 src/, tests/, assets/, shaders/ 복사
> - `Phase 01 Backup/` 및 빌드 산출물(bin/, .git/, *.user 등) 제외
> - **백업 완료 후 `Phase 02 Backup/` 폴더 안의 파일은 절대 수정하지 않는다.**

현재 `OcclusionCuller::IsOccluded()`는 항상 `false`를 반환하는 스텁이다.
CPU Readback 간이 방식을 거치지 않고 GPU Hi-Z 방식으로 바로 구현한다.

| 작업 | 설명 |
|------|------|
| Compute Shader 인프라 | `D3D12ComputePipeline.h/.cpp` 신규, `D3D12Context::Dispatch()` 추가, UAV descriptor 관리 |
| Hi-Z Buffer 생성 | 이전 프레임 Depth → `R32_FLOAT` SRV 복사 후 Compute로 Mip chain(UAV) 생성 (max 필터) |
| GPU-side AABB 비교 | AABB 8코너 → NDC → screen-space min/max, 최적 Mip 레벨 샘플링, 근거리 Z 비교 |
| Readback + Fence 동기화 | GPU 판정 결과 → Readback Buffer → CPU 읽기 (1프레임 레이턴시) |
| `occlusionCulledNodes` 통계 | CullStats 반영, DebugHUD 표시 |
| Optimization 메뉴 항목 추가 | `ID_OPTIM_OCCLUSION_CULL = 8004` (Win32Menu + Engine 콜백 연결) |

---

### Phase 34 — Point Light Cube Map Shadowing

`castShadow = true`인 Point Light에 대해 6면 TextureCube 기반 Omnidirectional Shadow Map 구현.

| 작업 | 설명 |
|------|------|
| TextureCube 리소스 생성 | `TEXTURE2D_ARRAY` (ArraySize=6, D32_FLOAT), 6개 DSV + 1개 SRV, 최대 4광원 |
| 6-pass Shadow Depth | 광원 1개당 ±X/±Y/±Z 방향 6회 depth pass, FOV=90°, aspect=1.0 |
| HLSL TextureCube 샘플링 | `TextureCube PointShadowMap[]` 바인딩, `lightToPixel` 방향 벡터로 depth lookup |
| LightConstants 타입 구분 | Directional/Spot(Texture2D) vs Point(TextureCube) 구분 플래그 추가 |
| LightCuller 연동 | shadow casting Point light도 거리 기반 culling 적용 |

---

### Phase 35 — Skeletal Animation

glTF Node Transform 애니메이션(키프레임)과 Skeletal Animation(본/스킨) 구현.

**Part A: Node Transform Animation**

| 작업 | 설명 |
|------|------|
| `Animation.h` | `AnimationClip`, `AnimationChannel`, `Keyframe<T>` 구조체 |
| 키프레임 보간 | LINEAR (Lerp/Slerp), STEP, CUBICSPLINE |
| SceneLoader 확장 | `aiAnimation` → `AnimationClip` 변환, target name → SceneNode 매핑 |
| `AnimationController` | `Update(dt)`, Play/Pause/Loop, 클립 목록 메뉴 |

**Part B: Skeletal Animation**

| 작업 | 설명 |
|------|------|
| `Skeleton.h` | `Bone`, `Skeleton`, `Skin` 구조체, inverse bind matrix |
| SceneLoader 확장 | `aiMesh::mBones` → `Skeleton/Skin` 생성, per-vertex joint/weight 추출 |
| Vertex 포맷 확장 | `JOINTS_0` (XMUINT4) + `WEIGHTS_0` (XMFLOAT4) 추가, Input Layout/HLSL 갱신 |
| GPU Skinning | `SkinCB : register(b4)` (joint matrix palette 128개), PBR.hlsl VS 스키닝 계산 |

---

### Phase 36 — RRScenePreprocessor 확장 (Skeletal Animation 지원)

| 작업 | 설명 |
|------|------|
| `.rrscene` v2 포맷 확장 | `RRSceneFormat.h`: version=2, Skeleton Section + Animation Section 추가 |
| 전처리기: Skeleton/Animation 직렬화 | Bone/Skin + TRS 키프레임 직렬화 |
| 렌더러 고속 경로 확장 | v2 로딩: Skeleton/Skin + AnimationController 등록 + 자동 재생 |
| 하위 호환 | v1 파일: Skeleton/Animation 섹션 없음 → 비애니메이션으로 정상 로딩 |

---

## Phase 03: 고급 렌더링 기법 (Phase 37~48)

Phase 36 완료 후 진행. G-Buffer / Post-Processing / Ray Tracing / Neural Rendering 단계.

---

### Phase 37 — Deferred Rendering

| 작업 | 설명 |
|------|------|
| G-Buffer MRT 생성 | RT0(Albedo+Metallic) / RT1(Normal+Roughness) / RT2(Emissive+AO) / Depth SRV |
| Geometry Pass PSO | Opaque G-Buffer Fill PSO, Alpha Mask clip() |
| Lighting Pass | Full-Screen Quad, G-Buffer SRV → HDR RT, Cook-Torrance BRDF |
| Forward+ Alpha | Alpha Blend 오브젝트 기존 Forward 유지 |
| G-Buffer 디버그 뷰 | Render 메뉴: Albedo/Normal/MetalRoughness/Depth 시각화 |

---

### Phase 38 — HDR Pipeline + Tone Mapping

| 작업 | 설명 |
|------|------|
| HDR Render Target | R16G16B16A16_FLOAT, Lighting Pass 출력 |
| Tone Mapping Pass | Reinhard / ACES Filmic, Render 메뉴 선택 |
| Auto-Exposure | Compute Shader 평균 Luminance → EV 자동 조절 |
| sRGB 출력 | Tone Map → R8G8B8A8_UNORM_SRGB SwapChain |

---

### Phase 39 — SSAO

| 작업 | 설명 |
|------|------|
| SSAO Buffer | R8_UNORM 렌더 타겟 |
| SSAO Pass | Hemisphere Kernel(16~64), 노이즈 텍스처 |
| Bilateral Blur | Depth/Normal 경계 보존, 2패스 분리 |
| Lighting 통합 | AO × Ambient Light |
| 메뉴 토글 | ID_OPTIM_SSAO, AO Buffer 시각화 |

---

### Phase 40 — Bloom + Post-Processing 파이프라인

| 작업 | 설명 |
|------|------|
| PostProcessor 클래스 | Ping-Pong HDR RT, AddPass() 프레임워크 |
| Bright Pass | Luminance 임계값 필터 |
| Blur Pyramid | 6단계 다운샘플→업샘플, Dual Kawase Blur |
| Composite | Bloom Additive Blend |

---

### Phase 41 — TAA

| 작업 | 설명 |
|------|------|
| Halton Jitter | 8~16프레임 서브픽셀 오프셋 |
| Velocity Buffer | R16G16_FLOAT, 카메라/오브젝트 속도 |
| History Buffer | 이전 프레임 TAA 출력 SRV |
| TAA Resolve | Variance Clipping + 블렌딩 |
| 메뉴 | TAA / MSAA / None 전환 |

---

### Phase 42 — Motion Blur + Depth of Field

| 작업 | 설명 |
|------|------|
| Tile-based Max Velocity | Compute Shader, N×N 타일 최대 속도 |
| Motion Blur | 속도 방향 N샘플 평균, 셔터 속도 스케일 |
| CoC 계산 | Depth → CoC 반경 (F-Number, Focus Distance) |
| Bokeh Blur | Separable Gaussian / Hexagonal Bokeh |

---

### Phase 43 — SSR + Refraction

| 작업 | 설명 |
|------|------|
| Hi-Z Raymarching | G-Buffer Depth 계층, 반사 Ray 교차 |
| SSR Color 샘플링 | Fresnel + Roughness 블러 |
| Envmap Fallback | 화면 경계/미스 → Skybox Cubemap |
| Refraction | IOR 기반 UV 오프셋, Depth 관통 방지 |

---

### Phase 44 — Screen Space Subsurface Scattering

| 작업 | 설명 |
|------|------|
| Material 확장 | subsurfaceColor + scatterWidth 파라미터 |
| Stencil 마스크 | SSS 픽셀 분리 |
| Separable SSS | 6-weight Gaussian × RGB 채널, 2패스 분리 |

---

### Phase 45 — Global Illumination (DDGI)

| 작업 | 설명 |
|------|------|
| Probe Grid | 3D Grid (8×4×8=256), Octahedral Map 텍스처 |
| Probe Update | DXR Ray (Phase 46 연동) / Static Fallback |
| Probe Sampling | 삼선형 보간, SH2 Irradiance |
| Lighting 통합 | Indirect Diffuse += Probe × Albedo / π |

---

### Phase 46 — DXR Hybrid Ray Tracing

| 작업 | 설명 |
|------|------|
| DXR 인프라 | Feature 감지, DXR PSO, BLAS/TLAS, ShaderTable |
| RT Shadow | Shadow Ray per 광원, AnyHit 투과, PCF 대체 |
| RT Reflection | Cone Sampling, 재귀 1~2레벨 |
| 폴백 | DXR 미지원 시 PCF/SSR/DDGI Static 유지 |

---

### Phase 47 — Nanite-style Virtual Geometry

| 작업 | 설명 |
|------|------|
| Meshlet 분할 | ~128 삼각형, 바운딩 스피어 + 노말 Cone |
| Mesh Shader PSO | Amplification + Mesh Shader |
| Cluster LOD | Projected Error 기준 GPU-side LOD 전환 |
| GPU-Driven Indirect | Compute → DrawArgs → ExecuteIndirect() |
| 폴백 | Mesh Shader 미지원 시 기존 DrawIndexedInstanced |

---

### Phase 48 — Neural Upscaling + Neural Denoising

| 작업 | 설명 |
|------|------|
| FSR 3 통합 | FidelityFX SDK, Quality Mode 메뉴 |
| DLSS 3 통합 (선택) | Streamline SDK, RTX 감지, FSR 폴백 |
| Neural Denoising | NRD SDK 또는 자체 Temporal Accumulation Denoiser |
| 렌더 해상도 관리 | 출력 해상도 50~75%, TAA Jitter 연동 |

---

### Phase 49 — Phase 03 코드 리뷰, 최적화, 버그 수정 & 아키텍처 문서화

**코드 리뷰**

| 작업 | 설명 |
|------|------|
| Dead code 제거 + include 정리 | 네이밍 일관성(PascalCase/camelCase) 검증 |
| G-Buffer MRT 코드 리뷰 | 바인딩 순서, 포맷 일관성 확인 |
| DXR 코드 리뷰 | ShaderTable 빌드, BLAS/TLAS 갱신 주기 |
| Mesh Shader 코드 리뷰 | Meshlet 분할 경계 조건, DispatchMesh 파라미터 |
| Neural SDK 연동 코드 리뷰 | FSR/DLSS/NRD 초기화 순서, 리소스 lifetime |
| D3D12 Debug Layer 경고 0건 | 리소스 상태 전이 누락, lifetime 위반 수정 |

**성능 최적화**

| 작업 | 설명 |
|------|------|
| 패스별 GPU 타임스탬프 측정 | PIX for Windows 또는 D3D12 Timestamp Query |
| G-Buffer 포맷 최적화 | RT1: R10G10B10A2 축소 검토 |
| SSAO/TAA/Bloom 파라미터 튜닝 | 샘플 수, 블렌딩 계수, 피라미드 단계 |
| DXR TLAS Refit | 정적 BLAS 재사용, 동적만 Rebuild |

**최종 벤치마크**

| 씬 | 목표 |
|----|------|
| Sponza (Full Phase 03) | Deferred+SSAO+Bloom+TAA+SSR+DDGI 60fps |
| Bistro (Full Phase 03) | 동일 파이프라인 60fps |
| DXR 활성 | RT Shadow+RT Reflection 포함 성능 측정 |
| FSR 3 (67%) | 품질 vs 성능 비교 기록 |

---

## 권장 구현 순서

```
[버그] Bistro 씬 렌더링 버그 수정 (나중에)
    │           Frustum Culling 버그 재조사 (Frustum 시각화 도구 구현 후)
    │           Shadow Map 시각적 튜닝 + SceneSettings.md 기록
    │           doubleSided PSO 분기는 Phase 32에서 처리
    │
Phase 30    통합 & 벤치마크 (Sponza/Bistro 60fps 목표)
    │
Phase 31    RRScenePreprocessor (.rrscene 오프라인 전처리 도구)
    │           Assimp 파싱·이미지 디코딩·LOD·Mip chain 오프라인 처리
    │           렌더러 이중 로딩 경로 (고속/.rrscene + 표준/Assimp)
    │           → Sponza/Bistro 로딩 시간 ~90% 단축
    │
Phase 32    코드 리뷰 + doubleSided PSO 분기(RM-11) + CalcShadow X4000 재검토 + ARCHITECTURE.md
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

- **PBR.hlsl CalcShadow X4000 경고**: FXC 컴파일러 한계 — 비교 샘플러(`SamplerComparisonState`)와 동적 cbuffer 인덱스 조합 quirk. Phase 32에서 `Texture2DArray` 방식으로 리팩터링 검토.
- **Occlusion Culling Optimization 메뉴 항목**: Phase 33 구현 시 `ID_OPTIM_OCCLUSION_CULL = 8004` 추가.
