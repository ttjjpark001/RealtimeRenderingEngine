# PLAN: 실시간 렌더링 엔진 구현 계획

## 프로젝트 구조

```
RealtimeRenderingEngine/
├── RealtimeRenderingEngine.sln     # VS2022 솔루션
├── vcpkg.json                      # vcpkg 의존성 (Google Test)
├── .gitignore
├── PRD.md / PLAN.md / PROMPT.md / CLAUDE.md
├── src/
│   ├── RREngine.vcxproj            # 엔진 프로젝트 (Windows Application)
│   ├── main.cpp                    # WinMain 엔트리 포인트
│   ├── Core/
│   │   ├── Engine.h / .cpp         # 엔진 메인 루프
│   │   └── Types.h                 # 공용 타입 + DirectXMath 별칭
│   ├── Math/
│   │   └── MathUtil.h              # DirectXMath 헬퍼/유틸리티
│   ├── Platform/
│   │   └── Win32/
│   │       ├── Win32Window.h / .cpp # Win32 윈도우 생성/관리
│   │       ├── Win32Input.h / .cpp  # 마우스/키보드 입력
│   │       └── Win32Menu.h / .cpp   # Win32 메뉴바 (오브젝트 선택)
│   ├── RHI/
│   │   ├── RHIDevice.h             # 추상 디바이스 인터페이스
│   │   ├── RHIBuffer.h             # 추상 버퍼 인터페이스
│   │   ├── RHIContext.h            # 추상 렌더링 컨텍스트
│   │   └── D3D12/
│   │       ├── D3D12Device.h / .cpp     # ID3D12Device 래핑
│   │       ├── D3D12Context.h / .cpp    # Command Queue/List 관리
│   │       ├── D3D12Buffer.h / .cpp     # D3D12 리소스 버퍼
│   │       ├── D3D12SwapChain.h / .cpp  # DXGI Swap Chain
│   │       ├── D3D12PipelineState.h / .cpp # PSO, Root Signature
│   │       └── D3D12DescriptorHeap.h / .cpp # 디스크립터 힙
│   ├── Renderer/
│   │   ├── Vertex.h                # 버텍스 데이터 구조
│   │   ├── Mesh.h / .cpp           # 메시 (버텍스 + 인덱스)
│   │   ├── MeshFactory.h / .cpp    # 기본 도형 생성 + 면 색상 그래프 컬러링
│   │   ├── FaceColorPalette.h     # 8색 팔레트 정의 + 인접면 색상 배정
│   │   ├── Renderer.h / .cpp       # 렌더러 (RHI 사용)
│   │   └── DebugHUD.h / .cpp       # 화면 상태 표시 오버레이
│   ├── Scene/
│   │   ├── SceneNode.h / .cpp      # Scene Graph 노드
│   │   ├── SceneGraph.h / .cpp     # Scene Graph 관리
│   │   ├── Transform.h / .cpp      # Transform 컴포넌트
│   │   └── Camera.h / .cpp         # 카메라 (Perspective/Orthographic)
│   └── Lighting/
│       └── PointLight.h / .cpp     # 포인트 광원 (위치, 색상, 감쇠)
└── tests/
    ├── RREngineTests.vcxproj       # 테스트 프로젝트 (Console Application)
    ├── test_main.cpp               # Google Test 엔트리
    ├── unit/
    │   ├── test_MathUtil.cpp       # DirectXMath 유틸리티 테스트
    │   ├── test_Transform.cpp
    │   ├── test_SceneGraph.cpp
    │   ├── test_FaceColoring.cpp   # 면 색상 인접 규칙 검증
│   └── test_Camera.cpp        # 카메라 투영/전환 테스트
    └── smoke/
        ├── test_EngineInit.cpp
        └── test_RHIBackend.cpp
```

## 기술 선택

