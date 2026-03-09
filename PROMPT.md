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

---

## Phase 02: glTF 2.0 씬 로딩 및 PBR 렌더링

> Phase 01 완료 코드는 `Phase 01 Backup/` 폴더에 백업됨. 해당 폴더는 참조/수정하지 않는다.
> Phase 02 프롬프트에서는 PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 함께 참조한다.

---

## Prompt 12: Assimp 설정 + SceneLoader 기본

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 12를 구현하라.

1. vcpkg로 Assimp을 설치한다.
   vcpkg install assimp:x64-windows
   vcpkg integrate install
   RREngine.vcxproj에 assimp-vc143-mt.lib 링크 추가.

2. src/Asset/SceneLoader.h/.cpp를 만든다.
   - class SceneLoader
   - LoadScene(const std::string& filePath) → SceneData 반환
   - SceneData 구조체:
     - std::vector<Mesh> meshes
     - std::vector<Material> materials
     - std::unique_ptr<SceneNode> rootNode
     - BoundingBox sceneBounds
     - std::optional<CameraInfo> camera (씬 파일 내 카메라)
   - 내부에서 Assimp::Importer 사용:
     - aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace
       | aiProcess_FlipUVs
   - aiMesh → Mesh 변환: position, normal, UV, tangent, index 추출
   - aiNode 계층 → SceneNode 트리 재귀 변환
   - aiCamera → CameraInfo (위치, lookAt, FOV) 추출 (없으면 std::nullopt)
   - 씬 전체 AABB(바운딩 박스) 계산

3. tests/smoke/test_SceneLoader.cpp를 만든다.
   - 테스트용 간단한 glTF 파일을 assets/ 폴더에 준비
   - LoadScene() 호출 후 meshes.size() > 0, rootNode != nullptr 확인
   - 바운딩 박스가 유효한지 확인

빌드하여 Assimp 링크가 정상이고, glTF 파일을 파싱하여
Mesh + SceneNode 트리가 생성되는지 확인하라.
```

---

## Prompt 13: Vertex 포맷 확장 + Material 시스템

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 13를 구현하라.

1. src/Renderer/Vertex.h를 확장한다.
   - struct Vertex에 추가:
     XMFLOAT2 texCoord;  // UV 좌표
     XMFLOAT4 tangent;   // 탄젠트 (w = handedness)
   - D3D12 Input Layout에 TEXCOORD, TANGENT 시맨틱 추가
   - static_assert로 새 멤버 오프셋과 sizeof(Vertex) 검증

2. src/Asset/Material.h/.cpp를 만든다.
   - class Material
   - 텍스처 참조: baseColorTexture, metallicRoughnessTexture, normalTexture,
     emissiveTexture, occlusionTexture (모두 Texture* nullable)
   - Factor 값: baseColorFactor(XMFLOAT4), metallicFactor(float),
     roughnessFactor(float), emissiveFactor(XMFLOAT3)
   - 렌더 상태: AlphaMode(Opaque/Mask/Blend), alphaCutoff, doubleSided
   - Dirty Flag: bool m_dirty = true
   - SetDirty(), IsDirty(), ClearDirty()

3. SceneLoader를 확장한다.
   - aiMaterial → Material 객체 변환
   - PBR metallic-roughness 파라미터 추출:
     AI_MATKEY_BASE_COLOR, AI_MATKEY_METALLIC_FACTOR, AI_MATKEY_ROUGHNESS_FACTOR
   - 텍스처 경로 추출 (aiTextureType_DIFFUSE, _NORMALS, _METALNESS 등)

4. SceneNode에 Material* 참조를 추가한다.
   - Material이 nullptr이면 Phase 01의 vertex-color 방식으로 폴백

5. tests/unit/test_Material.cpp를 만든다.
   - Material 기본값 검증 (baseColorFactor = {1,1,1,1}, metallic = 1.0 등)
   - Dirty flag 동작: 생성 시 dirty, ClearDirty 후 isDirty=false, Set 후 isDirty=true

빌드하여 확장된 Vertex 크기가 Input Layout과 일치하고,
Material 파라미터가 정상 추출되는지 확인하라.
```

---

## Prompt 14: Texture 시스템 + 비동기 로딩

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 14를 구현하라.

1. src/Asset/Texture.h/.cpp를 만든다.
   - class Texture
   - 이미지 데이터 → ID3D12Resource (TEXTURE2D, default heap) 생성
   - Upload Buffer를 통해 CPU → GPU 복사
   - 리소스 상태 전이: COPY_DEST → PIXEL_SHADER_RESOURCE
   - SRV 디스크립터 생성
   - SRGB 처리:
     baseColor(albedo) = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
     normal/roughness/metallic = DXGI_FORMAT_R8G8B8A8_UNORM

2. src/Asset/TextureCache.h/.cpp를 만든다.
   - std::unordered_map<std::string, std::unique_ptr<Texture>> 캐시
   - GetOrLoad(filePath, isSRGB) → Texture*
   - 동일 파일 경로면 기존 텍스처 반환 (중복 로딩 방지)
   - 폴백 텍스처: 1×1 white 텍스처 (생성자에서 미리 생성)
   - Clear(): 모든 캐시된 텍스처 해제

3. 비동기 텍스처 로딩을 구현한다.
   - 워커 스레드(std::async)에서 이미지 파일 디코딩 (stb_image 또는 WIC)
   - 디코딩 완료 후 메인 스레드에서 Upload Buffer → Default Heap 복사
   - Material의 텍스처 포인터를 폴백 → 실제 텍스처로 원자적 교체
   - 로딩 상태: Pending → Loading → Ready

4. SceneLoader에서 Material의 텍스처를 비동기 로딩 트리거한다.
   - LoadScene 시 각 Material의 텍스처 경로 → TextureCache에 비동기 요청

빌드하여 텍스처가 GPU에 정상 업로드되고 SRV로 바인딩 가능한지,
로딩 중 폴백 텍스처가 표시되는지 확인하라.
```

---

## Prompt 15: RHI 확장 (Root Signature, Descriptor Heap, PSO, CBPool)

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 15를 구현하라.

1. Root Signature를 확장한다.
   - register b0: Per-Object CB (descriptor table)
   - register b1: Per-Frame/Light CB (descriptor table)
   - register b2: Per-Material CB (descriptor table)
   - register t0~t4: Material SRV (baseColor, normal, metallicRoughness, emissive, occlusion)
   - register t5: Light Structured Buffer (또는 CB 배열)
   - register t6~t13: Shadow Maps
   - Static Sampler s0: Linear Wrap (Anisotropic, MaxAnisotropy=16)
   - Comparison Sampler s1: PCF용 (LESS_EQUAL)

2. Descriptor Heap을 확장한다.
   - CBV + SRV 통합 관리
   - MAX_DRAW_CALLS 제한(16)을 수백~수천으로 확장
   - 프레임당 디스크립터 할당 관리

3. src/RHI/D3D12/D3D12CBPool.h/.cpp를 만든다.
   - 하나의 큰 Upload Heap (기본 4MB~16MB)
   - 영구 맵핑 (Map / 비해제)
   - Allocate(uint32 size) → CBAllocation {gpuAddress, cpuPtr, offset}
     - alignedSize = (size + 255) & ~255  (256바이트 정렬 필수)
   - ResetFrame(frameIndex) — 링 버퍼 방식으로 프레임 영역 리셋
   - 더블 버퍼링: 프레임 0/1 영역을 번갈아 사용

4. PSO를 추가한다.
   - PBR PSO: 확장 Input Layout + PBR VS/PS
   - Shadow Depth PSO: depth-only, color write off, DepthBias=1000, SlopeScaledDepthBias=1.0
   - Wireframe PSO: FillMode = WIREFRAME, 단색 PS
   - Alpha Mask PSO: depth write + alpha test
   - Alpha Blend PSO: 블렌딩 ON, depth write OFF
   - Double-sided 변형: CullMode = NONE

빌드하여 확장된 Root Signature/Descriptor Heap이 정상 생성되고,
CBPool에서 256바이트 정렬 슬롯이 할당/리셋되며,
복수 PSO가 오류 없이 생성되는지 확인하라.
```

---

## Prompt 16: Cook-Torrance BRDF 셰이더 + Gamma Correction

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 16를 구현하라.

1. src/Shaders/PBR.hlsl를 만든다.
   - cbuffer PerObjectCB : register(b0) { matrix World; matrix ViewProj; ... };
   - cbuffer LightsCB : register(b1) { LightData lights[MAX_LIGHTS]; uint numActiveLights; ... };
   - cbuffer PerMaterialCB : register(b2) {
       float4 baseColorFactor; float metallicFactor; float roughnessFactor;
       float alphaCutoff; uint hasAlbedoMap; uint hasNormalMap;
       uint hasMetallicRoughnessMap; ... };
   - Texture2D: t0(albedo), t1(normal), t2(metallicRoughness), t3(emissive), t4(occlusion)
   - SamplerState: s0 (Anisotropic Wrap)

2. Cook-Torrance BRDF를 구현한다.
   - NDF (D): GGX/Trowbridge-Reitz
     α = roughness²
     D = α² / (π · ((N·H)²·(α²-1)+1)²)
   - Geometry (G): Smith-Schlick GGX
     k = (roughness+1)² / 8
     G1(N,X) = N·X / (N·X·(1-k)+k)
     G = G1(N,V) · G1(N,L)
   - Fresnel (F): Schlick 근사
     F0 = lerp(0.04, albedo, metallic)
     F = F0 + (1-F0) · (1-V·H)^5
   - Specular = D × G × F / (4 · N·L · N·V + 0.0001)
   - Diffuse = (1-F) · (1-metallic) · albedo / π

3. Normal Map 변환을 구현한다.
   - VS에서 TBN 행렬 구축: T(tangent), B(bitangent = cross(N,T)*T.w), N(normal)
   - PS에서 Normal Map 샘플 → TBN으로 월드 공간 변환

4. 텍스처-factor 폴백 분기를 구현한다.
   - if (hasAlbedoMap) albedo = tex.Sample(sampler, uv).rgb; else albedo = baseColorFactor.rgb;
   - normal, metallicRoughness 등도 동일 패턴

5. Gamma Correction을 적용한다.
   - 방법 A (권장): 렌더 타겟을 DXGI_FORMAT_R8G8B8A8_UNORM_SRGB로 설정
   - 방법 B: PS 최종 출력에 pow(color, 1.0/2.2) 적용
   - 주의: 방법 A와 B를 동시에 적용하면 이중 감마, 하나만 선택

6. 기존 BasicColor.hlsl은 변경 없이 유지한다.

빌드하여 PBR 텍스처가 적용된 오브젝트가 Cook-Torrance 라이팅으로
정상 렌더링되고, Gamma Correction이 적용되어 밝기가 자연스러운지 확인하라.
```

---

## Prompt 17: 다중 광원 시스템

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 17를 구현하라.

1. src/Lighting/Light.h를 만든다.
   - enum class LightType { Directional, Point, Spot };
   - struct Light {
       LightType type;
       XMFLOAT3 color; float intensity;
       XMFLOAT3 position;      // Point, Spot
       XMFLOAT3 direction;     // Directional, Spot
       float Kc, Kl, Kq;       // 감쇠 (Point, Spot)
       float innerConeAngle;   // Spot (cos값)
       float outerConeAngle;   // Spot (cos값)
       bool castShadow;
     };

2. src/Lighting/LightManager.h/.cpp를 만든다.
   - std::vector<Light> 관리
   - AddLight(), RemoveLight(), GetLight(), GetActiveLightCount()
   - GetLightBuffer() → GPU에 전달할 구조체 배열 반환
   - 기존 Phase 01 PointLight를 Light 구조체로 마이그레이션

3. GPU 전달 방식을 구현한다.
   - LightsCB (register b1): LightData 배열(최대 MAX_LIGHTS=16) + numActiveLights
   - 또는 StructuredBuffer<LightData> (register t5)

4. PBR.hlsl을 확장한다.
   - for (uint i = 0; i < numActiveLights; i++) { ... } 루프
   - 타입별 분기:
     Directional: 감쇠 없음, lightDir = -light.direction
     Point: 거리 감쇠, lightDir = normalize(lightPos - worldPos)
     Spot: 거리 감쇠 × smoothstep(outer, inner, dot(-lightDir, spotDir))
   - 각 광원 기여를 합산: Σ (diffuse + specular) × lightColor × attenuation

5. 기존 Phase 01 메뉴 (Light → Color/Position)와 호환을 유지한다.

빌드하여 Directional + Point + Spot 광원이 동시에 오브젝트를 비추고,
셰이더에서 합산 렌더링되는지 확인하라.
```

---

## Prompt 18: Shadow Mapping

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 18를 구현하라.

1. Shadow Map D3D12 리소스를 생성한다.
   - ID3D12Resource (TEXTURE2D, DXGI_FORMAT_D32_FLOAT), 1024×1024
   - DSV: shadow depth pass에서 depth write
   - SRV: DXGI_FORMAT_R32_FLOAT로 라이팅 pass에서 읽기
   - 광원당 1장, 최대 8장 (register t6~t13)

2. src/Shaders/ShadowDepth.hlsl를 만든다.
   - cbuffer: LightViewProj matrix
   - VS: position × LightViewProj 변환
   - PS: 없음 (depth write만) 또는 Alpha Mask용 텍스처 clip

3. Shadow Depth PSO를 사용하여 depth-only 렌더링한다.
   - Directional Light: XMMatrixOrthographicLH 투영
   - Spot Light: XMMatrixPerspectiveFovLH 투영
   - Depth Bias 설정: DepthBias=1000, SlopeScaledDepthBias=1.0

4. Light-View-Projection 행렬을 계산하여 CB로 전달한다.
   - Directional: LookAtLH(lightPos, lightPos+lightDir, up) × OrthoLH(w,h,n,f)
   - Spot: LookAtLH(lightPos, lightPos+lightDir, up) × PerspectiveFovLH(fov,1,n,f)
   - GPU 전달 시 반드시 XMMatrixTranspose() 적용

5. PBR.hlsl에 PCF를 구현한다.
   - Comparison Sampler (s1): D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT
   - 3×3 PCF 커널:
     float shadow = 0;
     for (int y = -1; y <= 1; y++)
       for (int x = -1; x <= 1; x++)
         shadow += shadowMap.SampleCmpLevelZero(shadowSampler, uv + float2(x,y)*texelSize, depth);
     shadow /= 9.0;
   - 그림자 영역: 해당 광원의 diffuse+specular 차단, ambient 유지

6. 렌더 패스 순서를 구현한다.
   - Pass 1: Shadow Depth Pass (castShadow=true인 광원별)
   - Pass 2: Main Lighting Pass (Shadow Map SRV 바인딩)

