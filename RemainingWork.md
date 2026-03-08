# 잔여 구현 항목 정리

> 최종 업데이트: 2026-03-08 (Phase 25 완료 — Bistro 씬 분석 + glTF 에셋 준비)
> Phase 25까지 구현된 내용을 바탕으로, 이후 필요한 작업을 정리한다.

---

## 현재 구현 완료 범위 (Phase 25)

| 항목 | 상태 |
|------|------|
| FrustumCuller (AABB vs Frustum 교차) | ✅ 완료 |
| LightCuller (거리·기여도 기반 광원 컬링) | ✅ 완료 |
| LODSelector (거리 기반 + 비동기 자동 LOD 생성) | ✅ 완료 |
| OcclusionCuller | ⚠️ P0 스텁 (항상 false, 실제 컬링 없음) |
| CullStats + DebugHUD 이중 폴리곤 표시 | ✅ 완료 |
| Optimization 메뉴 (Frustum / Light / LOD 토글) | ✅ 완료 |
| Shadow Mapping (Directional/Spot, PCF) | ✅ 완료 |
| PBR 셰이더 + 텍스처 로딩 | ✅ 완료 |
| 프리미티브 → SceneNode 분리 + Per-Mesh AABB | ✅ 완료 |
| Shadow Map 해상도 자동 선택 (씬 크기 기반) | ✅ 완료 |
| Shadow Ortho/Perspective 범위 자동 계산 | ✅ 완료 |
| shadowTexelSize GPU 동적 전달 | ✅ 완료 |
| PBR.hlsl HLSL X4000 경고 최소화 시도 | ⚠️ 부분 완료 (CalcShadow 경고 1건 잔존) |
| Orbit Light → Directional + castShadow=true | ✅ 완료 |
| Bistro 씬 분석 + SceneSettings.md 문서화 | ✅ 완료 |
| assets/test-models/Bistro/ 클론 (niagara_bistro, 3.6 GB) | ✅ 완료 |

---

## 잔여 구현 항목

### Phase 31 — RRScenePreprocessor (오프라인 전처리 도구 + 백그라운드 자동 생성)

glTF/GLB/FBX 씬을 처리하여 엔진 전용 바이너리(`.rrscene`)로 저장하는 파이프라인.
CLI 도구와 렌더링 앱 내 백그라운드 자동 생성의 두 진입점을 제공한다.

**구현 항목**

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

**절감 효과 (Sponza급 씬 기준)**: 첫 로딩은 표준 경로 + 백그라운드 전처리, 두 번째부터 ~90% 단축 (10~40초 → 1~3초)

---

### Phase 25 — Bistro 씬 분석 + glTF 에셋 준비 ✅ 완료

`niagara_bistro` (github.com/zeux/niagara_bistro) glTF 변환본을 에셋으로 준비하고,
씬 스케일·카메라·광원·Shadow Map 추천 세팅을 분석하여 SceneSettings.md에 문서화한다.
(구현 없음, 설계·에셋 준비 단계)

| 작업 | 상태 | 설명 |
|------|------|------|
| niagara_bistro 실측 분석 | ✅ 완료 | Exterior+Interior 통합 파일, 삼각형 1,753,630개, diagonal ≈ 166m |
| SceneSettings.md Bistro 섹션 | ✅ 완료 | bistro vs bistrox 비교, 씬 스케일, 카메라, Shadow Map 자동 설정, VRAM 분석 |
| assets/test-models/Bistro/ 클론 | ✅ 완료 | `git clone --depth=1` 완료, 3.6 GB, .gitignore 제외 |

**완료 기준**: SceneSettings.md Bistro 섹션 작성 완료, 빌드 불필요 — **달성**

---

### Phase 26 — Bistro! 빠른 로드 메뉴 + 씬 전용 설정 + Interior 조명 토글 + Shadow Map 시각적 튜닝

