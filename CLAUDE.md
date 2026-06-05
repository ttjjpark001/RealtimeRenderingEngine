# CLAUDE.md — 실시간 렌더링 엔진 프로젝트 가이드

> **Phase 01, 02 완료 및 백업 안내**
> Phase 01 백업: `Phase 01 Backup/`, Phase 02 백업: `Phase 02 Backup/`
> **두 폴더 안의 파일은 절대 참조하거나 수정하지 않는다. 어떠한 작업에서도 이 폴더들을 건드리지 않는다.**
> 이후 작업은 프로젝트 루트의 `src/`, `tests/` 등 현재 디렉토리에서 진행한다.

---

## 프로젝트 개요

Win32 API + DirectX 12 + C++17 기반 실시간 렌더링 엔진. RHI(Rendering Hardware Interface) 추상화 계층을 통해 렌더링 백엔드를 분리하고, Scene Graph로 오브젝트 계층을 관리한다. 수학 연산은 DirectXMath(SIMD 최적화)를 사용한다.

## 빌드

Visual Studio 2022에서 `RealtimeRenderingEngine.sln`을 열고 빌드한다.
- 구성: Debug|x64, Release|x64
- 출력 경로: `bin/Debug/` 또는 `bin/Release/`

Google Test 설치 (vcpkg):
```bash
vcpkg install gtest:x64-windows
vcpkg integrate install
```

테스트 실행: VS에서 RREngineTests 프로젝트를 시작 프로젝트로 설정 후 실행.

## 솔루션 구조

```
RealtimeRenderingEngine.sln
├── RREngine (src/)                  — 엔진 (Windows Application, SubSystem: Windows)
├── RREngineTests (tests/)           — 테스트 (Console Application, SubSystem: Console)
├── RRScenePreprocessor (src/Tools/) — 오프라인 씬 전처리 CLI 도구 (Console Application) [Phase 35]
└── docs (가상 폴더)                 — PRD.md, PLAN.md, PROMPT.md, CLAUDE.md
```

## 디렉토리 구조

```
src/
  Asset/        — glTF 로더, Material, Texture, TextureCache, TextureStreamer, ScenePreprocessor, RRSceneFormat
  Core/         — Engine 메인 루프, 공용 타입 (Types.h에 DirectXMath 별칭)
  Math/         — MathUtil.h (DirectXMath 헬퍼)
  Platform/     — Win32 윈도우/입력/메뉴 (플랫폼별 분리)
  RHI/          — 렌더링 하드웨어 추상화 인터페이스 (IRHIDevice, IRHIBuffer, IRHIContext)
    D3D12/      — DirectX 12 백엔드 (Device, Context, SwapChain, Buffer, PSO, DescriptorHeap, CBPool)
  Renderer/     — Vertex, Mesh, MeshFactory, Renderer, DebugHUD, FrustumCuller, LODSelector, LightCuller, InstanceBatcher, OcclusionCuller
  Scene/        — SceneNode, SceneGraph, Transform, Camera
  Lighting/     — Light (Directional/Point/Spot), LightManager
tests/
  unit/         — DirectXMath 유틸리티, Scene Graph, 면 색상 규칙, 카메라 유닛 테스트
  smoke/        — 엔진 초기화, RHI 초기화 스모크 테스트
```

## 핵심 아키텍처

### RHI 패턴
- `IRHIDevice`, `IRHIBuffer`, `IRHIContext`는 순수 가상 클래스
- D3D12 백엔드는 `RHI/D3D12/` 하위에 위치
- 새 백엔드(Vulkan 등) 추가 시 해당 인터페이스를 구현하면 됨
- Engine은 RHI 인터페이스만 사용, 구체 백엔드를 직접 참조하지 않음

### DirectX 12 파이프라인
- ID3D12Device → Command Queue → Command List → PSO → Draw
- 더블 버퍼링 (IDXGISwapChain4)
- Fence 기반 GPU 동기화
- Depth Stencil Buffer (DXGI_FORMAT_D24_UNORM_S8_UINT) + DSV Heap 생성/관리, 리사이즈 시 재생성
- CBV_SRV_UAV DescriptorHeap(shader-visible) + Upload Buffer(256바이트 정렬) 기반 Constant Buffer 관리
- 셰이더는 빌드 타임에 .cso 파일로 사전 컴파일 (VS HLSL Compiler), 런타임 D3DCompileFromFile 미사용

