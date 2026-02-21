# Realtime Rendering Engine - Architecture Document

Win32 + DirectX 12 + C++17 기반 실시간 렌더링 엔진의 전체 구조, 실행 흐름, 주요 클래스 및 함수를 설명한다.

---

## 1. 디렉토리 구조

```
src/
  main.cpp                       -- 진입점 (WinMain)
  Core/
    Engine.h / Engine.cpp        -- 메인 엔진 루프 (Initialize → Run → Shutdown)
    Types.h                      -- 공용 타입 별칭 (uint32, Vector3, Matrix4x4 등)
  Math/
    MathUtil.h                   -- DirectXMath 헬퍼 함수
  Platform/Win32/
    Win32Window.h / .cpp         -- Win32 윈도우 생성, 메시지 루프, 리사이즈/전체화면
    Win32Menu.h / .cpp           -- 메뉴바 (View, Object, Animation, Light, Camera)
  RHI/                           -- Rendering Hardware Interface (추상화 계층)
    RHIDevice.h                  -- IRHIDevice 인터페이스
    RHIBuffer.h                  -- IRHIBuffer 인터페이스
    RHIContext.h                 -- IRHIContext 인터페이스
    D3D12/                       -- DirectX 12 백엔드 구현
      D3D12Device.h / .cpp       -- 디바이스 생성, WARP 지원
      D3D12Context.h / .cpp      -- 커맨드 큐/리스트, PSO, CB, 텍스트 렌더링
      D3D12SwapChain.h / .cpp    -- 더블 버퍼 스왑체인, RTV
      D3D12Buffer.h / .cpp       -- 버텍스/인덱스 버퍼 (Upload Heap)
      D3D12PipelineState.h / .cpp -- 루트 시그니처, PSO, 셰이더 로딩
      D3D12DescriptorHeap.h / .cpp -- 디스크립터 힙 관리
  Renderer/
    Vertex.h                     -- Vertex 구조체 (position, color, normal)
    Mesh.h                       -- Mesh (vertices + indices + adjacency)
    MeshFactory.h / .cpp         -- 4종 메시 생성 (Sphere, Tetrahedron, Cube, Cylinder)
    FaceColorPalette.h           -- 8색 그래프 컬러링
    Renderer.h / .cpp            -- SceneGraph 순회 기반 렌더링
    DebugHUD.h / .cpp            -- 화면 HUD 텍스트 (FPS, 해상도, 광원/카메라 정보)
  Scene/
    Transform.h / .cpp           -- 위치/회전/스케일 → 로컬 행렬
    SceneNode.h / .cpp           -- 트리 노드 (Transform + Mesh 참조)
    SceneGraph.h / .cpp          -- 루트 노드 + 깊이 우선 순회
    Camera.h / .cpp              -- 뷰/투영 행렬, Perspective/Orthographic
  Lighting/
    PointLight.h                 -- 포인트 광원 (위치, 색상, 감쇠)
  Shaders/
    BasicColor.hlsl              -- 버텍스/픽셀 셰이더 (WVP 변환 + 조명)

tests/
  unit/
    test_MathUtil.cpp            -- DirectXMath 유틸리티 테스트
    test_SceneGraph.cpp          -- Scene Graph 구축/순회 테스트
    test_FaceColoring.cpp        -- 면 색상 규칙(인접면 다른 색) 테스트
    test_Camera.cpp              -- 카메라 뷰/투영 행렬 테스트
  smoke/
    test_RHIBackend.cpp          -- WARP 디바이스 초기화 스모크 테스트
    test_EngineInit.cpp          -- 엔진 통합 스모크 테스트 (SceneGraph + Renderer)
```

---

## 2. 전체 실행 흐름

### 2.1 시작 (WinMain → Engine::Initialize)

