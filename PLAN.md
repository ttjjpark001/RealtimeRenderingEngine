# PLAN: 실시간 렌더링 엔진 구현 계획

> Phase 01, 02는 구현 완료. 아카이브: `PLAN_Phase01-02_Archive.md`

---

## Phase 03: 고급 렌더링 기법

> Phase 02 완료 코드 위에 GPU-Driven 컬링, 고급 섀도잉, 스켈레탈 애니메이션,
> 지연 렌더링(Deferred Shading), 포스트 프로세싱, 레이 트레이싱, 신경망 업스케일링 등
> 최신 실시간 렌더링 기법을 단계적으로 추가한다.

**포함 Phase**: Phase 32 ~ Phase 48

---

### Phase 32: Occlusion Culling — Hi-Z GPU ✅
**목표**: 현재 P0 스텁(항상 false)인 `OcclusionCuller`를 GPU Hi-Z 방식으로 완전 구현.
CPU Readback 간이 방식을 거치지 않고 바로 Hi-Z로 구현한다.
현재 엔진에 Compute Shader 인프라가 없으므로, 먼저 인프라를 구축한 뒤 Hi-Z를 구현한다.

1. **Compute Shader 인프라 구축**:
   - `D3D12ComputePipeline.h/.cpp`: CS 전용 Root Signature + ID3D12PipelineState(CS)
   - `D3D12Context::Dispatch(x, y, z)` 지원 추가
   - UAV descriptor 관리 (CBV_SRV_UAV heap 확장)

2. **Hi-Z (Hierarchical-Z) Buffer 생성**:
   - 이전 프레임 Depth Buffer(`DXGI_FORMAT_D32_FLOAT`)를 `DXGI_FORMAT_R32_FLOAT` SRV로 복사
   - Compute Shader로 반씩 축소하는 Mip chain 생성 (UAV write, SRV read 교차)
   - 최대 `floor(log2(max(w,h)))` 단계 Mip 생성

3. **GPU-side AABB depth 비교**:
   - Compute Shader: SceneNode AABB 8개 코너 → ViewProj → NDC → screen-space min/max
   - 최적 Mip 레벨 계산: `floor(log2(maxExtent_pixels))`
   - Hi-Z Mip 샘플링 후 AABB 근거리 Z와 비교 → occluded 여부 판정
   - 판정 결과를 GPU Buffer → CPU readback (1프레임 레이턴시)

4. **OcclusionCuller P0 스텁 교체**:
   - `OcclusionCuller::IsOccluded()`를 Hi-Z GPU 결과 버퍼 반환으로 교체
   - `occlusionCulledNodes` 통계 CullStats 반영, DebugHUD 표시
   - Optimization 메뉴 항목 추가: `ID_OPTIM_OCCLUSION_CULL = 8005` (`8004`는 MipMap 토글에서 사용 중)

**완료 기준**: Sponza에서 Occlusion Culling 활성화 시 드로우콜 절감 수치 DebugHUD 확인, GPU stall 없이 1프레임 레이턴시로 동작

---

### Phase 33: Shadow Quality — Cube Map Shadow + CSM + PCSS + Spot Light Shadow
**목표**: (1) `castShadow = true`인 Point Light에 대해 6면 Cube Map 기반 Omnidirectional Shadow Map 구현,
(2) Directional Light에 Cascaded Shadow Maps(CSM) 적용으로 근거리-원거리 그림자 품질 개선,
(3) PCSS(Percentage Closer Soft Shadows)로 접촉 경화 그림자 구현,
(4) Spot Light Perspective 투영 기반 Shadow Map 구현.

**Part B 완료**: CSM (Cascaded Shadow Maps) ✅
- Practical Split Scheme(λ=0.5) 3-cascade 분할
- cascade별 frustum AABB → OrthographicOffCenter 투영 행렬 계산
- 3-pass Shadow Depth (슬롯 0/1/2)
- HLSL GetCascadeIndex / CalcShadowCSM / 디버그 컬러 뷰 (red/green/blue)
- Optimization 메뉴 CSM on/off(8006) + CSM Debug View(8007) 토글
- 유닛 테스트 12개 (SplitDepths 5, CascadeIndex 5, ShadowConstants 2) — Debug/Release 모두 통과

**Part C 완료**: PCSS (Percentage Closer Soft Shadows) ✅
- Poisson Disk 16샘플 Blocker Search → Penumbra Width → 가변 커널 PCF
- CSM cascade와 통합: cascade 선택 후 PCSS 적용
- lightSize = sceneDiagonal × 0.02, filterRadius clamp [1.5~9.0 texel]
- ShadowConstants 확장: pcssEnabled, lightSize 추가 (560→576 bytes)
- Optimization 메뉴 PCSS on/off(8008) 토글, DebugHUD Shadow Mode 표시 (PCF/PCSS)
- 유닛 테스트 12개 (PenumbraWidth 5, FilterRadius 4, ShadowConstants 3) — Debug/Release 모두 통과