빌드하여 그림자가 정확히 생성되고, PCF로 부드러운 경계를 보이며,
shadow acne가 없는지 확인하라.
```

---

## Prompt 19: 씬 파일 로딩 UI + 카메라 네비게이션

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 19를 구현하라.

1. Win32Menu에 "File" 메뉴를 추가한다.
   - "Open Scene..." (ID_FILE_OPEN_SCENE)
   - Win32 GetOpenFileName 파일 다이얼로그:
     필터: "3D Scene Files (*.gltf;*.glb;*.fbx)\0*.gltf;*.glb;*.fbx\0All Files\0*.*\0"

2. 씬 로딩 워크플로우를 구현한다.
   - 파일 선택 → GPU 작업 완료 대기(WaitForGPU)
   - 기존 씬 해제: SceneGraph 초기화, Mesh/Material/Texture 캐시 클리어
   - SceneLoader::LoadScene(filePath) 호출
   - 새 씬 구축: SceneNode 트리 + Material + 텍스처 비동기 로딩
   - 카메라 배치:
     씬 파일에 카메라 노드가 있으면 → 해당 위치/방향으로 카메라 설정
     없으면 → Fit to Scene (바운딩 박스 기반 카메라 자동 배치)
   - 렌더링 시작 (폴백 텍스처로 즉시 렌더링)

3. Fit to Scene을 구현한다.
   - 씬 바운딩 박스의 중심을 lookAt 타겟으로 설정
   - 대각선 길이 기반 적절한 카메라 거리 산출
   - "Camera" 메뉴에 "Fit to Scene" 항목으로 수동 호출 가능

4. 드래그 앤 드롭을 구현한다. (→ Phase 32에서 구현 예정)
   - DragAcceptFiles(hwnd, TRUE) 호출
   - WM_DROPFILES → DragQueryFile로 파일 경로 추출 → 위 2와 동일 흐름

5. 카메라 마우스 네비게이션을 구현한다.
   - 우클릭 드래그 (WM_RBUTTONDOWN + WM_MOUSEMOVE):
     마우스 이동량 → Yaw/Pitch 회전 (FPS 스타일)
   - 마우스 휠 (WM_MOUSEWHEEL):
     전진/후진 (돌리 줌)
   - 중클릭 드래그 (WM_MBUTTONDOWN + WM_MOUSEMOVE): (→ Phase 32에서 구현 예정)
     상하좌우 패닝
   - 이동 속도 자동 조절: 씬 바운딩 박스 크기에 비례

6. 카메라 키보드 네비게이션을 구현한다.
   - WASD: 카메라 시선 방향 기준 전진/후퇴/좌/우 이동
   - Q/E: 월드 Y축 기준 상/하 이동
   - +/-: FOV 증가/감소 (Perspective 모드)
   - 씬 로딩 후에도 키보드 이동이 동작하도록 Engine의 키 입력 처리에 통합
   - 이동 속도: 씬 바운딩 박스 크기에 비례하여 자동 조절 (마우스와 동일)

빌드하여 메뉴에서 glTF/FBX 파일을 열어 씬이 교체되고,
카메라가 자동 배치되며, 마우스와 키보드로 네비게이션 가능한지 확인하라.
```

---

## Prompt 20: PBR 파이프라인 통합 + 렌더링 모드 선택

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 20를 구현하라.

이 Phase는 두 파트로 나뉜다:
- Part A: 이미 구현된 PBR 인프라를 Engine에 연결하여 텍스처 렌더링 활성화
- Part B: 5단계 렌더링 모드 메뉴 전환

=== Part A: PBR 파이프라인 통합 ===

현재 상태:
- PBR 인프라는 Phase 13-16에서 이미 구현됨:
  · Vertex에 texCoord(XMFLOAT2), tangent(XMFLOAT4) 포함 (64바이트)
  · PBR PSO + Root Signature (t0-t4 SRV, s0/s1 sampler) 존재
  · PBR.hlsl: Cook-Torrance BRDF + 다중 광원 + Shadow PCF + Normal Mapping 완성
  · D3D12Context::DrawPrimitivesPBR() 존재
  · Renderer::RenderScene()에서 Material이 있으면 PBR 경로 사용
  · TextureCache 클래스 존재 (Initialize, GetOrLoad, GetFallback, Clear)
- 누락: Engine에서 TextureCache를 생성/연결하지 않아 텍스처가 렌더링되지 않음

1. Engine에 TextureCache를 생성하고 Renderer에 연결한다.
   - Engine.h에 std::unique_ptr<TextureCache> m_textureCache 멤버 추가
   - Engine::Initialize()에서:
     · m_textureCache = std::make_unique<TextureCache>()
     · m_textureCache->Initialize(device, context)  // device/context는 RHI에서 획득
     · m_renderer->SetTextureCache(m_textureCache.get())

2. Engine::LoadScene()에서 텍스처를 로딩한다.
   - SceneLoader가 반환한 각 Material에 대해:
     · Material의 텍스처 경로(baseColorPath, normalPath, metallicRoughnessPath,
       emissivePath, occlusionPath)를 확인
     · 각 경로가 비어있지 않으면 m_textureCache->GetOrLoad(path, isSRGB) 호출
       - baseColor: isSRGB = true (sRGB 공간)
       - normal, metallicRoughness, occlusion: isSRGB = false (Linear 공간)
       - emissive: isSRGB = true
     · 반환된 Texture 포인터를 Material에 설정
     · 로드 실패 시 m_textureCache->GetFallback() 사용 (1×1 white)
   - 씬 교체 시: 기존 m_textureCache->Clear() 호출 후 새 텍스처 로딩

3. Alpha Mask/Blend 패스를 구현한다.
   - Renderer::RenderScene()에서 Material의 alphaMode별 렌더링을 분리:
     · Pass 1: Opaque (alphaMode == Opaque) — front-to-back 정렬
     · Pass 2: Alpha Mask (alphaMode == Mask) — PBR.hlsl에 clip(alpha - alphaCutoff)
     · Pass 3: Alpha Blend (alphaMode == Blend) — back-to-front 정렬
   - Alpha Blend PSO: 기존 PBR PSO 기반, BlendEnable=true,
     SrcBlend=SRC_ALPHA, DestBlend=INV_SRC_ALPHA, DepthWriteMask=ZERO
   - PBR.hlsl: Alpha Mask 모드일 때 baseColor.a < alphaCutoff이면 clip(-1) 추가
     (이미 구현되어 있으면 확인만)

=== Part B: 렌더링 모드 선택 ===

4. enum class RenderMode를 정의한다.
   - Wireframe, Solid, BaseColorOnly, FullPBR, FullPBRShadows

5. Win32Menu에 "Render" 메뉴를 추가한다.
   - "Wireframe"           (ID_RENDER_WIREFRAME)
   - "Solid (No Texture)"  (ID_RENDER_SOLID)
   - "Base Color Only"     (ID_RENDER_BASECOLOR)
   - "Full PBR"            (ID_RENDER_FULLPBR)
   - "Full PBR + Shadows"  (ID_RENDER_FULLPBR_SHADOWS)  ← 기본 선택
   - CheckMenuRadioItem으로 현재 선택 체크 표시

6. src/Shaders/Wireframe.hlsl를 만든다.
   - VS: position × WVP 변환
   - PS: 단색 출력 (예: float4(0.8, 0.8, 0.8, 1.0))

7. Renderer에 렌더링 모드를 적용한다.
   - SetRenderMode(RenderMode mode)
   - 모드별 동작:
     Wireframe: Wireframe PSO 사용, 라이팅/텍스처 미적용
     Solid: PBR PSO, CB에서 모든 텍스처 플래그 = 0 강제 → factor 값만으로 렌더링
     BaseColorOnly: PBR PSO, hasAlbedoMap만 원래값, 나머지 텍스처 플래그 = 0
     FullPBR: PBR PSO, 모든 텍스처 활성, Shadow Pass 스킵
     FullPBRShadows: Shadow Depth Pass + PBR PSO + Shadow Map 바인딩

8. DebugHUD에 현재 렌더링 모드 이름을 표시한다. (→ Phase 29에서 구현 예정)
   - "Render: Full PBR + Shadows" 등

=== Part C: PBR 라이팅 버그 수정 ===

9. HLSL cbuffer 배열 패킹 규칙에 의한 C++/HLSL 구조체 레이아웃 불일치를 수정한다.
   - PBR.hlsl의 LightData 구조체에서 `float _pad1[2]`를 `float2 _pad1`로 변경
   - HLSL cbuffer에서 float 배열은 각 원소가 16바이트 경계에 정렬되어
     C++ float[2](8바이트) ≠ HLSL float[2](32바이트) 크기 불일치 발생
   - 이로 인해 numActiveLights 오프셋이 밀려 0으로 읽힘 → 라이팅 미적용(검은 화면)
   - 규칙: HLSL cbuffer 내에서 스칼라 배열(float[], uint[]) 대신 벡터 타입(float2 등) 사용

=== Part D: 자동 3-포인트 라이팅 + PointLight 레거시 제거 ===

10. Phase 01의 PointLight 클래스 의존성을 완전 제거한다.
    - src/Lighting/PointLight.h 참조 제거 (vcxproj에서도 삭제)
    - Engine.h: m_pointLight, m_lightSphereMesh, m_lightSphereVB/IB 멤버 삭제
    - Engine.cpp: PointLight.h include, 생성/사용/정리 코드 전부 제거
    - Renderer.h/cpp: RenderLightIndicator() 삭제, RenderScene() 시그니처에서 PointLight* 제거
    - Win32Menu.h/cpp: "Reset Position" 항목, LightResetCallback, ID_LIGHT_RESET_POS 삭제

11. 광원 인디케이터 구 렌더링 및 방향키 광원 이동 기능을 삭제한다.
    - Engine::Render()에서 RenderLightIndicator() 호출 제거
    - Engine::Update()에서 방향키(Arrow/PgUp/PgDn) 광원 위치 이동 블록 전체 제거

12. 기본 씬에서 LightManager에 3-포인트 라이팅을 설정한다.
    - Engine::Initialize()에서 PointLight 생성 대신 3개 광원을 LightManager에 추가:
      · Key Light: pos=(2,2.5,-2), color=(1.0,0.95,0.9), intensity=12, Kl=0.027, Kq=0.005
      · Fill Light: pos=(-2.5,1.5,1.5), color=(0.8,0.85,1.0), intensity=6
      · Back Light: pos=(0,3,2.5), color=(1,1,1), intensity=8

13. 씬 로드 시 바운딩 박스 기반 3-포인트 라이팅을 자동 배치한다.
    - Engine::LoadScene()에서 기존 1-light 배치를 3-light로 교체:
      · LightManager::Clear() 후 center/radius 기반 Key/Fill/Back 배치
      · radius = sceneDiagonal * 0.5
      · 감쇠: Kl = 0.027 / (diagonal*0.1+1), Kq = 0.005 / (diagonal*0.1+1)

14. 색상 변경 메뉴 콜백을 LightManager의 모든 광원 색상 일괄 변경으로 수정한다.

15. DebugHUD 광원 정보를 LightManager에서 직접 조회하도록 수정한다.
    - LightManager 첫 번째 광원의 position과 color로 정보 표시
    - 색상명은 RGB 값 비교로 자동 산출 (White/Red/Green 등)

=== Part E: glTF/GLB 좌표계 변환 + GLB 임베딩 텍스처 로딩 ===

16. Assimp 임포트 플래그에 `aiProcess_ConvertToLeftHanded`를 추가한다.
    - 기존: `aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace`
    - 추가: `aiProcess_ConvertToLeftHanded` (= MakeLeftHanded | FlipUVs | FlipWindingOrder)
    - 이유: glTF는 우수 좌표계(RH), DirectX는 좌수 좌표계(LH)
      · MakeLeftHanded: 정점/노말/탄젠트 Z축 반전, 노드 행렬 조정, UV.y 반전
      · FlipUVs: UV.y 재반전 (MakeLeftHanded와 상쇄 → 원래 UV 유지)
      · FlipWindingOrder: CCW→CW (D3D12 FrontCounterClockwise=FALSE)

17. stbi_set_flip_vertically_on_load(false)로 설정한다.
    - glTF UV 원점(top-left)은 D3D12 텍스처 좌표와 동일
    - stbi 기본값(row 0 = top)이 D3D12와 일치하므로 반전 불필요

18. Transform에 SetLocalMatrix(XMMATRIX)를 추가한다.
    - Assimp의 aiMatrix4x4를 전치 후 직접 저장
    - TRS 분해→재합성 round-trip에서 Euler 회전 순서 불일치로 인한 기하 왜곡 방지
    - m_directMatrix (XMFLOAT4X4) + m_useDirectMatrix 플래그
    - GetLocalMatrix()에서 플래그 확인 후 직접 행렬 또는 기존 TRS 행렬 반환

19. GLB 임베딩 텍스처 로딩을 구현한다.
    - SceneData에 embeddedTextures 맵 추가 (키: "*N", 값: EmbeddedTextureData)
    - SceneLoader의 extractTexturePath에서 "*N" 경로 감지:
      · aiScene::mTextures[N]에서 데이터 추출
      · mHeight==0: compressed (PNG/JPG), 크기=mWidth 바이트
      · mHeight!=0: raw ARGB, aiTexel(b,g,r,a)→RGBA 변환
    - TextureCache에 GetOrLoadFromMemory() 추가
    - Engine::LoadScene()에서 path[0]=='*' 분기 처리

=== 검증 ===

빌드하여 다음을 확인하라:
- DamagedHelmet.glb 등 GLB 파일을 로드하면 임베딩 텍스처가 정상 표시됨
- 텍스처 좌우/상하 반전 없이 정상 매핑됨
- 모델이 앞면(front face)을 카메라 방향으로 올바르게 향함
- Duck.gltf 등 외부 텍스처 참조 glTF 파일도 정상 로드됨
- Alpha Mask/Blend 오브젝트가 올바르게 렌더링됨
- 메뉴에서 5단계 렌더링 모드를 전환할 수 있고, 각 모드별로 정상 렌더링됨
- 기존 Phase 01 데모 씬(vertex-color 큐브)은 BasicColor 경로로 여전히 정상 동작
- 기본 씬과 glTF 씬 로드 시 3-포인트 라이팅이 자동 배치됨
- PointLight 관련 레거시 기능(인디케이터 구, 방향키 이동, Reset Position)이 삭제됨
- Light 메뉴의 색상 변경 시 모든 광원에 일괄 적용됨
- MetalRoughSpheres.glb 등에서 metallic/roughness 텍스처가 정상 적용됨
- 다양한 익스포터의 normal/occlusion 텍스처가 올바르게 로드됨
```

### Part F: PBR 머티리얼 추출 파이프라인 강화

```
=== 목표 ===
Assimp이 glTF 머티리얼을 aiTextureType으로 매핑할 때 익스포터/버전별 차이에 대응하여,
metallic-roughness, normal, occlusion 텍스처와 factor 값이 누락 없이 추출되도록 한다.

=== 작업 ===