Phase 24 Sponza! 구현과 동일한 패턴으로 Bistro 씬 전용 메뉴·카메라·광원 자동 설정을 구현하고,
L 키로 Exterior/Interior 조명을 전환하며, 실제 렌더링을 통해 Shadow Map 파라미터를 결정한다.

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| `niagara_bistro` clone | ✅ 완료 | `assets/test-models/Bistro/` 배치 완료 (Phase 25에서 수행) |
| Win32Menu: "Bistro!" 메뉴 항목 | 없음 | `ID_FILE_BISTRO = 6003`, "File > Sponza!" 아래 추가 |
| `Engine::LoadBistroScene()` | 없음 | 파일 다이얼로그 + 카메라·광원 세팅 + Orbit 비활성 |
| `Engine::ApplyBistroLighting()` | 없음 | `m_bistroInteriorMode`에 따라 Exterior(2개) / Interior(6개) 배치 |
| L 키 Exterior ↔ Interior 토글 | 없음 | `m_isBistroScene && L 키 에지` → `m_bistroInteriorMode` 전환 |
| Shadow Map 자동 설정 확인 | 없음 | diagonal≈166m → 4096×4096 (RTX) / 2048×2048 (UHD 630) 자동 선택 |
| Shadow Map 시각적 튜닝 | 없음 | 외부 바닥, 기둥, 계단, 원거리 경계 등 5개 영역 순서별 확인 |
| SceneSettings.md 기록 | 없음 | 최종 DepthBias, SlopeScaledDepthBias, 해상도 값 기록 |

**L 키 조명 토글 모드**

| 모드 | 광원 구성 | 총 광원 수 |
|------|-----------|-----------|
| **Exterior** (기본) | Directional Evening Sun (intensity=8) + Point Fill | 2개 |
| **Interior** (L 키) | Directional Evening Sun (intensity=3, 감쇠) + 천장 펜던트×3 + 바 카운터×1 + 앰비언트 필×1 | 6개 |

**Shadow Map 시각적 튜닝 판단 기준**

| 증상 | 원인 | 조치 |
|------|------|------|
| 바닥·벽에 계단형 줄무늬 (Shadow Acne) | DepthBias 부족 | `DepthBias` 증가 |
| 오브젝트와 그림자 사이 공백 (Peter Panning) | DepthBias 과다 | `DepthBias` 감소 |
| 경사면에만 Acne 잔존 | SlopeScaledDepthBias 부족 | `SlopeScaledDepthBias` 증가 |
| 원거리 그림자 경계가 픽셀 블록으로 보임 | 해상도 부족 | 해상도 증가 또는 PCF 커널 확대 |

**확인 영역 우선순위**: 외부 바닥 → 기둥 접촉면 → 계단/경사 지붕 → 원거리 가로등 경계 → 실내외 개구부

**완료 기준**: "File > Bistro!" 메뉴로 bistro.gltf 로드, 카메라·광원 자동 적용,
L 키 Exterior↔Interior 조명 토글 (Bistro 전용), Shadow Map 최적값 SceneSettings.md에 기록, 빌드 성공

---

### Phase 27 — Texture Streaming + Mip-Mapping

현재 모든 텍스처가 최고 해상도(단일 Mip)로 메인 스레드에서 한 번에 로드된다.

**구현 항목**

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| `TextureStreamer.h/.cpp` | 미존재 | 가시성·거리 기반 우선순위 큐, Mip 레벨 동적 로딩/해제 |
| Mip chain 생성 | `MipLevels = 1` | `floor(log2(max(w,h))) + 1` 전체 Mip 생성 |
| Anisotropic Sampler | Linear Wrap 사용 중 | `D3D12_FILTER_ANISOTROPIC`, MaxAnisotropy = 16 |
| 스트리밍 우선순위 | 없음 | `priority = isVisible ? (1/distance) : 0` |
| VRAM 예산 연동 | 없음 | VRAM 초과 시 LRU + 거리 기반 Mip 해제 |