| 구분 | 선택 | 비고 |
|------|------|------|
| 렌더링 API | DirectX 12 | ID3D12Device, Command Queue/List, PSO |
| 수학 라이브러리 | DirectXMath | XMVECTOR, XMMATRIX (SIMD 최적화) |
| 셰이더 | HLSL | vs_5_1 / ps_5_1, 런타임 컴파일 or 사전 컴파일 |
| 빌드 | VS2022 Solution (v143) | x64 Debug/Release |
| 테스트 | Google Test (vcpkg) | `vcpkg install gtest:x64-windows` |

## 구현 단계

### Phase 1: 프로젝트 기반 구축
**목표**: VS 솔루션과 기본 윈도우 생성

1. VS2022 솔루션/프로젝트 구성 (C++17, D3D12 링크 설정)
2. `Types.h` — 공용 타입 정의 + DirectXMath 타입 별칭
3. `Win32Window` — CreateWindowEx로 윈도우 생성, 메시지 루프
4. 윈도우 리사이즈 처리 (`WM_SIZE`, `WM_SIZING`)
5. 윈도우 모드 전환 지원: SetWindowed(width, height), SetFullscreen(), 스타일 전환
6. `Engine` — 기본 게임 루프 (Init → Update → Render → Shutdown)

**완료 기준**: 윈도우가 뜨고 마우스로 자유롭게 리사이즈 가능

### Phase 2: DirectXMath 유틸리티
**목표**: DirectXMath를 활용한 수학 헬퍼 구축

1. `MathUtil.h` — DirectXMath 편의 함수 래핑
   - XMFLOAT3/4 ↔ XMVECTOR 변환 헬퍼
   - TRS 행렬 조합 함수 (Translation × Rotation × Scale)
   - 오일러 각도 → 회전 행렬 변환
   - 근사 비교(epsilon) 유틸리티
2. **유닛 테스트**: 행렬 곱셈, 회전, TRS 조합 검증

**완료 기준**: 모든 수학 유닛 테스트 통과

### Phase 3: RHI 추상화 + DirectX 12 백엔드
**목표**: RHI 인터페이스 정의 및 D3D12 구현

1. `IRHIDevice` — 디바이스 초기화/해제 인터페이스
2. `IRHIBuffer` — 버텍스/인덱스 버퍼 추상화
3. `IRHIContext` — BeginFrame, EndFrame, DrawPrimitives, DrawText
4. `D3D12Device` — ID3D12Device, DXGI Factory, Adapter 열거
5. `D3D12SwapChain` — IDXGISwapChain4, 더블/트리플 버퍼링
6. `D3D12Context` — Command Queue, Command Allocator, Command List, Fence
7. `D3D12DescriptorHeap` — RTV, DSV, CBV/SRV/UAV 힙
8. `D3D12PipelineState` — Root Signature, PSO (기본 컬러 셰이더)
9. `D3D12Buffer` — Committed Resource로 버텍스/인덱스 버퍼 생성
10. 기본 HLSL 셰이더 (Position+Color 입력, 단색 출력)
11. OnResize: SwapChain ResizeBuffers, RTV 재생성
12. **스모크 테스트**: D3D12 디바이스 초기화/해제 사이클

**완료 기준**: D3D12로 화면 Clear + 리사이즈 정상 동작

### Phase 4: Vertex & Mesh
**목표**: 버텍스 데이터 구조와 메시 정의

1. `Vertex` — XMFLOAT3 position + XMFLOAT4 color + XMFLOAT3 normal
2. D3D12 Input Layout 정의 (POSITION, COLOR, NORMAL 시맨틱)
3. `Mesh` — Vertex 배열 + Index 배열
4. `FaceColorPalette` — 8색 팔레트 정의 및 인접면 색상 배정
   - 팔레트: Red, Green, Blue, Cyan, Magenta, Yellow, Black, White
   - AssignFaceColors(faceAdjacency) → 그래프 컬러링 알고리즘으로 색상 배정
   - 이웃한 면(edge 공유)은 반드시 다른 색상
5. `MeshFactory` — 기본 도형 생성 팩토리 (FaceColorPalette 사용)
   - CreateSphere(segments, rings) — UV 구, 인접 패치별 색상 구분
   - CreateTetrahedron() — 정사면체 (4면, 4색 필요)
   - CreateCube() — 정육면체 (6면, 인접면 다른 색)
   - CreateCylinder(segments, height) — 실린더, 옆면 스트립 교차 색상
   - 각 면의 모든 Vertex에 동일 색상 (flat shading)
