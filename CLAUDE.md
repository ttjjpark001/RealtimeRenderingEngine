# CLAUDE.md — 실시간 렌더링 엔진 프로젝트 가이드

> **Phase 01 완료 및 백업 안내**
> Phase 01에서 구현한 모든 내용은 `Phase 01 Backup/` 폴더에 백업되었다.
> 해당 폴더의 파일은 참조하거나 수정하지 않는다.
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
├── RREngine (src/)              — 엔진 (Windows Application, SubSystem: Windows)
├── RREngineTests (tests/)       — 테스트 (Console Application, SubSystem: Console)
└── docs (가상 폴더)             — PRD.md, PLAN.md, PROMPT.md, CLAUDE.md
```

## 디렉토리 구조

```
src/
  Asset/        — [Phase 02] glTF 로더, Material, Texture 관리
  Core/         — Engine 메인 루프, 공용 타입 (Types.h에 DirectXMath 별칭)
  Math/         — MathUtil.h (DirectXMath 헬퍼)
  Platform/     — Win32 윈도우/입력/메뉴 (플랫폼별 분리)
  RHI/          — 렌더링 하드웨어 추상화 인터페이스 (IRHIDevice, IRHIBuffer, IRHIContext)
    D3D12/      — DirectX 12 백엔드 (Device, Context, SwapChain, Buffer, PSO, DescriptorHeap)
  Renderer/     — Vertex, Mesh, FaceColorPalette, MeshFactory, Renderer, DebugHUD, FrustumCuller, LODSelector, InstanceBatcher
  Scene/        — SceneNode, SceneGraph, Transform, Camera
  Lighting/     — Light (Directional/Point/Spot), LightManager, PointLight(Phase 01 호환)
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
- **멀티 드로우콜**: 프레임당 최대 16회 DrawPrimitives 지원. CB를 16슬롯(각 256바이트)으로 분할하고 슬롯별 CBV 디스크립터를 생성. BeginFrame에서 인덱스 리셋, DrawPrimitives마다 다음 슬롯 사용
- HLSL 셰이더: Position + Color + Normal 입력, WVP 변환 + Per-Pixel Diffuse 라이팅 + Unlit 모드 지원
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
- WorldMatrix = Local.TRS_Matrix × Parent.WorldMatrix (local * parent 순서)
- 깊이 우선 순회로 렌더링
- 데모 장면 구조: Root → Parent(원점, Y축 회전) → Child(3,0,0 오프셋, 부모 중심으로 공전)

### Renderer
- `Renderer` 클래스가 SceneGraph를 Traverse하며 각 노드의 Mesh를 DrawPrimitives로 렌더링
- Mesh→VB/IB 캐시: `std::unordered_map<Mesh*, MeshBuffers>` — 처음 만나는 Mesh는 자동 업로드
- `RenderScene()`: ViewProjection 설정 → LightData 설정 → SceneGraph 순회 → 각 노드 DrawPrimitives
- `RenderLightIndicator()`: 광원 위치에 Unlit 모드로 작은 구를 렌더링
- Engine은 Renderer를 통해 렌더링하며, 직접 VB/IB를 관리하지 않음 (광원 구 제외)

### 상태 표시 HUD (DebugHUD)
- 화면 왼쪽 상단에 렌더링 통계를 텍스트 오버레이로 표시
- 기본 표시: FPS, 해상도(WxH), 종횡비, 전체 폴리곤 수, 초당 폴리곤 처리 속도
- 광원 정보 (토글): 광원 색상명, 광원 위치
- 카메라 정보 (토글): 투영 모드, 카메라 위치/방향, FOV
- D3D11On12 + D2D1 + DirectWrite interop으로 텍스트 렌더링

### 면 색상 규칙 (Face Coloring)
- 8색 팔레트: Red, Green, Blue, Cyan, Magenta, Yellow, Black, White
- 이웃한 면(edge 공유)에는 반드시 다른 색상 적용 (Greedy 그래프 컬러링)
- `FaceColorPalette::AssignFaceColors(adjacency)`로 면별 색상 자동 결정
- 같은 면의 모든 Vertex는 동일 색상 (flat shading → per-face vertex 중복 생성)