---

### Phase 28 — Instanced Rendering + 멀티스레드 로딩

**Instanced Rendering**

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| `InstanceBatcher.h/.cpp` | 미존재 | 동일 Mesh+Material 그룹핑, Instance Buffer 생성 |
| `DrawIndexedInstanced` | 미사용 | 현재 동일 Mesh라도 노드마다 개별 Draw 호출 |
| per-instance Input Layout | 없음 | slot 1: `INSTANCE_WORLD` (4×float4, per-instance) |
| HLSL `SV_InstanceID` | 없음 | VS에서 Instance Buffer World Matrix 인덱싱 |

**멀티스레드 로딩**

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| `ThreadPool.h/.cpp` | 미존재 | CPU 코어 수 기반 워커 스레드 |
| 텍스처 디코딩 병렬화 | 순차 실행 | 씬 로드 중 메인 스레드 프리즈 원인 |
| Copy Queue | 없음 | Graphics Queue와 병렬 GPU 업로드 (Phase 28) |

---

### Phase 29 — GPU 메모리 최적화

**CB 풀링**

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| `D3D12CBPool.h/.cpp` | 미존재 | Upload Heap 풀, 256바이트 정렬, 링 버퍼 (프레임 0/1 영역 분리) |
| Per-Object / Per-Material CB 분리 | 없음 | `register b0` (오브젝트), `register b2` (Material) |

**갱신 최적화**

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| Transform Dirty Flag | 없음 | 변경 없으면 CB 갱신 스킵 |
| Material Dirty Flag | 없음 | Material 파라미터 미변경 시 갱신 스킵 |
| Light CB Dirty Flag | 없음 | 광원 데이터 미변경 시 갱신 스킵 |
| VRAM 모니터링 | 없음 | `IDXGIAdapter3::QueryVideoMemoryInfo`, DebugHUD 표시 |
| 적응적 CB 갱신 | 없음 | VRAM > Budget 80% 시 저우선순위 오브젝트 갱신 빈도 감소 |

**정렬 최적화**

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| Opaque Front-to-Back 정렬 | 없음 | Early-Z rejection 극대화 |
| Material 기반 드로우콜 정렬 | 없음 | PSO/텍스처 상태 전환 최소화 |

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

### Phase 32 — 코드 리뷰 & 문서화

| 작업 | 설명 |
|------|------|
| 전체 코드 리뷰 | Dead code 제거, include 정리, 네이밍 일관성 검증 |
| PBR.hlsl CalcShadow X4000 경고 | FXC 컴파일러 한계 (비교 샘플러 + 동적 cbuffer 인덱스). Phase 24에서 최소화 시도 후 1건 잔존. Texture2DArray로의 리팩터링 또는 FXC 업데이트로 재검토 |
| GPU 리소스 해제 누락 검사 | Fence 대기 후 해제 보장, ComPtr 사용 일관성 |
| PIX / 타임스탬프 쿼리 프로파일링 | 병목 구간 식별 및 최적화 |
| `ARCHITECTURE.md` 작성 | 전체 엔진 구조, 모듈 간 의존성, 렌더 파이프라인 다이어그램 |

---

### Phase 33 — Occlusion Culling (Hi-Z GPU)

> **Phase 33 시작 전**: Phase 02 Backup 생성 (1회)
> - `Phase 02 Backup/` 폴더에 src/, tests/, assets/, shaders/ 복사
> - `Phase 01 Backup/` 및 빌드 산출물(bin/, .git/, *.user 등) 제외
> - `Phase 02 Backup/README.md`에 백업 일시 + Phase 02 완료 상태 기록
> - **백업 완료 후 `Phase 02 Backup/` 폴더 안의 파일은 절대 수정하지 않는다. 이후 어떠한 Phase 구현에서도 이 폴더를 참조만 하고 절대 건드리지 않는다.**