6. D3D12 버텍스/인덱스 버퍼 업로드 (Upload Heap → Default Heap)
7. **유닛 테스트**: 4종 도형의 인접면 색상 중복 여부 검증

**완료 기준**: 인접면이 다른 색상인 큐브가 D3D12로 렌더링

### Phase 5: 렌더링 파이프라인 강화
**목표**: Depth Stencil Buffer, 상수 버퍼 관리, 빌드 타임 셰이더 컴파일 등 D3D12 렌더링 인프라 완성 (Phase 1~4 완료 후 진행)

1. Depth Stencil Buffer 생성 및 관리:
   - `D3D12Device`에서 DXGI_FORMAT_D24_UNORM_S8_UINT 형식 Depth Stencil Buffer 생성
   - DSV Heap (D3D12_DESCRIPTOR_HEAP_TYPE_DSV) 생성 및 DSV 생성
   - `D3D12Context::Clear`에서 `OMSetRenderTargets`에 DSV 포함, Depth Buffer 클리어
   - `OnResize` 시 Depth Stencil Buffer 및 DSV 재생성

2. CBV DescriptorHeap + Upload Buffer 기반 Constant Buffer 관리:
   - D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV 힙 생성 (shader-visible)
   - Upload Heap에 256바이트 정렬 Constant Buffer 생성 (per-frame)
   - `struct PerObjectConstants { XMFLOAT4X4 world; XMFLOAT4X4 viewProj; }`
   - 매 프레임 Map/Unmap으로 CPU → GPU 데이터 업데이트
   - Root Signature에 CBV descriptor table 추가
   - `DrawPrimitives`에서 CBV 바인딩

3. HLSL 셰이더를 .cso 파일로 빌드 타임 컴파일:
   - `RREngine.vcxproj`에서 src/Shaders/ HLSL 파일 빌드 액션을 VS HLSL Compiler로 설정
   - VS/PS Entry Point, Shader Model, 출력 경로 설정 (`$(OutDir)Shaders/`)
   - `D3D12PipelineState`에서 D3DCompileFromFile 제거 → `D3DReadFileToBlob`으로 .cso 로드

4. Vertex 구조체 정렬 빌드 타임 검증:
   - `Vertex.h`에 `static_assert(offsetof(Vertex, position) == 0)` 등 추가
   - `static_assert(sizeof(Vertex) == 40)` 추가

**완료 기준**: DSV로 깊이 테스트 정상 동작, CBV로 행렬 전달, .cso 빌드 및 로드 성공, static_assert 통과

### Phase 6: Scene Graph
**목표**: 계층적 오브젝트 관리 시스템

1. `Transform` — XMFLOAT3 position/rotation/scale → XMMATRIX 생성
2. `SceneNode` — Transform + Mesh 참조 + 부모/자식 포인터
3. `SceneGraph` — 루트 노드, 노드 추가/제거, 트리 순회
4. 부모-자식 Transform 연쇄 (World Matrix = Parent × Local)
5. **유닛 테스트**: 노드 추가/제거, 계층 Transform 계산

**완료 기준**: 부모 회전 시 자식도 함께 회전

### Phase 7: 상태 표시 HUD
**목표**: 화면 왼쪽 상단에 렌더링 통계 오버레이

1. `DebugHUD` — 상태 정보를 화면에 텍스트로 렌더링
2. 통계 수집: FPS, 해상도, 종횡비, 폴리곤 수, 초당 폴리곤 처리 속도
3. `IRHIContext::DrawText()` 메서드 추가
4. D3D12에서 텍스트 렌더링: DirectWrite + D2D interop 또는 스프라이트 폰트
5. 리사이즈 시 해상도/종횡비 값 실시간 갱신

**완료 기준**: 화면 좌상단에 FPS, 해상도, 종횡비, 폴리곤 수, 폴리곤/초가 표시