```
main.cpp: WinMain()
  └─ Engine::Initialize(params)
       ├─ Win32Window::Initialize()          -- HWND 생성
       ├─ D3D12Device::Initialize()          -- IDXGIFactory, ID3D12Device 생성
       │    ├─ D3D12Context::Initialize()    -- CommandQueue, CommandList, Fence, PSO, DSV, CBV
       │    ├─ D3D12SwapChain::Initialize()  -- IDXGISwapChain4, RTV 생성
       │    └─ D3D12Context::InitializeD2D() -- D3D11On12 + D2D1 + DirectWrite (텍스트용)
       ├─ MeshFactory::Create{Sphere|Tetrahedron|Cube|Cylinder}()  -- 4종 메시 생성
       ├─ SceneGraph 구축                    -- Root → Parent(mesh) + OrbitPivot → Child(mesh)
       ├─ Renderer::SetContext()             -- D3D12Context* 연결
       ├─ Win32Menu::Initialize()            -- 메뉴바 생성 + 콜백 등록
       ├─ PointLight 생성                    -- 기본 위치 (2, 3, -2), 흰색
       ├─ Camera 생성                        -- 기본 위치 (0, 0, -5), FOV 60도
       └─ DebugHUD 생성
```

### 2.2 메인 루프 (Engine::Run)

```
Engine::Run()
  └─ while (window.IsRunning())
       ├─ Win32Window::ProcessMessages()   -- WM_PAINT, WM_SIZE, WM_COMMAND 등
       ├─ Engine::Update(deltaTime)        -- 로직 업데이트
       └─ Engine::Render()                 -- GPU 렌더링
```

### 2.3 프레임 업데이트 (Engine::Update)

```
Engine::Update(deltaTime)
  ├─ 애니메이션 (m_isAnimating == true일 때)
  │    ├─ m_rotationAngle += 1.0 * dt       -- 부모 자전 (Y축, 시계 방향)
  │    ├─ m_orbitAngle += 0.6 * dt           -- 자식 공전 (Y축)
  │    └─ m_childRotationAngle -= 1.5 * dt   -- 자식 자전 (Y축, 반시계 방향)
  ├─ SceneNode::GetTransform().SetRotation() -- 각 노드에 회전 값 적용
  ├─ PointLight 이동 (방향키 + PgUp/PgDn)
  ├─ Camera 이동 (WASD + QE, +/- FOV)
  └─ DebugHUD::Update(deltaTime, stats)    -- FPS 계산, 통계 갱신
```

### 2.4 렌더링 (Engine::Render)

```
Engine::Render()
  ├─ D3D12Context::BeginFrame()
  │    ├─ CommandAllocator::Reset()
  │    ├─ CommandList::Reset()
  │    └─ m_drawCallIndex = 0
  │
  ├─ D3D12Context::Clear(cobaltBlue)
  │    ├─ Barrier: PRESENT → RENDER_TARGET
  │    ├─ ClearRenderTargetView
  │    ├─ ClearDepthStencilView
  │    ├─ OMSetRenderTargets (RTV + DSV)
  │    └─ RSSetViewports / RSSetScissorRects
  │
  ├─ Renderer::RenderScene(sceneGraph, camera, light, aspectRatio)
  │    ├─ Camera::GetViewMatrix() → GetProjectionMatrix() → ViewProj 행렬 계산
  │    ├─ SetViewProjection / SetLightData
  │    └─ SceneGraph::Traverse(visitor)     -- 깊이 우선 순회
  │         └─ visitor(node, worldMatrix)
  │              ├─ UploadMesh() (캐시 miss 시)
  │              ├─ XMMatrixTranspose(worldMatrix)
  │              └─ D3D12Context::DrawPrimitives(vb, ib, world)
  │
  ├─ Renderer::RenderLightIndicator()       -- 광원 위치에 작은 구 (Unlit 모드)
  │
  ├─ DebugHUD::Render(context)              -- DrawText() 호출들 → TextCommand 큐잉
  │
  └─ D3D12Context::EndFrame()
       ├─ (텍스트 없으면) Barrier: RENDER_TARGET → PRESENT
       ├─ CommandList::Close()
       ├─ ExecuteCommandLists()
       ├─ FlushTextCommands()               -- D3D11On12 + D2D 텍스트 렌더링
       ├─ SwapChain::Present(1)
       └─ WaitForGPU()                      -- Fence 동기화
```

### 2.5 종료 (Engine::Shutdown)

