# PROMPT: 단계별 구현 프롬프트

각 Phase를 구현할 때 아래 프롬프트를 순서대로 사용한다.

---

## Prompt 1: 프로젝트 기반 구축

```
PRD.md와 PLAN.md를 참조하여 Phase 1을 구현하라.

1. VS2022 솔루션이 이미 생성되어 있다 (RealtimeRenderingEngine.sln).
   RREngine.vcxproj에 필요한 소스 파일을 추가하라.
   - C++17, x64, D3D12 링크 (d3d12.lib, dxgi.lib, d3dcompiler.lib, dxguid.lib)
   - 프로젝트 설정은 이미 vcxproj에 구성되어 있다

2. src/Core/Types.h를 확장한다.
   - 기본 타입 별칭은 이미 정의됨 (uint8, uint32 등)
   - DirectXMath 타입 별칭도 이미 정의됨 (Vector3, Vector4, Matrix4x4)

3. src/Platform/Win32/Win32Window.h/.cpp를 만든다.
   - Win32Window 클래스: 생성자에서 윈도우 클래스 등록 및 CreateWindowEx 호출
   - Initialize(width, height, title) → HWND 반환
   - ProcessMessages() → PeekMessage 기반 non-blocking 처리
   - IsRunning() → 윈도우 활성 여부
   - GetWidth(), GetHeight() → 현재 클라이언트 영역 크기
   - GetHWND() → 윈도우 핸들 (D3D12 SwapChain 생성에 필요)
   - WM_SIZE 처리: 리사이즈 시 내부 크기 갱신 + 리사이즈 콜백
   - WM_DESTROY 처리: PostQuitMessage
   - 윈도우 스타일은 WS_OVERLAPPEDWINDOW로 자유 리사이즈 허용
   - SetWindowed(uint32 width, uint32 height):
     윈도우 모드로 전환, AdjustWindowRect로 클라이언트 영역 정확히 맞춤,
     SetWindowLong + SetWindowPos로 크기 변경
   - SetFullscreen():
     현재 윈도우 위치/크기를 저장, WS_POPUP 스타일로 변경,
     모니터 전체 크기로 SetWindowPos (Borderless Windowed 방식)
   - IsFullscreen() → bool
   - Esc 키 입력 시 풀스크린이면 이전 윈도우 모드로 복귀

4. src/Core/Engine.h/.cpp를 만든다.
   - Engine 클래스: Initialize(), Run(), Shutdown()
   - Run()은 while 루프: ProcessMessages → Update → Render
   - deltaTime 계산 (QueryPerformanceCounter 사용)

5. src/main.cpp를 업데이트한다.
   - WinMain 엔트리 포인트
   - Engine 인스턴스 생성 → Initialize → Run → Shutdown

RREngine.vcxproj에 새 소스/헤더 파일을 ItemGroup에 추가하고,
빌드하여 빈 윈도우가 뜨고 마우스로 자유롭게 리사이즈되는지 확인하라.
```

---

## Prompt 2: DirectXMath 유틸리티

```
PRD.md와 PLAN.md를 참조하여 Phase 2를 구현하라.

1. src/Math/MathUtil.h를 만든다.
   - namespace RRE::Math
   - DirectXMath를 사용한 편의 함수:
     - LoadVector3(const XMFLOAT3&) → XMVECTOR
     - StoreVector3(XMVECTOR) → XMFLOAT3
     - LoadVector4(const XMFLOAT4&) → XMVECTOR
     - StoreVector4(XMVECTOR) → XMFLOAT4
     - CreateTRSMatrix(position, rotationEuler, scale) → XMMATRIX
       (Translation × RotationZ × RotationX × RotationY × Scale)
     - NearEqual(float a, float b, float epsilon = 1e-5f) → bool
     - NearEqualVector3(XMFLOAT3, XMFLOAT3, float epsilon) → bool
     - NearEqualMatrix(XMFLOAT4X4, XMFLOAT4X4, float epsilon) → bool
   - 모든 함수는 inline 또는 constexpr

2. tests/unit/test_MathUtil.cpp를 만든다.
   - 항등행렬 × 벡터 = 원본 벡터
   - RotationY(π/2) 후 (1,0,0)이 (0,0,-1)로 변환되는지
   - TRS 행렬 생성: Translation(1,2,3) + 무회전 + Scale(1,1,1) → (1,2,3) 위치
   - 행렬 곱셈 결합법칙: (A×B)×C = A×(B×C)
   - NearEqual 유틸리티 검증

RREngineTests.vcxproj에 테스트 파일 추가하고,
모든 유닛 테스트가 통과하는지 확인하라.
```