#### Part A: Point Light Cube Map Shadow ✅
> **Part C 추가 완료 항목**:
> - PCSS Blocker Search perspective-aware 공식 구현 ✅
>   `searchWidth = lightSize × (receiverDepth - nearNorm) / receiverDepth × shadowTexelSize`
>   `shadowNearNorm = camera.nearPlane / shadowFarZ` — ShadowCB로 전달
> - PCSS 기본값 ON ✅

1. **TextureCube D3D12 리소스 생성**:
   - `TEXTURE2D_ARRAY` (ArraySize=6, `DXGI_FORMAT_D32_FLOAT`) 리소스 생성
   - 각 면에 대해 DSV 6개 (depth write) + SRV 1개 (TextureCube sampling) 생성
   - 최대 `MAX_POINT_SHADOW_LIGHTS = 4`개 Point Light shadow 지원

2. **6-pass Shadow Depth 렌더링**:
   - 광원 1개당 ±X/±Y/±Z 방향으로 6회 Shadow Depth Pass
   - View 행렬: 각 면 방향의 `XMMatrixLookAtLH`, Projection: `XMMatrixPerspectiveFovLH(π/2, 1.0f, 0.01f, farPlane)`
   - 기존 `BeginShadowPass / DrawShadowDepth / EndShadowPass` 패턴 재사용

3. **HLSL 확장 (PBR.hlsl)**:
   - `TextureCube PointShadowMap[MAX_POINT_SHADOW_LIGHTS]` 바인딩 (register 확장)
   - Point light shadow factor: `lightToPixel` 방향 벡터로 cube map lookup → depth 비교 (+ bias)
   - `SamplerComparisonState` 또는 수동 depth 비교 (`ShadowCubeMap.Sample` + manual compare)

4. **LightConstants 확장**:
   - `shadowMapIndex`: Point light는 Cube map 슬롯 인덱스로 재사용
   - Directional/Spot(Texture2D) vs Point(TextureCube) 타입 구분 플래그 추가

5. **성능 관리**:
   - 최대 4개 Point light shadow 허용 (6pass × 4 = 24 depth pass/frame)
   - LightCuller와 연동: shadow casting Point light도 거리 기반 culling 적용
   - DebugHUD에 Cube Shadow Pass 수 표시

#### Part B: CSM (Cascaded Shadow Maps)

6. **Cascade Frustum 분할**:
   - 카메라 Frustum을 N=3 구간으로 분할 (near→c0→c1→far)
   - Practical Split Scheme: `split_i = λ·log_split + (1-λ)·uniform_split` (λ=0.5 기본값)
   - 각 cascade에 독립 Orthographic Shadow Map(DXGI_FORMAT_D32_FLOAT) 할당
   - `m_shadowMaps[0~2]`: cascade 0~2, `m_shadowMaps[3~]`: Spot/Point 용도 유지

7. **Cascade별 Shadow Depth Pass**:
   - Directional Light 1개당 cascade 수만큼 Shadow Depth Pass 실행 (3 pass)
   - 각 cascade Ortho 범위: cascade frustum AABB를 광원 뷰 공간에서 계산
   - `ShadowConstants.lightViewProj[0~2]`에 cascade 행렬 저장
   - `ShadowCB`에 cascade split depth (view-space Z) 배열 추가

8. **HLSL 확장 — Cascade 선택 및 블렌딩**:
   - PBR.hlsl: 픽셀의 view-space depth로 cascade 인덱스 결정
   - 인접 cascade 경계 blend band: PCF 비율 보간으로 경계선 제거
   - 디버그 뷰: cascade 색상 시각화 (cascade 0=적, 1=녹, 2=청)

#### Part C: PCSS (Percentage Closer Soft Shadows)

9. **Blocker Search**:
   - 픽셀 주변 Shadow Map을 searchWidth 반경으로 샘플링 → 차폐 텍셀 평균 depth(d_blocker) 계산
   - `searchWidth = lightSize × (receiver_depth - nearPlane) / receiver_depth`

10. **Penumbra Width 계산**:
    - `penumbraWidth = (receiver_depth - d_blocker) / d_blocker × lightSize`
    - `lightSize`: Directional Light 가상 광원 크기 파라미터 (기본값: sceneDiagonal × 0.02)

11. **가변 커널 PCF**:
    - penumbraWidth에 비례하는 반경으로 PCF 수행 (최소 3×3, 최대 9×9)
    - Poisson Disk 샘플 패턴 사용 (16~32 샘플, 블루노이즈 회전으로 밴딩 제거)
    - CSM 각 cascade에 동일 PCSS 적용

12. **성능 제어**:
    - Optimization 메뉴에 PCSS on/off 토글 추가 (off 시 기존 PCF 3×3 폴백)
    - DebugHUD에 Shadow Mode 표시 (PCF / PCSS)

**완료 기준**: Point Light Cube Map 구면 그림자 동작(Sponza 횃불 확인), CSM 3 cascade 전환 디버그 색상 시각화 확인(Bistro 원거리 그림자 품질 개선), PCSS on/off 시 접촉 경화 그림자 비교 가능, Spot Light 그림자 정상 렌더링

#### Part D: Spot Light Shadow