```
Engine::Shutdown()
  ├─ IRHIDevice::Shutdown()
  │    ├─ D3D12Context::WaitForGPU()
  │    ├─ D3D12Context::ShutdownD2D()
  │    └─ 모든 COM 리소스 해제
  └─ 모든 unique_ptr 리셋 (Renderer, SceneGraph, Mesh, Light, Camera, Menu, Window)
```

---

## 3. 핵심 클래스 상세

### 3.1 Engine (`Core/Engine.h`)

엔진의 최상위 클래스. 모든 서브시스템의 소유자(unique_ptr).

| 멤버 | 타입 | 역할 |
|------|------|------|
| `m_window` | `unique_ptr<Win32Window>` | 윈도우 관리 |
| `m_rhiDevice` | `unique_ptr<IRHIDevice>` | GPU 디바이스 (D3D12) |
| `m_renderer` | `unique_ptr<Renderer>` | 씬 렌더링 |
| `m_sceneGraph` | `unique_ptr<SceneGraph>` | 오브젝트 계층 트리 |
| `m_parentNode` | `SceneNode*` (비소유) | 부모 오브젝트 노드 |
| `m_orbitPivotNode` | `SceneNode*` (비소유) | 공전 피봇 노드 (메시 없음) |
| `m_childNode` | `SceneNode*` (비소유) | 자식 오브젝트 노드 |
| `m_pointLight` | `unique_ptr<PointLight>` | 포인트 광원 |
| `m_camera` | `unique_ptr<Camera>` | 카메라 |
| `m_debugHUD` | `unique_ptr<DebugHUD>` | HUD 오버레이 |
| `m_menu` | `unique_ptr<Win32Menu>` | 메뉴바 |

**주요 메서드:**
- `Initialize(params)` — 모든 서브시스템 생성 및 초기화
- `Run()` — 메시지 루프 + Update + Render
- `Update(dt)` — 애니메이션, 입력 처리, HUD 갱신
- `Render()` — GPU 렌더 커맨드 발행
- `OnViewModeChanged()` — 해상도/전체화면 전환
- `OnMeshTypeChanged(type)` — 메시 교체 (양쪽 노드 + 캐시 클리어)
- `OnAnimationToggle()` — Play/Pause 전환

### 3.2 RHI 추상화 계층 (`RHI/`)

```
IRHIDevice  ←── D3D12Device
  ├─ Initialize(hwnd, w, h)
  ├─ Shutdown()
  ├─ OnResize(w, h)
  └─ GetContext() → IRHIContext*

IRHIContext  ←── D3D12Context
  ├─ BeginFrame()
  ├─ EndFrame()
  ├─ Clear(color)
  ├─ DrawPrimitives(vb, ib, worldMatrix)
  └─ DrawText(x, y, text, color)

IRHIBuffer  ←── D3D12Buffer
  ├─ SetData(data, size, stride)
  ├─ GetSize()
  └─ GetStride()
```

Engine은 `IRHIDevice`/`IRHIContext`/`IRHIBuffer` 인터페이스만 참조한다.
구체 타입(D3D12*)은 초기화 시점과 Renderer 내부에서만 캐스트한다.

### 3.3 D3D12Context (`RHI/D3D12/D3D12Context.h`)

DirectX 12 렌더링의 핵심 구현체.

**GPU 파이프라인 관리:**
- `m_commandQueue` / `m_commandAllocator` / `m_commandList` — 커맨드 제출
- `m_fence` / `m_fenceValue` / `m_fenceEvent` — CPU-GPU 동기화
- `m_pipelineState` (D3D12PipelineState) — 루트 시그니처 + PSO
- `m_depthBuffer` / `m_dsvHeap` — 깊이 테스트

**Constant Buffer (다중 드로우 콜 지원):**
```
MAX_DRAW_CALLS = 16
m_constantBuffer: 256 bytes × 16 slots = 4096 bytes (Upload Heap, persistently mapped)
m_cbvHeap: CBV_SRV_UAV 디스크립터 힙 (shader-visible, 16 디스크립터)
```
`DrawPrimitives()` 호출마다:
1. `m_drawCallIndex` 번째 슬롯에 `PerObjectConstants` 복사 (`memcpy`)
2. 해당 슬롯의 CBV 디스크립터를 루트 테이블에 바인딩
3. DrawIndexedInstanced 호출
4. `m_drawCallIndex++`