---

## Prompt 3: RHI 추상화 + DirectX 12 백엔드

```
PRD.md와 PLAN.md를 참조하여 Phase 3를 구현하라.

1. src/RHI/RHIDevice.h를 만든다.
   - class IRHIDevice (순수 가상 클래스)
   - virtual bool Initialize(HWND windowHandle, uint32 width, uint32 height) = 0
   - virtual void Shutdown() = 0
   - virtual void OnResize(uint32 width, uint32 height) = 0
   - virtual IRHIContext* GetContext() = 0

2. src/RHI/RHIBuffer.h를 만든다.
   - class IRHIBuffer (순수 가상 클래스)
   - virtual void SetData(const void* data, uint32 size, uint32 stride) = 0
   - virtual uint32 GetSize() = 0
   - virtual uint32 GetStride() = 0

3. src/RHI/RHIContext.h를 만든다.
   - class IRHIContext (순수 가상 클래스)
   - virtual void BeginFrame() = 0
   - virtual void EndFrame() = 0
   - virtual void Clear(const XMFLOAT4& color) = 0
   - virtual void DrawPrimitives(IRHIBuffer* vb, IRHIBuffer* ib,
     const XMFLOAT4X4& worldMatrix) = 0
   - virtual void DrawText(int x, int y, const char* text,
     const XMFLOAT4& color) = 0

4. src/RHI/D3D12/D3D12Device.h/.cpp를 만든다.
   - IRHIDevice 구현
   - IDXGIFactory6로 하드웨어 어댑터 열거
   - ID3D12Device 생성
   - Debug Layer 활성화 (_DEBUG 빌드)
   - D3D12Context, D3D12SwapChain 소유

5. src/RHI/D3D12/D3D12SwapChain.h/.cpp를 만든다.
   - IDXGISwapChain4 래핑
   - 더블 버퍼링 (DXGI_SWAP_CHAIN_DESC1)
   - RTV 생성 (ID3D12DescriptorHeap)
   - ResizeBuffers 처리
   - Present 호출

6. src/RHI/D3D12/D3D12Context.h/.cpp를 만든다.
   - ID3D12CommandQueue (DIRECT)
   - ID3D12CommandAllocator + ID3D12GraphicsCommandList
   - ID3D12Fence + HANDLE fenceEvent (GPU 동기화)
   - BeginFrame: CommandAllocator Reset, CommandList Reset
   - EndFrame: CommandList Close, ExecuteCommandLists, Present, WaitForGPU
   - Clear: Resource Barrier (PRESENT→RENDER_TARGET), ClearRenderTargetView
   - Viewport, ScissorRect 설정

7. src/RHI/D3D12/D3D12DescriptorHeap.h/.cpp를 만든다.
   - 범용 디스크립터 힙 관리 클래스
   - Allocate() → D3D12_CPU_DESCRIPTOR_HANDLE

8. Engine에 D3D12 RHI를 연결한다.
   - Engine::Initialize에서 D3D12Device 생성
   - 윈도우 리사이즈 콜백에서 OnResize 호출
   - Engine::Render에서 BeginFrame → Clear(코발트 블루) → EndFrame

9. tests/smoke/test_RHIBackend.cpp를 만든다.
   - D3D12Device 초기화 → Clear → EndFrame → Shutdown 사이클 테스트
   (헤드리스 환경에서는 WARP 어댑터 사용)

빌드하여 윈도우에 코발트 블루 배경이 표시되고,
리사이즈 시에도 정상 동작하는지 확인하라.
```

---

## Prompt 4: Vertex 데이터 구조 & Mesh