### 화면 모드 (View Menu)
- "View" 메뉴에서 프리셋 해상도 및 전체 화면 전환
- 프리셋: 800x450 (윈도우), 960x540 (윈도우, 기본), Full Screen
- Full Screen은 Borderless Windowed 방식 (모니터 전체 크기 + WS_POPUP 스타일)
- Esc 키로 전체 화면에서 이전 윈도우 모드로 복귀
- 프리셋 변경 후에도 마우스 드래그 리사이즈는 독립적으로 유지
- Win32Window에서 SetWindowed(w, h) / SetFullscreen() 메서드로 구현
- 모드 전환 시 RHI OnResize → SwapChain ResizeBuffers → RTV 재생성

### 오브젝트 선택 메뉴
- Win32 메뉴바 "Object" 메뉴에서 표시할 3D 오브젝트를 런타임 전환
- 선택 가능: Sphere(구), Tetrahedron(정사면체), Cube(정육면체, 기본), Cylinder(실린더)
- MeshFactory로 4종 Mesh를 미리 생성, 메뉴 선택 시 Scene Graph의 Mesh 포인터만 교체
- CheckMenuRadioItem으로 현재 선택 항목 체크 표시

### 애니메이션 제어
- "Animation" 메뉴에서 Play/Pause 토글, 또는 Space 키로 토글
- 멈춤 상태: 회전 각도 유지, 렌더링은 계속 (정지 프레임)
- 기본 상태: Play (회전 애니메이션 재생)

### 광원 시스템 (Lighting)
- **Phase 01**: 장면에 1개의 포인트 광원이 존재
- 속성: 위치(XMFLOAT3), 색상(XMFLOAT3), 감쇠 계수 (constant/linear/quadratic)
- 라이팅은 Pixel Shader에서 픽셀 단위(Per-Pixel Lighting)로 계산
- 거리 기반 감쇠 수식: `attenuation = 1 / (Kc + Kl·d + Kq·d²)` (기본값: Kc=1.0, Kl=0.09, Kq=0.032)
- HLSL Pixel Shader에서 Diffuse + Ambient 라이팅 계산: `(ambient + diff * lightColor * attenuation) * faceColor`
- Constant Buffer로 매 프레임 광원 데이터를 GPU에 전달
- 화면에 광원 정보(색상명, 위치) 표시 가능, "Light" 메뉴에서 on/off 토글
- 메뉴에서 광원 색상 선택 (White/Red/Green/Blue/Yellow/Cyan/Magenta)
- 방향키 + PgUp/PgDn으로 광원 위치 실시간 이동
- 광원 인디케이터: 광원 정보 표시 시 광원 위치에 작은 구(스케일 0.06) 렌더링, Unlit 모드(광원 색상 그대로)

### 카메라 (Camera)
- Scene/Camera.h/.cpp에 위치
- 투영 모드: Perspective (기본) / Orthographic 전환 가능
- 속성: 위치(XMFLOAT3), 시선 방향(Yaw/Pitch), FOV, near/far plane
- GetViewMatrix(): XMMatrixLookAtLH로 뷰 행렬 생성
- GetProjectionMatrix(aspectRatio): 투영 모드에 따라 Perspective/Orthographic 행렬 생성
- **키보드 이동**: WASD+QE 키로 카메라 위치 이동, +/- 키로 FOV 조절
- **마우스 네비게이션 (Phase 02)**:
  - 우클릭 드래그: Yaw/Pitch 회전 (FPS 스타일 시선 제어)
  - 마우스 휠: 전진/후진 (돌리 줌)
  - 중클릭 드래그: 상하좌우 패닝 (P1)
- **Fit to Scene**: 씬 바운딩 박스를 계산하여 카메라를 씬 전체가 보이는 위치로 자동 배치
  - 바운딩 박스 중심을 lookAt 타겟으로, 대각선 길이 기반으로 적절한 거리 산출
  - 씬 로드 시 자동 호출, "Camera" 메뉴의 "Fit to Scene" 항목으로도 수동 호출
- **이동 속도 자동 조절**: 씬 바운딩 박스 크기에 비례하여 WASD/휠 이동 속도 조절 (P1)
- DebugHUD에 카메라 정보(투영 종류, 위치, 방향, FOV) 표시 가능, "Camera" 메뉴에서 on/off 토글
- 메뉴에서 투영 모드 전환, FOV 조절, Reset, Fit to Scene 가능