**PerObjectConstants 구조 (224 bytes):**
```cpp
struct PerObjectConstants {
    XMFLOAT4X4 world;           // 64 bytes
    XMFLOAT4X4 viewProj;        // 64 bytes
    XMFLOAT3 lightPosition;     // 12 + 4(pad)
    XMFLOAT3 lightColor;        // 12 + 4(pad)
    XMFLOAT3 cameraPosition;    // 12 + 4(pad)
    XMFLOAT3 ambientColor;      // 12 + 4(pad)
    float Kc, Kl, Kq, unlit;    // 16
    XMFLOAT3 colorOverride;     // 12 + 4(pad)
};  // → 256-byte aligned slot
```

**텍스트 렌더링 (D3D11On12 Interop):**
```
DrawText() → TextCommand 큐에 추가
EndFrame()
  └─ FlushTextCommands()
       ├─ D3D11On12::AcquireWrappedResources (RT 상태 확보)
       ├─ D2D1DeviceContext::BeginDraw()
       ├─ DrawText() (DirectWrite, Consolas 14pt)
       ├─ D2D1DeviceContext::EndDraw()
       └─ D3D11On12::ReleaseWrappedResources (PRESENT 상태로 전환)
```

### 3.4 D3D12PipelineState (`RHI/D3D12/D3D12PipelineState.h`)

- `CreateRootSignature()` — CBV 1개 바인딩 (descriptor table, visibility: all)
- `LoadShaders()` — `Shaders/VertexShader.cso`, `Shaders/PixelShader.cso` 파일 로딩 (빌드 타임 컴파일)
- `CreatePipelineState()` — Input Layout(POSITION+COLOR+NORMAL), Depth Enable, Backface Culling

### 3.5 D3D12SwapChain (`RHI/D3D12/D3D12SwapChain.h`)

- 더블 버퍼링 (`BUFFER_COUNT = 2`)
- `DXGI_FORMAT_R8G8B8A8_UNORM`
- `ResizeBuffers()` — 리사이즈 시 RTV 재생성 + D2D 렌더타겟 재생성
- `Present(syncInterval)` — VSync 동기화

### 3.6 Renderer (`Renderer/Renderer.h`)

SceneGraph 순회 기반 렌더링을 수행한다.

**Mesh → VB/IB 캐시:**
```cpp
struct MeshBuffers {
    unique_ptr<IRHIBuffer> vb;
    unique_ptr<IRHIBuffer> ib;
    uint32 indexCount;
};
std::unordered_map<Mesh*, MeshBuffers> m_meshCache;
```
- `UploadMesh(mesh)` — 처음 렌더되는 메시만 D3D12Buffer로 업로드 (idempotent)
- `ClearMeshCache()` — 메시 교체 시 캐시 전체 삭제

**RenderScene 흐름:**
1. `Camera::GetViewMatrix()` × `GetProjectionMatrix()` → ViewProj 계산
2. `D3D12Context::SetViewProjection()` / `SetLightData()`
3. `SceneGraph::Traverse()` — 각 노드의 월드 행렬로 DrawPrimitives 호출

**RenderLightIndicator:**
- 광원 위치에 스케일 0.06의 작은 구를 Unlit 모드로 렌더링
- `SetUnlitMode(true, lightColor)` → DrawPrimitives → `SetUnlitMode(false)`

### 3.7 SceneGraph / SceneNode / Transform (`Scene/`)

**트리 구조:**
```
SceneGraph
  └─ m_root (SceneNode, unique_ptr)
       ├─ Parent (메시 있음, 자전)
       └─ OrbitPivot (메시 없음, 공전 속도 제어)
            └─ Child (메시 있음, 위치 오프셋 + 반대방향 자전)
```

**SceneNode:**
- `m_localTransform` (Transform) — 위치, 회전(오일러, 라디안), 스케일
- `m_mesh` (Mesh*, 비소유) — 렌더링할 메시 포인터
- `m_children` (vector<unique_ptr<SceneNode>>) — 자식 노드 소유
- `m_parent` (SceneNode*, 비소유) — 부모 참조

**Transform::GetLocalMatrix():**
```
LocalMatrix = Scale × RotationX × RotationY × RotationZ × Translation
```