```
PRD.md와 PLAN.md를 참조하여 Phase 4를 구현하라.

1. src/Renderer/Vertex.h를 만든다.
   - struct Vertex { XMFLOAT3 position; XMFLOAT4 color; XMFLOAT3 normal; }
   - D3D12_INPUT_ELEMENT_DESC 배열 정의:
     { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, ... }
     { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, ... }
     { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, ... }

2. src/Renderer/Mesh.h/.cpp를 만든다.
   - class Mesh
   - std::vector<Vertex> vertices
   - std::vector<uint32> indices
   - GetPolygonCount() → indices.size() / 3

3. src/Renderer/FaceColorPalette.h를 만든다.
   - namespace RRE
   - 8색 팔레트를 XMFLOAT4 배열로 정의:
     Red     = {1, 0, 0, 1}
     Green   = {0, 1, 0, 1}
     Blue    = {0, 0, 1, 1}
     Cyan    = {0, 1, 1, 1}
     Magenta = {1, 0, 1, 1}
     Yellow  = {1, 1, 0, 1}
     Black   = {0, 0, 0, 1}
     White   = {1, 1, 1, 1}
   - AssignFaceColors(adjacencyList) → std::vector<uint32> (각 면의 팔레트 인덱스)
     - Greedy 그래프 컬러링: 각 면을 순회하며, 이웃 면에서 사용된 색을 제외하고
       팔레트에서 가장 낮은 인덱스의 색상을 선택
     - 8색이면 모든 3D 다면체의 면 인접 그래프를 커버 가능 (4색 정리)
   - GetColor(paletteIndex) → XMFLOAT4

4. src/Renderer/MeshFactory.h/.cpp를 만든다.
   - 각 도형 생성 시 FaceColorPalette::AssignFaceColors를 사용
   - 같은 면의 모든 Vertex에 동일한 색상 지정 (flat shading)
     → 면별로 Vertex를 중복 생성해야 함 (shared vertex가 아닌 per-face vertex)
   - 모든 도형에서 면 법선(Normal)을 계산하여 Vertex에 포함
     (flat shading이므로 면의 두 변의 외적으로 법선 계산)
   - static Mesh CreateSphere(uint32 segments = 16, uint32 rings = 16)
     - UV 구 생성, 각 패치(quad→2tri)를 하나의 면으로 취급
     - 인접 패치가 다른 색
   - static Mesh CreateTetrahedron()
     - 정사면체 (4면), 4면 모두 다른 색
   - static Mesh CreateCube()
     - 정육면체 (6면), 마주보지 않는 인접면은 다른 색
   - static Mesh CreateCylinder(uint32 segments = 16, float height = 2.0f)
     - 윗면/아랫면/옆면 스트립, 인접 면 교차 색상

4. src/RHI/D3D12/D3D12Buffer.h/.cpp를 만든다.
   - IRHIBuffer 구현
   - Upload Heap에 데이터 업로드
   - D3D12_VERTEX_BUFFER_VIEW, D3D12_INDEX_BUFFER_VIEW 반환

5. src/RHI/D3D12/D3D12PipelineState.h/.cpp를 만든다.
   - Root Signature 생성 (단일 32비트 상수 or CBV)
   - HLSL 셰이더 컴파일 (D3DCompileFromFile 또는 런타임 문자열 컴파일)
   - Pipeline State Object 생성 (Input Layout, VS, PS, 래스터라이저 설정)

6. 기본 HLSL 셰이더 (src/Shaders/BasicColor.hlsl):
   - cbuffer: World, ViewProjection matrix
   - VS 입력: position, color, normal
   - VS: position × WVP 변환, normal을 World 행렬로 변환, color + worldPos pass-through
   - PS: 입력 color 그대로 출력 (라이팅은 Phase 8에서 추가)

7. D3D12Context::DrawPrimitives를 구현한다.
   - PSO 바인딩
   - Root Signature 바인딩
   - VB/IB 바인딩
   - World 행렬을 Root Constants 또는 CBV로 전달
   - DrawIndexedInstanced 호출

9. Engine에서 MeshFactory::CreateCube()로 큐브를 만들어 렌더링한다.

10. tests/unit/test_FaceColoring.cpp를 만든다.
   - 4종 도형 각각에 대해 인접면 색상 중복 검증:
     같은 edge를 공유하는 두 면의 색상이 다른지 확인
   - 모든 면에 유효한 팔레트 색상이 지정되었는지 확인

빌드하여 인접면이 서로 다른 색인 큐브가 D3D12로 화면에 렌더링되는지 확인하라.
면 색상 유닛 테스트도 통과하는지 확인하라.
```

---

## Prompt 5: 렌더링 파이프라인 강화