### 수학 (DirectXMath)
- 저장 타입: XMFLOAT3, XMFLOAT4, XMFLOAT4X4 (멤버 변수용)
- 연산 타입: XMVECTOR, XMMATRIX (SIMD 레지스터, 로컬 연산용)
- 변환 패턴: XMLoadFloat3 → SIMD 연산 → XMStoreFloat3
- Types.h 별칭: Vector3 = XMFLOAT3, Vector4 = XMFLOAT4, Matrix4x4 = XMFLOAT4X4
- **행렬 전치 규칙 (CPU → GPU 전달 시 필수)**:
  - DirectXMath는 행 우선(row-major), HLSL cbuffer는 기본 열 우선(column-major)
  - GPU로 행렬을 전달하기 전에 반드시 `XMMatrixTranspose()`로 전치해야 한다
  - HLSL에서 `row_major` 키워드를 사용하지 않는다 (CBV 방식에서 불안정)
  - 패턴: `XMStoreFloat4x4(&dst, XMMatrixTranspose(matrix))` → Constant Buffer에 복사

### Scene Graph
- 트리 구조: 루트 노드 아래 부모-자식 계층
- 각 SceneNode는 Transform(위치/회전/스케일)과 Mesh 참조를 보유
- WorldMatrix = Local.TRS_Matrix × Parent.WorldMatrix (local × parent 순서)
- 깊이 우선 순회로 렌더링
- glTF 로딩 시 단일 aiNode의 복수 aiMesh를 각각 별도 SceneNode로 분리 (노드 단위 Culling/LOD/Instancing)
- SceneNode에 `BoundingBox m_worldAABB` + `bool m_aabbDirty` 캐싱
  - `GetWorldAABB()`: 로컬 AABB를 WorldMatrix로 변환하여 월드 공간 AABB 반환

### Renderer
- `Renderer` 클래스가 SceneGraph를 Traverse하며 각 노드의 Mesh를 DrawPrimitives로 렌더링
- Mesh→VB/IB 캐시: `std::unordered_map<Mesh*, MeshBuffers>` — 처음 만나는 Mesh는 자동 업로드
- `RenderScene()`: Frustum 빌드 → Light Culling → Shadow Depth Pass → Opaque Pass → Alpha Blend Pass
- `ClearMeshCache()`: GPU 메시 캐시 + LOD 등록 모두 클리어 (씬 교체 전 호출)
- `RegisterMeshesForLOD()`: 씬 로딩 후 sceneDiagonal 확정 시점에 호출
- `GetLastCullStats()`: 직전 RenderScene() 호출의 컬링/LOD 통계 반환
- `struct CullStats`: `visibleNodes`, `frustumCulledNodes`, `occlusionCulledNodes`, `activeLights`, `culledLights`, `renderedPolygons`
- Engine은 Renderer를 통해 렌더링하며, 직접 VB/IB를 관리하지 않음

### 상태 표시 HUD (DebugHUD)
- 화면 왼쪽 상단에 렌더링 통계를 텍스트 오버레이로 표시
- 기본 표시: FPS, 해상도, 렌더링 모드명, 노드 수, Polys(scene/rendered), Poly/sec, Visible/Culled, Lights active/culled
- 광원 정보 토글, 카메라 정보 토글 (Camera 메뉴)
- D3D11On12 + D2D1 + DirectWrite interop으로 텍스트 렌더링

### 광원 시스템 (Lighting)
- LightManager 기반 3-포인트 라이팅 자동 배치
- 씬 로드 시 Key/Fill/Back 3개 포인트 광원 + Orbit Directional 광원 자동 배치
  - Key Light: warm(1.0, 0.95, 0.9), intensity=12 — 우측 상단 전면 (Point)
  - Fill Light: cool(0.8, 0.85, 1.0), intensity=6 — 좌측 (Point)
  - Back Light: neutral(1, 1, 1), intensity=8 — 후면 상단 rim (Point)
  - **Orbit Light**: white, intensity=6, `castShadow=true` — Directional, 월드 Y축 궤도 회전 (0.8 rad/s), 45° 앙각 고정
    - `lightDir = { -cosElev·cos(θ), -sinElev, -cosElev·sin(θ) }`, 카메라 독립