**SceneGraph::Traverse(visitor):**
- 깊이 우선 순회 (DFS)
- 각 노드: `worldMatrix = localMatrix * parentWorldMatrix`
- visitor에 `(node, worldMatrix)` 전달

**월드 행렬 누적:**
```
Root (Identity)
  └─ Parent: Local(RotY) × Identity = RotY
  └─ OrbitPivot: Local(RotY_orbit) × Identity = RotY_orbit
       └─ Child: Local(Translate(3,0,0) × RotY_child) × RotY_orbit
```
→ 자식은 부모(OrbitPivot)의 회전에 의해 원점 주위를 공전

### 3.8 Camera (`Scene/Camera.h`)

| 속성 | 기본값 | 설명 |
|------|--------|------|
| `m_position` | (0, 0, -5) | 카메라 위치 |
| `m_lookAt` | (0, 0, 0) | 시선 대상 |
| `m_fov` | 60도 (XM_PI/3) | 수직 시야각 |
| `m_nearPlane` | 0.1 | 근접 클리핑 |
| `m_farPlane` | 100.0 | 원거리 클리핑 |
| `m_projectionMode` | Perspective | 투영 모드 |

**주요 메서드:**
- `GetViewMatrix()` — `XMMatrixLookAtLH(eye, target, up)`
- `GetProjectionMatrix(aspectRatio)` — Perspective: `XMMatrixPerspectiveFovLH`, Orthographic: `XMMatrixOrthographicLH`
- `MoveForward/Right/Up(distance)` — eye와 lookAt을 동시 이동
- `AdjustFov(deltaDegrees)` — 10~120도 클램프

### 3.9 PointLight (`Lighting/PointLight.h`)

| 속성 | 기본값 | 설명 |
|------|--------|------|
| `m_position` | (2, 3, -2) | 광원 위치 |
| `m_color` | (1, 1, 1) | 광원 색상 (White) |
| `m_Kc` | 1.0 | 상수 감쇠 |
| `m_Kl` | 0.045 | 선형 감쇠 |
| `m_Kq` | 0.0075 | 이차 감쇠 |

**감쇠 공식 (Pixel Shader):**
```
attenuation = 1.0 / (Kc + Kl * d + Kq * d * d)
```
여기서 `d`는 광원과 픽셀 사이의 거리.

### 3.10 Mesh / MeshFactory / FaceColorPalette (`Renderer/`)

**Vertex 구조체 (40 bytes):**
```cpp
struct Vertex {
    XMFLOAT3 position;  // offset 0,  12 bytes
    XMFLOAT4 color;     // offset 12, 16 bytes
    XMFLOAT3 normal;    // offset 28, 12 bytes
};
```

**Mesh:**
- `vertices` (vector<Vertex>) — 정점 배열
- `indices` (vector<uint32>) — 인덱스 배열 (삼각형 단위)
- `faceAdjacency` (vector<vector<uint32>>) — 면 간 인접 관계

**MeshFactory:**
- `CreateSphere(segments=16, rings=16)` — UV 구
- `CreateTetrahedron()` — 정사면체 (4면)
- `CreateCube()` — 정육면체 (12삼각형)
- `CreateCylinder(segments=16, height=2.0)` — 실린더 (측면 + 상하 캡)

각 생성 함수는:
1. 고유 정점 위치 계산
2. 면별 인접 관계(adjacency) 구축
3. `FaceColorPalette::AssignFaceColors(adjacency)` — Greedy 그래프 컬러링으로 인접면 다른 색 보장
4. Per-face 정점 중복 생성 (flat shading을 위해 면마다 고유 정점 + 법선 + 색상)

**FaceColorPalette::AssignFaceColors():**
- 8색 팔레트: Red, Green, Blue, Cyan, Magenta, Yellow, Black, White
- Greedy 알고리즘: 각 면에 대해 이웃이 사용하지 않는 최소 인덱스 색상 선택

### 3.11 DebugHUD (`Renderer/DebugHUD.h`)

- `Update(dt, stats)` — FPS를 0.5초 간격으로 평균 계산
- `Render(context)` — IRHIContext::DrawText()로 텍스트 큐잉