```
PRD.md와 PLAN.md를 참조하여 Phase 5를 구현하라. (Phase 1~4는 이미 완료됨)

1. Depth Stencil Buffer 생성 및 관리:
   - src/RHI/D3D12/D3D12Device.h/.cpp에서 DSB 생성 로직 추가:
     - DXGI_FORMAT_D24_UNORM_S8_UINT 형식의 ID3D12Resource 생성 (Committed Resource)
     - D3D12_RESOURCE_STATE_DEPTH_WRITE 초기 상태
   - src/RHI/D3D12/D3D12DescriptorHeap.h/.cpp 확장:
     - D3D12_DESCRIPTOR_HEAP_TYPE_DSV 힙 추가 (non-shader-visible)
     - DSV 생성 (CreateDepthStencilView)
   - src/RHI/D3D12/D3D12Context.h/.cpp 수정:
     - Clear()에서 OMSetRenderTargets의 두 번째 인자에 DSV 포함
     - BeginFrame()에서 ClearDepthStencilView 호출
   - OnResize 처리:
     - 기존 DSB 리소스 해제 후 새 크기로 재생성
     - DSV 재생성

2. CBV DescriptorHeap + Upload Buffer 기반 Constant Buffer 관리:
   - src/RHI/D3D12/D3D12DescriptorHeap.h/.cpp 확장:
     - D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV 힙 추가 (shader-visible)
   - Constant Buffer 리소스 생성:
     - Upload Heap (D3D12_HEAP_TYPE_UPLOAD)에 256바이트 정렬 버퍼 생성
     - struct PerObjectConstants { XMFLOAT4X4 world; XMFLOAT4X4 viewProj; };
     - sizeof(PerObjectConstants)를 256바이트로 align: ((sz + 255) & ~255)
   - CBV 디스크립터 생성 (CreateConstantBufferView)
   - Root Signature 수정:
     - descriptor table (CBV) 를 Root Parameter로 추가
   - D3D12Context::DrawPrimitives 수정:
     - Map/Unmap으로 world, viewProj 행렬을 Constant Buffer에 업데이트
     - SetGraphicsRootDescriptorTable로 CBV 바인딩

3. HLSL 셰이더를 .cso 파일로 빌드 타임 컴파일:
   - RREngine.vcxproj에서 src/Shaders/BasicColor.hlsl 항목의 빌드 규칙을 "HLSL Compiler"로 설정
     (VS 속성 → 일반 → 항목 유형: HLSL Compiler)
   - VS/PS Shader Type, Shader Model(5.1), Entry Point, Output File 지정:
     - VS: EntryPoint=VSMain, Output=$(OutDir)Shaders\VertexShader.cso
     - PS: EntryPoint=PSMain, Output=$(OutDir)Shaders\PixelShader.cso
   - src/RHI/D3D12/D3D12PipelineState.h/.cpp 수정:
     - D3DCompileFromFile 제거
     - D3DReadFileToBlob("...VertexShader.cso", &vsBlob) 로 대체
     - D3DReadFileToBlob("...PixelShader.cso", &psBlob) 로 대체
   - HLSL Constant Buffer를 cbuffer 블록으로 정의:
     cbuffer PerObjectCB : register(b0) { matrix World; matrix ViewProj; };

4. Vertex 구조체 바이트 오프셋 빌드 타임 검증:
   - src/Renderer/Vertex.h에 static_assert 추가:
     static_assert(offsetof(Vertex, position) == 0,  "position offset mismatch");
     static_assert(offsetof(Vertex, color)    == 12, "color offset mismatch");
     static_assert(offsetof(Vertex, normal)   == 28, "normal offset mismatch");
     static_assert(sizeof(Vertex)             == 40, "Vertex size mismatch");

빌드하여 다음을 확인하라:
- Depth Stencil Buffer가 생성되고, 깊이 테스트로 은면 처리가 올바른지 확인
- .cso 파일이 빌드 시 bin/Debug/Shaders/ 에 생성되고 파이프라인이 정상 동작
- Constant Buffer를 통해 World/ViewProj 행렬이 정상 전달되어 오브젝트가 올바르게 변환
- static_assert가 컴파일 타임에 통과하여 Vertex 레이아웃이 Input Layout과 일치함을 보장
```

---

## Prompt 6: Scene Graph

```
PRD.md와 PLAN.md를 참조하여 Phase 6를 구현하라.

1. src/Scene/Transform.h/.cpp를 만든다.
   - class Transform
   - XMFLOAT3 position, rotation(오일러 각도, 라디안), scale
   - GetLocalMatrix() → MathUtil::CreateTRSMatrix 사용 → XMMATRIX 반환
   - SetPosition(), SetRotation(), SetScale()

2. src/Scene/SceneNode.h/.cpp를 만든다.
   - class SceneNode
   - Transform localTransform
   - Mesh* mesh (nullable, 렌더링 대상이 아닌 노드 가능)
   - SceneNode* parent
   - std::vector<std::unique_ptr<SceneNode>> children
   - AddChild(node), RemoveChild(node)
   - GetWorldMatrix() → 부모의 WorldMatrix × 자신의 LocalMatrix (재귀)

3. src/Scene/SceneGraph.h/.cpp를 만든다.
   - class SceneGraph
   - std::unique_ptr<SceneNode> root (루트 노드)
   - GetRoot() → 루트 노드 반환
   - Traverse(visitor) → 깊이 우선 순회, 각 노드에 visitor(node, worldMatrix) 호출
   - GetTotalPolygonCount() → 모든 Mesh의 폴리곤 수 합산

4. tests/unit/test_SceneGraph.cpp를 만든다.
   - 노드 추가/제거 테스트
   - 부모-자식 WorldMatrix 계산 검증 (DirectXMath 사용)
   - 부모 회전 시 자식 위치 변환 검증

5. tests/unit/test_Transform.cpp를 만든다.
   - TRS 행렬 생성 검증
   - 회전 행렬 적용 검증

모든 유닛 테스트가 통과하는지 확인하라.
```