### Vertex 데이터
- `struct Vertex { XMFLOAT3 position; XMFLOAT4 color; XMFLOAT3 normal; }` — 연속 메모리 배치
- D3D12 Input Layout: POSITION (R32G32B32_FLOAT, offset 0) + COLOR (R32G32B32A32_FLOAT, offset 12) + NORMAL (R32G32B32_FLOAT, offset 28)
- `static_assert`로 각 멤버 오프셋과 `sizeof(Vertex) == 40`을 빌드 타임 검증
- Mesh는 Vertex 배열 + Index 배열로 구성

## 코딩 컨벤션

- **언어**: C++17
- **네이밍**: PascalCase(클래스, 메서드), camelCase(변수, 파라미터), UPPER_SNAKE_CASE(상수/매크로)
- **헤더 가드**: `#pragma once`
- **스마트 포인터**: `std::unique_ptr`로 소유권 관리, raw pointer는 비소유 참조에만 사용
- **COM 포인터**: `Microsoft::WRL::ComPtr<T>`로 D3D12/DXGI COM 객체 관리
- **네임스페이스**: `namespace RRE { }` (Realtime Rendering Engine)
- **include 순서**: 자기 헤더 → 프로젝트 헤더 → DirectX/Windows 헤더 → 표준 라이브러리
- **HRESULT 체크**: ThrowIfFailed 매크로 또는 유틸리티 함수 사용

## 링크 라이브러리

```
d3d12.lib       — DirectX 12 core
dxgi.lib        — DXGI (SwapChain, Adapter)
d3dcompiler.lib — HLSL 런타임 컴파일
dxguid.lib      — DirectX GUIDs
d3d11.lib       — D3D11On12 (HUD 텍스트 렌더링용)
d2d1.lib        — Direct2D (HUD 텍스트 렌더링용)
dwrite.lib      — DirectWrite (HUD 텍스트 렌더링용)
assimp-vc143-mt.lib — [Phase 02] Assimp (glTF/GLB 로딩)
```

## 테스트 규칙

- DirectXMath 유틸리티 변경 시: `tests/unit/test_MathUtil.cpp` 실행
- Scene Graph 변경 시: `tests/unit/test_SceneGraph.cpp` 실행
- MeshFactory/색상 변경 시: `tests/unit/test_FaceColoring.cpp` 실행
- Camera 변경 시: `tests/unit/test_Camera.cpp` 실행
- RHI/D3D12 변경 시: `tests/smoke/test_RHIBackend.cpp` 실행
- Renderer/Engine 통합 변경 시: `tests/smoke/test_EngineInit.cpp` 실행
- 전체 통합 변경 시: 모든 테스트 실행
- D3D12 스모크 테스트는 WARP 어댑터로 GPU 없이도 실행 가능

## 주요 참조 문서

- `PRD.md` — 제품 요구사항 정의
- `PLAN.md` — 구현 단계 및 프로젝트 구조
- `PROMPT.md` — 각 Phase별 구현 프롬프트

## 주의사항