### Phase 8: 메뉴 (오브젝트 선택 + 애니메이션 제어)
**목표**: Win32 메뉴를 통한 오브젝트 전환 및 애니메이션 시작/멈춤

1. `Win32Menu` — Win32 메뉴바 생성 및 WM_COMMAND 처리
2. 메뉴 구조:
   - "View" → 800x450 / 960x540(기본) / Full Screen
   - "Object" → Sphere / Tetrahedron / Cube(기본) / Cylinder
   - "Animation" → Play / Pause (토글)
3. 오브젝트 선택 시 CheckMenuRadioItem으로 체크 표시
4. Animation → Play/Pause 선택 시 체크 표시 전환
5. Space 키로도 애니메이션 시작/멈춤 토글 (WM_KEYDOWN 처리)
6. View 메뉴 선택 시 Win32Window::SetWindowed(w, h) 또는 SetFullscreen() 호출
7. Esc 키로 Full Screen에서 이전 윈도우 모드로 복귀
8. Engine에 콜백 연결:
   - OnViewModeChanged: 윈도우 크기/모드 변경 + RHI OnResize
   - OnMeshTypeChanged: Scene Graph Mesh 교체
   - OnAnimationToggle: 회전 업데이트 활성/비활성
7. 멈춤 상태에서는 현재 회전 각도 유지, 렌더링은 계속

**완료 기준**: View 메뉴로 해상도/풀스크린 전환, 메뉴/Space로 오브젝트 전환 및 애니메이션 시작/멈춤 가능

### Phase 9: 포인트 광원
**목표**: 포인트 광원 추가 및 라이팅 셰이더, 광원 정보 표시/편집

1. `PointLight` — 위치(XMFLOAT3), 색상(XMFLOAT3), 감쇠 계수
2. HLSL 셰이더 확장 (Per-Pixel Lighting):
   - Vertex에 Normal 입력 추가
   - Constant Buffer에 LightPosition, LightColor, CameraPosition, 감쇠 계수 추가
   - Pixel Shader에서 픽셀 단위 Diffuse + Ambient 라이팅 계산:
     - `attenuation = 1 / (Kc + Kl·d + Kq·d²)` 감쇠 적용
     - `result = (ambient + diff * lightColor * attenuation) * faceColor`
3. DebugHUD에 광원 정보 표시 추가:
   - "Light Color: White", "Light Pos: (2.0, 3.0, -1.0)" 등
   - 표시 on/off 토글 가능
4. Win32Menu에 "Light" 메뉴 추가:
   - "Show Info" (체크 토글, 기본: on)
   - "Color" → White / Red / Green / Blue / Yellow / Cyan / Magenta
   - "Reset Position" → 기본 위치로 복원
5. 키보드로 광원 위치 이동:
   - 방향키(←→↑↓): X/Z 평면 이동
   - PgUp/PgDn: Y축 이동
   - 이동 속도: deltaTime 기반
6. MeshFactory에서 면 법선(Normal) 계산하여 Vertex에 포함
7. Constant Buffer 업데이트: 매 프레임 광원 데이터 전달

**완료 기준**: 광원에 의한 라이팅이 적용되고, 광원 정보 표시/숨김/색상 변경/위치 이동 가능

### Phase 10: 카메라
**목표**: Perspective/Orthographic 카메라 구현 및 카메라 정보 표시/편집

1. `Camera` — 카메라 클래스 (Scene/Camera.h/.cpp)
   - 투영 모드: Perspective (기본) / Orthographic
   - XMFLOAT3 position (기본값: 0.0, 0.0, -5.0)
   - XMFLOAT3 lookAt (기본값: 0.0, 0.0, 0.0)
   - float fov (기본값: XM_PIDIV4 = 45도)
   - float nearPlane = 0.1f, farPlane = 100.0f
   - GetViewMatrix() → XMMatrixLookAtLH
   - GetProjectionMatrix(aspectRatio) → XMMatrixPerspectiveFovLH 또는 XMMatrixOrthographicLH
   - SetProjectionMode(), GetProjectionMode()
   - GetDirection() → normalize(lookAt - position)