---

## Prompt 7: 상태 표시 HUD

```
PRD.md와 PLAN.md를 참조하여 Phase 7를 구현하라.

1. src/Renderer/DebugHUD.h/.cpp를 만든다.
   - class DebugHUD
   - struct RenderStats { float fps; uint32 width; uint32 height;
     float aspectRatio; uint32 totalPolygons; float polygonsPerSec; }
   - Update(deltaTime, renderStats) → 통계 값 갱신
   - FPS는 일정 간격(0.5초)마다 평균을 계산하여 안정적으로 표시
   - Render(rhiContext) → 화면 왼쪽 상단에 텍스트 출력

2. D3D12Context::DrawText를 구현한다.
   - 방법 A: D2D1 + DirectWrite interop (D3D12 위에 D2D 오버레이)
     - ID2D1Factory, ID2D1RenderTarget, IDWriteFactory
     - D3D11On12Device를 통해 D2D와 D3D12 연동
   - 방법 B: (간단한 대안) GDI interop
     - IDXGISwapChain::GetBuffer → DC 획득 → TextOut
   - 고정폭 폰트(Consolas) 사용

3. 표시 항목 (각 줄):
   - FPS: 60.0
   - Resolution: 1280 x 720
   - Aspect Ratio: 16:9 (1.778)
   - Polygons: 36
   - Polygons/sec: 2,160

4. Engine에서 매 프레임 RenderStats를 수집하여 DebugHUD에 전달한다.
   - totalPolygons: SceneGraph::GetTotalPolygonCount()
   - polygonsPerSec: totalPolygons × fps

빌드하여 화면 좌상단에 상태 정보가 정상 표시되고,
윈도우 리사이즈 시 해상도/종횡비가 실시간 갱신되는지 확인하라.
```

---

## Prompt 8: 메뉴 (오브젝트 선택 + 애니메이션 제어)

```
PRD.md와 PLAN.md를 참조하여 Phase 8를 구현하라.

1. src/Platform/Win32/Win32Menu.h/.cpp를 만든다.
   - class Win32Menu
   - enum class MeshType { Sphere, Tetrahedron, Cube, Cylinder }
   - Initialize(HWND hwnd) → HMENU 생성 및 메뉴바 설정
   - 메뉴 구조:
     - "View" 메뉴
       - "800 x 450"    (ID_VIEW_800x450)
       - "960 x 540"    (ID_VIEW_960x540)       ← 기본 선택, 체크 표시
       - separator
       - "Full Screen"  (ID_VIEW_FULLSCREEN)
     - "Object" 메뉴
       - "Sphere"       (ID_OBJECT_SPHERE)
       - "Tetrahedron"  (ID_OBJECT_TETRAHEDRON)
       - "Cube"         (ID_OBJECT_CUBE)        ← 기본 선택, 체크 표시
       - "Cylinder"     (ID_OBJECT_CYLINDER)
     - "Animation" 메뉴
       - "Play"         (ID_ANIM_PLAY)          ← 기본 선택, 체크 표시
       - "Pause"        (ID_ANIM_PAUSE)
   - HandleCommand(WPARAM wParam) → 명령 처리
   - View 메뉴: 해상도 선택 시 CheckMenuRadioItem으로 체크, Full Screen은 별도 체크 토글
   - Object 메뉴: CheckMenuRadioItem으로 선택 항목 체크
   - Animation 메뉴: CheckMenuRadioItem으로 Play/Pause 체크

2. Win32Window의 WndProc에 WM_COMMAND + WM_KEYDOWN 처리를 추가한다.
   - WM_COMMAND 수신 시 Win32Menu::HandleCommand 호출
   - WM_KEYDOWN + VK_SPACE 수신 시 애니메이션 토글
   - WM_KEYDOWN + VK_ESCAPE 수신 시 풀스크린이면 윈도우 모드로 복귀
   - 콜백 (std::function)으로 Engine에 전달

3. Engine에 콜백을 연결한다.
   - OnViewModeChanged(width, height, isFullscreen) 콜백:
     Win32Window::SetWindowed(w, h) 또는 SetFullscreen() 호출
     RHI OnResize 호출로 SwapChain/RTV 갱신
     메뉴 체크 상태 갱신
   - Initialize에서 4종류 Mesh를 MeshFactory로 미리 생성
     (sphereMesh, tetrahedronMesh, cubeMesh, cylinderMesh)
   - bool isAnimating = true (기본: 재생)
   - OnMeshTypeChanged(MeshType) 콜백:
     Scene Graph의 대상 노드의 Mesh 포인터를 교체
     D3D12 버텍스/인덱스 버퍼 재업로드
   - OnAnimationToggle() 콜백:
     isAnimating = !isAnimating
     메뉴의 Play/Pause 체크 상태 갱신
   - Engine::Update에서:
     if (isAnimating) → 회전 각도 += rotationSpeed * deltaTime
     else → 회전 각도 유지 (렌더링은 계속)

빌드하여 다음을 확인하라:
- View → 800 x 450 선택 시 윈도우가 해당 해상도로 변경된다
- View → 960 x 540 선택 시 윈도우가 해당 해상도로 변경된다
- View → Full Screen 선택 시 전체 화면으로 전환된다
- Esc 키로 전체 화면에서 윈도우 모드로 복귀한다
- 해상도 변경 후에도 마우스 드래그로 자유 리사이즈가 가능하다
- 메뉴에서 오브젝트를 선택하면 즉시 화면의 물체가 바뀐다
- Animation → Pause 선택 또는 Space 키로 회전이 멈춘다
- 다시 Play 또는 Space 키로 회전이 재개된다
- 멈춤 중에도 화면은 정상 렌더링된다
```