20. Metallic/Roughness factor를 항상 할당하도록 수정한다.
    - 기존: mat->Get() 성공 시에만 result에 할당 → 실패 시 Material.h 기본값에 의존
    - 변경: mat->Get() 호출 후 결과와 무관하게 항상 result->metallicFactor/roughnessFactor에 할당
    - glTF 스펙 기본값(1.0)이 Assimp 조회 실패 시에도 올바르게 적용됨

21. Normal map 텍스처에 폴백 타입을 추가한다.
    - 기존: aiTextureType_NORMALS만 조회
    - 변경: NORMALS → HEIGHT → NORMAL_CAMERA 순으로 폴백 조회
    - 일부 익스포터는 HEIGHT나 NORMAL_CAMERA로 노말맵을 저장함

22. Occlusion 텍스처 조회 순서를 수정한다.
    - 기존: LIGHTMAP → AMBIENT_OCCLUSION 순
    - 변경: AMBIENT_OCCLUSION → LIGHTMAP 순 (glTF 표준 타입 우선)

=== 검증 ===

빌드하여 다음을 확인하라:
- MetalRoughSpheres.glb에서 metallic/roughness 값이 구체별로 다르게 적용됨
- DamagedHelmet.glb의 occlusion/normal 텍스처가 정상 로드됨
- 다양한 익스포터로 생성된 glTF 파일에서 PBR 텍스처가 누락 없이 로드됨
```

### Part G: 궤도 회전 광원 + 4-광원 밸런싱

```
=== 목표 ===
PBR 효과(노멀맵, 금속성, 거칠기, AO 등)를 실시간으로 시각 검증할 수 있도록
카메라 시선 축 주변을 궤도 회전하는 포인트 라이트를 추가하고,
4-광원 체제에 맞게 전체 광원 강도를 밸런싱한다.

=== 작업 ===

23. Engine.h에 궤도 광원 멤버 변수를 추가한다.
    - `size_t m_orbitLightIndex` — LightManager 내 궤도 광원 인덱스
    - `float m_orbitLightAngle` — 현재 궤도 각도 (라디안)

24. Engine::Initialize()와 Engine::LoadScene()에서 3-포인트 라이팅 뒤에 궤도 광원을 추가한다.
    - LightType::Point, 흰색(1,1,1), intensity=6
    - 감쇠 계수는 나머지 3-포인트 라이팅과 동일

25. Engine::Update()에서 매 프레임 궤도 광원 위치를 갱신한다.
    - 궤도 중심: 카메라 위치에서 시선 방향으로 씬 대각선의 30% 지점
    - 궤도 반지름: 씬 대각선의 25%
    - 시선 벡터에 수직인 로컬 좌표축(right, up) 생성 → cos/sin으로 원형 궤도
    - 회전 속도: ~0.8 rad/s (약 8초에 1바퀴)
    - 애니메이션 토글과 독립적으로 항상 회전

26. 4-광원 체제 intensity 밸런싱:
    - Key: 12→8, Fill: 6→3, Back: 8→4, Orbit: 6
    - Initialize()와 LoadScene() 모두 동일 적용

=== 검증 ===

빌드하여 다음을 확인하라:
- DamagedHelmet.glb 로드 시 광원이 물체 주변을 회전함
- 금속/거칠기 표면에 스페큘러 하이라이트가 이동하며 변화함
- 전체 밝기가 과도하지 않고 적절함
- 카메라 이동/회전 시 궤도 광원이 자연스럽게 따라옴
```

---

## Prompt 21: 레거시 코드 정리 + 테스트 재정비

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 21을 구현하라.

=== 목표 ===
Phase 02 마이그레이션 과정에서 남은 레거시 코드를 제거하고,
테스트를 현재 API에 맞게 업데이트하여 전체 테스트 통과를 확인한다.

=== 작업 ===

1. src/Lighting/PointLight.h를 삭제한다.
   - Phase 01의 단일 포인트 라이트 클래스
   - Light struct + LightManager로 완전 대체됨
   - src/RREngine.vcxproj에서 참조가 있으면 제거

2. tests/smoke/test_EngineInit.cpp를 현재 API에 맞게 수정한다.
   - `#include "Lighting/PointLight.h"` → `#include "Lighting/LightManager.h"` + `#include "Lighting/Light.h"`
   - `RRE::PointLight light;` → `RRE::LightManager lightManager;` + 기본 광원 추가
   - `renderer.RenderScene(sceneGraph, camera, &light, aspectRatio)`
     → `renderer.RenderScene(sceneGraph, camera, aspectRatio, &lightManager)`

3. src/RREngine.vcxproj.filters를 재생성한다.
   - Phase 02에서 추가된 모든 파일을 적절한 필터에 배치:
     · Asset/ (SceneLoader, Material, Texture, TextureCache)
     · Lighting/ (Light.h, LightManager)
     · 새 셰이더 (PBR.hlsl, ShadowDepth.hlsl, Wireframe.hlsl)
     · RHI/D3D12 추가 파일

4. 전체 테스트를 빌드하고 실행한다.
   - RREngineTests 프로젝트 빌드
   - 모든 유닛 테스트 + 스모크 테스트 통과 확인
   - 실패하는 테스트가 있으면 현재 API에 맞게 수정

=== 검증 ===

빌드하여 다음을 확인하라:
- PointLight.h 파일이 삭제되고 어디에서도 참조되지 않음
- RREngineTests 프로젝트가 빌드 성공
- 모든 유닛 테스트 (MathUtil, SceneGraph, FaceColoring, Camera, Transform, Material) 통과
- 모든 스모크 테스트 (RHIBackend, EngineInit, SceneLoader, Texture, CBPool) 통과
- VS 솔루션 탐색기에서 파일이 올바른 필터에 표시됨
```

---

## Prompt 22: 초기 씬 제거 + Object/Animation 메뉴 삭제 + 프리미티브 SceneNode 분리 + Per-Mesh AABB

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 22를 구현하라.

=== A. 초기 씬 제거 + Object/Animation 메뉴 삭제 ===

1. 앱 시작 시 빈 씬이 되도록 Engine::Initialize()를 수정한다 (src/Core/Engine.cpp).
   - MeshFactory로 4종 Mesh를 생성하는 코드 제거
     (m_sphereMesh, m_tetrahedronMesh, m_cubeMesh, m_cylinderMesh)
   - 초기 SceneNode 트리 구성 제거
     (m_parentNode, m_orbitPivotNode, m_childNode 및 해당 SceneNode 생성/AddChild 코드)
   - Initialize()에서 3-포인트 광원 자동 배치 코드 제거 (씬 로드 시에만 배치)
   - Engine::Shutdown()에서 위 멤버들 초기화 코드 제거

2. Engine.h에서 관련 멤버 변수와 메서드를 제거한다.
   - 멤버 변수: m_parentNode, m_orbitPivotNode, m_childNode
   - 멤버 변수: m_sphereMesh, m_tetrahedronMesh, m_cubeMesh, m_cylinderMesh, m_currentMesh
   - 멤버 변수: m_isAnimating, m_rotationAngle, m_orbitAngle, m_childRotationAngle
   - 메서드 선언: OnMeshTypeChanged(), OnAnimationToggle()

3. Engine::Update()에서 애니메이션 갱신 코드를 제거한다 (src/Core/Engine.cpp).
   - m_isAnimating 조건 블록(m_rotationAngle, m_orbitAngle, m_childRotationAngle 갱신) 제거
   - m_parentNode, m_orbitPivotNode, m_childNode Transform 적용 코드 제거
   - OnMeshTypeChanged(), OnAnimationToggle() 메서드 정의 제거

4. Object 메뉴를 Win32Menu에서 삭제한다 (src/Platform/Win32/Win32Menu.h/.cpp).
   - Win32Menu.h: MeshType enum, MeshCallback typedef, ID_OBJECT_* 상수 제거
     SetMeshCallback(), m_meshCallback, m_objectMenu 멤버 제거
   - Win32Menu.cpp: "Object" 팝업 메뉴 생성 코드 제거
     HandleCommand()의 ID_OBJECT_SPHERE/TETRAHEDRON/CUBE/CYLINDER case 제거
   - Engine.cpp: SetMeshCallback 등록 코드 제거

5. Animation 메뉴를 Win32Menu에서 삭제한다 (src/Platform/Win32/Win32Menu.h/.cpp).
   - Win32Menu.h: AnimCallback typedef, ID_ANIM_PLAY/PAUSE 상수 제거
     SetAnimCallback(), UpdateAnimCheckMark(), m_animCallback, m_animMenu 멤버 제거
   - Win32Menu.cpp: "Animation" 팝업 메뉴 생성 코드 제거
     HandleCommand()의 ID_ANIM_PLAY/PAUSE case 제거
     UpdateAnimCheckMark() 메서드 정의 제거
   - Engine.cpp: SetAnimCallback 등록 코드, Space 키 애니메이션 토글 코드 제거
   - Engine::LoadScene()에서 m_isAnimating 관련 코드 제거

=== B. 프리미티브 → SceneNode 분리 + Per-Mesh AABB ===

6. Mesh에 AABB를 추가한다 (src/Renderer/Mesh.h).
   - DirectX::BoundingBox aabb 멤버 추가
   - #include <DirectXCollision.h>

7. SceneLoader에서 Per-Mesh AABB를 계산한다 (src/Asset/SceneLoader.cpp).
   - ConvertMesh() 끝에서 BoundingBox::CreateFromPoints()로 로컬 AABB 생성

8. SceneNode에 월드 AABB 캐싱을 추가한다 (src/Scene/SceneNode.h/.cpp).
   - BoundingBox m_worldAABB, bool m_aabbDirty = true 멤버 추가
   - GetWorldAABB(): dirty면 Mesh 로컬 AABB를 WorldMatrix로 변환 후 캐시
   - Transform 변경 시 dirty 설정

9. ProcessNode의 프리미티브 분리를 확인/강화한다 (src/Asset/SceneLoader.cpp).
   - aiNode가 여러 aiMesh를 참조할 때 각각 별도 SceneNode 자식으로 생성
   - Sponza: 단일 aiNode에 N개 aiMesh → N개 SceneNode로 분리

10. DebugHUD에 노드/메시 통계를 추가한다 (src/Renderer/DebugHUD.cpp).
    - 총 SceneNode 수, 총 Mesh 수를 HUD에 표시

11. 유닛 테스트를 작성한다.
    - AABB 계산 정확성 테스트
    - 월드 AABB: Transform 적용 후 올바른 값 반환
    - 프리미티브 분리: 여러 aiMesh를 가진 노드가 올바르게 분리되는지

=== C. 미사용 코드 완전 제거 ===

12. 사용되지 않는 파일을 삭제한다.
    - src/Renderer/MeshFactory.h 삭제
    - src/Renderer/MeshFactory.cpp 삭제
    - src/Renderer/FaceColorPalette.h 삭제
      (FaceColorPalette는 MeshFactory에서만 사용됨)
    - tests/unit/test_FaceColoring.cpp 삭제
      (MeshFactory/FaceColorPalette에 전적으로 의존하는 테스트)

13. 테스트 파일을 정리한다 (tests/smoke/test_EngineInit.cpp).
    - #include "Renderer/MeshFactory.h" 제거
    - MeshTypeChangeUpdatesSceneNodes() 테스트 삭제
      (메시 타입 전환 기능 자체가 제거됨)
    - MeshFactory를 사용하는 테스트(SceneGraphWithRendererOneCycle 등)는
      수동으로 간단한 Mesh를 생성하는 코드로 교체

14. 프로젝트 파일을 업데이트한다.
    - src/RREngine.vcxproj: MeshFactory.cpp, FaceColorPalette.h, MeshFactory.h 항목 제거
    - src/RREngine.vcxproj.filters: 동일 항목 제거
    - tests/RREngineTests.vcxproj: MeshFactory.cpp 항목 제거
    - tests/RREngineTests.vcxproj.filters: test_FaceColoring.cpp 항목 제거

빌드하여 앱 시작 시 빈 화면이 표시되고,
Object/Animation 메뉴가 없으며,
MeshFactory/FaceColorPalette 파일이 삭제되어 있고,
glTF 씬 로드 시 정상 렌더링되고,
Sponza 로딩 시 103개+ SceneNode가 분리되는지 확인하라.
잔여 테스트(MeshFactory 제외 전체)가 통과하는지 확인하라.
```

---

## Prompt 23: 렌더링 최적화 — Culling + LOD

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 23를 구현하라.

1. src/Renderer/FrustumCuller.h/.cpp를 만든다.
   - DirectX::BoundingFrustum을 View-Projection 행렬에서 생성
     BoundingFrustum::CreateFromMatrix(proj) → frustumVS.Transform(m_frustum, invView)로 월드 공간 변환
   - IsVisible(const BoundingBox& aabb) → bool (m_frustum.Intersects(aabb))
   - BoundingBox vs Frustum 6-plane 교차 검사 (BoundingBox::Intersects)
   - Scene Graph 순회 시 culled 노드는 DrawPrimitives 스킵
   - Shadow Depth Pass에도 적용 (광원 시점 frustum) — Phase 30에서 구현 예정

2. src/Renderer/OcclusionCuller.h/.cpp를 만든다.
   - P0 스텁: IsOccluded() 항상 false 반환 (보수적 판정, popping 방지)
   - Occluded 판정: CB 갱신 + Draw 모두 스킵

3. src/Renderer/LODSelector.h/.cpp를 만든다.
   - struct LODMesh { Mesh* meshLODs[3]; float switchDistances[3]; uint32 lodCount; }
   - SelectLOD(Mesh* original, float distance) → Mesh* (적절한 LOD 반환)
   - 자동 LOD 생성 (Auto-LOD): 그리드 기반 버텍스 클러스터링
     - LOD 1: 원본 삼각형 수의 ~50% 축소 (switchDistances[1] = sceneDiagonal * 0.5)
     - LOD 2: 원본 삼각형 수의 ~25% 축소 (switchDistances[2] = sceneDiagonal * 2.0)
     - std::async(std::launch::async)로 백그라운드 스레드에서 비동기 생성
     - std::atomic<bool> lodsReady (release/acquire 메모리 순서)로 스레드 안전 접근
     - LOD 생성 완료 전까지 원본 메시(LOD 0)로 렌더링
   - 주의: Windows SDK min/max 매크로 충돌 방지를 위해
     LODSelector.cpp 최상단에 #define NOMINMAX 필수

4. src/Renderer/LightCuller.h/.cpp를 만든다.
   - 광원 컬링: 너무 멀거나 가려진 광원을 라이팅 계산에서 제외
   - 거리 기반 컬링:
     Point/Spot 광원의 유효 범위(감쇠로 기여도가 임계값 이하가 되는 거리) 계산
     → 카메라 Frustum과 광원 유효 범위(BoundingSphere) 교차 검사
     → Frustum 밖의 광원은 활성 광원 목록에서 제외
   - 기여도 기반 컬링:
     광원~카메라 거리 및 광원 강도로 화면 기여도 추정
     → 기여도가 임계값(예: 0.01) 이하인 광원은 제외
   - Directional Light는 항상 포함 (무한 거리이므로 컬링 대상 아님)
   - CullLights(frustum, cameraPos, lightManager) → 활성 광원 인덱스 목록(vector<uint32_t>) 반환