- 거리 기반 감쇠: `attenuation = 1 / (Kc + Kl·d + Kq·d²)` (기본: Kc=1.0, Kl=0.027, Kq=0.005)
- **Sponza 전용 태양 방향 토글 — L 키**: 두 방향 프리셋 전환
  - 기본: `normalize(-0.3, -1.0, 0.5)`, Alt: `normalize(-0.3, -1.5, 0.3)`
- Bistro 씬에서는 Orbit Light 비활성화 (`m_orbitLightIndex = SIZE_MAX`)

### 카메라 (Camera)
- Scene/Camera.h/.cpp에 위치
- 투영 모드: Perspective (기본) / Orthographic 전환
- **키보드 이동**: WASD+QE 위치 이동, +/- FOV 조절
- **마우스 네비게이션**: 우클릭 드래그(Yaw/Pitch), 휠(돌리 줌), 중클릭 드래그(패닝)
- **Fit to Scene**: 씬 바운딩 박스 기반 자동 배치 (씬 로드 시 자동 호출)
- 이동 속도는 씬 바운딩 박스 크기에 비례하여 자동 조절

### Vertex 데이터 (현재 포맷, Phase 02+)
```
struct Vertex {
    XMFLOAT3 position;   // offset  0 (12 bytes)
    XMFLOAT4 color;      // offset 12 (16 bytes)
    XMFLOAT3 normal;     // offset 28 (12 bytes)
    XMFLOAT2 texCoord;   // offset 40 ( 8 bytes)
    XMFLOAT4 tangent;    // offset 48 (16 bytes)  ← w: handedness
};  // sizeof(Vertex) == 64
```
- D3D12 Input Layout: POSITION + COLOR + NORMAL + TEXCOORD + TANGENT
- `static_assert`로 멤버 오프셋과 sizeof 빌드 타임 검증
- Mesh는 Vertex 배열 + Index 배열 + `DirectX::BoundingBox aabb`로 구성

### 화면 모드 (View Menu)
- 프리셋: 800×450, 960×540 (기본), Full Screen (Borderless Windowed)
- Esc 키로 전체 화면 → 이전 윈도우 모드 복귀
- 모드 전환 시 RHI OnResize → SwapChain ResizeBuffers → RTV 재생성

## 코딩 컨벤션

- **언어**: C++17
- **네이밍**: PascalCase(클래스, 메서드), camelCase(변수, 파라미터), UPPER_SNAKE_CASE(상수/매크로)
- **헤더 가드**: `#pragma once`
- **스마트 포인터**: `std::unique_ptr`로 소유권 관리, raw pointer는 비소유 참조에만 사용
- **COM 포인터**: `Microsoft::WRL::ComPtr<T>`로 D3D12/DXGI COM 객체 관리
- **네임스페이스**: `namespace RRE { }`
- **include 순서**: 자기 헤더 → 프로젝트 헤더 → DirectX/Windows 헤더 → 표준 라이브러리
- **HRESULT 체크**: ThrowIfFailed 매크로 또는 유틸리티 함수 사용

## 링크 라이브러리

```
d3d12.lib       — DirectX 12 core
dxgi.lib        — DXGI (SwapChain, Adapter)
d3dcompiler.lib — HLSL 런타임 컴파일
dxguid.lib      — DirectX GUIDs
d3d11.lib       — D3D11On12 (HUD 텍스트 렌더링용)
d2d1.lib        — Direct2D
dwrite.lib      — DirectWrite
assimp-vc143-mt.lib — Assimp (glTF/GLB 로딩)
```

## 테스트 규칙

- DirectXMath 유틸리티 변경 시: `tests/unit/test_MathUtil.cpp`
- Scene Graph 변경 시: `tests/unit/test_SceneGraph.cpp`
- MeshFactory/색상 변경 시: `tests/unit/test_FaceColoring.cpp`
- Camera 변경 시: `tests/unit/test_Camera.cpp`
- RHI/D3D12 변경 시: `tests/smoke/test_RHIBackend.cpp`
- Renderer/Engine 통합 변경 시: `tests/smoke/test_EngineInit.cpp`
- D3D12 스모크 테스트는 WARP 어댑터로 GPU 없이도 실행 가능

## PBR 테스트 모델

저장 위치: `assets/test-models/`