13. **Perspective 투영 Shadow Map**:
    - `castShadow = true`인 Spot Light에 대해 기존 Texture2D Shadow Map 슬롯 재사용
    - Projection: `XMMatrixPerspectiveFovLH(outerConeAngle * 2, 1.0f, nearPlane, farPlane)`
    - View 행렬: `XMMatrixLookAtLH(lightPos, lightPos + lightDir, upVector)`
    - 기존 `BeginShadowPass / DrawShadowDepth / EndShadowPass` 패턴 재사용

14. **HLSL 확장 (PBR.hlsl)**:
    - Spot Light의 `shadowType == 0` (Texture2D) 경로에서 기존 PCF/PCSS 그대로 적용
    - `lightViewProj` 슬롯에 Spot Light 투영 행렬 저장

15. **LightManager 연동**:
    - Spot Light `castShadow=true` 시 Shadow Map 슬롯 자동 할당 (Directional/Point와 동일 방식)

---

### Phase 34: Skeletal Animation
**목표**: glTF Node Transform 애니메이션(키프레임)과 Skeletal Animation(본/스킨) 구현.
Part A가 Part B의 전제 조건이므로 순서대로 구현한다.

#### Part A: Node Transform Animation (G-08)

1. **Animation 데이터 구조**:
   - `src/Asset/Animation.h`: `AnimationChannel` (target node, property: TRS, keyframes), `AnimationClip` (name, duration, channels 배열)
   - 키프레임 보간 지원: LINEAR, STEP, CUBICSPLINE (glTF `sampler.interpolation`)
   - Assimp `aiAnimation`, `aiNodeAnim`에서 TRS 키프레임 추출

2. **AnimationController**:
   - `src/Core/AnimationController.h/.cpp`: 현재 재생 시간 추적, Play/Pause/Loop 제어
   - `Update(float dt)`: 시간 전진 → 각 채널의 현재 TRS 보간 → 해당 SceneNode Transform 갱신
   - Engine::Update()에서 AnimationController::Update() 호출
   - "Animation" 메뉴: 클립 선택, 재생 속도 조절

3. **SceneLoader 확장**:
   - `SceneLoader::LoadAnimations()`: aiScene의 aiAnimation 배열 순회 → AnimationClip 생성
   - 채널 target name → SceneNode 포인터 매핑

#### Part B: Skeletal Animation (G-09)

4. **Skeleton / Skin 데이터 구조**:
   - `src/Asset/Skeleton.h`: `Bone` (name, parentIndex, inverseBindMatrix), `Skeleton` (bones 배열)
   - `Skin` (skeleton 참조, joint 인덱스 배열, inverse bind matrices)
   - SceneLoader: aiMesh의 aiBone 배열 → Skeleton 생성, per-vertex joint/weight 추출

5. **Vertex 포맷 확장**:
   - `Vertex` 구조체에 `XMUINT4 joints` (JOINTS_0) + `XMFLOAT4 weights` (WEIGHTS_0) 추가
   - D3D12 Input Layout, HLSL 입력 구조체, `static_assert` 갱신

6. **GPU Skinning**:
   - Joint matrix palette CB: `cbuffer SkinCB : register(b4)` — 최대 128개 bone matrix
   - `src/Shaders/PBR.hlsl`: `#define SKINNING` 조건부 컴파일로 스킨드/비스킨드 분기
     - `float4 skinnedPos = Σ(weights[i] * mul(jointMatrices[joints[i]], localPos))`
     - Normal, Tangent도 동일 변환 적용
   - AnimationController::Update() 후 현재 bone world matrix 계산 → GPU 업로드

**완료 기준**: glTF 애니메이션 파일(예: CesiumMan.glb, RiggedFigure.glb)에서 노드 TRS 애니메이션 및 스킨 메시 애니메이션이 정상 재생, 모든 테스트 통과

---

### Phase 35: RRScenePreprocessor — 오프라인 씬 전처리 도구 + Skeletal Animation 통합 지원
**목표**: Phase 34에서 구현한 Skeleton/Skin/AnimationClip이 이미 완성된 시점에서,
glTF/GLB/FBX 씬을 엔진 전용 바이너리(`.rrscene`)로 저장하는 파이프라인을 처음부터 통합 구현한다.
기본 씬 데이터(Mesh/Material/Texture/Light)와 Skeletal Animation 데이터를 단일 포맷으로 지원하며,
CLI 도구(`RRScenePreprocessor.exe`)와 렌더링 앱 내 백그라운드 자동 생성의 두 진입점을 제공한다.