현재 `OcclusionCuller::IsOccluded()`는 항상 `false`를 반환하는 스텁이다.
CPU Readback 간이 방식을 거치지 않고 GPU Hi-Z 방식으로 바로 구현한다.
Compute Shader 파이프라인 인프라 구축이 선행 조건이다.

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
| DebugHUD | Cube Shadow Pass 수 표시 |

---

### Phase 35 — Skeletal Animation

glTF Node Transform 애니메이션(키프레임)과 Skeletal Animation(본/스킨) 구현.

**Part A: Node Transform Animation (G-08)**

| 작업 | 설명 |
|------|------|
| `Animation.h` | `AnimationClip`, `AnimationChannel`, `Keyframe<T>` 구조체 |
| 키프레임 보간 | LINEAR (Lerp/Slerp), STEP, CUBICSPLINE |
| SceneLoader 확장 | `aiAnimation` → `AnimationClip` 변환, target name → SceneNode 매핑 |
| `AnimationController` | `Update(dt)`, Play/Pause/Loop, 클립 목록 메뉴 |

**Part B: Skeletal Animation (G-09)**

| 작업 | 설명 |
|------|------|
| `Skeleton.h` | `Bone`, `Skeleton`, `Skin` 구조체, inverse bind matrix |
| SceneLoader 확장 | `aiMesh::mBones` → `Skeleton/Skin` 생성, per-vertex joint/weight 추출 |
| Vertex 포맷 확장 | `JOINTS_0` (XMUINT4) + `WEIGHTS_0` (XMFLOAT4) 추가, Input Layout/HLSL 갱신 |
| GPU Skinning | `SkinCB : register(b4)` (joint matrix palette 128개), PBR.hlsl VS 스키닝 계산 |
| Joint palette 업로드 | AnimationController::Update() 후 bone world matrix → GPU 복사 |

---

### Phase 36 — RRScenePreprocessor 확장 (Skeletal Animation 지원)

Phase 33 완료 후 `.rrscene` 포맷을 v2로 버전 업하여 Skeleton/Skin/Animation 데이터를 통합.
애니메이션 씬도 고속 로딩 경로를 사용할 수 있도록 전처리기와 렌더러 양쪽을 확장한다.

| 작업 | 설명 |
|------|------|
| `.rrscene` v2 포맷 확장 | `RRSceneFormat.h`: version=2, Skeleton Section + Animation Section 추가, 스킨 Vertex에 joints/weights 필드 |
| 전처리기: Skeleton 직렬화 | `aiMesh::mBones` → Bone/Skin 직렬화, per-vertex joint/weight 기록 |
| 전처리기: Animation 직렬화 | `aiAnimation` → AnimationClip TRS 키프레임 + Interpolation 직렬화 |
| 렌더러 고속 경로 확장 | v2 로딩: Skeleton/Skin → 객체 생성, Animation → AnimationController 등록 + 자동 재생 |
| 하위 호환 | v1 파일(`version == 1`): Skeleton/Animation 섹션 없음 → 비애니메이션으로 정상 로딩 |
| 버전 감지 | Header.version으로 분기, 구버전 파일도 경고 없이 처리 |

---

## Phase 03: 고급 렌더링 기법 (Phase 37~48)

Phase 36 완료 후 진행. G-Buffer / Post-Processing / Ray Tracing / Neural Rendering 단계.

---

### Phase 37 — Deferred Rendering

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| G-Buffer MRT 생성 | 없음 | RT0(Albedo+Metallic) / RT1(Normal+Roughness) / RT2(Emissive+AO) / Depth SRV |
| Geometry Pass PSO | 없음 | Opaque G-Buffer Fill PSO, Alpha Mask clip() |
| Lighting Pass | 없음 | Full-Screen Quad, G-Buffer SRV → HDR RT, Cook-Torrance BRDF |
| Forward+ Alpha | 없음 | Alpha Blend 오브젝트 기존 Forward 유지 |
| G-Buffer 디버그 뷰 | 없음 | Render 메뉴: Albedo/Normal/MetalRoughness/Depth 시각화 |