**표시 항목 (화면 좌상단, 초록색 Consolas 14pt):**
1. FPS
2. Resolution (WxH)
3. Aspect Ratio (16:9, 16:10, 4:3 레이블 자동 감지)
4. Polygons (전체 폴리곤 수)
5. Poly/sec (초당 폴리곤 처리 속도, M 단위)
6. [조건부] Light 색상명 + 위치
7. [조건부] Camera 투영 종류 + 위치 + 방향 + FOV

### 3.12 Win32Window (`Platform/Win32/Win32Window.h`)

- `Initialize(w, h, title, hInstance)` — WNDCLASSEXW 등록 + CreateWindowExW
- `ProcessMessages()` — PeekMessage 루프
- `WndProc()` → `HandleMessage()` — WM_SIZE, WM_COMMAND, WM_KEYDOWN, WM_DESTROY 처리
- `SetWindowed(w, h)` — 윈도우 모드 복귀 (WS_OVERLAPPEDWINDOW)
- `SetFullscreen()` — 전체 화면 (모니터 크기 + WS_POPUP), Esc 키로 복귀

**콜백 시스템:**
- `ResizeCallback` — 크기 변경 시 Engine::OnResize 호출
- `KeyCallback` — Space 키 → 애니메이션 토글

### 3.13 Win32Menu (`Platform/Win32/Win32Menu.h`)

Win32 메뉴바를 통한 사용자 인터페이스.

| 메뉴 | 항목 | 콜백 |
|------|------|------|
| View | 800x450, 960x540, Full Screen | `ViewCallback(w, h, fullscreen)` |
| Object | Sphere, Tetrahedron, Cube, Cylinder | `MeshCallback(MeshType)` |
| Animation | Play/Pause | `AnimCallback()` |
| Light | Show Info, 7색 선택, Reset Position | `LightColorCallback`, `LightToggleInfoCallback`, `LightResetCallback` |
| Camera | Show Info, Perspective/Orthographic, FOV +-5, Reset | `CameraProjectionCallback`, `CameraFovCallback`, `CameraResetCallback` |

---

## 4. HLSL 셰이더 (`Shaders/BasicColor.hlsl`)

빌드 타임에 `VertexShader.cso`와 `PixelShader.cso`로 컴파일된다 (런타임 컴파일 없음).

### 4.1 Vertex Shader (VSMain)

```hlsl
PSInput VSMain(VSInput input)
{
    float4 worldPos = mul(float4(input.position, 1.0), World);
    output.position = mul(worldPos, ViewProj);
    output.normal = normalize(mul(input.normal, (float3x3)World));
    output.color = input.color;
    return output;
}
```
- 로컬 좌표 → 월드 좌표 (World 행렬)
- 월드 좌표 → 클립 좌표 (ViewProj 행렬)
- 법선 벡터를 월드 공간으로 변환

### 4.2 Pixel Shader (PSMain)

```hlsl
float4 PSMain(PSInput input) : SV_TARGET
{
    // Unlit 모드: 단색 출력
    if (Unlit > 0.5) return float4(ColorOverride, 1.0);

    // Per-pixel 조명 계산
    float d = length(LightPosition - input.worldPos);
    float attenuation = 1.0 / (Kc + Kl * d + Kq * d * d);
    float diff = max(dot(normal, lightDir), 0.0);
    float3 result = (AmbientColor + diff * LightColor * attenuation) * faceColor;
    return float4(result, alpha);
}
```
- **Unlit 모드**: 광원 인디케이터 구에 사용 (단색, 조명 계산 없음)
- **Lit 모드**: Ambient + Diffuse 조명, 거리 감쇠 적용

---

## 5. 데이터 흐름 다이어그램

### 5.1 입력 → 상태 갱신

```
[키보드/메뉴]
    │
    ├─ Space / Menu:Animation ──→ m_isAnimating 토글
    ├─ 방향키 / PgUp/PgDn ──────→ PointLight::SetPosition()
    ├─ WASD / QE ───────────────→ Camera::MoveForward/Right/Up()
    ├─ +/- ──────────────────────→ Camera::AdjustFov()
    ├─ Menu:Object ──────────────→ Engine::OnMeshTypeChanged()
    ├─ Menu:View ────────────────→ Engine::OnViewModeChanged()
    ├─ Menu:Light ───────────────→ PointLight::SetColor() / Reset()
    └─ Menu:Camera ──────────────→ Camera::SetProjectionMode() / Reset()
```