5. LightManager에 BuildFilteredLightConstants(activeIndices) 메서드를 추가한다.
   - 컬링 후 활성 인덱스 목록만 받아 LightConstants(GPU CB) 빌드

6. Renderer.h에 CullStats 구조체를 추가한다.
   struct CullStats {
       uint32 visibleNodes, frustumCulledNodes, occlusionCulledNodes;
       uint32 activeLights, culledLights;
       uint32 renderedPolygons;  // Culling + LOD 후 실제 제출된 삼각형 수
   };
   - RenderScene() 내 Pass 1(Opaque) + Pass 2(Alpha Blend) 모두에서 accumulate
   - GetLastCullStats() const → CullStats

7. DebugHUD (RenderStats)에 다음 항목을 추가/수정한다.
   - totalPolygons: 씬 전체 폴리곤 수 (Culling/LOD 미적용)
   - renderedPolygons: Culling + LOD 후 실제 렌더링된 폴리곤 수
   - polygonsPerSec: rendered 기준 초당 폴리곤 (renderPolygons / deltaTime)
   - visibleNodes, frustumCulledNodes, activeLights, culledLights
   HUD 출력 형식:
     Polys (scene):    N
     Polys (rendered): M
     Poly/sec: X.XM
     Visible: N  Culled: M
     Lights: N active  M culled

8. tests/unit/test_FrustumCuller.cpp를 만든다 (8개 테스트).
   - 미빌드/빌드 후 IsBuilt 상태
   - Frustum 안의 AABB → visible
   - Frustum 밖의 AABB → not visible
   - Frustum 경계의 AABB → visible (보수적)
   - 뒤쪽/측면 AABB, 카메라를 완전히 감싸는 큰 AABB, rebuild 후 동작

9. tests/unit/test_LightCuller.cpp를 만든다 (7개 테스트).
   - 빈 LightManager → 활성 광원 0
   - Directional Light → 항상 활성
   - Frustum 안의 Point Light → 활성
   - Frustum 밖의 Point Light → 컬링
   - 기여도 임계값 이하의 약한 광원 → 컬링
   - Directional + Point 혼합, 호출 간 상태 독립성

10. Renderer 파이프라인에 Culling + LOD + Light Culling을 통합한다.
    - Scene Graph 순회 → Frustum Culling → Occlusion Culling → LOD 선택 → Draw
    - 라이팅 패스 전 Light Culling → 활성 광원만 GPU에 전달
    - ClearMeshCache()에서 LOD 등록도 함께 클리어 (m_lodSelector.Clear())
    - RegisterMeshesForLOD()는 씬 sceneDiagonal 확정 후 LoadScene 마지막에 호출

빌드하여 Frustum 밖 오브젝트가 culled되고, 거리별 LOD가 전환되며,
원거리 광원이 컬링되는지 확인하라.
DebugHUD에 씬 전체/실제 렌더링 폴리곤 수, culled 오브젝트 수, culled 광원 수를 표시하라.
```

---

## Prompt 24: HLSL 경고 수정 + Shadow Map 자동 크기 조정 + Sponza 빠른 로드

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 24 섹션을 참조하여 Phase 24를 구현하라.
이 단계는 PBR.hlsl X4000 경고를 최소화하고, Shadow Map 해상도·투영 범위를 씬 크기에 맞춰
자동 조정하며, Sponza 빠른 로드 메뉴를 추가한다.
Phase 24는 이미 구현 완료(✅)이므로, 이 프롬프트는 재현·참조용이다.

1. PBR.hlsl X4000 경고를 최소화한다.
   a. SampleShadowMap() 함수 구조 변경:
      - [branch] switch 방식 → float result = 1.0f; if/else-if 체인 방식으로 교체
      - result를 명시적으로 초기화(1.0f = lit 기본값)하여 FXC가 초기화 추적 가능하도록 함
      - 이로써 SampleShadowMap X4000 경고 제거
   b. CalcShadow() 함수:
      - shadowIdx = min(shadowIdx, MAX_SHADOW_MAPS - 1) 클램프 추가
      - PCF 누적: shadow += saturate(SampleShadowMap(...))
      - CalcShadow X4000 경고 1건은 FXC 컴파일러 한계(비교 샘플러 + 동적 cbuffer 인덱스
        조합)로 제거 불가 — Phase 29에서 Texture2DArray 리팩터링으로 재검토

2. ShadowCB(b3)에 shadowTexelSize 필드를 추가한다.
   - cbuffer ShadowCB: float shadowTexelSize 필드 추가
   - PCF 루프의 하드코딩 1.0f/1024.0f → shadowTexelSize cbuffer 값으로 교체
   - CPU 측: shadowConst.shadowTexelSize = 1.0f / GetShadowMapSize()로 매 프레임 계산

3. D3D12Context에 런타임 Shadow Map 해상도 변경 지원을 추가한다.
   - SHADOW_MAP_SIZE 상수 제거 → m_shadowMapSize = 1024 런타임 멤버로 교체
   - SetShadowMapSize(uint32): [512, 4096] 범위에서 2의 제곱수로 스냅
   - RecreateShadowMaps(): Fence 대기 후 기존 리소스 해제 → CreateShadowMaps() 재호출
   - GetShadowMapSize(): 현재 해상도 반환
   - BeginShadowPass()의 viewport/scissor에 m_shadowMapSize 사용

4. Renderer에 씬 크기 기반 Shadow 투영 자동 조정을 추가한다.
   - m_sceneDiagonal 멤버 추가 (기본값 10.0f)
   - SetSceneDiagonal(float d): 씬 로드 시 호출
   - Directional Shadow:
     - orthoSize = diagonal × 1.5f, farPlane = diagonal × 3.0f
     - nearPlane = diagonal × 0.5f  ← 씬 앞쪽 잘림 방지
     - shadowCamPos = sceneCenter - dir*(farPlane*0.5f)
       → 카메라를 씬 중심에서 farPlane/2 뒤에 배치하여 깊이 범위(near~far) 내에 씬 전체 포함
     - XMMatrixOrthographicLH(orthoSize, orthoSize, nearPlane, farPlane)
   - Spot Shadow:
     - farPlane = diagonal × 3.0f, nearPlane = diagonal × 0.05f
   - shadowNormalBiasWorld = (diagonal × 1.5f) / shadowMapSize × 2.0f
     → 씬 크기·해상도에 비례하는 월드 공간 노말 바이어스 (Shadow Acne 방지)
   - 위 값을 ShadowCB의 shadowNormalBiasWorld 필드로 매 프레임 GPU에 전달

5. Engine::LoadScene()에 씬 로드 후 Shadow Map 자동 설정을 추가한다.
   - Renderer::SetSceneDiagonal(m_sceneDiagonal) 호출
   - 해상도 선택: diagonal > 100m → 4096, > 10m → 2048, else → 1024
   - context->SetShadowMapSize(shadowSize); context->RecreateShadowMaps();

6. Orbit Light를 Directional Light로 전환하고 castShadow=true를 설정한다.
   - Engine::LoadScene()의 orbit light 생성 코드에서 type = LightType::Directional로 변경
   - position, Kc/Kl/Kq 제거; direction = {0,-1,0} (초기값, 매 프레임 갱신됨) 설정
   - castShadow = true 설정 (씬 로드 시 DirectX Depth Pass 1회/프레임 활성화)
   - Engine::Update()에서 position 대신 direction을 갱신:
     - 월드 Y축을 중심으로 회전하며 45° 앙각 고정 (카메라 독립)
     - kElevRad = π/4,  cosElev = cos(45°) ≈ 0.707,  sinElev = sin(45°) ≈ 0.707
     - lightDir = { -cosElev*cos(θ),  -sinElev,  -cosElev*sin(θ) }  (단위 벡터, 정규화 불필요)
     - 항상 원점(씬 중심)을 가리키며 카메라 위치·방향에 무관하게 동작

7. Sponza 빠른 로드 메뉴를 추가한다.
   - Win32Menu에 ID_FILE_OPEN_SPONZA = 6002, FileSponzaCallback 추가
   - File 메뉴 하단에 "Sponza!" 항목 추가 (WM_COMMAND → m_fileSponzaCallback 호출)
   - Engine::LoadSponzaScene() 구현:
     a. 파일 다이얼로그로 Sponza.gltf 경로를 사용자에게 선택하게 함
     b. LoadScene(utf8Path)로 표준 씬 로드 수행
     c. 카메라를 Sponza 전용 프리셋으로 설정:
        - SetPosition({10, 4.5, 4}), LookAt({0,0,0}), FOV 60°
        - SetMoveSpeedScale(sceneDiagonal / 40.0f)
     d. 기존 광원을 모두 클리어 후 Sponza 전용 광원 배치:
        - Directional "Sun" (warm white 1,0.95,0.8, intensity=10, castShadow=true),
          direction normalize({-0.3, -1.0, 0.5})
        - Point "Sky Fill" (cool blue 0.4,0.5,0.7, intensity=1.75, position {-6,10,0})
        - Point "Torch" × 4 (orange 1.0,0.45,0.08, intensity=8, fast falloff Kl=0.7/Kq=1.8)
          — 코너 4곳 배치 (y=1.836)
     e. m_orbitLightIndex = SIZE_MAX — Sponza에서 Orbit 조명 비활성화
   - Engine 생성자에서 m_menu->SetFileSponzaCallback([this]() { LoadSponzaScene(); }) 등록

8. Sponza 태양 방향 토글을 구현한다 (L 키, Sponza! 메뉴 로드 시에만 활성).
   - Engine.h에 멤버 추가:
     - bool m_isSponzaScene = false
     - bool m_sponzaSunAltMode = false
     - bool m_sponzaSunToggleKeyWasDown = false
     - size_t m_sponzaSunKeyIndex = 0   // LightManager 내 sun light 인덱스
   - Engine::LoadSponzaScene(): m_lightManager->Clear() 직후에
     m_isSponzaScene=true, m_sponzaSunAltMode=false, m_sponzaSunKeyIndex=0 설정
   - Engine::LoadScene(): 함수 진입 시 m_isSponzaScene=false 설정
     (LoadSponzaScene()이 내부에서 LoadScene() 호출 후 true로 덮어씀)
   - Engine::Update(): L 키 에지(pressed, not held) 감지:
     bool keyDown = (GetAsyncKeyState('L') & 0x8000) != 0;
     if (keyDown && !m_sponzaSunToggleKeyWasDown) → m_sponzaSunAltMode 토글
     - false(기본): XMVector3Normalize({-0.3, -1.0, 0.5}) — 앙각 ≈ 60°
     - true(alt):   XMVector3Normalize({-0.3, -1.5, 0.3}) — 앙각 ≈ 74° (1층 더 밝음)
     m_sponzaSunToggleKeyWasDown = keyDown 저장

빌드하여 X4000 경고가 최소화되고, 씬 로드 시 Shadow Map 해상도 및 투영 범위가
씬 크기에 맞게 자동 설정되는지 확인하라. Orbit Directional Light가 매 프레임 회전하며
그림자를 생성하는지 확인하라. File 메뉴의 "Sponza!" 항목으로 Sponza 씬이 전용 카메라·광원
프리셋으로 로드되고, L 키로 태양 방향이 60°↔74° 사이에서 토글되는지 확인하라.
```

---

## Prompt 25: Bistro 씬 분석 + glTF 에셋 준비

```
PRD.md, PLAN.md, CLAUDE.md, SceneSettings.md를 참조하여 Phase 25를 수행하라.
이 Phase는 구현 없이 에셋 준비 + 문서화만 수행한다.

1. niagara_bistro (github.com/zeux/niagara_bistro) 레포지토리를 분석한다.
   - Exterior + Interior 파일 목록, 폴리곤 수, 텍스처 개수 파악
   - 씬 스케일(바운딩 박스), 좌표계, 단위(m/cm) 확인
   - Exterior diagonal ≈ 50m 기준으로 SceneSettings.md 항목 작성

2. Bistro 씬 SceneSettings.md 섹션 작성:
   - 씬 스케일 표 (X×Y×Z, diagonal, 삼각형 수, glTF root scale)
   - 카메라 세팅 추천 (Position, LookAt, FOV, MoveSpeedScale)
   - 광원 세팅 추천:
     · Key Light (Directional, 태양): direction, color, intensity, castShadow
     · Fill Light (Point, 하늘빛): position, color, intensity, 감쇠 계수
     · 추가 Point Light 여부 (Torch 등)
   - Shadow Map 자동 설정:
     · diagonal≈50m → 해상도 2048, orthoSize 75m, far 150m (자동 결정 공식 적용)
     · DepthBias, SlopeScaledDepthBias 추천값

3. assets/test-models/Bistro/ 경로 계획 문서화 (실제 다운로드는 Phase 26에서 수행)

빌드는 수행하지 않는다. SceneSettings.md에 Bistro 섹션을 추가하고 PLAN.md 완료 기준과 일치하는지 확인하라.
```

---

## Prompt 26: Bistro! 빠른 로드 메뉴 + 씬 전용 설정 + Interior 조명 토글 + Shadow Map 시각적 튜닝