---

## Prompt 9: 포인트 광원

```
PRD.md와 PLAN.md를 참조하여 Phase 9를 구현하라.

1. src/Lighting/PointLight.h/.cpp를 만든다.
   - class PointLight
   - XMFLOAT3 position (기본값: 2.0, 3.0, -2.0)
   - XMFLOAT3 color (기본값: 1.0, 1.0, 1.0 = White)
   - float constantAttenuation = 1.0f
   - float linearAttenuation = 0.09f
   - float quadraticAttenuation = 0.032f
   - SetPosition(), GetPosition()
   - SetColor(), GetColor()
   - GetColorName() → 현재 색상의 이름 문자열 ("White", "Red" 등)

2. HLSL 셰이더를 확장한다 (src/Shaders/BasicColor.hlsl) — Per-Pixel Lighting:
   - cbuffer LightBuffer (register b1):
     float3 lightPosition, float3 lightColor, float3 cameraPosition
     float3 ambientColor (= 0.15, 0.15, 0.15)
     float Kc (= 1.0), float Kl (= 0.09), float Kq (= 0.032)
   - PS에서 픽셀 단위 Diffuse 라이팅 계산 (Per-Pixel Lighting):
     float3 lightDir = normalize(lightPosition - worldPos)
     float diff = max(dot(worldNormal, lightDir), 0.0)
     float d = length(lightPosition - worldPos)
     float attenuation = 1.0 / (Kc + Kl * d + Kq * d * d)
     float3 diffuse = diff * lightColor * attenuation
     float3 result = (ambientColor + diffuse) * faceColor.rgb
     return float4(result, faceColor.a)

3. Constant Buffer 구조체를 확장한다.
   - 기존 WVP 외에 LightPosition, LightColor, CameraPosition 추가
   - 매 프레임 Constant Buffer 업데이트

4. DebugHUD에 광원 정보 표시를 추가한다.
   - bool showLightInfo = true
   - 표시 내용 (HUD 하단에 추가):
     "Light Color: White"
     "Light Pos: (2.0, 3.0, -2.0)"
   - showLightInfo가 false이면 이 항목 숨김

5. Win32Menu에 "Light" 메뉴를 추가한다.
   - "Show Info"     (ID_LIGHT_SHOW_INFO)   ← 체크 토글, 기본: 체크됨
   - separator
   - "White"         (ID_LIGHT_WHITE)       ← 기본 선택
   - "Red"           (ID_LIGHT_RED)
   - "Green"         (ID_LIGHT_GREEN)
   - "Blue"          (ID_LIGHT_BLUE)
   - "Yellow"        (ID_LIGHT_YELLOW)
   - "Cyan"          (ID_LIGHT_CYAN)
   - "Magenta"       (ID_LIGHT_MAGENTA)
   - separator
   - "Reset Position" (ID_LIGHT_RESET_POS)

6. 키보드로 광원 위치 이동 (WM_KEYDOWN 또는 GetAsyncKeyState):
   - ← → : X축 이동 (좌/우)
   - ↑ ↓ : Z축 이동 (전/후)
   - PgUp / PgDn : Y축 이동 (상/하)
   - 이동 속도: 3.0f * deltaTime

7. Engine에 PointLight를 통합한다.
   - Engine::Initialize에서 PointLight 생성
   - Engine::Update에서 키 입력으로 광원 위치 갱신
   - Engine::Render에서 Constant Buffer에 광원 데이터 전달

빌드하여 다음을 확인하라:
- 오브젝트에 포인트 광원에 의한 라이팅이 적용된다
- 화면에 광원 색상/위치 정보가 표시된다
- Light → Show Info로 표시를 on/off 할 수 있다
- Light 메뉴에서 색상을 변경하면 라이팅 색이 바뀐다
- 방향키/PgUp/PgDn으로 광원을 이동하면 라이팅이 실시간 변한다
```