---

### Phase 38 — HDR Pipeline + Tone Mapping

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| HDR Render Target | 없음 | R16G16B16A16_FLOAT, Lighting Pass 출력 |
| Tone Mapping Pass | 없음 | Reinhard / ACES Filmic, Render 메뉴 선택 |
| Auto-Exposure | 없음 | Compute Shader 평균 Luminance → EV 자동 조절 |
| sRGB 출력 | 없음 | Tone Map → R8G8B8A8_UNORM_SRGB SwapChain |

---

### Phase 39 — SSAO

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| SSAO Buffer | 없음 | R8_UNORM 렌더 타겟 |
| SSAO Pass | 없음 | Hemisphere Kernel(16~64), 노이즈 텍스처 |
| Bilateral Blur | 없음 | Depth/Normal 경계 보존, 2패스 분리 |
| Lighting 통합 | 없음 | AO × Ambient Light |
| 메뉴 토글 | 없음 | ID_OPTIM_SSAO, AO Buffer 시각화 |

---

### Phase 40 — Bloom + Post-Processing 파이프라인

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| PostProcessor 클래스 | 없음 | Ping-Pong HDR RT, AddPass() 프레임워크 |
| Bright Pass | 없음 | Luminance 임계값 필터 |
| Blur Pyramid | 없음 | 6단계 다운샘플→업샘플, Dual Kawase Blur |
| Composite | 없음 | Bloom Additive Blend |

---

### Phase 41 — TAA

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| Halton Jitter | 없음 | 8~16프레임 서브픽셀 오프셋 |
| Velocity Buffer | 없음 | R16G16_FLOAT, 카메라/오브젝트 속도 |
| History Buffer | 없음 | 이전 프레임 TAA 출력 SRV |
| TAA Resolve | 없음 | Variance Clipping + 블렌딩 |
| 메뉴 | 없음 | TAA / MSAA / None 전환 |

---

### Phase 42 — Motion Blur + Depth of Field

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| Tile-based Max Velocity | 없음 | Compute Shader, N×N 타일 최대 속도 |
| Motion Blur | 없음 | 속도 방향 N샘플 평균, 셔터 속도 스케일 |
| CoC 계산 | 없음 | Depth → CoC 반경 (F-Number, Focus Distance) |
| Bokeh Blur | 없음 | Separable Gaussian / Hexagonal Bokeh |

---

### Phase 43 — SSR + Refraction

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| Hi-Z Raymarching | 없음 | G-Buffer Depth 계층, 반사 Ray 교차 |
| SSR Color 샘플링 | 없음 | Fresnel + Roughness 블러 |
| Envmap Fallback | 없음 | 화면 경계/미스 → Skybox Cubemap |
| Refraction | 없음 | IOR 기반 UV 오프셋, Depth 관통 방지 |

---

### Phase 44 — Screen Space Subsurface Scattering

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| Material 확장 | 없음 | subsurfaceColor + scatterWidth 파라미터 |
| Stencil 마스크 | 없음 | SSS 픽셀 분리 |
| Separable SSS | 없음 | 6-weight Gaussian × RGB 채널, 2패스 분리 |

---

### Phase 45 — Global Illumination (DDGI)

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| Probe Grid | 없음 | 3D Grid (8×4×8=256), Octahedral Map 텍스처 |
| Probe Update | 없음 | DXR Ray (Phase 46 연동) / Static Fallback |
| Probe Sampling | 없음 | 삼선형 보간, SH2 Irradiance |
| Lighting 통합 | 없음 | Indirect Diffuse += Probe × Albedo / π |

---