| 모델 | 크기 | 테스트 항목 |
|------|------|------------|
| **DamagedHelmet.glb** | 3.6 MB | Normal + MetallicRoughness + Emissive + AO (전체 PBR 채널) |
| **MetalRoughSpheres.glb** | 11 MB | Metallic/Roughness 조합 그리드 |
| **NormalTangentTest.glb** | 1.8 MB | Normal map + 탄젠트 공간 변환 정확성 |
| **WaterBottle.glb** | 8.6 MB | Normal + MetallicRoughness + Emissive |
| **Lantern.glb** | 9.2 MB | Normal + MetallicRoughness + Emissive (복잡 멀티파트) |
| **Sponza** | glTF+JPG, 52 MB | `assets/test-models/Sponza/glTF/Sponza.gltf` — 성능/최적화 벤치마크 |
| **FlightHelmet** | glTF+PNG, 47 MB | `assets/test-models/FlightHelmet.gltf` — 멀티 머티리얼 검증 |

**추천 순서**: DamagedHelmet → MetalRoughSpheres → NormalTangentTest → Sponza

외부 대형 씬: Bistro (glTF 변환본 권장 — `niagara_bistro` 또는 `bevy_bistro_scene`)
이슈 상세: `BistroFBX.md` 참고

## 주요 참조 문서

- `PRD.md` — 제품 요구사항 정의
- `PLAN.md` — Phase 03 (32~48) 구현 설계
- `PROMPT.md` — Phase 03 (32~48) 구현 프롬프트
- `ARCHITECTURE.md` — 아키텍처 문서 (Phase 48에서 완성 예정)
- `RemainingWork.md` — 미해결 버그 및 런타임 검증 항목

## 주의사항

- Win32 API 코드는 `Platform/Win32/`에만 위치시킨다. 다른 모듈에서 Windows.h를 직접 include하지 않는다.
- RHI 인터페이스에 D3D12 전용 타입(ID3D12Device, ComPtr 등)이 노출되면 안 된다.
- DirectXMath의 XMVECTOR/XMMATRIX는 함수 파라미터로 직접 전달 시 FXMVECTOR/CXMMATRIX 사용 규칙을 따른다.
- float 비교 시 epsilon 기반 비교를 사용한다 (XMVector3NearEqual 등).
- D3D12 리소스 해제 전 GPU 작업 완료를 반드시 대기한다 (Fence).
- **행렬을 Constant Buffer(CBV)로 GPU에 전달할 때 반드시 `XMMatrixTranspose()`로 전치한다.**
- MSBuild로 `.vcxproj`를 직접 빌드할 때 `/p:SolutionDir=<루트경로>\`를 명시해야 한다.

---

## 씬 로딩 · PBR · Shadow (현재 구현 상태)

### Assimp 임포트 플래그

```cpp
importer.ReadFile(filePath,
    aiProcess_Triangulate | aiProcess_GenNormals |
    aiProcess_CalcTangentSpace |
    aiProcess_ConvertToLeftHanded);  // = MakeLeftHanded | FlipUVs | FlipWindingOrder