---

## Prompt 10: 카메라

```
PRD.md와 PLAN.md를 참조하여 Phase 10를 구현하라.

1. src/Scene/Camera.h/.cpp를 만든다.
   - class Camera
   - enum class ProjectionMode { Perspective, Orthographic }
   - ProjectionMode projectionMode = ProjectionMode::Perspective
   - XMFLOAT3 position (기본값: 0.0, 0.0, -5.0)
   - XMFLOAT3 lookAt (기본값: 0.0, 0.0, 0.0)
   - XMFLOAT3 up (기본값: 0.0, 1.0, 0.0)
   - float fov = XM_PIDIV4 (45도)
   - float nearPlane = 0.1f, farPlane = 100.0f
   - float orthoSize = 5.0f (Orthographic 뷰 볼륨의 절반 높이)
   - GetViewMatrix() → XMMatrixLookAtLH(position, lookAt, up)
   - GetProjectionMatrix(float aspectRatio):
     - Perspective → XMMatrixPerspectiveFovLH(fov, aspectRatio, nearPlane, farPlane)
     - Orthographic → XMMatrixOrthographicLH(orthoSize*aspectRatio*2, orthoSize*2, nearPlane, farPlane)
   - GetDirection() → normalize(lookAt - position) → XMFLOAT3 반환
   - GetFovDegrees() → XMConvertToDegrees(fov)
   - GetProjectionModeName() → "Perspective" 또는 "Orthographic"
   - SetProjectionMode(mode), SetFov(radians), SetPosition(), SetLookAt()
   - MoveForward(float distance): position과 lookAt을 카메라 방향으로 이동
   - MoveRight(float distance): 카메라 right 벡터 방향으로 position/lookAt 이동
   - MoveUp(float distance): Y축 방향으로 position/lookAt 이동
   - AdjustFov(float deltaDegrees): fov 값 조절 (10도~120도 클램프)
   - Reset(): 기본값으로 복원

2. DebugHUD에 카메라 정보 표시를 추가한다.
   - bool showCameraInfo = true
   - 표시 내용 (HUD에 추가):
     "Camera: Perspective"
     "Cam Pos: (0.0, 0.0, -5.0)"
     "Cam Dir: (0.0, 0.0, 1.0)"
     "FOV: 45.0°"
   - showCameraInfo가 false이면 이 항목 숨김

3. Win32Menu에 "Camera" 메뉴를 추가한다.
   - "Show Info"     (ID_CAMERA_SHOW_INFO)    ← 체크 토글, 기본: 체크됨
   - separator
   - "Perspective"   (ID_CAMERA_PERSPECTIVE)  ← 기본 선택, 라디오 체크
   - "Orthographic"  (ID_CAMERA_ORTHOGRAPHIC)
   - separator
   - "FOV+"          (ID_CAMERA_FOV_UP)       (Perspective에서만 유효)
   - "FOV-"          (ID_CAMERA_FOV_DOWN)     (Perspective에서만 유효)
   - separator
   - "Reset"         (ID_CAMERA_RESET)

4. 키보드로 카메라 조작 (WM_KEYDOWN 또는 GetAsyncKeyState):
   - W/S: 전진/후퇴 (Camera::MoveForward)
   - A/D: 좌/우 이동 (Camera::MoveRight)
   - Q/E: 상/하 이동 (Camera::MoveUp)
   - +/=: FOV 증가 (+5도)
   - -: FOV 감소 (-5도)
   - 이동 속도: 3.0f * deltaTime

5. Engine에서 Camera를 통합한다.
   - Engine::Initialize에서 Camera 생성
   - 기존 XMMatrixLookAtLH/XMMatrixPerspectiveFovLH 호출을
     Camera::GetViewMatrix()/GetProjectionMatrix()로 교체
   - Engine::Update에서 키 입력으로 카메라 조작
   - OnProjectionModeChanged 콜백: Camera 투영 모드 전환
   - OnCameraReset 콜백: Camera 기본값 복원

6. tests/unit/test_Camera.cpp를 만든다.
   - Perspective 모드에서 GetProjectionMatrix가 유효한 행렬 반환
   - Orthographic 모드에서 GetProjectionMatrix가 유효한 행렬 반환
   - 투영 모드 전환 테스트
   - MoveForward/MoveRight/MoveUp 후 위치 변경 확인
   - FOV 변경 (클램프 범위 검증)
   - GetDirection이 정규화된 벡터 반환

빌드하여 다음을 확인하라:
- 화면에 카메라 정보가 표시된다
- Camera → Show Info로 표시를 on/off 할 수 있다
- Camera 메뉴에서 Perspective/Orthographic을 전환하면 즉시 렌더링이 변한다
- WASD+QE로 카메라를 이동할 수 있다
- +/- 키로 FOV를 조절할 수 있다
- Camera → Reset으로 카메라가 초기 상태로 돌아간다
```