2. DebugHUD에 카메라 정보 표시 추가:
   - "Camera: Perspective", "Cam Pos: (0.0, 0.0, -5.0)"
   - "Cam Dir: (0.0, 0.0, 1.0)", "FOV: 45.0°"
   - bool showCameraInfo = true, 표시 on/off 토글 가능
3. Win32Menu에 "Camera" 메뉴 추가:
   - "Show Info"     (ID_CAMERA_SHOW_INFO)    ← 체크 토글, 기본: 체크됨
   - separator
   - "Perspective"   (ID_CAMERA_PERSPECTIVE)  ← 기본 선택
   - "Orthographic"  (ID_CAMERA_ORTHOGRAPHIC)
   - separator
   - "FOV+"          (ID_CAMERA_FOV_UP)
   - "FOV-"          (ID_CAMERA_FOV_DOWN)
   - "Reset"         (ID_CAMERA_RESET)
4. 키보드로 카메라 조작:
   - W/S: 전진/후퇴 (카메라 방향 기준)
   - A/D: 좌/우 이동 (카메라 방향 수직)
   - Q/E: 상/하 이동 (Y축)
   - +/-: FOV 증가/감소 (Perspective 모드)
   - 이동 속도: 3.0f * deltaTime
5. Engine에서 기존 하드코딩 View/Projection 행렬을 Camera 사용으로 교체
6. **유닛 테스트**: Perspective/Orthographic 전환, View 행렬 생성, FOV 변경 검증

**완료 기준**: 카메라 정보 표시/숨김, 투영 전환, WASD+QE 이동, FOV 조절 가능

### Phase 11: 통합 & 스모크 테스트
**목표**: 전체 파이프라인 통합 및 데모

1. `Renderer` — Scene Graph 순회 → RHI 드로우 콜 발행
2. Constant Buffer로 World/View/Projection + Light 데이터 전달
3. 엔진 루프에 Renderer + DebugHUD + Menu + PointLight + Camera 통합
4. 데모 장면: 라이팅이 적용된 회전 오브젝트 + HUD + 메뉴 + 광원 제어 + 카메라 제어
5. **스모크 테스트**: 엔진 초기화 → 1프레임 렌더 → 종료

**완료 기준**: 라이팅된 오브젝트를 메뉴로 전환, 광원 편집, 카메라 조작, HUD 표시, 모든 테스트 통과

## 의존성 그래프

```
Phase 1 (기반)
    ├── Phase 2 (DirectXMath)
    │       └── Phase 6 (Scene Graph) ──┐
    └── Phase 3 (RHI + D3D12)           │
            ├── Phase 4 (Vertex/Mesh)   │
            │       └── Phase 5 (렌더링 파이프라인 강화) ──┤
            ├── Phase 7 (HUD) ──────────┤
            ├── Phase 8 (메뉴) ─────────┼── Phase 11 (통합)
            ├── Phase 9 (광원) ─────────┤
            └── Phase 10 (카메라) ──────┘
```

## 리스크 & 대응

| 리스크 | 대응 |
|--------|------|
| D3D12 초기화 복잡도 높음 | 최소한의 파이프라인으로 시작 (단일 PSO, 단일 Root Signature) |
| D3D12 텍스트 렌더링 어려움 | D2D/DirectWrite interop 또는 비트맵 폰트 스프라이트로 대체 |
| GPU 동기화 실수 (Fence) | Flush 패턴으로 시작, 이후 멀티 프레임 최적화 |
| Scene Graph 복잡도 증가 | 단순 트리 구조로 시작, 컴포넌트 시스템은 향후 확장 |
| Win32 메시지 루프와 렌더 루프 충돌 | PeekMessage 기반 non-blocking 루프 사용 |
| Orthographic 투영 시 오브젝트 크기 부자연스러움 | Ortho 뷰 볼륨을 화면 종횡비에 맞춰 조정 |
| Full Screen 전환 시 SwapChain 오류 | Borderless Windowed 방식으로 대체 가능 (DXGI 모드 전환 회피) |