```

| 플래그 | 역할 |
|--------|------|
| `MakeLeftHanded` | 정점/노말/탄젠트 Z축 반전, UV.y 반전 |
| `FlipUVs` | UV.y 재반전 (MakeLeftHanded와 상쇄 → 원래 UV 유지) |
| `FlipWindingOrder` | CCW→CW 변환 (D3D12 `FrontCounterClockwise=FALSE` 일치) |

- `stbi_set_flip_vertically_on_load(false)` 필수 (glTF UV top-left = D3D12 동일)
- Transform에 `SetLocalMatrix()`로 Assimp 행렬 직접 저장 (TRS round-trip 손실 방지)

### 씬 파일 로딩 플로우

1. "File" → "Open Scene..." 또는 드래그 앤 드롭 → 파일 경로 획득
2. `WaitForGPU()` 후 기존 씬 해제 (SceneGraph 초기화, Mesh/Material/Texture 캐시 클리어)
3. `SceneLoader::LoadScene(filePath)` — Assimp 파싱 → SceneNode 트리 + Material 생성
4. TextureCache를 통해 텍스처 비동기 디코딩 시작 (로딩 전까지 1×1 폴백 텍스처)
5. 카메라 배치: 씬 파일의 aiCamera 노드 → 없으면 Fit to Scene
6. `Renderer::SetSceneDiagonal()` 호출 → Shadow Map 크기/LOD 전환 거리 자동 갱신

### Shadow Mapping 파라미터

- Shadow Map 해상도 자동 선택: `diagonal ≤ 10m` → 1024, `≤ 100m` → 2048, `> 100m` → 4096
- Directional 투영: ortho width/height = `diagonal × 1.5f`, far = `diagonal × 3.0f`, near = `diagonal × 0.5f`
- Shadow 카메라 배치: `shadowCamPos = sceneCenter - dir × (farPlane × 0.5f)`
- Shadow Normal Bias: `shadowNormalBiasWorld = (diagonal × 1.5f) / shadowMapSize × 2.0f`
- `shadowTexelSize = 1.0f / shadowMapResolution` → ShadowCB(b3)로 GPU 전달
- Depth Bias: `DepthBias=1000`, `SlopeScaledDepthBias=1.0`
- PCF: 3×3 커널, `SampleCmpLevelZero` (comparison sampler s1, `LESS_EQUAL`)
- Shadow Map 리소스: `DXGI_FORMAT_D32_FLOAT` — DSV(depth write) + SRV(`R32_FLOAT`, depth 비교)

### Root Signature / PSO 종류

Root Signature 슬롯:
- b0: PerObjectCB, b1: PerFrameCB/LightsCB, b2: PerMaterialCB, b3: ShadowCB
- t0~t4: PBR 텍스처 (albedo, normal, metallicRoughness, emissive, occlusion)
- t5~t13: LightStructuredBuffer + Shadow Maps
- s0: Linear Wrap sampler, s1: PCF Comparison sampler

PSO 종류 (현재 구현):

| PSO | 용도 |
|-----|------|
| BasicColor | vertex-color Phase 01 오브젝트, CullMode=BACK |
| PBR CullBack | doubleSided=false, Opaque/AlphaMask |
| PBR CullNone | doubleSided=true, Opaque/AlphaMask |
| PBR CullBack AlphaBlend | doubleSided=false, 블렌딩 |
| PBR CullNone AlphaBlend | doubleSided=true, 블렌딩 |
| Shadow Depth | depth-only, color write 비활성, depth bias 활성 |
| Wireframe | FillMode=Wireframe, 단색 PS |

### Cook-Torrance BRDF (PBR.hlsl)

- **NDF (GGX)**: `D = α² / (π · ((N·H)²·(α²-1)+1)²)` — α = roughness²
- **G (Smith-Schlick)**: `G = G1(N,V) · G1(N,L)`, `G1 = N·X / (N·X·(1-k)+k)` — k = (roughness+1)²/8
- **F (Schlick)**: `F = F0 + (1-F0)·(1-V·H)^5` — 비금속 F0=0.04, 금속 F0=albedo
- **Specular**: `D × G × F / (4 · N·L · N·V)`
- **Diffuse (Lambertian)**: `(1-F) · (1-metallic) · albedo / π`
- **최종**: `Σ shadowFactor · (diffuse + specular) · lightColor · N·L · attenuation + ambient`
- Gamma: 렌더 타겟 `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB` → GPU 자동 리니어→sRGB 변환

### 렌더링 모드 (Render 메뉴, 5단계)

| 모드 | 설명 |
|------|------|
| **Wireframe** | FillMode=Wireframe, 라이팅/텍스처 없음 |
| **Solid (No Texture)** | PBR 셰이더, 텍스처 플래그 모두 0 강제 |
| **Base Color Only** | hasAlbedoMap만 활성, 나머지 기본값 |
| **Full PBR** | 모든 PBR 텍스처 적용, Shadow Pass 스킵 |
| **Full PBR + Shadows** | 완전한 PBR + Shadow Map (기본) |

### Optimization 메뉴

| 메뉴 항목 | ID | 기본 | Renderer API |
|-----------|-----|------|--------------|
| LOD | `ID_OPTIM_LOD = 8001` | ON | `SetLODEnabled(bool)` |
| Frustum Culling | `ID_OPTIM_FRUSTUM_CULL = 8002` | ON | `SetFrustumCullingEnabled(bool)` |
| Light Culling | `ID_OPTIM_LIGHT_CULL = 8003` | ON | `SetLightCullingEnabled(bool)` |
| Occlusion Culling | `ID_OPTIM_OCCLUSION_CULL = 8005` | — | Phase 32 구현 예정 (8004는 MipMap 토글 사용 중) |

### 렌더 파이프라인 실행 순서

1. Scene Graph 순회 → AABB + 월드 행렬 수집
2. Frustum Culling → 시야 밖 오브젝트 제외
3. Occlusion Culling → 가려진 오브젝트 제외 (현재 P0 스텁)
4. LOD 선택 → 카메라 거리 기준 (전환: 2×/6× sceneDiagonal)
5. Light Culling → Frustum 밖/저기여 광원 제외
6. Instance Batching → 동일 Mesh+Material 그룹핑
7. Texture Streaming → 가시성+거리 기반 Mip 레벨 업데이트
8. CB 갱신 → Dirty Flag 체크, 256바이트 정렬 링 버퍼 풀에서 슬롯 할당
9. Material 정렬 → PSO 상태 변경 최소화
10. Opaque Front-to-Back 정렬 → Early-Z rejection 극대화
11. Shadow Depth Pass → 그림자 생성 광원별 depth-only 렌더링
12. Main Pass → Opaque → Alpha Mask → Alpha Blend (back-to-front)

### RRScenePreprocessor (Phase 35 예정)

glTF/GLB/FBX 씬을 엔진 전용 바이너리(`.rrscene`)로 변환하여 이후 로딩 시 GPU 업로드만 수행한다.

- `src/Asset/ScenePreprocessor.h/.cpp` — 전처리 파이프라인 (CLI + 엔진 공유)
  - `Generate(sourcePath, outputPath)`: 동기 전처리 (CLI용)
  - `GenerateAsync(sourcePath)`: 백그라운드 실행 `std::future<bool>` (엔진 내 자동 생성)
  - 파이프라인: Assimp → Vertex/Index + Tangent → 프리미티브 분리 → Mesh AABB → Auto-LOD(QEM) → 이미지 디코딩 → Mip chain → 직렬화
- `src/Asset/RRSceneFormat.h` — `.rrscene` 포맷 (magic "RRSC", version, sourceHash, 섹션 테이블)
- **고속 경로**: `.rrscene` 존재 + sourceHash 일치 → 바이너리 직접 읽기 (1~3초)
- **표준 경로**: `.rrscene` 없거나 해시 불일치 → Assimp 파싱 후 `GenerateAsync()` 백그라운드 생성

---

## Phase 03: 고급 렌더링 기법 (Phase 32~48)

상세 설계: `PLAN.md` / 구현 프롬프트: `PROMPT.md`

> **Backup 정책**: `Phase 01 Backup/` 및 `Phase 02 Backup/` 폴더 안의 파일은 절대 참조하거나 수정하지 않는다. 어떠한 작업에서도 이 폴더들을 건드리지 않는다.

**Phase 32 완료**: Occlusion Culling (Hi-Z GPU + Compute Shader 인프라)
- Hi-Z Buffer (R32_FLOAT 멀티밉, MAX 필터 다운샘플) + GPU Occlusion Test Compute Shader
- Depth Buffer 포맷 변경: D24_UNORM_S8_UINT → R32_TYPELESS (DSV=D32_FLOAT, SRV=R32_FLOAT)
- D3D12ComputePipeline 클래스 신규 추가 (HiZDownsample.hlsl, OcclusionTest.hlsl)
- 1-frame latency 설계: Frame N GPU dispatch → Frame N+1 CPU readback 반영
- Optimization 메뉴 ID_OPTIM_OCCLUSION_CULL(8005) 연동 완료

**Phase 33 Part B 완료**: CSM (Cascaded Shadow Maps)
- Practical Split Scheme(λ=0.5) 3-cascade 분할, cascade별 frustum AABB → OrthographicOffCenter 투영
- ShadowConstants 확장: csmEnabled, cascadeSplitDepths, csmDebugView, cameraForward 추가 (560 bytes)
- HLSL: GetCascadeIndex / CalcShadowCSM / cascade 디버그 컬러 뷰 (red/green/blue)
- Optimization 메뉴 ID_OPTIM_CSM(8006) / ID_OPTIM_CSM_DEBUG(8007) 연동
- 유닛 테스트 12개 (tests/unit/test_CSM.cpp) — Debug/Release 모두 통과

**다음 구현 대상**: Phase 33 Part A (Point Light Cube Map Shadow) 또는 Part C (PCSS)