### Phase 46 — DXR Hybrid Ray Tracing

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| DXR 인프라 | 없음 | Feature 감지, DXR PSO, BLAS/TLAS, ShaderTable |
| RT Shadow | 없음 | Shadow Ray per 광원, AnyHit 투과, PCF 대체 |
| RT Reflection | 없음 | Cone Sampling, 재귀 1~2레벨 |
| 폴백 | 없음 | DXR 미지원 시 PCF/SSR/DDGI Static 유지 |

---

### Phase 47 — Nanite-style Virtual Geometry

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| Meshlet 분할 | 없음 | ~128 삼각형, 바운딩 스피어 + 노말 Cone |
| Mesh Shader PSO | 없음 | Amplification + Mesh Shader |
| Cluster LOD | 없음 | Projected Error 기준 GPU-side LOD 전환 |
| GPU-Driven Indirect | 없음 | Compute → DrawArgs → ExecuteIndirect() |
| 폴백 | 없음 | Mesh Shader 미지원 시 기존 DrawIndexedInstanced |

---

### Phase 48 — Neural Upscaling + Neural Denoising

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| FSR 3 통합 | 없음 | FidelityFX SDK, Quality Mode 메뉴 |
| DLSS 3 통합 (선택) | 없음 | Streamline SDK, RTX 감지, FSR 폴백 |
| Neural Denoising | 없음 | NRD SDK 또는 자체 Temporal Accumulation Denoiser |
| 렌더 해상도 관리 | 없음 | 출력 해상도 50~75%, TAA Jitter 연동 |

---

### Phase 49 — Phase 03 코드 리뷰, 최적화, 버그 수정 & 아키텍처 문서화

Phase 33~48 완료 후 전체 Phase 03 코드 품질 점검 및 최종 문서화.

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
| Denoiser 수렴 속도 조정 | Hysteresis 파라미터, 고스팅 트레이드오프 |

**버그 수정**

| 작업 | 설명 |
|------|------|
| 리소스 상태 전이 누락 | D3D12_RESOURCE_STATE_* 바리어 보완 |
| TAA History 초기화 | 씬 전환 직후 History Buffer 리셋 |
| SSR 경계 아티팩트 | 화면 경계 Fade 파라미터 튜닝 |
| DDGI Probe 튀는 현상 | Irradiance Hysteresis 파라미터 조정 |
| FSR/DLSS Motion Vector 스케일 불일치 | 렌더/출력 해상도 비율 보정 |

**아키텍처 문서화**

| 작업 | 설명 |
|------|------|
| `ARCHITECTURE.md` 완성 | 전체 렌더 파이프라인 다이어그램 (Phase 01~48) |
| 렌더 패스 의존성 다이어그램 | Shadow→G-Buffer→Lighting→Post→TAA→Upscale |
| 모듈 의존성 | Engine/Renderer/SceneGraph/RHI/Asset/Lighting 관계도 |
| G-Buffer 레이아웃 + 레지스터 맵 | Descriptor Heap 구조, Root Signature |
| DXR 가속 구조 | BLAS/TLAS 업데이트 주기, ShaderTable 구성 |
| GPU-Driven 렌더링 흐름 | Compute→DrawArgs→ExecuteIndirect |
| 스레딩 모델 | 메인/Compute Queue/Copy Queue/Worker Thread 관계 |

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
Phase 24   완료 ✅  (HLSL 경고 + Shadow Map 자동 크기 조정)
    │
Phase 25   완료 ✅  Bistro 씬 분석 + glTF 에셋 준비
    │           niagara_bistro 실측값 문서화 (diagonal=166m, 삼각형 1.75M)
    │           assets/test-models/Bistro/ 클론 완료 (3.6 GB, .gitignore 제외)
    │