```
PRD.md, PLAN.md, CLAUDE.md, SceneSettings.md(Bistro 섹션)를 참조하여 Phase 26을 구현하라.
Phase 24의 Sponza! 구현(Engine.cpp LoadSponzaScene/ApplySponzaLighting, Win32Menu, L키 토글)을 참조한다.

> assets/test-models/Bistro/bistro.gltf (3.6 GB) — Phase 25에서 이미 클론 완료.
> bistro.gltf: Exterior + Interior 통합 씬 (메시 551개, 삼각형 1,753,630개, diagonal ≈ 166m).

1. Win32Menu에 "Bistro!" 메뉴 항목 추가 (ID_FILE_BISTRO = 6003):
   - "File" 메뉴에 "Sponza!" 아래 추가
   - 메뉴 핸들러: Engine::LoadBistroScene() 호출

2. Engine::LoadBistroScene() 구현:
   - 파일 열기 다이얼로그(bistro.gltf 선택) → LoadScene() 호출 (표준 로딩)
   - SceneSettings.md의 Bistro 카메라 세팅 적용 (Position, LookAt, FOV)
   - m_isBistroScene = true, m_bistroInteriorMode = false 설정
   - m_orbitLightIndex = SIZE_MAX (Orbit 조명 비활성)
   - ApplyBistroLighting() 호출 (Exterior 기본 조명 배치)

3. Engine::ApplyBistroLighting() 구현 (SceneSettings.md Interior Point Lights 섹션 기준):
   - m_lightManager->Clear()로 전체 리셋 후 m_bistroInteriorMode에 따라 분기:

   [Exterior 모드 — m_bistroInteriorMode == false]
   - Directional "Evening Sun": color {1.0, 0.85, 0.6}, intensity=8, castShadow=true
   - Point Fill (sky ambient): color {0.4, 0.5, 0.7}, intensity=2, Kl=0.007, Kq=0.0002
   → 총 2개

   [Interior 모드 — m_bistroInteriorMode == true]
   - Directional "Evening Sun": 동일 direction, intensity=3 (창문 통과 간접광으로 감쇠)
   - Point 천장 펜던트 ×3: color {1.0, 0.85, 0.55}, intensity=5, Kl=0.7, Kq=0.9
     positions: {-2.0, 3.5, 3.0} / {-2.0, 3.5, 7.0} / {-2.0, 3.5, 11.0}
   - Point 바 카운터 ×1: color {1.0, 0.75, 0.40}, intensity=4, Kl=0.35, Kq=0.12
     position: {2.0, 3.0, 5.0}
   - Point 앰비언트 필 ×1: color {0.7, 0.65, 0.90}, intensity=0.8, Kl=0.022, Kq=0.0019
     position: {0.0, 8.0, 6.0}
   → 총 6개 (MAX_PBR_LIGHTS(16) 여유 충분)

   ※ Interior 좌표는 추정값. 씬 로드 후 실제 위치 확인하며 조정.

4. Bistro 조명 토글 — L 키 (Exterior ↔ Interior):
   - Engine.h에 m_isBistroScene, m_bistroInteriorMode, m_bistroLightKeyWasDown 추가
   - Engine::LoadScene(): m_isBistroScene = false (일반 씬 로드 시 토글 비활성)
   - Engine::Update(): m_isBistroScene && L 키 에지 감지 시:
     - m_bistroInteriorMode 토글
     - ApplyBistroLighting() 재호출
   - Bistro! 메뉴로 로드된 경우에만 동작

5. 빌드 및 동작 확인:
   - "File > Bistro!" 메뉴로 bistro.gltf 로드, Exterior 조명(2개) 자동 배치
   - L 키 → Interior 조명(6개)으로 전환, 카메라를 실내로 이동하며 조명 확인
   - L 키 재누름 → Exterior 조명(2개)으로 복귀
   - Shadow Map 자동 선택 확인: diagonal≈166m → 4096×4096 (RTX) 또는 2048×2048 (UHD 630)

6. Shadow Map 시각적 튜닝 (해상도 / DepthBias / SlopeScaledDepthBias 결정):

   [확인 순서 및 판단 기준]

   a) 외부 바닥 (Ground plane, 태양광 얕은 입사각):
      - Shadow Acne(바닥에 줄무늬·계단형 패턴)가 보이면 → DepthBias 증가
      - 기둥·벽 하단 그림자가 발밑에서 분리(Peter Panning)되면 → DepthBias 감소
      - 현재 기본값: DepthBias=1000, SlopeScaledDepthBias=1.0

   b) 기둥·아치 주변 접촉면:
      - 기둥이 바닥에 닿는 경계의 그림자 품질 → 전반적 Bias 밸런스 재확인

   c) 계단 / 경사 지붕 표면:
      - 경사면에서만 Acne가 잔존하면 → SlopeScaledDepthBias 증가

   d) 원거리 가로등·아치 그림자 경계:
      - 경계가 블록(계단형 픽셀)으로 보이면 → 해상도 4096으로 증가 또는 PCF 커널 확대

   e) 실내·실외 경계 개구부 (아치 / 창문):
      - L 키로 Interior 모드 전환 후 창문 빛 누출 품질 시각 확인

   [판단 기준 요약]
   - Shadow Acne   : 바닥·벽에 자기 자신에 의한 계단형 줄무늬 → Bias 부족
   - Peter Panning : 오브젝트와 그림자 사이 공백(뜨는 느낌) → Bias 과다
   - 해상도 부족   : 원거리 그림자 경계가 픽셀 블록으로 보임 → 해상도 증가 또는 PCF 커널 확대
   - SlopeScaledDepthBias: 경사면에만 Acne → SlopeScaledDepthBias 특별히 증가

7. 결정된 값 기록:
   - SceneSettings.md Bistro 섹션에 최종 Shadow Map 해상도, DepthBias, SlopeScaledDepthBias 기록
   - D3D12Context 기본값 업데이트 또는 씬 로드 시 SetShadowBias() 호출로 반영

빌드하여 Bistro 씬 로드, L 키 Exterior↔Interior 토글, Shadow Map 품질이 모두 정상인지 확인하라.
```

---

## Prompt 27: Texture Streaming + Mip-Mapping

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 27를 구현하라.

1. src/Asset/TextureStreamer.h/.cpp를 만든다.
   - 텍스처별 요구 Mip 레벨 관리
   - 우선순위 계산: priority = isVisible ? (1/distance) : 0
   - 우선순위 큐: 높은 순서로 스트리밍 대역폭 할당

2. 스트리밍 흐름을 구현한다.
   - 초기 로드: 하위 Mip(64×64 이하)만 GPU 업로드
   - 렌더링 중: 가시성 + 거리 기반 우선순위 → 상위 Mip 비동기 로딩
   - 로딩 완료: Copy Queue 또는 메인 스레드에서 GPU 업로드
   - Mip 해제: Frustum 밖 또는 카메라 멀어짐 → 상위 Mip 해제

3. Mip chain 생성을 구현한다.
   - 전체 Mip 수: floor(log2(max(width, height))) + 1
   - D3D12 텍스처: MipLevels 파라미터 설정
   - Mip 데이터: CPU box filter (또는 GPU compute shader)

4. Sampler를 업그레이드한다.
   - s0: D3D12_FILTER_ANISOTROPIC, MaxAnisotropy = 16

5. 메모리 예산을 구현한다.
   - IDXGIAdapter3::QueryVideoMemoryInfo로 VRAM 모니터링
   - 예산 초과 시 LRU + 거리 기반 Mip 해제

6. DebugHUD에 스트리밍 통계를 추가한다.
   - 현재 스트리밍 중인 리소스 개수
   - 남은 스트리밍 대역폭(큐 잔량 또는 MB/s)

빌드하여 카메라 접근 시 고해상도 Mip이 로딩되고,
멀어지면 저해상도로 대체되는지 확인하라.
```

---

## Prompt 28: Instanced Rendering + 멀티스레드 로딩

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 28를 구현하라.

1. src/Renderer/InstanceBatcher.h/.cpp를 만든다.
   - Scene Graph에서 동일 Mesh+Material 조합을 그룹핑
   - struct InstanceData { XMFLOAT4X4 world; }; (전치 적용)
   - Instance Buffer 생성: InstanceData 배열 → Upload Buffer → GPU

2. D3D12 Input Layout에 per-instance 슬롯을 추가한다.
   - slot 0: per-vertex (position, color, normal, texCoord, tangent)
   - slot 1: per-instance (INSTANCE_WORLD, 4×float4, InstanceDataStepRate=1)

3. DrawIndexedInstanced를 사용한다.
   - instanceCount > 1: 인스턴싱
   - instanceCount == 1: 기존 단일 드로우콜과 동일 동작

4. PBR.hlsl의 VS에서 SV_InstanceID로 World Matrix를 인덱싱한다.

5. src/Core/ThreadPool.h/.cpp를 만든다.
   - CPU 코어 수 기반 워커 스레드 생성 (std::thread::hardware_concurrency())
   - Submit(task) → std::future<T> 반환
   - 텍스처 이미지 디코딩을 워커 스레드에 분배

6. Copy Queue를 구현한다.
   - D3D12 Copy Queue 전용 Command Allocator + Command List
   - Graphics Queue와 병렬로 리소스 업로드
   - Fence로 Copy 완료 동기화

빌드하여 동일 메시 인스턴싱으로 드로우콜이 감소하고,
멀티스레드 텍스처 디코딩이 동작하는지 확인하라.
```

---

## Prompt 29: GPU 메모리 최적화

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 29를 구현하라.

1. CBPool을 Renderer에 통합한다.
   - 매 프레임 ResetFrame(frameIndex) 호출
   - DrawPrimitives마다 Allocate()로 CB 슬롯 획득
   - 개별 CreateCommittedResource 호출 제거

2. VRAM 모니터링을 구현한다.
   - IDXGIAdapter3::QueryVideoMemoryInfo(DXGI_MEMORY_SEGMENT_GROUP_LOCAL)
   - Budget 필드의 80%를 임계값으로 설정
   - 임계값 초과 시: 우선순위 낮은 오브젝트 CB 갱신 빈도를 N프레임마다 1회로 감소
   - 우선순위: 카메라 거리, 화면 차지 비율, 움직임 여부

3. Shared Material CB를 구현한다.
   - PerObjectCB (register b0): world, viewProj (오브젝트별)
   - PerMaterialCB (register b2): baseColorFactor, metallic, roughness 등 (재질별)
   - 동일 Material의 오브젝트는 같은 PerMaterialCB 슬롯 참조
   - 드로우콜 시 이전과 같은 Material이면 b2 바인딩 스킵

4. Dirty Flag 기반 갱신 스킵을 구현한다.
   - Per-Object: Transform 미변경 → memcpy 스킵
   - Per-Material: 파라미터 미변경 → 갱신 스킵
   - Light: 광원 데이터 미변경 → 갱신 스킵
   - Occluded 오브젝트: CB + Draw 모두 스킵

5. Opaque Front-to-Back 정렬을 구현한다.
   - Opaque 패스 오브젝트를 카메라 거리 기준 앞→뒤 정렬
   - GPU Early-Z rejection 극대화

6. DebugHUD에 최적화 통계를 추가한다.
   - VRAM 사용량 (Used / Budget)
   - frustum culled 수, occlusion culled 수
   - 드로우콜 수, 인스턴스 수
   - 현재 렌더링 모드

빌드하여 CBPool에서 슬롯이 정상 할당되고,
Dirty Flag로 불필요한 갱신이 스킵되며,
VRAM 사용량이 HUD에 표시되는지 확인하라.
```

---

## Prompt 30: Phase 02 통합 & 최종 검증

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 30를 구현하라.

1. 전체 렌더 파이프라인을 12단계로 통합한다.
   ① Scene Graph 순회 → AABB + 월드 행렬 수집
   ② Frustum Culling → 시야 밖 제외
   ③ Occlusion Culling → 가려진 오브젝트 제외 (CB + Draw 스킵)
   ④ LOD 선택 → 거리 기반 Mesh 결정
   ⑤ Instance Batching → 동일 Mesh+Material 그룹핑
   ⑥ Texture Streaming → 가시성+거리 우선순위로 Mip 업데이트
   ⑦ CB 갱신 → Dirty Flag + VRAM 예산 기반 적응적 갱신
   ⑧ Material 정렬 → PSO 전환 최소화
   ⑨ Opaque Front-to-Back 정렬
   ⑩ Shadow Depth Pass → 광원별 depth-only 렌더링
   ⑪ Main Pass → Opaque(인스턴싱) → Alpha Mask → Alpha Blend(back-to-front)

2. Shadow Depth Pass에 Frustum Culling을 적용한다.
   - 각 그림자 광원에 대해 광원 시점의 BoundingFrustum을 생성한다.
     · Directional: XMMatrixOrthographicLH + LVP로부터 BoundingFrustum 생성
     · Spot: XMMatrixPerspectiveFovLH + LVP로부터 BoundingFrustum 생성
   - Shadow Depth Pass 내부의 SceneGraph Traverse 람다에서 FrustumCuller를 호출하여
     광원 시야 밖 오브젝트의 DrawShadowDepth 호출을 스킵한다.
   - DebugHUD에 shadowCulledNodes 수 표시 (CullStats 확장)

3. 대형 씬 벤치마크를 수행한다.
   - Sponza (glTF): 로딩 → PBR + Shadow + 최적화 렌더링
   - Bistro (glTF): 대규모 씬 로딩 및 네비게이션
   - 60fps 이상 유지 확인 (DebugHUD FPS 모니터링)

4. 5단계 렌더링 모드를 전체 검증한다.
   - Wireframe → Solid → Base Color → Full PBR → Full PBR + Shadows
   - 각 모드에서 정상 렌더링되는지 확인

5. 전체 기능을 통합 검증한다.
   - File 메뉴에서 glTF/FBX 파일 열기 (파일 다이얼로그)
   - 씬 파일 내 카메라 있으면 해당 위치, 없으면 Fit to Scene
   - WASD+QE 키보드 이동, Render 메뉴 모드 전환
   - 다중 광원 (Directional + Point + Spot), 그림자
   - DebugHUD: FPS, 해상도, 폴리곤, culled/occluded, 드로우콜,
     인스턴스, VRAM, 스트리밍, 렌더모드
   - Phase 01 오브젝트(vertex-color)도 BasicColor PSO로 정상 렌더링

6. 모든 테스트를 실행한다.
   - 기존 Phase 01 유닛/스모크 테스트 통과 확인
   - Phase 02 테스트: test_Material, test_FrustumCuller, test_SceneLoader

문제가 있으면 수정하라.
```

---

## Prompt 31: RRScenePreprocessor — 오프라인 씬 전처리 도구 + 백그라운드 자동 생성

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 31 섹션과 GoodToPreprocess.md를 참조하여 Phase 31를 구현하라.
이 단계는 .rrscene 전처리 파이프라인을 공용 클래스로 구현하고,
CLI 도구와 렌더링 앱 내 백그라운드 자동 생성의 두 진입점을 제공한다.

1. .rrscene 바이너리 포맷을 정의한다 (src/Asset/RRSceneFormat.h, 공용 헤더).
   헤더:
     · char magic[4] = "RRSC"
     · uint32 version = 1
     · uint64 sourceHash  (원본 파일 크기 ^ 수정 시각, 변경 감지용)
     · uint32 sectionCount
     · SectionEntry[] { SectionType type; uint64 offset; uint64 size; }
   섹션 타입: Scene / Mesh / Material / Texture / Light
   각 섹션 세부 구조:
   - Scene: 노드 수, 노드별(부모 인덱스, 이름, 로컬 TRS 행렬, meshIndex, materialIndex), 씬 AABB, 카메라 초기(position/yaw/pitch/fov)
   - Mesh: 메시 수, 메시별(vertex 수, index 수, Vertex 배열 raw dump, Index 배열 raw dump, AABB, LOD 수, LOD별 vertex/index + 전환 거리)
   - Material: 재질 수, 재질별(PBR factor, AlphaMode, doubleSided, textureIndex 참조 5개, sRGB 플래그)
   - Texture: 텍스처 수, 텍스처별(width, height, mipLevels, DXGI_FORMAT, 전체 Mip chain 픽셀 데이터 연속 배치)
   - Light: 광원 수, 광원별(type, color, intensity, position, direction, Kc/Kl/Kq, innerCone, outerCone, castShadow, bsRadius)