- Win32 API 코드는 `Platform/Win32/`에만 위치시킨다. 다른 모듈에서 Windows.h를 직접 include하지 않는다.
- RHI 인터페이스에 D3D12 전용 타입(ID3D12Device, ComPtr 등)이 노출되면 안 된다.
- DirectXMath의 XMVECTOR/XMMATRIX는 함수 파라미터로 직접 전달 시 FXMVECTOR/CXMMATRIX 사용 규칙을 따른다.
- float 비교 시 epsilon 기반 비교를 사용한다 (XMVector3NearEqual 등).
- D3D12 리소스 해제 전 GPU 작업 완료를 반드시 대기한다 (Fence).
- **행렬을 Constant Buffer(CBV)로 GPU에 전달할 때 반드시 `XMMatrixTranspose()`로 전치한다.** DirectXMath(row-major)와 HLSL cbuffer(column-major) 간의 메모리 레이아웃 불일치로, 전치 없이 전달하면 변환이 깨져 오브젝트가 화면에 비정상적으로 표시된다.
- MSBuild로 `.vcxproj`를 직접 빌드할 때 `$(SolutionDir)`이 `.sln` 위치가 아닌 `.vcxproj` 위치로 잡힌다. 올바른 출력 경로를 위해 `/p:SolutionDir=<루트경로>\`를 명시해야 한다.

---

## Phase 02: glTF 2.0 씬 로딩 및 PBR 렌더링

### Phase 02 개요

Phase 01의 기본 렌더링 엔진 위에 glTF 2.0 씬 로딩, Material/Texture 시스템, PBR 셰이더를 추가한다. 대형 벤치마크 씬(Sponza, Bistro 등)도 로딩하여 렌더링할 수 있어야 한다.

- **glTF 로더**: Assimp 라이브러리 (vcpkg: `assimp:x64-windows`)
- **새 디렉토리**: `src/Asset/` — glTF 로더, Material, Texture 클래스
- **씬 파일 로딩**: "File" 메뉴에서 glTF/GLB 파일을 열어 씬 교체 + 카메라 네비게이션
- Phase 01 기능(vertex-color 오브젝트, 메뉴, HUD 등)은 그대로 유지

### Assimp 설치

```bash
vcpkg install assimp:x64-windows
vcpkg integrate install
```

### Asset 모듈 구조 (`src/Asset/`)

```
src/Asset/
  GLTFLoader.h/.cpp    — Assimp을 이용한 glTF/GLB 파일 로딩
                          · Mesh 데이터 추출 (position, normal, UV, tangent, index)
                          · Material 정보 추출 (PBR 파라미터 + 텍스처 경로)
                          · Scene Graph 변환 (aiNode → SceneNode 트리)
                          · 텍스처 비동기 로딩 트리거
                          · 씬 바운딩 박스 계산 (카메라 Fit to Scene 용)
  Material.h/.cpp      — PBR Material 클래스 (baseColor, metallic, roughness, normal, emissive, occlusion)
  Texture.h/.cpp       — 텍스처 로딩 및 D3D12 GPU 리소스 관리 (비동기 로딩 지원)
  TextureCache.h/.cpp  — 텍스처 중복 로딩 방지 캐시 + 폴백 텍스처 관리
  TextureStreamer.h/.cpp — Mip 레벨 기반 텍스처 스트리밍 관리
```

### 씬 파일 로딩 워크플로우

**메뉴에서 씬 열기:**
1. "File" → "Open Scene..." 선택 → Win32 `GetOpenFileName` 파일 다이얼로그 표시 (필터: `*.gltf;*.glb`)
2. 사용자가 파일 선택 → `GLTFLoader::LoadScene(filePath)` 호출
3. 기존 씬 해제: SceneGraph 초기화, Mesh/Material/Texture 캐시 클리어, GPU 리소스 해제 (Fence 대기 후)
4. 새 씬 구축: glTF 파싱 → SceneNode 트리 생성 → Material 객체 생성 → 텍스처 비동기 로딩 시작
5. 씬 바운딩 박스 계산 → 카메라 Fit to Scene 자동 실행
6. 렌더링 시작 (텍스처 로딩 완료 전에는 폴백 텍스처로 렌더링)

**드래그 앤 드롭 (P1):**
- Win32 `WM_DROPFILES` 메시지 처리 → 드롭된 파일 경로 추출 → 위 2~6 동일 흐름

**씬 교체 시 주의사항:**
- GPU 작업 완료 대기(`WaitForGPU`) 후 리소스 해제 (D3D12 리소스 lifetime 보장)
- Renderer의 `m_meshCache` 전체 클리어
- TextureCache도 클리어하여 이전 씬의 GPU 텍스처 해제

### Vertex 포맷 확장

Phase 01 Vertex: `{ XMFLOAT3 position; XMFLOAT4 color; XMFLOAT3 normal; }` (40 bytes)

Phase 02에서 UV 좌표와 탄젠트를 추가:
- `XMFLOAT2 texCoord` — 텍스처 매핑용 UV 좌표
- `XMFLOAT4 tangent` — Normal Map 적용을 위한 탄젠트 벡터 (w: handedness)
- D3D12 Input Layout, HLSL 입력 구조체, `static_assert` 모두 수정 필요

### Material 시스템

```
Material {
    // 텍스처 참조 (nullptr이면 factor만 사용)
    Texture* baseColorTexture;
    Texture* metallicRoughnessTexture;
    Texture* normalTexture;
    Texture* emissiveTexture;
    Texture* occlusionTexture;

    // Factor 값
    XMFLOAT4 baseColorFactor = {1,1,1,1};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    XMFLOAT3 emissiveFactor = {0,0,0};

    // 렌더링 상태
    AlphaMode alphaMode = Opaque;  // Opaque, Mask, Blend
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
}
```

- 각 Mesh(또는 Sub-Mesh)가 Material 포인터를 보유
- Material이 없으면 Phase 01의 vertex-color 방식으로 폴백

### Texture 시스템

- 이미지 데이터 → `ID3D12Resource` (TEXTURE2D, default heap) 생성
- Upload buffer를 통해 CPU → GPU 복사 (`UpdateSubresources` 또는 직접 copy command)
- 리소스 상태 전이: `COPY_DEST` → `PIXEL_SHADER_RESOURCE`
- SRV(Shader Resource View) 디스크립터 생성
- TextureCache: 파일 경로 기반 `std::unordered_map<string, Texture*>`으로 중복 방지
- SRGB 포맷 처리: baseColor(albedo) = `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`, normal/roughness/metallic = `DXGI_FORMAT_R8G8B8A8_UNORM` (Linear)

### 비동기 텍스처 로딩

- **목적**: 대형 씬(Sponza 등)의 수십~수백 장 텍스처를 로딩하는 동안 렌더링이 멈추지 않도록 함
- **워커 스레드**: `std::async` 또는 스레드 풀에서 이미지 디코딩(CPU 작업) 수행
- **폴백 텍스처**: 로딩 완료 전까지 1×1 white 텍스처를 바인딩하여 factor 값만으로 렌더링
- **GPU 업로드**: 이미지 디코딩 완료 후 메인 스레드(렌더 루프)에서 Upload Buffer → Default Heap 복사 및 SRV 생성
- **교체**: Material의 텍스처 포인터를 폴백 → 실제 텍스처로 원자적 교체 (렌더링 중 race condition 방지)
- **로딩 상태**: Material/Texture에 로딩 상태 플래그 (Pending, Loading, Ready)

### 광원 시스템 확장 (Phase 02)

**광원 타입:**
```
enum class LightType { Directional, Point, Spot };