1. **`.rrscene` 바이너리 포맷 정의** (`src/Asset/RRSceneFormat.h`, 공용 헤더):
   - Header: magic("RRSC"), version(uint32=1), sourceHash(uint64, 크기^수정시각), 섹션 오프셋 테이블
   - Scene Section: 노드 수, 노드별(부모 인덱스, 이름, 로컬 TRS 행렬, meshIndex, materialIndex), 씬 AABB, 카메라 초기 상태
   - Mesh Section: 메시별 — `isSkinned` 플래그, Vertex 배열(스킨 메시는 joints/weights 포함) raw dump, Index 배열, AABB, LOD 데이터(LOD 0~2 Vertex/Index + 전환 거리)
   - Material Section: PBR factor 값, AlphaMode, doubleSided, 텍스처 인덱스, sRGB/Linear 포맷 힌트
   - Texture Section: 텍스처별 — 너비/높이/Mip 수, DXGI_FORMAT, 전체 Mip chain 픽셀 데이터(연속 배치)
   - Light Section: 타입/색상/강도/위치/방향/감쇠/원뿔각/castShadow/BoundingSphere radius
   - **Skeleton Section**: 본 수, 본별(이름, parentIndex, inverseBindMatrix), Skin 수, Skin별(skeletonIndex, jointIndices 배열)
   - **Animation Section**: 클립 수, 클립별(이름, 재생 시간, 채널 수), 채널별(targetNodeIndex, Property(TRS), Interpolation, 키프레임 수, 키프레임 배열)
   - 스킨 메시 없는 씬은 Skeleton/Animation Section 생략 (SectionCount 기반 감지)

2. **전처리 파이프라인 구현** (`src/Asset/ScenePreprocessor.h/.cpp`, CLI와 엔진이 공유):
   - `ScenePreprocessor::Generate(sourcePath, outputPath)`: 동기 전처리 (CLI 도구용)
   - `ScenePreprocessor::GenerateAsync(sourcePath)`: `std::async`로 백그라운드 실행, `std::future<bool>` 반환 (엔진 내 자동 생성용)
   - 내부 파이프라인: Assimp 파싱 → Vertex/Index 변환 + Tangent → 프리미티브 분리 → 메시별 AABB → Auto-LOD(QEM, LOD1=50%/LOD2=25%) → Skeleton/Skin 추출(스킨 메시 시) → Animation 추출(aiAnimation 존재 시) → 이미지 디코딩(stb_image) → Mip chain(CPU box filter) → 씬 직렬화
   - 원자적 파일 쓰기: 임시 파일(`.rrscene.tmp`) 완성 후 원본 경로로 rename (부분 파일 방지)

3. **VS 프로젝트 `RRScenePreprocessor` 추가** (솔루션 내 Console Application):
   - `ScenePreprocessor` (엔진 헤더 공유)를 호출하는 얇은 CLI 래퍼
   - 진입점: `main(argc, argv)` — 입력 파일 경로를 인수로 받아 `Generate()` 호출
   - 출력: `bin/Debug/RRScenePreprocessor.exe`

4. **렌더링 앱 이중 로딩 경로 추가** (`src/Asset/SceneLoader`):
   - **고속 경로**: `.rrscene` 발견 + sourceHash 일치 → 섹션 순서대로 SceneNode/Mesh/Material/Texture/Light 객체 생성 + Skeleton Section 존재 시 Skeleton/Skin 생성 + Animation Section 존재 시 AnimationClip 생성 및 AnimationController 등록 → GPU 업로드(VB/IB/Texture)만 수행 (Assimp 파싱 없음)
   - **표준 경로**: `.rrscene` 없거나 해시 불일치 → Assimp 런타임 파싱 → 로딩 완료 후 항목 5 실행
   - DebugHUD에 로딩 경로 표시: "Fast (.rrscene)" / "Standard (Assimp)"

5. **표준 경로 로딩 후 백그라운드 자동 전처리** (`Engine::LoadScene()`):
   - 표준 경로(Assimp) 로딩 완료 직후: `ScenePreprocessor::GenerateAsync(sourcePath)` 호출
   - 렌더링을 블로킹하지 않고 백그라운드 스레드에서 전처리 파이프라인 실행 (Skeleton/Animation 포함)
   - DebugHUD에 진행 상태 표시: "Preprocessing scene..." (완료 후 사라짐)
   - 완료 시: `.rrscene` 파일 원자적 저장, 콘솔 로그 출력 ("Sponza.rrscene saved")
   - 다음 로딩 시 자동으로 고속 경로 사용

**완료 기준**: 신규 씬 첫 로딩 시 표준 경로 + 백그라운드 자동 생성 동작 확인, 두 번째 로딩 시 자동으로 고속 경로 사용 확인(1~3초), CLI 도구(`RRScenePreprocessor.exe`)로도 동일한 `.rrscene` 생성 가능, CesiumMan.glb를 전처리 후 고속 로딩으로 스켈레탈 애니메이션 정상 재생 확인, 스킨 없는 씬(Sponza)과 스킨 있는 씬(CesiumMan) 모두 렌더링 결과 동일

---

### Phase 36: Deferred Rendering — G-Buffer 기반 렌더링 파이프라인
**목표**: 기존 Forward Rendering 파이프라인을 Deferred Shading으로 전환.
G-Buffer에 기하학 정보를 저장하고 Lighting Pass에서 화면 공간 라이팅을 수행.
다수 Point Light의 라이팅 비용을 O(픽셀 × 광원)에서 O(픽셀)로 분리한다.

1. **G-Buffer MRT 생성** (`D3D12Context`):
   - RT0: `R8G8B8A8_UNORM_SRGB` — Albedo(RGB) + Metallic(A)
   - RT1: `R16G16B16A16_FLOAT` — World Normal(XYZ) + Roughness(A)
   - RT2: `R8G8B8A8_UNORM` — Emissive(RGB) + AO(A)
   - Depth: `D32_FLOAT` (기존 Depth Buffer 재사용, SRV 겸용)