---

## Prompt 11: 통합 & 스모크 테스트

```
PRD.md와 PLAN.md를 참조하여 Phase 11를 구현하라.

1. src/Renderer/Renderer.h/.cpp를 만든다.
   - class Renderer
   - SetRHIContext(context)
   - RenderScene(sceneGraph, camera, lights)
   - 내부: SceneGraph를 순회하며, 각 노드의 WorldMatrix와 Mesh를 가져와
     RHIContext::DrawPrimitives 호출
   - Camera에서 View/Projection 행렬 획득
   - Constant Buffer로 World/View/Projection + Light + Camera 데이터 전달

2. Engine에 SceneGraph, Renderer, DebugHUD, Menu, Camera를 통합한다.
   - Engine::Initialize에서 데모 장면 구성:
     - 4종 Mesh를 MeshFactory로 미리 생성
     - 루트 노드 아래에 부모 오브젝트 배치 (기본: Cube)
     - 부모 아래에 자식 오브젝트 배치 (오프셋 위치, 같은 Mesh)
     - 메뉴 초기화 및 콜백 연결
   - Engine::Update에서:
     - 부모 큐브의 Y축 회전을 매 프레임 증가 (deltaTime 기반)
     - Camera에서 View/Projection 행렬 획득 (Camera::GetViewMatrix/GetProjectionMatrix)
     - 키보드 입력으로 카메라 위치/FOV 갱신
     - RenderStats 수집 → DebugHUD::Update 호출
   - Engine::Render에서:
     - BeginFrame → Clear
     - Renderer::RenderScene
     - DebugHUD::Render
     - EndFrame

3. tests/smoke/test_EngineInit.cpp를 만든다.
   - Engine 초기화 → SceneGraph에 노드 추가 → 1프레임 Update+Render → Shutdown
   - WARP 어댑터로 테스트 (GPU 없는 CI 환경 대응)
   - 크래시 없이 완료되는지 검증

4. 전체 빌드 및 실행하여 확인한다.
   - 윈도우가 열리고 (기본: 960x540 윈도우 모드)
   - View 메뉴에서 800x450, 960x540, Full Screen을 전환할 수 있고
   - Full Screen에서 Esc로 윈도우 모드로 복귀할 수 있고
   - 오브젝트가 Y축으로 회전하고
   - 자식 오브젝트가 부모를 따라 공전하고
   - 메뉴에서 Sphere/Tetrahedron/Cube/Cylinder를 전환할 수 있고
   - 각 도형의 인접면이 서로 다른 색상이고
   - Animation 메뉴 또는 Space 키로 회전을 시작/멈출 수 있고
   - 오브젝트에 포인트 광원 라이팅이 적용되고
   - Light 메뉴에서 색상 변경 (White/Red/Green/Blue/Yellow/Cyan/Magenta), 방향키로 위치 이동 가능하고
   - 광원 정보 표시를 on/off 할 수 있고
   - Camera 메뉴에서 Perspective/Orthographic 전환 가능하고
   - WASD+QE로 카메라 이동, +/-로 FOV 조절 가능하고
   - 카메라 정보 표시를 on/off 할 수 있고
   - 좌상단 HUD에 FPS/해상도/종횡비/폴리곤 통계 + 광원/카메라 정보가 표시되고
   - 윈도우 리사이즈 시 HUD 값이 실시간 갱신되고
   - 모든 유닛 테스트와 스모크 테스트가 통과

문제가 있으면 수정하라.
```