2. 전처리 파이프라인을 공용 클래스로 구현한다 (src/Asset/ScenePreprocessor.h/.cpp).
   - static bool Generate(const std::string& sourcePath, const std::string& outputPath):
     동기 실행, CLI 도구와 엔진에서 모두 호출 가능
     a. Assimp 파싱: aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded
     b. Vertex/Index 변환: aiMesh → 엔진 Vertex 구조체
        · Tangent 없으면 Gram-Schmidt 재직교화로 생성
     c. 프리미티브 분리: aiNode의 복수 aiMesh → 각각 SceneNode로 분리
     d. 메시별 AABB: BoundingBox::CreateFromPoints()
     e. Auto-LOD 생성:
        · LOD 1: 원본 삼각형 50% (QEM Edge Collapse)
        · LOD 2: 원본 삼각형 25%
        · 전환 거리: sceneDiagonal × 2.0f (LOD 1), × 6.0f (LOD 2)
     f. 이미지 디코딩: stb_image로 PNG/JPEG → RGBA 픽셀 버퍼
        · baseColor/emissive: sRGB 플래그 설정
        · normal/metallicRoughness/occlusion: Linear 플래그 설정
     g. Mip chain 생성: CPU box filter, floor(log2(max(w,h))) + 1 레벨
     h. 씬 구조 직렬화: 노드 계층, 씬 AABB, 카메라 초기 배치, Material, Light(BoundingSphere 포함)
     i. 원자적 파일 쓰기: 임시 파일(.rrscene.tmp) 완성 후 최종 경로로 rename
   - static std::future<bool> GenerateAsync(const std::string& sourcePath):
     std::async로 백그라운드 스레드에서 Generate() 실행, future 반환

3. CLI 도구 프로젝트를 추가한다 (RRScenePreprocessor, Console Application).
   - ScenePreprocessor::Generate()를 호출하는 얇은 래퍼
   - main(argc, argv): 입력 파일 경로 인수 받음, 출력 경로 = 입력과 동일 디렉토리 + .rrscene 확장자
   - 출력: bin/Debug/RRScenePreprocessor.exe

4. 렌더링 앱에 이중 로딩 경로를 추가한다 (src/Asset/SceneLoader).
   - SceneLoader::LoadScene(path):
     a. 동일 디렉토리에 path.rrscene 존재 여부 확인
     b. 존재하면: LoadRRScene(rrscenePath) 시도
        · Header magic/version 검증
        · sourceHash와 원본 파일 해시 비교 → 불일치 시 표준 경로로 폴백 + 로그
        · 검증 통과 시: 섹션 순서대로 SceneNode/Mesh/Material/Texture/Light 객체 생성
        · GPU 업로드(VB/IB/Texture)만 수행 (Assimp 파싱 없음)
     c. 없거나 실패 시: 기존 Assimp 표준 경로 사용 → 로딩 완료 후 항목 5 실행
   - DebugHUD에 로딩 경로 표시: "Fast (.rrscene)" 또는 "Standard (Assimp)"

5. 표준 경로 로딩 후 백그라운드 자동 전처리를 구현한다 (Engine::LoadScene()).
   - 표준 경로(Assimp) 로딩 완료 직후: ScenePreprocessor::GenerateAsync(sourcePath) 호출
     · 반환된 std::future<bool>을 Engine 멤버(m_preprocessFuture)에 저장
   - 렌더링 블로킹 없이 백그라운드 스레드에서 전처리 파이프라인 실행
   - DebugHUD에 진행 상태 표시:
     · 진행 중: "Preprocessing scene..." (m_preprocessFuture가 유효한 동안)
     · 완료 후: 메시지 사라짐
   - 매 프레임 Engine::Update()에서 future 완료 여부 폴링:
     · future.wait_for(0ms) == ready → 결과 확인, 성공 시 콘솔 로그 출력
       ("Sponza.rrscene saved — next load will use fast path")
     · m_preprocessFuture 초기화(reset)
   - 씬 교체 시 이전 전처리 future가 실행 중이면 detach(취소 불가) 후 진행

6. 동작을 검증한다.
   - Sponza.gltf 첫 로딩: 표준 경로(Assimp) 사용 + DebugHUD "Preprocessing scene..." 표시 확인
   - 전처리 완료 후: Sponza.rrscene 파일 생성 확인, 콘솔 로그 확인
   - Sponza.gltf 두 번째 로딩: 자동으로 고속 경로 사용(~90% 단축) 확인
   - CLI 도구로 동일한 .rrscene 생성 후 렌더링 앱에서 고속 로딩 확인
   - 원본 파일 변경 후 로딩: 해시 불일치 감지 → 표준 경로 폴백 + 재전처리 시작 확인

빌드하여 첫 로딩 시 백그라운드 자동 생성이 동작하고, 두 번째 로딩부터 고속 경로가 사용되는지 확인하라.
```

---

## Prompt 32: 코드 리뷰, 최적화, 버그 수정 & 아키텍처 문서화

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 02 섹션을 참조하여 Phase 32를 구현하라.
이 단계는 Phase 02의 마지막 단계로, UX 개선 2건을 구현하고 전체 코드 품질을 점검하며 ARCHITECTURE.md를 작성한다.

1. UX 개선 항목을 구현한다.
   a. 드래그 앤 드롭 씬 로딩을 구현한다.
      - Win32Window::Initialize()에서 DragAcceptFiles(hwnd, TRUE) 호출
      - WndProc에 WM_DROPFILES 핸들러 추가:
        · DragQueryFile(hDrop, 0, path, MAX_PATH)로 첫 번째 파일 경로 추출
        · 확장자 확인: .gltf / .glb / .fbx인 경우에만 Engine::LoadScene() 호출
        · DragFinish(hDrop) 호출
   b. Camera 중클릭 드래그 패닝을 구현한다.
      - Win32Input에서 WM_MBUTTONDOWN / WM_MBUTTONUP / WM_MOUSEMOVE 처리
      - 중클릭 드래그 델타 → Camera right/up 벡터 기준 position 이동
        · panSpeed = m_moveSpeedScale * deltaPixels * panSensitivity
        · position += right * (-deltaX * panSpeed) + up * (deltaY * panSpeed)

2. 전체 코드 리뷰를 수행한다.
   - src/ 하위 모든 소스 파일(.h, .cpp, .hlsl)을 순회하며 코드 품질을 점검한다.
   - 점검 항목:
     a. 사용되지 않는 코드(dead code), 불필요한 #include, 중복 로직 → 제거
     b. 네이밍 컨벤션 일관성: PascalCase(클래스/메서드), camelCase(변수), UPPER_SNAKE_CASE(상수)
     c. 보안 점검: 버퍼 오버플로우, 범위 초과 접근, null 역참조, 초기화되지 않은 변수
     d. COM 객체/GPU 리소스 해제 누락: Fence 대기 후 해제가 보장되는지 확인
     e. 스마트 포인터(unique_ptr) 및 ComPtr 사용 일관성
     f. HLSL 셰이더: 미사용 register, 불필요한 분기, 0으로 나누기 방지 (NdotL, NdotV 등)
     g. 헤더 가드: 모든 .h 파일에 #pragma once 확인
     h. include 순서: 자기 헤더 → 프로젝트 → DirectX/Windows → 표준 라이브러리
   - 발견된 문제를 즉시 수정한다.

3. 성능 최적화를 수행한다.
   - D3D12 Debug Layer를 활성화하고 Warning/Error 메시지를 전수 확인한다.
     모든 경고를 0건으로 만든다.
   - D3D12 Live Object 리포트로 메모리 누수를 점검한다.
     (ID3D12DebugDevice::ReportLiveDeviceObjects)
   - CPU 측 핫 루프 점검: 불필요한 메모리 할당, 과도한 std::vector 복사,
     매 프레임 반복되는 비효율적 연산 식별 및 최적화
   - GPU 측 점검: 드로우콜 수, PSO 상태 전환 횟수가 Material 정렬로 최소화되었는지 확인
   - 셰이더 최적화: 불필요한 동적 분기를 상수 분기로 대체 가능한 곳 확인

4. 버그 수정 및 엣지 케이스를 처리한다.
   - 모든 유닛 테스트 + 스모크 테스트를 재실행한다. 실패 항목이 있으면 수정한다.
   - 엣지 케이스 검증:
     a. 빈 씬 (메시 0개): 크래시 없이 빈 화면 렌더링
     b. Material 없는 Mesh: vertex-color 폴백으로 정상 렌더링
     c. 텍스처 없는 Material: factor 값으로 폴백 렌더링
     d. 대형 씬 로딩 중 메모리 부족: 오류 메시지 출력 후 graceful 복구
     e. 윈도우 리사이즈/모드 전환 중 씬 로딩: 크래시 없이 처리
     f. 잘못된 파일 경로/손상된 파일 로딩: 오류 처리 및 사용자 알림
     g. **glTF `doubleSided` PSO 분기**: 현재 PBR PSO가 전역 `CullMode=NONE`(임시 방편)인 상태를 수정한다.
        - `D3D12PipelineState`에 `CullMode=BACK` PBR PSO와 `CullMode=NONE` PBR PSO를 별도로 생성한다.
        - `Renderer::RenderScene()`에서 `material->doubleSided` 여부에 따라 PSO를 선택한다.
        - `PBR.hlsl`의 `SV_IsFrontFace` 법선 반전은 `CullMode=NONE` PSO 드로우콜에서만 의미 있음 (그대로 유지).
   - 멀티스레드 안전성: 텍스처 교체, 상태 플래그 읽기/쓰기에 race condition 없는지 확인

5. ARCHITECTURE.md를 프로젝트 루트에 작성한다.
   다음 내용을 포함한다:

   a. 프로젝트 개요 (1~2문단)
   b. 전체 디렉토리 구조 (트리 형태) + 각 디렉토리/파일의 역할 설명
   c. 모듈 의존성 다이어그램 (텍스트 기반):
      Engine → Renderer → RHI(IRHIDevice/IRHIContext) → D3D12 백엔드
      Engine → SceneGraph → SceneNode → Mesh + Material
      Engine → Asset(SceneLoader/TextureCache/TextureStreamer)
      Engine → Lighting(LightManager)
      Engine → Platform(Win32Window/Win32Menu/Win32Input)
   d. 엔진 라이프사이클:
      Initialize → MainLoop(ProcessMessages → Update → Render) → Shutdown
      각 단계에서 호출되는 주요 함수/클래스
   e. 프레임당 렌더링 파이프라인 (12단계 상세):
      각 단계의 입력/출력, 담당 클래스, 데이터 흐름
   f. 주요 클래스 관계도 (텍스트 기반 UML 스타일):
      Engine, Renderer, SceneGraph, Camera, LightManager,
      SceneLoader, Material, Texture, TextureCache, TextureStreamer,
      CBPool, FrustumCuller, OcclusionCuller, LODSelector, InstanceBatcher
   g. D3D12 리소스 라이프사이클:
      생성(CreateCommittedResource) → 상태 전이(Resource Barrier) → 사용(Draw) → 해제(Fence 대기)
   h. 데이터 흐름 다이어그램:
      파일(glTF/FBX) → Assimp 파싱 → SceneGraph/Material/Texture(CPU)
      → Upload Buffer → Default Heap(GPU) → CB/VB/IB/SRV → 셰이더
   i. 스레딩 모델:
      메인 스레드(게임 루프, GPU 커맨드) vs 워커 스레드(텍스처 디코딩) vs Copy Queue(GPU 업로드)
   j. 셰이더 바인딩 맵:
      register b0~b2(CB), t0~t13(SRV), s0~s1(Sampler)
      각 register에 바인딩되는 데이터 설명
   k. 렌더링 모드별 파이프라인 차이:
      Wireframe / Solid / BaseColor / FullPBR / FullPBR+Shadows
   l. 참조 문서: PRD.md, PLAN.md, PROMPT.md, CLAUDE.md 역할 설명

빌드하여 모든 테스트가 통과하고, D3D12 Debug Layer 경고가 0건이며,
ARCHITECTURE.md가 프로젝트 루트에 생성되었는지 확인하라.
```

---

## Prompt 33: Occlusion Culling — Hi-Z GPU

```
PRD.md, PLAN.md(Phase 33), CLAUDE.md를 참조하여 Phase 33을 구현하라.
이 단계는 현재 P0 스텁(항상 false)인 OcclusionCuller를 GPU Hi-Z 방식으로 완전 구현한다.
CPU Readback 간이 방식을 거치지 않고 바로 Hi-Z로 구현한다.
현재 엔진에 Compute Shader 인프라가 없으므로, 먼저 인프라를 구축한다.

0. Phase 02 Backup을 생성한다 (구현 시작 전 최초 1회만 수행).
   - 프로젝트 루트에 "Phase 02 Backup/" 폴더를 생성한다.
   - src/, tests/, assets/, shaders/ 등 소스 파일 전체를 복사한다.
   - "Phase 01 Backup/" 폴더는 복사 대상에서 반드시 제외한다 (이중 백업 방지).
   - bin/, .git/, *.user, *.suo, ipch/, x64/ 등 빌드 산출물 및 IDE 캐시는 제외한다.
   - 백업 완료 후 "Phase 02 Backup/README.md"를 생성하여 백업 일시와
     Phase 02 최종 완료 상태(구현된 기능 목록)를 기록한다.
   - **백업 완료 후 "Phase 02 Backup/" 폴더 안의 파일은 절대 수정하지 않는다.
     이후 어떠한 Phase 구현에서도 이 폴더를 참조만 하고 절대 건드리지 않는다.**

1. Compute Shader 인프라를 구축한다.
   - src/RHI/D3D12/D3D12ComputePipeline.h/.cpp를 신규 생성한다.
     · CS 전용 Root Signature 생성 (UAV, SRV, CBV 슬롯 정의)
     · ID3D12PipelineState (ComputePipelineStateDesc) 생성/관리
   - D3D12Context에 Dispatch(x, y, z) 메서드를 추가한다.
   - CBV_SRV_UAV DescriptorHeap을 UAV 슬롯이 포함되도록 확장한다.

2. Hi-Z (Hierarchical-Z) Buffer를 생성한다.
   - 이전 프레임 Depth Buffer(DXGI_FORMAT_D32_FLOAT)를 DXGI_FORMAT_R32_FLOAT로 복사한다.
     · CopyTextureRegion 또는 Compute Shader를 사용하여 복사한다.
   - 복사된 텍스처를 시작으로 반씩 축소하는 Mip chain을 Compute Shader로 생성한다.
     · 각 Mip 단계: UAV(write) 바인딩, SRV(read) 바인딩 교차
     · 축소 필터: max(depth) — 보수적 occlusion 판정을 위해 최대값 사용
     · 최대 floor(log2(max(width, height))) 단계 생성
   - Hi-Z HLSL 파일: src/Shaders/HiZDownsample.hlsl

3. GPU-side AABB depth 비교 Compute Shader를 구현한다.
   - src/Shaders/OcclusionTest.hlsl을 신규 생성한다.
   - 입력: SceneNode AABB(center + extents) 배열 StructuredBuffer, ViewProj 행렬 CBV
   - 처리:
     a. AABB 8개 코너를 NDC로 변환
     b. screen-space min/max (UV 공간) 계산
     c. 최적 Mip 레벨 계산: floor(log2(maxExtent_pixels))
     d. Hi-Z Mip 텍스처에서 해당 영역 depth 샘플링
     e. AABB 근거리 Z와 비교 → RWByteAddressBuffer에 결과(0/1) 기록
   - 출력 결과를 Readback Buffer로 복사, 1프레임 레이턴시로 CPU에서 읽는다.

4. OcclusionCuller P0 스텁을 Hi-Z 방식으로 교체한다.
   - src/Renderer/OcclusionCuller.h/.cpp를 수정한다.
   - IsOccluded()가 GPU Hi-Z 결과 버퍼의 값을 반환하도록 구현한다.
   - occlusionCulledNodes 통계를 CullStats에 반영하고 DebugHUD에 표시한다.
   - Optimization 메뉴 항목을 추가한다:
     · Win32Menu에 ID_OPTIM_OCCLUSION_CULL = 8004 추가
     · "Occlusion Culling" 체크 토글 항목 (Optimization 메뉴)
     · Engine 콜백 → Renderer::SetOcclusionCullingEnabled(bool) 연결

5. 성능을 검증한다.
   - Sponza 씬에서 Hi-Z Occlusion Culling 활성화 시 드로우콜 수 감소 확인
   - DebugHUD에서 occlusionCulledNodes 수치 및 FPS 개선 확인
   - CPU readback 방식 대비 GPU stall 감소 확인

빌드하여 모든 테스트가 통과하고, Hi-Z Occlusion Culling이 Sponza에서 정상 동작하는지 확인하라.
```