2. **Geometry Pass**: Opaque 메시 → G-Buffer Fill, Alpha Mask(clip 적용)
3. **Lighting Pass**: Full-Screen Quad, G-Buffer SRV + Shadow Map SRV 바인딩, Cook-Torrance BRDF, HDR RT 출력
4. **Forward+ 투명 패스**: Alpha Blend 메시는 기존 Forward 방식으로 HDR RT에 합성
5. **G-Buffer 디버그 뷰**: Albedo/Normal/Metallic-Roughness/Depth 시각화 뷰 모드 ("Render" 메뉴 확장)

**완료 기준**: G-Buffer MRT 생성·시각화, Deferred Lighting Pass 동작, Alpha Blend Forward 합성, 기존 PBR 품질 유지

---

### Phase 37: HDR Pipeline + Tone Mapping
**목표**: 16-bit HDR 렌더 파이프라인 구축 및 Tone Mapping, Auto-Exposure 적용.

1. **HDR Render Target**: `DXGI_FORMAT_R16G16B16A16_FLOAT` (Lighting Pass 출력, Bloom 입력)
2. **Tone Mapping Pass** — 선택 가능한 알고리즘:
   - Reinhard: `L_out = L_in / (1 + L_in)`
   - ACES Filmic: 시네마틱 색조 (Hill 근사)
3. **Auto-Exposure**: Compute Shader로 평균 Luminance 계산 → EV 노출값 자동 조절 (+ Manual EV offset)
4. **sRGB 출력**: Tone Map 결과 → `R8G8B8A8_UNORM_SRGB` SwapChain 출력
5. **DebugHUD**: Tone Mapping 모드, Average Luminance, EV 노출값 표시

**완료 기준**: HDR 렌더 타겟, Reinhard/ACES Tonemapping 메뉴 전환, Auto-Exposure 동작

---

### Phase 38: SSAO (Screen Space Ambient Occlusion)
**목표**: G-Buffer Depth/Normal 활용 화면 공간 주변 차폐 계산. 접촉 그림자와 크레비스 음영으로 장면 깊이감 향상.

1. **SSAO Buffer**: `R8_UNORM` 별도 렌더 타겟
2. **SSAO Pass**: Hemisphere Sample Kernel(16~64개) + 노이즈 텍스처 기반 랜덤화
   - Depth → View-Space Position 재구성, G-Buffer Normal → View-Space 변환
   - 반구형 샘플 오프셋으로 주변 깊이 비교 → Raw AO 계산
3. **Blur Pass**: Bilateral Blur (Depth/Normal 경계 보존), 수평→수직 2패스 분리
4. **Lighting Pass 통합**: AO를 Ambient Light에 곱하여 자연스러운 차폐 표현
5. **메뉴/HUD**: SSAO on/off 토글, AO Buffer 시각화 뷰 모드

**완료 기준**: SSAO Buffer + Blur 적용, Lighting Pass 통합, AO on/off 비교 가능

---

### Phase 39: Bloom + Post-Processing 파이프라인
**목표**: Bloom 효과 구현 및 Ping-Pong Buffer 기반 Post-Processing 프레임워크 구축.

1. **Ping-Pong Buffer 프레임워크**: HDR RT 2개 교대 사용, `PostProcessor::AddPass(shader)` 패스 등록
2. **Bright Pass**: Luminance 임계값(기본 1.0) 이상 픽셀 추출
3. **Gaussian Blur Pyramid**: 6단계 다운샘플 → 업샘플 합성 (Dual Kawase Blur 활용)
4. **Bloom Composite**: Bloom 레이어를 HDR RT에 Additive Blend
5. **파이프라인 순서 확정**: Bloom → Tone Mapping → TAA → sRGB 출력
6. **메뉴/HUD**: Bloom on/off, 임계값·Intensity 조정

**완료 기준**: Bloom 효과 동작, Post-Processing 프레임워크 완성

---

### Phase 40: TAA (Temporal Anti-Aliasing)
**목표**: Halton Sequence 지터링 + History Buffer 블렌딩 + Variance Clipping으로 AA 및 서브픽셀 디테일 개선.

1. **Jitter Matrix**: 8~16프레임 Halton Sequence(base 2, 3)로 투영 행렬 서브픽셀 오프셋
2. **Motion Vector Buffer**: Velocity Buffer(G-Buffer RT 추가 또는 별도)
   - 정적: 카메라 움직임으로 계산, 동적(Skeletal): 이전 프레임 WorldMatrix → Reprojection
3. **History Buffer**: 이전 프레임 TAA 출력 SRV 보관
4. **TAA Resolve Pass**: Current + Reprojected History 블렌딩(α≈0.1~0.15)
   - Variance Clipping: 3×3 이웃 통계로 History AABB clip → 고스팅 억제
   - Velocity 크기에 따른 블렌딩 가중치 감소
5. **메뉴**: TAA/MSAA/None 전환

**완료 기준**: TAA on/off 비교, 고스팅 억제 동작, 정적 씬 SSAA 수준 품질

---