Phase 26    Bistro! 빠른 로드 메뉴 + Interior 조명 토글 + Shadow Map 시각적 튜닝
    │           File 메뉴 "Bistro!" + LoadBistroScene() + ApplyBistroLighting()
    │           L 키: Exterior(2개) ↔ Interior(6개) 조명 토글 (Bistro 전용)
    │           실제 렌더링 후 DepthBias/SlopeScaledDepthBias/해상도 최적값 결정
    │
Phase 27    Texture Streaming + Mip-Mapping
    │           + Anisotropic Sampler 교체
    │
Phase 28    Instanced Rendering + 멀티스레드 로딩
    │
Phase 29    GPU 메모리 최적화
    │           (CBPool + Dirty Flag + Front-to-Back + VRAM 모니터링)
    │
Phase 30    통합 & 벤치마크 (Sponza/Bistro 60fps 목표)
    │
Phase 31    RRScenePreprocessor (.rrscene 오프라인 전처리 도구)
    │           Assimp 파싱·이미지 디코딩·LOD·Mip chain 오프라인 처리
    │           렌더러 이중 로딩 경로 (고속/.rrscene + 표준/Assimp)
    │           → Sponza/Bistro 로딩 시간 ~90% 단축
    │
Phase 32    코드 리뷰 + CalcShadow X4000 재검토 + ARCHITECTURE.md
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
    │           하위 호환 (v1 파일도 계속 로딩 가능)
    │
    │   ── Phase 03 시작 ──
    │
Phase 37    Deferred Rendering
    │           G-Buffer MRT (Albedo/Normal/MetalRough/Depth)
    │           Geometry Pass → Lighting Pass → Forward+ Alpha
    │
Phase 38    HDR Pipeline + Tone Mapping (ACES/Reinhard + Auto-Exposure)
    │
Phase 39    SSAO (Hemisphere Kernel + Bilateral Blur)
    │
Phase 40    Bloom + Post-Processing 파이프라인 (Ping-Pong Buffer)
    │
Phase 41    TAA (Halton Jitter + Variance Clipping + Velocity Buffer)
    │
Phase 42    Motion Blur (Tile-based) + Depth of Field (Bokeh CoC)
    │
Phase 43    SSR (Hi-Z Raymarching) + Refraction (IOR)
    │
Phase 44    Screen Space Subsurface Scattering (Separable 6-weight Gaussian)
    │
Phase 45    Global Illumination — DDGI (Irradiance Probe 3D Grid)
    │
Phase 46    DXR Hybrid Ray Tracing (BLAS/TLAS + RT Shadow + RT Reflection)
    │
Phase 47    Nanite-style Virtual Geometry (Meshlet + Mesh Shader + GPU-Driven)
    │
Phase 48    Neural Upscaling (FSR 3 / DLSS 3) + Neural Denoising (NRD SDK)
    │
Phase 49    Phase 03 코드 리뷰 + 최적화 + 버그 수정 + ARCHITECTURE.md 완성
            Sponza/Bistro Full Phase 03 벤치마크 (60fps 목표)
```

---

## 기타 메모

- **PBR.hlsl CalcShadow X4000 경고**: FXC 컴파일러 한계 — 비교 샘플러(`SamplerComparisonState`)와 동적 cbuffer 인덱스 조합에서 발생하는 고유 quirk. `SampleShadowMap`을 `if/else-if` 체인 + 명시적 초기화로 변경하여 SampleShadowMap 경고는 제거했지만 CalcShadow 경고 1건 잔존. Phase 32에서 `Texture2DArray` 방식으로 리팩터링 검토.
- **Occlusion Culling Optimization 메뉴 항목**: Phase 33 구현 시 `ID_OPTIM_OCCLUSION_CULL = 8004` 추가 (Phase 23.5 별도 구현 없이 Phase 33에서 통합)
- **Shadow Map SRV 누수**: `RecreateShadowMaps()` 호출 시 persistent descriptor heap에 이전 SRV 8개가 남음 (최대 1024개 중). 개발 엔진 용량 내 허용 범위이나 Phase 32에서 정리 권장.