---

## Prompt 34: Point Light Cube Map Shadowing

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 34 섹션을 참조하여 Phase 34를 구현하라.
이 단계는 castShadow = true인 Point Light에 대해 Omnidirectional Shadow Map(TextureCube)을 구현한다.

1. TextureCube D3D12 리소스를 생성한다.
   - D3D12Context에 Point light 전용 Cube Shadow Map 리소스를 추가한다.
     · ID3D12Resource: TEXTURE2D_ARRAY (ArraySize=6, DXGI_FORMAT_D32_FLOAT)
     · 각 면에 대해 DSV 6개 (depth write용) 생성
     · SRV 1개 (TextureCube로 전체 6면 샘플링) 생성
     · 최대 MAX_POINT_SHADOW_LIGHTS = 4개 Point light shadow 지원
   - CreateCubeShadowMaps(), RecreateCubeShadowMaps() 메서드 추가

2. 6-pass Shadow Depth 렌더링을 구현한다.
   - Renderer::RenderScene()의 Shadow Depth Pass 루프를 확장한다.
   - Point light 분기에서 기존 'continue' 스킵을 제거하고 6-pass를 실행한다.
     · 6면 방향: +X(-Z up), -X(-Z up), +Y(-X up), -Y(+X up), +Z(-Z up), -Z(-Z up)
       (D3D12 TextureCube 면 순서: +X, -X, +Y, -Y, +Z, -Z)
     · 각 면 View 행렬: XMMatrixLookAtLH(lightPos, lightPos+faceDir, faceUp)
     · Projection: XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, nearPlane, farPlane)
       (XM_PIDIV2 = 90°, aspect=1.0, farPlane = m_sceneDiagonal * 3.0f)
   - D3D12Context::BeginShadowPass(shadowIdx, faceIndex)와
     EndShadowPass(shadowIdx, faceIndex) 오버로드 또는 별도 BeginCubeShadowPass() 추가

3. HLSL PBR.hlsl을 확장한다.
   - TextureCube 바인딩 추가:
     TextureCube PointShadowMap0 : register(t13);
     TextureCube PointShadowMap1 : register(t14);
     TextureCube PointShadowMap2 : register(t15);
     TextureCube PointShadowMap3 : register(t16);
   - SamplePointShadow(uint idx, float3 lightToPixel, float depth) 함수 구현:
     · lightToPixel = normalize(pixelWorldPos - lightPos)
     · TextureCube에서 SampleCmpLevelZero 또는 Sample + 수동 depth 비교
     · depth = length(pixelWorldPos - lightPos) / farPlane (정규화)
   - CalcShadow()를 Point light 타입에서 SamplePointShadow()를 호출하도록 분기

4. LightConstants와 LightData를 확장한다.
   - Light.h: shadowType 필드 추가 (0=Texture2D, 1=TextureCube)
   - LightManager::BuildLightConstants(): Point light castShadow에 pointShadowIdx 할당
   - HLSL LightData 구조체: shadowType 필드 추가 (uint)

5. Root Signature를 확장한다.
   - D3D12Context의 Root Signature에 t13~t16 SRV 슬롯 추가
   - 라이팅 패스에서 Cube Shadow Map SRV 바인딩

6. 성능을 관리한다.
   - 최대 4개 Point light shadow 허용 (6pass × 4 = 24 depth pass/frame)
   - LightCuller와 연동: shadow casting Point light도 거리 기반 culling 적용
   - DebugHUD에 Cube Shadow Pass 수 표시

빌드하여 모든 테스트가 통과하고, Sponza 씬에서 횃불 위치(castShadow=true Point light)의
구면 그림자가 정상 렌더링되는지 확인하라.
```

---

## Prompt 35: Skeletal Animation

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 35 섹션을 참조하여 Phase 35를 구현하라.
Part A(Node Transform Animation)를 먼저 완성한 뒤 Part B(Skeletal Animation)를 구현한다.

=== Part A: Node Transform Animation (G-08) ===

1. Animation 데이터 구조를 만든다.
   - src/Asset/Animation.h를 신규 생성한다.
     · struct Keyframe<T> { float time; T value; }
     · struct AnimationChannel {
           SceneNode* targetNode;
           enum Property { Translation, Rotation, Scale } property;
           std::vector<Keyframe<XMFLOAT3>> posKeys;   // Translation/Scale
           std::vector<Keyframe<XMFLOAT4>> rotKeys;   // Rotation (quaternion)
           enum Interpolation { Linear, Step, CubicSpline } interpolation;
       }
     · struct AnimationClip { std::string name; float duration; std::vector<AnimationChannel> channels; }
   - 보간 함수 구현:
     · Linear: XMVectorLerp / XMQuaternionSlerp
     · Step: 현재 시간 이하의 마지막 키프레임 값 반환
     · CubicSpline: glTF cubic spline 공식 적용 (in-tangent, value, out-tangent 삼중 구조)

2. SceneLoader에 애니메이션 로딩을 추가한다.
   - SceneLoader::LoadAnimations(const aiScene*, SceneGraph*) 메서드 신규 추가
   - aiAnimation → AnimationClip 변환:
     · aiNodeAnim::mPositionKeys → Translation 채널
     · aiNodeAnim::mRotationKeys → Rotation 채널 (aiQuaternion → XMFLOAT4)
     · aiNodeAnim::mScalingKeys → Scale 채널
   - 채널 target name(aiNodeAnim::mNodeName) → SceneNode* 매핑
     (SceneGraph에서 name으로 노드 검색)
   - Engine::LoadScene() 에서 LoadAnimations() 호출

3. AnimationController를 구현한다.
   - src/Core/AnimationController.h/.cpp를 신규 생성한다.
   - AnimationController::Update(float dt):
     · m_currentTime += dt * m_playbackSpeed
     · 루프: m_currentTime >= clip.duration 시 0으로 리셋
     · 각 채널의 보간값 계산 → SceneNode의 Transform 갱신
       (SetLocalTranslation / SetLocalRotation / SetLocalScale 또는 SetLocalMatrix)
   - Play(), Pause(), SetClip(AnimationClip*), SetPlaybackSpeed(float) 메서드
   - Engine::Update()에서 AnimationController::Update(dt) 호출
   - "Animation" 메뉴에서 클립 선택 가능 (씬 로드 후 클립 목록 동적 생성)

=== Part B: Skeletal Animation (G-09) ===

4. Skeleton / Skin 데이터 구조를 만든다.
   - src/Asset/Skeleton.h를 신규 생성한다.
     · struct Bone { std::string name; int parentIndex; XMFLOAT4X4 inverseBindMatrix; }
     · struct Skeleton { std::vector<Bone> bones; int FindBone(const std::string& name) const; }
     · struct Skin { Skeleton* skeleton; std::vector<int> jointIndices; }
   - SceneLoader에서 aiMesh::mBones 배열을 순회하여 Skeleton/Skin 생성:
     · aiBone::mName → Bone.name
     · aiBone::mOffsetMatrix → Bone.inverseBindMatrix (Assimp 전치 주의)
     · aiBone::mWeights → per-vertex joint index + weight 저장
   - Mesh 구조체에 Skin* skin 포인터 추가

5. Vertex 포맷을 확장한다.
   - Vertex 구조체에 추가:
     · XMUINT4 joints  (JOINTS_0)  — 영향을 주는 본 인덱스 최대 4개
     · XMFLOAT4 weights (WEIGHTS_0) — 각 본의 가중치 (합=1.0)
   - D3D12 Input Layout에 슬롯 추가:
     · JOINTS_0:  R8G8B8A8_UINT,  offset = sizeof(이전 필드까지)
     · WEIGHTS_0: R32G32B32A32_FLOAT
   - HLSL VSInput 구조체에 동일 필드 추가
   - static_assert로 sizeof(Vertex) 및 각 멤버 오프셋 검증 갱신

6. GPU Skinning 셰이더를 구현한다.
   - PBR.hlsl에 Skinning 지원 추가:
     · cbuffer SkinCB : register(b4) { float4x4 jointMatrices[128]; uint jointCount; }
     · VSInput에 uint4 joints : JOINTS_0; float4 weights : WEIGHTS_0; 추가
     · VSMain 내 스키닝 계산:
       float4x4 skinMatrix =
           weights.x * jointMatrices[joints.x] +
           weights.y * jointMatrices[joints.y] +
           weights.z * jointMatrices[joints.z] +
           weights.w * jointMatrices[joints.w];
       float4 skinnedPos = mul(float4(input.position, 1.0f), skinMatrix);
       float4 worldPos = mul(skinnedPos, World);
       (Normal, Tangent도 동일 skin matrix로 변환)
     · 스킨 메시 여부: SkinCB의 jointCount > 0 이면 스키닝 적용
   - Renderer: 스킨 메시 드로우콜 전 SkinCB 바인딩 (jointCount=0이면 identity 바인딩)

7. AnimationController를 Part B 연동으로 확장한다.
   - Part A에서 각 채널이 SceneNode Transform을 갱신한 뒤,
     SceneGraph를 순회하여 bone world matrix 배열(joint palette)을 계산:
     · boneWorldMatrix[i] = parentBoneWorldMatrix * bone[i].localTRS
     · jointMatrix[i] = inverseBindMatrix[i] * boneWorldMatrix[i]
   - 계산된 jointMatrix 배열을 SkinCB Upload Buffer에 복사

빌드하여 CesiumMan.glb 또는 RiggedFigure.glb에서 노드 TRS 애니메이션과
스킨 메시 애니메이션이 정상 재생되는지 확인하라.
모든 유닛·스모크 테스트가 통과해야 한다.
```

---

## Prompt 36: RRScenePreprocessor 확장 — Skeletal Animation 지원

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 36 섹션과 GoodToPreprocess.md를 참조하여 Phase 36를 구현하라.
이 단계는 Phase 33에서 추가된 Skeleton/Skin/Animation 데이터를 .rrscene 포맷에 통합하여
전처리기와 렌더링 앱 고속 로딩 경로를 모두 확장한다.

1. .rrscene 포맷을 v2로 버전 업한다 (src/Asset/RRSceneFormat.h 수정).
   - Header.version: 1 → 2
   - Vertex 구조체에 joints(uint32×4), weights(float×4) 필드 추가
     · 스킨 메시: isSkinned 플래그 = true, 해당 필드 포함
     · 비스킨 메시: isSkinned = false, 해당 필드 생략 (파일 크기 절약)
   - Skeleton Section 추가 (SectionType::Skeleton):
     · uint32 boneCount
     · 본별: 이름(문자열), parentIndex(int32), inverseBindMatrix(float 4×4)
     · Skin 수, Skin별: skeletonIndex, jointIndices 배열
   - Animation Section 추가 (SectionType::Animation):
     · uint32 clipCount
     · 클립별: 이름, 재생 시간(float), 채널 수
     · 채널별: targetNodeIndex, Property(TRS enum), Interpolation enum,
               키프레임 수, 키프레임 배열(float time + float3/float4 value)
   - 하위 호환: version == 1 파일 로딩 시 Skeleton/Animation 섹션 미존재 → 관련 객체 미생성

2. RRScenePreprocessor를 확장한다.
   - Assimp aiMesh::mBones 순회:
     · aiBone::mName, mOffsetMatrix → Bone 생성 (Assimp 전치 주의)
     · aiBone::mWeights → per-vertex joint index + weight 기록
     · 스킨 메시 Vertex에 joints/weights 기록 후 Skeleton Section 직렬화
   - Assimp aiAnimation 순회:
     · aiNodeAnim::mPositionKeys → Translation 키프레임 (XMFLOAT3 + time)
     · aiNodeAnim::mRotationKeys → Rotation 키프레임 (XMFLOAT4 quaternion + time)
     · aiNodeAnim::mScalingKeys → Scale 키프레임 (XMFLOAT3 + time)
     · Interpolation: Assimp aiAnimBehaviour → Linear/Step/CubicSpline 매핑
     · target name → 노드 인덱스 매핑 후 Animation Section 직렬화
   - 버전 2 헤더와 전체 섹션 목록(Scene/Mesh/Material/Texture/Light/Skeleton/Animation) 기록

3. 렌더링 앱 고속 로딩 경로를 확장한다.
   - SceneLoader::LoadRRScene() 내 version 분기:
     · version == 2: Skeleton/Animation 섹션 파싱
       - Skeleton Section → Skeleton/Skin 객체 생성 → Mesh.skin 포인터 연결
       - Animation Section → AnimationClip 배열 생성 → AnimationController 등록
     · version == 1: 기존 로직 유지 (Skeleton/Animation 없이 로딩)
   - 스킨 Vertex(joints/weights 포함) → GPU VB 업로드 (Input Layout v2 사용)
   - AnimationController에 클립 자동 등록, 첫 번째 클립 자동 재생 시작