### Phase 41: Motion Blur + Depth of Field
**목표**: Per-Object Motion Blur와 Bokeh DoF로 영화적 품질 향상.

1. **Motion Blur** (Phase 40 Velocity Buffer 활용):
   - Tile-based Max Velocity: N×N 타일 내 최대 속도 계산
   - 속도 방향으로 N샘플 평균, Soft-Edge 처리, 셔터 속도 시뮬레이션
2. **Depth of Field**:
   - CoC(Circle of Confusion): Depth → CoC 반경 계산(Focus Distance, F-Number 파라미터)
   - Bokeh Blur: CoC 가변 반경 Gather Blur (기본: Separable Gaussian, 품질: Hexagonal Bokeh)
   - Near/Far Field 분리 처리
   - F-Number, Focal Length 메뉴 조정

**완료 기준**: Motion Blur per-object 동작, DoF CoC 기반 블러, 메뉴 파라미터 조정

---

### Phase 42: SSR (Screen Space Reflections) + Refraction
**목표**: G-Buffer Depth/Normal 기반 화면 공간 반사 및 굴절 구현.

1. **SSR (Screen Space Reflections)**:
   - G-Buffer Normal+Depth에서 반사 Ray Direction 계산
   - Hi-Z Raymarching: 계층적 Depth로 빠른 교차 검사
   - 교차점 화면 UV → SSR Color 샘플링
   - Fresnel 강도 (metallic/roughness 반영), Roughness 기반 블러
   - 화면 경계/낮은 각도 → Envmap Cubemap 폴백
2. **Refraction**: Alpha Blend 오브젝트(유리)에 IOR 기반 UV 오프셋 적용, Depth 비교로 penetration 방지
3. **Environment Map Fallback**: SSR miss 시 Skybox Cubemap 또는 Reflection Capture 사용

**완료 기준**: SSR 반사 동작, Roughness 기반 블러, Fresnel 강도, Refraction 오프셋 적용

---

### Phase 43: Screen Space Subsurface Scattering (SSSSS)
**목표**: 피부·밀랍·대리석 등 반투명 재질의 광 산란 효과.

1. **Subsurface Material**: `subsurfaceColor`(XMFLOAT3) + `scatterWidth`(float) 파라미터 추가
2. **SSS Pass (Separable)**:
   - Stencil 마스크로 SSS/비-SSS 픽셀 분리
   - 6-weight Gaussian Kernel × 3채널(R > G > B 확산 폭): 수평→수직 2패스 분리
   - R 채널 가장 넓게 산란 (붉은 피부 효과)
3. **참고 모델**: 피부 재질 테스트 모델 또는 ProceduralSphere + SSS 재질

**완료 기준**: SSS on/off 비교, RGB 채널별 확산 폭 조절, Stencil 마스크 동작

---

### Phase 44: Global Illumination — DDGI (Dynamic Diffuse GI)
**목표**: 씬 전역 동적 간접광 시뮬레이션. Irradiance Probe 기반 DDGI 구현.

1. **Probe Grid**: 씬 AABB 내 3D Grid(예: 8×4×8 = 256 Probe) 배치
2. **Probe Update**:
   - DXR 사용 가능 시: 각 Probe에서 구면 방향으로 Radiance Ray 발사 (Phase 45 연동)
   - DXR 미지원 시: 정적 Reflection Capture Probe로 폴백
   - Irradiance(L0) + Visibility(Depth) → Probe Texture(Octahedral Map) 저장
3. **Probe Sampling**: 픽셀 위치 → 3D Grid → 8코너 Probe 삼선형 보간, SH2/SH3 Irradiance 샘플링
4. **Lighting Pass 통합**: Indirect Diffuse += Probe Irradiance × Albedo (기존 Fill Light 대체/보완)
5. **디버그 뷰**: Probe 위치·Irradiance 시각화

**완료 기준**: Probe Grid 배치, Irradiance 업데이트, 씬 간접광 표현, 디버그 시각화

---

### Phase 45: DXR Hybrid Ray Tracing
**목표**: DirectX 12 Raytracing(DXR) API로 Ray-Traced Shadow·Reflection·GI 구현.
Rasterization과 Ray Tracing을 Hybrid 방식으로 결합, PCF Shadow/SSR 대비 최고 품질 달성.

1. **DXR 인프라**:
   - `ID3D12Device5::CreateStateObject()` — DXR PSO (RayGen/ClosestHit/Miss/AnyHit 셰이더)
   - BLAS: 메시별 생성 및 GPU 빌드 (정적/동적 BLAS 분리)
   - TLAS: 씬 전체 인스턴스 행렬 기반 매 프레임 갱신
   - ShaderTable: RayGen/Miss/HitGroup 테이블 빌드 및 업로드
2. **Ray-Traced Shadow**: Directional/Point/Spot 광원별 Shadow Ray, 반투명 AnyHit, PCF 대체
3. **Ray-Traced Reflection**: G-Buffer Normal+Roughness → 반사 Ray, Cone Sampling, 재귀 1~2레벨
4. **GI 연동**: Phase 44 DDGI Probe Update에 DXR Ray 활용
5. **Denoiser 연동**: Phase 47 Neural Denoiser 또는 Temporal Accumulation Denoiser
6. **하드웨어 감지 및 폴백**: DXR Tier 1.1 미지원 시 PCF Shadow/SSR로 자동 폴백