struct Light {
    LightType type;
    XMFLOAT3 color;        // 광원 색상
    float intensity;        // 강도
    XMFLOAT3 position;     // Point, Spot에서 사용
    XMFLOAT3 direction;    // Directional, Spot에서 사용
    float Kc, Kl, Kq;      // Point, Spot 감쇠 (constant/linear/quadratic)
    float innerConeAngle;   // Spot 내부 원뿔각 (cos값)
    float outerConeAngle;   // Spot 외부 원뿔각 (cos값)
    bool castShadow;        // 그림자 생성 여부
};
```

**다중 광원 (최대 8개 이상):**
- `src/Lighting/` 디렉토리에 `Light.h` (공용 광원 구조체), `LightManager.h/.cpp` (광원 목록 관리)
- GPU 전달: Structured Buffer (`StructuredBuffer<LightData>`, register t5) 또는 Constant Buffer 배열 (`cbuffer LightsCB : register(b1)`)
- 활성 광원 개수를 별도 상수로 전달 (`numActiveLights`)
- 픽셀 셰이더에서 `for (i = 0; i < numActiveLights; i++)` 루프로 각 광원 기여 합산

**Spot Light 감쇠:**
- 원뿔 페이드: `spotFactor = smoothstep(outerConeAngle, innerConeAngle, dot(-lightDir, spotDirection))`
- 최종 감쇠 = 거리 감쇠 × spotFactor

### Shadow Mapping

**개요:**
- 각 그림자를 생성하는 광원에 대해 광원 시점의 Depth-only 렌더 패스를 수행하여 Shadow Map을 생성
- 라이팅 패스에서 Shadow Map을 SRV로 바인딩하여 픽셀이 그림자 안에 있는지 판정

**Shadow Map 생성 (Depth Pass):**
- Directional Light: Orthographic 투영으로 Shadow Map 렌더링
- Spot Light: Perspective 투영 (FOV = outerConeAngle × 2)으로 Shadow Map 렌더링
- Point Light: 6면 Cube Map (Omnidirectional Shadow Map), P1 우선순위
- Shadow Map 해상도: 기본 1024×1024, 설정 가능
- Depth 전용 렌더 타겟: `DXGI_FORMAT_D32_FLOAT` 또는 `D24_UNORM_S8_UINT`
- Shadow depth 셰이더: VS에서 position 변환만 수행, PS 없음 또는 최소화

**Shadow Map D3D12 리소스:**
- `ID3D12Resource` (TEXTURE2D, D32_FLOAT), DSV + SRV 동시 생성
- DSV: shadow depth 패스에서 depth write용
- SRV: 라이팅 패스에서 depth 비교용 (`DXGI_FORMAT_R32_FLOAT`로 읽기)
- 광원별 Shadow Map → 최대 8장의 Shadow Map 텍스처 (register t6~t13 또는 Texture2DArray)

**Depth Bias (Shadow Acne 방지):**
- 래스터라이저 상태에서 `DepthBias`, `DepthBiasClamp`, `SlopeScaledDepthBias` 설정
- 기본값: DepthBias=1000, SlopeScaledDepthBias=1.0 (튜닝 필요)

**Percentage Closer Filtering (PCF):**
- Shadow Map 샘플링 시 주변 텍셀을 다중 비교하여 그림자 경계를 부드럽게 함
- 커널 크기: 기본 3×3 (설정 가능, 5×5, 7×7 등)
- 구현: texel 오프셋으로 주변 N×N 샘플을 `SampleCmpLevelZero`(comparison sampler) 또는 수동 depth 비교 후 평균
- Comparison Sampler: `D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT`, `D3D12_COMPARISON_FUNC_LESS_EQUAL` (register s1)
- HLSL 구현:
  ```
  float shadow = 0;
  for (int y = -halfKernel; y <= halfKernel; y++)
      for (int x = -halfKernel; x <= halfKernel; x++)
          shadow += shadowMap.SampleCmpLevelZero(shadowSampler, uv + float2(x,y) * texelSize, depth);
  shadow /= kernelSize * kernelSize;
  ```

**렌더링 파이프라인 순서:**
1. Shadow Depth Pass (광원별): 장면을 광원 시점으로 depth-only 렌더링
2. Main Lighting Pass: Shadow Map을 SRV로 바인딩, 라이팅 + 그림자 판정
3. 그림자 영역: 해당 광원의 diffuse + specular 기여 차단, ambient는 유지

**Light-View-Projection 행렬:**
- Directional: `XMMatrixLookAtLH(lightPos, lightPos + lightDir, up)` × `XMMatrixOrthographicLH(width, height, near, far)`
- Spot: `XMMatrixLookAtLH(lightPos, lightPos + lightDir, up)` × `XMMatrixPerspectiveFovLH(fov, 1.0, near, far)`
- 이 행렬을 Constant Buffer로 shadow depth pass 및 라이팅 pass에 전달

### RHI 확장 (D3D12)

**Root Signature 확장:**
- 기존: 루트 파라미터 1개 (CBV descriptor table, register b0)
- 추가: SRV descriptor table (register t0~t4: baseColor, metallicRoughness, normal, emissive, occlusion)
- 추가: SRV for Shadow Maps (register t5~t13: light structured buffer + shadow maps)
- 추가: Static Sampler (register s0, Linear Wrap) + Comparison Sampler (register s1, PCF용)

**Descriptor Heap 확장:**
- 기존: CBV 전용 16 디스크립터
- 변경: CBV + SRV 통합 관리, 드로우콜당 CBV 1개 + Material SRV N개 + Shadow Map SRV M개
- MAX_DRAW_CALLS 제한(16)을 대형 씬 지원을 위해 확장 필요

**PSO (Pipeline State Object) 추가:**
- Phase 01 PSO (BasicColor): vertex-color 전용, 기존 유지
- Phase 02 PSO (PBR): 텍스처 바인딩 + PBR 셰이더, 확장된 Input Layout
- Shadow Depth PSO: depth-only 렌더링, color write 비활성화, depth bias 활성화
- Alpha Mask용 PSO: depth write + alpha test
- Alpha Blend용 PSO: 블렌딩 활성화, depth write 비활성화
- Double-sided용: 래스터라이저 cull mode = none

### 셰이더 확장 — Cook-Torrance BRDF

**PBR 셰이더 (새 .hlsl 파일, 예: `PBR.hlsl`):**
- Texture2D 바인딩: `t0`(albedo), `t1`(normal), `t2`(metallicRoughness), `t3`(emissive), `t4`(occlusion)
- SamplerState: `s0` (Linear Wrap)
- Vertex 입력에 TEXCOORD, TANGENT 추가

**Cook-Torrance BRDF 구현 (HLSL):**
- **법선 분포 함수 (NDF)**: GGX/Trowbridge-Reitz — `D = α² / (π · ((N·H)²·(α²-1)+1)²)` (α = roughness²)
- **기하 감쇠 함수 (G)**: Smith-Schlick GGX — `G = G1(N,V) · G1(N,L)`, `G1 = N·X / (N·X·(1-k)+k)` (k = (roughness+1)²/8)
- **프레넬 (F)**: Schlick 근사 — `F = F0 + (1-F0)·(1-V·H)^5`
  - 비금속: `F0 = 0.04`, 금속: `F0 = albedo` (metallic으로 lerp)
- **Specular**: `D × G × F / (4 · N·L · N·V)`
- **Diffuse**: Lambertian — `(1-F) · (1-metallic) · albedo / π`
- **다중 광원 루프**: `for (i = 0; i < numActiveLights; i++)` 로 각 광원 기여 합산
- **광원 타입 분기**: Directional(감쇠 없음, 평행광), Point(거리 감쇠), Spot(거리 감쇠 × 원뿔 페이드)
- **그림자 판정**: 각 광원의 Shadow Map을 PCF로 샘플링하여 shadow factor(0~1) 계산
- **최종 출력**: `Σ (shadowFactor · (diffuse + specular) · lightColor · N·L · attenuation) + ambient`

**Shadow Depth 셰이더 (별도 .hlsl, 예: `ShadowDepth.hlsl`):**
- VS: position을 Light-View-Projection으로 변환
- PS: 없음 (depth write만) 또는 Alpha Mask용 텍스처 샘플링 + clip

**텍스처-factor 폴백:**
- 텍스처가 바인딩되지 않은 채널은 Constant Buffer의 factor 값을 사용
- `hasAlbedoMap`, `hasNormalMap`, `hasMetallicRoughnessMap` 등 플래그를 CB에 포함

**기존 BasicColor.hlsl은 Phase 01 오브젝트용으로 유지 (변경 없음)**

### Renderer 확장

- **렌더 패스 순서:**
  1. Shadow Depth Pass: 그림자 생성 광원별로 장면을 depth-only 렌더링
  2. Main Pass (Opaque): PBR 셰이더 + Shadow Map SRV 바인딩
  3. Main Pass (Alpha Mask): alpha test 적용
  4. Main Pass (Alpha Blend): 블렌딩, back-to-front 정렬
- Material 기반 PSO 선택 (BasicColor vs PBR, Opaque vs Mask vs Blend vs ShadowDepth)
- 드로우콜 전 Material의 텍스처 SRV + Shadow Map SRV 바인딩
- Mesh→VB/IB 캐시에 Material 정보 추가

### SceneNode 확장

- 기존 `Mesh*` 외에 `Material*` 참조 추가 (또는 Mesh 내부에 Material 포함)
- glTF에서 가져온 노드는 자동으로 Material이 설정됨

### 대형 씬 고려사항

- MAX_DRAW_CALLS(16) → 수백~수천 드로우콜 지원으로 확장
- Constant Buffer 관리: 프레임당 동적 할당 또는 링 버퍼 방식
- Material 기반 드로우콜 정렬로 GPU 상태 변경 최소화

### 렌더링 최적화

#### Frustum Culling

- 카메라의 View-Projection 행렬로부터 View Frustum의 6개 평면(Left, Right, Top, Bottom, Near, Far) 추출
- 각 SceneNode의 AABB(Axis-Aligned Bounding Box)를 월드 공간에서 계산
- AABB vs Frustum 교차 검사: 6개 평면 모두에 대해 AABB가 완전히 바깥이면 culled
- `DirectX::BoundingFrustum` + `BoundingBox::Intersects()` 활용 (DirectXCollision.h)
- Scene Graph 순회 시 culled 노드는 DrawPrimitives 스킵
- Shadow Depth Pass에도 적용 (광원 시점 frustum 기준, P1)
- 구현 위치: `src/Renderer/` 또는 `src/Scene/`에 FrustumCuller 클래스

#### LOD (Level of Detail)

- **LOD 구조체**: Mesh별 LOD 단계(High, Medium, Low)를 배열로 보유
  ```
  struct LODMesh {
      Mesh* meshLODs[MAX_LOD_LEVELS];  // LOD 0 = 최고 디테일
      float switchDistances[MAX_LOD_LEVELS];  // 전환 거리 임계값
      uint32 lodCount;
  };
  ```
- **거리 기반 LOD 선택**: 카메라~오브젝트 거리를 계산하여 적절한 LOD 단계 선택
- **glTF LOD 매핑**: glTF에 LOD 메시가 있으면 (`MSFT_lod` 확장 등) 자동 매핑, 없으면 단일 LOD
- **폴백**: LOD 메시가 없으면 LOD 0(원본)으로 동작
- 구현 위치: `src/Renderer/LODSelector.h/.cpp`

#### Texture Streaming

- **목적**: 대형 씬에서 모든 텍스처의 최대 해상도를 동시에 GPU에 올리면 메모리 초과 → 필요한 Mip만 로드
- **Mip 요구 레벨 결정**: 카메라 거리 및 오브젝트의 화면 차지 비율(screen coverage)에 따라 필요한 Mip 레벨 계산
- **스트리밍 흐름**:
  1. 초기 로드: 하위 Mip(저해상도, 예: 64×64 이하)만 GPU에 업로드
  2. 렌더링 중: 요구 Mip 레벨 계산 → 상위 Mip이 필요하면 비동기 로딩 요청
  3. 비동기 로딩 완료: 메인 스레드에서 해당 Mip을 GPU 텍스처에 업로드 (부분 업데이트)
  4. Mip 해제: 카메라가 멀어져 상위 Mip이 불필요하면 GPU 메모리에서 해제
- **메모리 예산**: GPU 텍스처 메모리 사용량 상한 설정 (P1), 초과 시 LRU 기반 Mip 해제
- **기존 비동기 텍스처 로딩과 통합**: TextureCache가 Mip 레벨별 로딩 상태를 관리
- 구현 위치: `src/Asset/TextureStreamer.h/.cpp`

#### Mip-Mapping

- **Mip chain 생성**: 텍스처 생성 시 전체 Mip 레벨 수를 계산 (`floor(log2(max(width, height))) + 1`)
- **D3D12 텍스처 리소스**: `MipLevels` 파라미터를 전체 Mip 수 또는 Texture Streaming에서 요구하는 수로 설정
- **Mip 데이터 생성**: CPU에서 이미지 축소(box filter 등) 또는 GPU에서 compute shader로 생성
- **Sampler 설정**:
  - Trilinear: `D3D12_FILTER_MIN_MAG_MIP_LINEAR` — Mip 간 선형 보간
  - Anisotropic: `D3D12_FILTER_ANISOTROPIC`, `MaxAnisotropy = 16` — 비등방성 필터링 (기본 권장)
- 기존 Static Sampler(s0)를 Anisotropic으로 업그레이드

#### Instanced Rendering

- **목적**: 동일 Mesh + Material 조합이 여러 위치에 있을 때 단일 `DrawIndexedInstanced` 호출로 묶어 드로우콜 수 감소
- **Instance Buffer**: 인스턴스별 World Matrix를 담는 추가 Vertex Buffer (per-instance data)
  ```
  struct InstanceData {
      XMFLOAT4X4 world;  // 인스턴스별 월드 행렬 (전치 적용)
  };
  ```
- **D3D12 Input Layout 확장**: 기존 per-vertex 슬롯(slot 0) + per-instance 슬롯(slot 1)
  - slot 1: `INSTANCE_WORLD` (4×float4, `D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA`, InstanceDataStepRate=1)
- **인스턴싱 수집**: Scene Graph 순회 시 동일 Mesh+Material 조합을 그룹핑 → InstanceData 배열 생성 → Instance Buffer 업로드
- **DrawIndexedInstanced(indexCount, instanceCount, ...)**: instanceCount > 1일 때 인스턴싱 적용
- **HLSL 셰이더**: VS에서 `SV_InstanceID`를 사용하여 Instance Buffer의 World Matrix를 인덱싱
- **폴백**: 인스턴스가 1개인 경우 기존 단일 드로우콜과 동일하게 동작
- 구현 위치: `src/Renderer/InstanceBatcher.h/.cpp`

#### 렌더 파이프라인 통합 (최적화 적용 순서)

1. **Scene Graph 순회** → 각 노드의 AABB + 월드 행렬 수집
2. **Frustum Culling** → 시야 밖 오브젝트 제외
3. **LOD 선택** → 카메라 거리에 따라 적절한 LOD Mesh 결정
4. **Instance Batching** → 동일 Mesh+Material 그룹핑, Instance Buffer 생성
5. **Texture Streaming** → 요구 Mip 레벨 업데이트, 비동기 로딩 요청
6. **Material 정렬** → PSO 상태 변경 최소화를 위해 Material 기준 정렬
7. **Shadow Depth Pass** → 그림자 생성 광원별 depth-only 렌더링
8. **Main Pass** → Opaque (인스턴싱 적용) → Alpha Mask → Alpha Blend