### 5.2 CPU → GPU 데이터 전달

```
[CPU Side]                              [GPU Side]
Vertex[] + Index[]                      Vertex Buffer (Upload Heap)
  └─ D3D12Buffer::Initialize()  ──→    Index Buffer (Upload Heap)

PerObjectConstants                      Constant Buffer (Upload Heap, 256B × 16 slots)
  └─ memcpy → m_cbData  ──────────→    CBV Descriptor Table → register(b0)

TextCommand[]                           D3D11On12 → D2D1 → Back Buffer
  └─ FlushTextCommands()  ─────────→   (텍스트 오버레이)
```

---

## 6. 주요 설계 패턴

### 6.1 RHI 추상화
- 순수 가상 인터페이스(`IRHIDevice`, `IRHIContext`, `IRHIBuffer`)로 렌더링 백엔드 분리
- 새 백엔드(Vulkan 등) 추가 시 인터페이스만 구현하면 됨
- Engine은 인터페이스를 통해서만 접근

### 6.2 Mesh 캐싱
- `Renderer::m_meshCache` (unordered_map<Mesh*, MeshBuffers>)
- 처음 렌더되는 Mesh만 GPU에 업로드, 이후 캐시 히트
- 메시 타입 교체 시 `ClearMeshCache()`로 전체 클리어

### 6.3 다중 드로우 콜 Constant Buffer
- 프레임당 최대 16개 드로우 콜
- 하나의 Upload Buffer에 16 × 256B 슬롯
- 각 슬롯에 대한 CBV 디스크립터를 미리 생성
- `m_drawCallIndex`로 현재 슬롯 추적

### 6.4 콜백 기반 이벤트 처리
- Win32Menu → std::function 콜백 → Engine 메서드
- Win32Window → ResizeCallback / KeyCallback → Engine 메서드
- 느슨한 결합: 메뉴/윈도우가 Engine을 직접 참조하지 않음

### 6.5 OrbitPivot 패턴 (공전 분리)
- 부모 자전과 자식 공전을 분리하기 위해 중간 "피봇" 노드 사용
- OrbitPivot: 메시 없음, Y축 회전만 담당
- Child: OrbitPivot의 자식으로 위치 오프셋 + 독립 자전

---

## 7. 빌드 및 테스트

### 7.1 빌드
```
Visual Studio 2022 → RealtimeRenderingEngine.sln
구성: Debug|x64, Release|x64
출력: bin/Debug/ 또는 bin/Release/
```

### 7.2 링크 라이브러리
```
d3d12.lib, dxgi.lib, d3dcompiler.lib, dxguid.lib  (DirectX 12)
d3d11.lib, d2d1.lib, dwrite.lib                    (텍스트 렌더링)
```

### 7.3 테스트 (Google Test)
| 테스트 파일 | 범위 |
|-------------|------|
| `test_MathUtil.cpp` | DirectXMath 유틸리티 |
| `test_SceneGraph.cpp` | 노드 추가/제거, 순회, 월드 행렬 |
| `test_FaceColoring.cpp` | 면 색상 규칙, 인접면 충돌 검사 |
| `test_Camera.cpp` | 뷰/투영 행렬, FOV 조절 |
| `test_RHIBackend.cpp` | WARP 디바이스 초기화 |
| `test_EngineInit.cpp` | SceneGraph + Renderer 1프레임 렌더, 메시 교체, 부모 회전 영향 |

WARP 어댑터를 사용하여 GPU 없이도 D3D12 스모크 테스트 실행 가능.

---

## 8. 키 바인딩 요약

| 키 | 동작 |
|----|------|
| Space | 애니메이션 Play/Pause |
| W/A/S/D | 카메라 전후좌우 이동 |
| Q/E | 카메라 상하 이동 |
| +/- | FOV 증감 |
| 방향키 | 광원 X/Z 이동 |
| PgUp/PgDn | 광원 Y 이동 |
| Esc | 전체 화면에서 윈도우 모드 복귀 |