4. 동작을 검증한다.
   - CesiumMan.glb를 RRScenePreprocessor에 입력 → CesiumMan.rrscene(v2) 생성
   - 렌더링 앱에서 CesiumMan.gltf 열기 → v2 고속 경로로 스켈레탈 애니메이션 재생 확인
   - 비애니메이션 씬의 v1 .rrscene 파일도 계속 정상 로딩되는지 확인 (하위 호환)
   - v1과 v2 rrscene 모두 Assimp 표준 경로 렌더링 결과와 동일한지 비교

빌드하여 모든 테스트가 통과하고, CesiumMan.rrscene에서 스켈레탈 애니메이션이
표준 로딩 경로와 동일하게 재생되는지 확인하라. v1 파일 하위 호환을 유지해야 한다.
```

---

## Prompt 37: Deferred Rendering — G-Buffer 기반 렌더링 파이프라인

```
PRD.md, PLAN.md(Phase 03), CLAUDE.md를 참조하여 Phase 37을 구현하라.
기존 Forward Rendering 파이프라인을 Deferred Shading으로 전환한다.
Alpha Blend 오브젝트는 Forward 패스를 유지하는 Hybrid 구조를 적용한다.

1. G-Buffer MRT 생성 (D3D12Context):
   - RT0: R8G8B8A8_UNORM_SRGB — Albedo(RGB) + Metallic(A)
   - RT1: R16G16B16A16_FLOAT — World Normal(XYZ) + Roughness(A)
   - RT2: R8G8B8A8_UNORM — Emissive(RGB) + AO(A)
   - Depth: D32_FLOAT (SRV 겸용), OMSetRenderTargets MRT 바인딩

2. Geometry Pass: Opaque → G-Buffer Fill, Shadow Pass 선행, Alpha Mask clip() 적용

3. Lighting Pass: Full-Screen Quad, G-Buffer SRV + Shadow SRV 바인딩, Cook-Torrance BRDF, HDR RT 출력

4. Forward+ 투명 패스: Alpha Blend 메시는 기존 Forward로 HDR RT 합성

5. G-Buffer 디버그 뷰 (Render 메뉴): Albedo / Normal / MetalRoughness / Depth 시각화

빌드하여 G-Buffer MRT, Deferred Lighting Pass, Alpha Blend 합성을 확인하라.
```

---

## Prompt 38: HDR Pipeline + Tone Mapping

```
PRD.md, PLAN.md(Phase 38), CLAUDE.md를 참조하여 Phase 38을 구현하라.

1. HDR Render Target: DXGI_FORMAT_R16G16B16A16_FLOAT (Lighting Pass 출력)
2. Tone Mapping Pass: Reinhard 또는 ACES Filmic — Render 메뉴 선택
3. Auto-Exposure: Compute Shader로 평균 Luminance → EV 자동 조절
4. sRGB 출력: R8G8B8A8_UNORM_SRGB SwapChain
5. DebugHUD: Tone Mapping 모드, Luminance, EV 표시

빌드하여 HDR RT, Tonemapping 전환, Auto-Exposure를 확인하라.
```

---

## Prompt 39: SSAO (Screen Space Ambient Occlusion)

```
PRD.md, PLAN.md(Phase 39), CLAUDE.md를 참조하여 Phase 39를 구현하라.

1. SSAO Buffer: R8_UNORM 렌더 타겟
2. SSAO Pass: Hemisphere Sample Kernel(16~64개) + 노이즈 텍스처 랜덤화
   - Depth → View-Space Position, G-Buffer Normal → View-Space
   - 반구형 샘플로 주변 깊이 비교 → Raw AO
3. Blur Pass: Bilateral Blur (Depth/Normal 경계 보존), 수평→수직 2패스
4. Lighting Pass 통합: AO × Ambient Light
5. Optimization 메뉴: SSAO on/off, AO Buffer 시각화

빌드하여 SSAO Buffer, Blur, Lighting 통합, on/off 비교를 확인하라.
```

---

## Prompt 40: Bloom + Post-Processing 파이프라인

```
PRD.md, PLAN.md(Phase 40), CLAUDE.md를 참조하여 Phase 40을 구현하라.

1. Ping-Pong Buffer 프레임워크: PostProcessor 클래스, HDR RT 2개 교대
2. Bright Pass: Luminance 임계값 이상 픽셀 추출
3. Gaussian Blur Pyramid: 6단계 다운샘플→업샘플 (Dual Kawase Blur)
4. Bloom Composite: Additive Blend
5. 파이프라인 순서: Lighting → SSAO → Bloom → Tone Mapping → TAA → sRGB
6. 메뉴: Bloom on/off, Threshold, Intensity

빌드하여 Bloom 효과, Post-Processing 프레임워크를 확인하라.
```

---

## Prompt 41: TAA (Temporal Anti-Aliasing)

```
PRD.md, PLAN.md(Phase 41), CLAUDE.md를 참조하여 Phase 41을 구현하라.

1. Jitter Matrix: 8~16프레임 Halton Sequence로 투영 행렬 서브픽셀 오프셋
2. Motion Vector Buffer: R16G16_FLOAT, 정적(카메라)/동적(WorldMatrix) Reprojection
3. History Buffer: 이전 프레임 TAA 출력 SRV
4. TAA Resolve: Current + History 블렌딩(α≈0.1~0.15)
   - Variance Clipping(3×3 AABB clip), Velocity 기반 가중치 감소
5. 메뉴: TAA / MSAA / None

빌드하여 TAA on/off, 고스팅 억제, 정적 씬 품질을 확인하라.
```

---

## Prompt 42: Motion Blur + Depth of Field

```
PRD.md, PLAN.md(Phase 42), CLAUDE.md를 참조하여 Phase 42를 구현하라.

1. Motion Blur: Tile-based Max Velocity (Compute) → 속도 방향 N샘플 평균, 셔터 속도 스케일
2. Depth of Field:
   - CoC: Depth → CoC 반경 (Focus Distance, F-Number)
   - Bokeh Blur: Separable Gaussian 또는 Hexagonal Bokeh, Near/Far 분리
3. Camera 메뉴: F-Number, Focal Length, Focus Distance, Motion Blur/DoF on/off

빌드하여 Motion Blur per-object, DoF CoC 블러, 메뉴 파라미터를 확인하라.
```

---

## Prompt 43: SSR (Screen Space Reflections) + Refraction

```
PRD.md, PLAN.md(Phase 43), CLAUDE.md를 참조하여 Phase 43을 구현하라.

1. SSR: G-Buffer Normal+Depth → 반사 Ray, Hi-Z Raymarching, Fresnel, Roughness 블러, Envmap Fallback
2. Refraction: Alpha Blend 오브젝트에 IOR 기반 UV 오프셋, Depth 비교로 penetration 방지
3. Material: IOR 파라미터 추가
4. Optimization 메뉴: SSR on/off

빌드하여 SSR 반사, Roughness 블러, Fresnel, Refraction을 확인하라.
```

---

## Prompt 44: Screen Space Subsurface Scattering (SSSSS)

```
PRD.md, PLAN.md(Phase 44), CLAUDE.md를 참조하여 Phase 44를 구현하라.

1. Material 확장: subsurfaceColor(XMFLOAT3) + scatterWidth(float)
2. SSS Pass: Stencil 마스크, 6-weight Gaussian × 3채널(R>G>B 확산 폭), 수평→수직 2패스
3. Lighting Pass 통합: SSS 결과를 Diffuse에 합성
4. Optimization 메뉴: SSSSS on/off, scatterWidth 조정

빌드하여 SSS on/off, RGB 채널 확산 폭, Stencil 마스크를 확인하라.
```

---

## Prompt 45: Global Illumination — DDGI (Dynamic Diffuse GI)

```
PRD.md, PLAN.md(Phase 45), CLAUDE.md를 참조하여 Phase 45를 구현하라.

1. Probe Grid: 씬 AABB 내 3D Grid (8×4×8=256 Probe), Octahedral Map 텍스처
2. Probe Update: DXR 가능 시 Radiance Ray, 미지원 시 정적 Reflection Capture Fallback
3. Probe Sampling: 삼선형 보간, SH2 Irradiance 샘플링
4. Lighting Pass: Indirect Diffuse += Probe Irradiance × Albedo / π
5. 디버그 뷰: Probe 위치·Irradiance 시각화

빌드하여 Probe 배치, 간접광 표현, 디버그 시각화를 확인하라.
```

---

## Prompt 46: DXR Hybrid Ray Tracing

```
PRD.md, PLAN.md(Phase 46), CLAUDE.md를 참조하여 Phase 46을 구현하라.
DXR Tier 1.1 미지원 시 PCF Shadow/SSR로 자동 폴백해야 한다.

1. DXR 인프라: Feature 감지, DXR PSO(RayGen/ClosestHit/Miss/AnyHit), BLAS(정적/동적), TLAS(매 프레임), ShaderTable
2. Ray-Traced Shadow: 광원별 Shadow Ray, Alpha AnyHit, PCF 대체 (메뉴 토글)
3. Ray-Traced Reflection: Normal+Roughness → Cone Sampling, 재귀 1~2레벨
4. GI 연동: DDGI Probe Update에 DXR Ray 활용
5. Denoiser 연동: Phase 48 Denoiser 또는 Temporal Accumulation
6. 폴백: DXR 미지원 시 PCF/SSR/DDGI Static

빌드하여 TLAS/BLAS, RT Shadow, RT Reflection, Hybrid 전환을 확인하라.
```

---

## Prompt 47: Nanite-style Virtual Geometry

```
PRD.md, PLAN.md(Phase 47), CLAUDE.md를 참조하여 Phase 47을 구현하라.
Mesh Shader 미지원 시 기존 DrawIndexedInstanced + LODSelector로 폴백해야 한다.

1. Meshlet 분할: ~128 삼각형, 바운딩 스피어 + 노말 Cone
2. Mesh Shader 파이프라인: Amplification(Frustum/Back-face Culling) + Mesh Shader(삼각형 출력)
3. Cluster LOD Hierarchy: 심플리피케이션으로 LOD 트리, GPU Projected Error 기준 전환
4. GPU-Driven: Compute → DrawArgs Buffer → ExecuteIndirect()
5. 디버그 뷰: Meshlet 색상, LOD 레벨 시각화
6. 폴백: Mesh Shader 미지원 시 기존 DrawIndexedInstanced

빌드하여 Meshlet 시각화, Amplification+Mesh Shader, GPU-Driven IndirectDraw를 확인하라.
```

---

## Prompt 48: Neural Upscaling (DLSS/FSR) + Neural Denoising

```
PRD.md, PLAN.md(Phase 48), CLAUDE.md를 참조하여 Phase 48을 구현하라.

1. FSR 3: FidelityFX SDK 연동, Color+Depth+MotionVector → 업스케일, Quality Mode 메뉴
2. DLSS 3 (선택): Streamline SDK, RTX 감지, 미지원 시 FSR 폴백
3. Neural Denoising: NRD SDK 또는 자체 Temporal Accumulation Denoiser
4. DebugHUD: Upscaling 모드, 렌더/출력 해상도, Denoiser 종류

빌드하여 FSR 업스케일, Quality Mode 전환, Denoiser를 확인하라.
```

---

## Prompt 49: Phase 03 코드 리뷰, 최적화, 버그 수정 & 아키텍처 문서화

```
PRD.md, PLAN.md(Phase 49), CLAUDE.md를 참조하여 Phase 49를 수행하라.
Phase 33~48에서 추가된 모든 고급 렌더링 기법의 코드 품질을 점검하고,
성능을 최적화하며, 버그를 수정하고, ARCHITECTURE.md를 완성한다.

1. 코드 리뷰를 수행한다.
   - Dead code 제거, include 순서 정리, 네이밍 일관성 검증
   - G-Buffer MRT 바인딩 순서 및 포맷 일관성 확인
   - DXR ShaderTable 빌드 로직, BLAS/TLAS 갱신 주기 검토
   - Mesh Shader / Amplification Shader 경계 조건 검토
   - Neural Upscaling SDK 연동 초기화 순서 확인
   - D3D12 Debug Layer 경고 0건 목표 (리소스 상태 전이, lifetime 위반 등)

2. 성능 최적화를 수행한다.
   - PIX for Windows 또는 D3D12 Timestamp Query로 각 렌더 패스 비용 측정
   - G-Buffer 포맷 최적화 (RT1: R10G10B10A2 축소 검토)
   - SSAO 샘플 수 / TAA 블렌딩 계수 / Bloom 피라미드 단계 수 튜닝
   - Hi-Z Mip chain 생성 비용 측정 및 최적화
   - DXR TLAS Refit (정적 BLAS 재사용, 동적만 Rebuild)
   - Nanite Meshlet 크기 및 LOD Projected Error 임계값 튜닝
   - Denoiser Temporal 수렴 속도 vs 고스팅 트레이드오프 조정

3. 버그를 수정한다.
   - 렌더 패스 간 리소스 상태 전이 누락 (D3D12_RESOURCE_STATE_*)
   - TAA 씬 전환 직후 History Buffer 초기화 누락
   - SSR 화면 경계 아티팩트 (Fade 파라미터 튜닝)
   - DDGI Probe Irradiance 튀는 현상 (Hysteresis 파라미터)
   - DXR AnyHit 투명 오브젝트 투과율 계산 오류
   - FSR/DLSS Motion Vector 스케일 불일치

4. ARCHITECTURE.md를 완성한다.
   - 전체 렌더 파이프라인 다이어그램 (Phase 01~48 누적 아키텍처)
   - 렌더 패스 순서 및 리소스 의존성 다이어그램
     (Shadow → G-Buffer → Lighting → SSAO → SSR → Bloom → TAA → Tone Mapping → Upscale)
   - 주요 모듈 간 의존성 (Engine / Renderer / SceneGraph / RHI / Asset / Lighting)
   - G-Buffer 레이아웃, Descriptor Heap 구조, Root Signature 레지스터 맵
   - DXR 가속 구조 (BLAS/TLAS) 업데이트 주기 및 ShaderTable 구성
   - Meshlet / GPU-Driven 렌더링 흐름 (Compute → DrawArgs → ExecuteIndirect)
   - Neural Upscaling 렌더 해상도 관리 흐름
   - 스레딩 모델: 메인 렌더 스레드 / Compute Queue / Copy Queue / Worker Thread

5. 최종 벤치마크를 수행한다.
   - Sponza + Bistro: Full Phase 03 파이프라인(Deferred + SSAO + Bloom + TAA + SSR + DDGI) 60fps 목표
   - DXR 활성 시 RT Shadow + RT Reflection 포함 성능 측정
   - FSR 3 활성 시 (렌더 해상도 67%) 품질 vs 성능 비교
   - 모든 유닛 테스트 + 스모크 테스트 통과 확인

D3D12 Debug Layer 경고 0건, 주요 패스 타임스탬프 측정 완료,
ARCHITECTURE.md 작성 완료, Sponza+Bistro 벤치마크 결과 기록 후 Phase 03 완료 선언.
```