**완료 기준**: TLAS/BLAS 빌드, RT Shadow 동작, RT Reflection 동작, DXR/Raster Hybrid 전환

---

### Phase 46: Nanite-style Virtual Geometry
**목표**: Cluster 기반 GPU-Driven LOD 시스템. Meshlet 렌더링으로 극단적 폴리곤 밀도 처리.

1. **Meshlet 분할**: ~128삼각형 단위 Meshlet 생성 (DirectX MeshShader 활용)
   - Meshlet당 바운딩 스피어 + 노말 Cone(back-face culling용) 계산
2. **Mesh Shader 파이프라인**: VS/IA 대신 Amplification Shader + Mesh Shader 사용
   - Amplification Shader: Meshlet Frustum/Back-face Culling → 가시 Meshlet 목록 생성
   - Mesh Shader: 가시 Meshlet 삼각형 출력
3. **Cluster LOD Hierarchy**: 메시 심플리피케이션으로 Cluster 계층 트리 구축
   - GPU에서 Projected Error 기준 LOD 전환 경계 결정 (기존 LODSelector 교체 또는 병행)
4. **GPU-Driven Indirect Rendering**: `ExecuteIndirect()` — Compute Shader가 DrawArgs Buffer 생성 → GPU 실행
5. **디버그 뷰**: Meshlet 색상 시각화, LOD 레벨 시각화

**완료 기준**: Meshlet 분할·시각화, Amplification+Mesh Shader 파이프라인 동작, GPU-Driven IndirectDraw

---

### Phase 47: Neural Upscaling (DLSS/FSR) + Neural Denoising
**목표**: AI/ML 기반 업스케일링으로 저해상도 렌더링 + 고품질 출력. Ray-Traced 노이즈 제거.

1. **FSR 3 (AMD FidelityFX Super Resolution)**:
   - FidelityFX SDK 연동: Color Buffer + Depth + Motion Vector → FSR3 업스케일 출력
   - Quality Mode 메뉴: Quality / Balanced / Performance / Ultra Performance
   - 렌더 해상도: 출력 해상도의 50%/67%/75%로 설정 가능
2. **DLSS 4 (NVIDIA DLSS) — 선택적**:
   - NVIDIA Streamline SDK 연동 (RTX 하드웨어 전용), 미지원 시 FSR로 자동 폴백
   - RTX 5060 Ti(Blackwell) 기준 DLSS 4 네이티브 지원 (4세대 Tensor 코어)
3. **Neural Denoising**:
   - 옵션 A: NRD (NVIDIA Real-time Denoising) SDK 연동 (Relax/Reblur 알고리즘)
   - 옵션 B: 자체 Temporal Accumulation Denoiser (모멘트 기반 분산 추정 + Bilateral Filter)
4. **DebugHUD**: Upscaling 모드, 렌더/출력 해상도, Denoiser 종류 표시

**완료 기준**: FSR 3 업스케일 동작, Quality Mode 전환, Denoiser 적용 (RT 결과 또는 독립 노이즈 입력)

---

### Phase 48: Phase 03 코드 리뷰, 최적화, 버그 수정 & 아키텍처 문서화
**목표**: Phase 32~47에서 추가된 모든 고급 렌더링 기법의 코드 품질 점검, 성능 최적화,
버그 수정, 그리고 전체 엔진 아키텍처를 반영한 최종 문서(`ARCHITECTURE.md`) 완성.

1. **코드 리뷰**:
   - Dead code 제거, include 순서 정리, 네이밍 일관성 (PascalCase/camelCase) 검증
   - G-Buffer / Deferred 파이프라인 코드 리뷰: MRT 바인딩 순서, 포맷 일관성
   - DXR ShaderTable 빌드 로직, BLAS/TLAS 갱신 주기 코드 리뷰
   - Mesh Shader / Amplification Shader 코드 리뷰 (Meshlet 분할 경계 조건)
   - Neural Upscaling SDK 연동 코드 리뷰 (FSR / DLSS / NRD 초기화 순서)
   - D3D12 Debug Layer 경고 0건 목표 (리소스 상태 전이 누락, lifetime 위반 등)

2. **성능 최적화**:
   - PIX for Windows 또는 D3D12 Timestamp Query로 각 렌더 패스 비용 측정
   - G-Buffer 포맷 최적화 (RT1을 R10G10B10A2로 축소 검토)
   - SSAO 샘플 수 / TAA 블렌딩 계수 / Bloom 피라미드 단계 수 튜닝
   - Hi-Z Mip chain 생성 비용 측정 및 다운샘플 단계 최적화
   - DXR TLAS Refit (정적 오브젝트 BLAS 재사용, 동적만 Rebuild)
   - Nanite Meshlet 크기 및 LOD 전환 Projected Error 임계값 튜닝
   - Denoiser Temporal 수렴 속도 vs 고스팅 트레이드오프 조정

3. **버그 수정**:
   - 렌더 패스 간 리소스 상태 전이 누락 수정 (D3D12_RESOURCE_STATE_*)
   - TAA 고스팅 엣지 케이스 (씬 전환 직후 History Buffer 초기화)
   - SSR 화면 경계 아티팩트 (경계 Fade 파라미터 튜닝)
   - DDGI Probe 갱신 시 Irradiance 튀는 현상 (Hysteresis 파라미터 조정)
   - DXR AnyHit 셰이더에서 투명 오브젝트 투과율 잘못 계산되는 케이스
   - FSR/DLSS Motion Vector 스케일 불일치 수정

4. **아키텍처 문서화** (`ARCHITECTURE.md` 완성):
   - 전체 렌더 파이프라인 다이어그램 (Phase 01 ~ Phase 47 누적 아키텍처)
   - 렌더 패스 순서 및 리소스 의존성 (Shadow → G-Buffer → Lighting → Post → TAA → Upscale)
   - 주요 모듈 간 의존성 (Engine / Renderer / SceneGraph / RHI / Asset / Lighting)
   - G-Buffer 레이아웃, Descriptor Heap 구조, Root Signature 레지스터 맵
   - DXR 가속 구조 (BLAS/TLAS) 업데이트 주기 및 ShaderTable 구성
   - Meshlet / GPU-Driven 렌더링 흐름 (Compute → DrawArgs → ExecuteIndirect)
   - Neural Upscaling 렌더 해상도 관리 흐름
   - 스레딩 모델: 메인 렌더 스레드 / Compute Queue / Copy Queue / Worker Thread 관계

5. **최종 벤치마크**:
   - Sponza + Bistro: Full Phase 03 파이프라인(Deferred + SSAO + Bloom + TAA + SSR + DDGI) 60fps 목표
   - DXR 활성 시 RT Shadow + RT Reflection 포함 성능 측정
   - FSR 3 활성 시 (렌더 해상도 67%) 품질 vs 성능 비교

**완료 기준**: D3D12 Debug Layer 경고 0건, 주요 패스 타임스탬프 측정 완료, ARCHITECTURE.md 작성 완료, Sponza+Bistro 벤치마크 결과 기록

---

## Phase 03 의존성 그래프

```
Phase 31 (Phase 02 Backup)
    │
    ├── Phase 32 (Occlusion Culling: Hi-Z GPU + Compute 인프라) ──────────────────┐
    ├── Phase 33 (Point Light Cube Map Shadowing) ─────────────────────────────────┤
    └── Phase 34 (Skeletal Animation) ─────────────────────────────────────────────┤
            └── Phase 35 (RRScenePreprocessor: .rrscene + Skeletal 통합) ──────────┤
                                                                                    │
                                              Phase 36 (Deferred Rendering) ────────┘
                                                             │
                                              Phase 37 (HDR Pipeline + Tone Mapping)
                                                             │
                                    ┌───────── Phase 38 (SSAO) ──────────────────┐
                                    │         Phase 39 (Bloom + PP 파이프라인)    │
                                    │         Phase 40 (TAA) ─────────────────────┤
                                    │         Phase 41 (Motion Blur + DoF) ───────┤
                                    │         Phase 42 (SSR + Refraction) ─────────┤
                                    │         Phase 43 (SSSSS) ────────────────────┤
                                    │                                              │
                                    └──────────────────────── Phase 44 (DDGI/GI) ──┤
                                                                       │           │
                                                        Phase 45 (DXR Hybrid RT) ──┘
                                                                       │
                                                        Phase 46 (Nanite: Virtual Geometry)
                                                                       │
                                                        Phase 47 (Neural Upscaling + Denoising)
                                                                       │
                                                        Phase 48 (코드 리뷰 + 최적화 + 버그 수정 + ARCHITECTURE.md)
```

## Phase 03 리스크 & 대응

| 리스크 | 대응 |
|--------|------|
| Deferred Rendering으로 전환 시 Alpha Blend 호환 | Hybrid Forward+: Alpha Blend 오브젝트는 기존 Forward 패스 유지 |
| G-Buffer VRAM 오버헤드 | RT0~RT2 + Depth = ~30MB (1080p 기준), VRAM 예산 확인 후 포맷 최적화 |
| TAA 고스팅 (Ghost Artifact) | Variance Clipping + Velocity 가중치로 억제, 움직임 큰 씬에서 블렌딩 감소 |
| DXR 미지원 하드웨어 (no RTX) | 런타임 DXR Tier 감지, PCF Shadow/SSR/DDGI Static Probe로 자동 폴백 |
| DDGI Probe Ray 비용 (256 Probe × 256 Ray) | 비동기 Compute Queue에서 Probe 부분 업데이트 (매 프레임 일부만 갱신) |
| Nanite Mesh Shader 미지원 (구형 GPU) | Feature Level 확인, 미지원 시 기존 LODSelector + DrawIndexedInstanced 폴백 |
| FSR/DLSS SDK 버전 관리 | vcpkg 또는 서브모듈로 SDK 버전 고정, 업데이트 시 통합 테스트 |
| Neural Denoiser latency (1프레임 딜레이) | Temporal Denoiser는 1프레임 레이턴시 허용 (RT Shadow/Reflection 결과에는 용인 범위) |
