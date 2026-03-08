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

## Phase 01 의존성 그래프

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

## Phase 01 리스크 & 대응

| 리스크 | 대응 |
|--------|------|
| D3D12 초기화 복잡도 높음 | 최소한의 파이프라인으로 시작 (단일 PSO, 단일 Root Signature) |
| D3D12 텍스트 렌더링 어려움 | D2D/DirectWrite interop 또는 비트맵 폰트 스프라이트로 대체 |
| GPU 동기화 실수 (Fence) | Flush 패턴으로 시작, 이후 멀티 프레임 최적화 |
| Scene Graph 복잡도 증가 | 단순 트리 구조로 시작, 컴포넌트 시스템은 향후 확장 |
| Win32 메시지 루프와 렌더 루프 충돌 | PeekMessage 기반 non-blocking 루프 사용 |
| Orthographic 투영 시 오브젝트 크기 부자연스러움 | Ortho 뷰 볼륨을 화면 종횡비에 맞춰 조정 |
| Full Screen 전환 시 SwapChain 오류 | Borderless Windowed 방식으로 대체 가능 (DXGI 모드 전환 회피) |

---

## Phase 02: glTF 2.0 씬 로딩 및 PBR 렌더링

> Phase 01 완료 코드는 `Phase 01 Backup/` 폴더에 백업됨. 해당 폴더는 참조/수정하지 않는다.

### Phase 02 프로젝트 구조 (Phase 01 대비 추가분)

```
src/
│   ├── Asset/                              [신규]
│   │   ├── SceneLoader.h / .cpp            # Assimp 기반 glTF/GLB/FBX 로딩
│   │   ├── Material.h / .cpp               # PBR Material 클래스
│   │   ├── Texture.h / .cpp                # D3D12 텍스처 리소스 관리
│   │   ├── TextureCache.h / .cpp           # 텍스처 캐싱 + 폴백 텍스처
│   │   └── TextureStreamer.h / .cpp         # Mip 레벨 기반 텍스처 스트리밍
│   ├── Core/
│   │   └── ThreadPool.h / .cpp             # [신규] CPU 코어 수 기반 스레드 풀
│   ├── Lighting/
│   │   ├── Light.h                         # [신규] 공용 Light 구조체 (Dir/Point/Spot)
│   │   └── LightManager.h / .cpp           # [신규] 다중 광원 관리
│   ├── RHI/D3D12/
│   │   ├── D3D12CBPool.h / .cpp            # [신규] Upload Heap 풀링 CB 관리
│   │   └── (기존 파일들 확장)
│   ├── Renderer/
│   │   ├── FrustumCuller.h / .cpp          # [신규] Frustum Culling
│   │   ├── OcclusionCuller.h / .cpp        # [신규] Occlusion Culling
│   │   ├── LODSelector.h / .cpp            # [신규] LOD 선택 + 자동 LOD 생성
│   │   ├── LightCuller.h / .cpp           # [신규] 광원 컬링 (거리/기여도 기반)
│   │   ├── InstanceBatcher.h / .cpp        # [신규] Instanced Rendering
│   │   └── (기존 파일들 확장)
│   └── Shaders/
│       ├── PBR.hlsl                        # [신규] Cook-Torrance BRDF 셰이더
│       ├── ShadowDepth.hlsl                # [신규] Shadow depth-only 셰이더
│       ├── Wireframe.hlsl                  # [신규] 와이어프레임 단색 셰이더
│       └── BasicColor.hlsl                 # (기존 유지)
tests/
    ├── unit/
    │   ├── test_Material.cpp               # [신규] Material 파라미터 테스트
    │   └── test_FrustumCuller.cpp          # [신규] Frustum Culling 테스트
    └── smoke/
        └── test_SceneLoader.cpp            # [신규] glTF 로딩 스모크 테스트
```

### Phase 02 기술 선택

| 구분 | 선택 | 비고 |
|------|------|------|
| 3D 에셋 로더 | Assimp (vcpkg) | glTF 2.0 / GLB / FBX 통합 로딩 |
| 이미지 디코딩 | stb_image 또는 WIC | PNG, JPEG 등 텍스처 디코딩 |
| PBR 셰이더 | Cook-Torrance BRDF | GGX NDF + Smith-Schlick G + Fresnel-Schlick |
| 그림자 | Shadow Mapping + PCF | Depth-only 패스 + 3×3 PCF 커널 |
| VRAM 모니터링 | IDXGIAdapter3 | QueryVideoMemoryInfo |

### Phase 02 구현 단계

### Phase 12: Assimp 설정 + SceneLoader 기본
**목표**: Assimp 라이브러리 연동, 기본 Mesh 데이터 로딩

1. vcpkg로 Assimp 설치 (`assimp:x64-windows`), vcxproj에 링크 설정
2. `src/Asset/SceneLoader.h/.cpp` — Assimp으로 glTF/GLB/FBX 파싱
3. Mesh 데이터 추출: position, normal, UV, tangent, index
4. aiNode 계층 → SceneNode 트리 변환
5. 씬 바운딩 박스 계산
6. 카메라 노드 추출 (aiCamera → 시작 위치/방향, 없으면 Fit to Scene 폴백)
7. 스모크 테스트: 테스트 glTF 파일 로딩 → 정점 수 > 0 확인

**완료 기준**: glTF 파일을 Assimp으로 파싱하여 Mesh + SceneNode 트리 생성 성공

### Phase 13: Vertex 포맷 확장 + Material 시스템
**목표**: UV/Tangent 포함 Vertex, PBR Material 클래스

1. Vertex 구조체 확장: `XMFLOAT2 texCoord`, `XMFLOAT4 tangent` 추가
2. D3D12 Input Layout 확장 (TEXCOORD, TANGENT 시맨틱)
3. `static_assert`로 새 Vertex 크기/오프셋 검증
4. `src/Asset/Material.h/.cpp` — PBR Material 클래스
   - baseColor/metallic/roughness/normal/emissive/occlusion 텍스처 + factor
   - AlphaMode (Opaque, Mask, Blend), doubleSided
   - Dirty Flag 지원
5. SceneLoader에서 Material 정보 추출 (aiMaterial → Material 객체)
6. SceneNode에 Material 참조 추가
7. 유닛 테스트: Material 파라미터 기본값, dirty flag 동작

**완료 기준**: 확장된 Vertex로 Mesh 생성, Material 객체에 PBR 파라미터 저장

### Phase 14: Texture 시스템 + 비동기 로딩
**목표**: D3D12 텍스처 리소스 생성, 비동기 로딩, 캐싱

1. `src/Asset/Texture.h/.cpp` — 이미지 → ID3D12Resource (TEXTURE2D) 생성
2. Upload Buffer → Default Heap 복사, 상태 전이 (COPY_DEST → PIXEL_SHADER_RESOURCE)
3. SRV 디스크립터 생성
4. SRGB 포맷 처리: baseColor = `_SRGB`, normal/roughness/metallic = `_UNORM`
5. `src/Asset/TextureCache.h/.cpp` — 파일 경로 기반 중복 방지 캐시
6. 폴백 텍스처: 1×1 white (로딩 완료 전 바인딩용)
7. 비동기 텍스처 로딩: 워커 스레드에서 이미지 디코딩 → 메인 스레드에서 GPU 업로드
8. 로딩 상태 관리: Pending → Loading → Ready

**완료 기준**: glTF의 텍스처를 비동기 로딩하여 GPU SRV로 바인딩 가능, 로딩 중 폴백 표시

### Phase 15: RHI 확장 (Root Signature, Descriptor Heap, PSO)
**목표**: PBR 렌더링을 위한 D3D12 파이프라인 확장

1. Root Signature 확장: CBV(b0, b1, b2) + SRV table(t0~t4) + Static Sampler(s0) + Comparison Sampler(s1)
2. Descriptor Heap 확장: CBV + SRV 통합, MAX_DRAW_CALLS 제한 해제
3. `src/RHI/D3D12/D3D12CBPool.h/.cpp` — Upload Heap 풀링 CB 관리
   - 256바이트 정렬 슬롯 할당, 링 버퍼 방식
4. PBR PSO: 확장 Input Layout + PBR 셰이더
5. Shadow Depth PSO: depth-only, color write off, depth bias
6. Wireframe PSO: FillMode = Wireframe
7. Alpha Mask / Alpha Blend / Double-sided 용 PSO 변형

**완료 기준**: 복수 PSO 생성, CBPool 할당/리셋 동작, SRV 바인딩 가능

### Phase 16: Cook-Torrance BRDF 셰이더 + Gamma Correction
**목표**: PBR 라이팅 셰이더 구현

1. `src/Shaders/PBR.hlsl` — Cook-Torrance BRDF:
   - NDF: GGX/Trowbridge-Reitz
   - Geometry: Smith-Schlick GGX
   - Fresnel: Schlick 근사
   - Diffuse: Lambertian (albedo / π)
2. Texture2D 샘플링: t0(albedo), t1(normal), t2(metallicRoughness), t3(emissive), t4(occlusion)
3. TBN 행렬로 Normal Map 변환 (탄젠트 공간 → 월드 공간)
4. 텍스처-factor 폴백: hasAlbedoMap 등 플래그로 분기
5. Gamma Correction: 리니어 공간 연산 후 sRGB 출력 (`_SRGB` 렌더 타겟 또는 `pow(1/2.2)`)
6. Per-Material CB (register b2): PerMaterialCB 구조체
7. 기존 BasicColor.hlsl은 Phase 01 오브젝트용으로 유지

**완료 기준**: PBR 텍스처가 적용된 오브젝트가 Cook-Torrance 라이팅으로 렌더링, Gamma Correction 적용

### Phase 17: 다중 광원 시스템
**목표**: Directional/Point/Spot 광원, 최대 8개 이상 동시 처리

1. `src/Lighting/Light.h` — 공용 Light 구조체 (LightType, color, position, direction, 감쇠, cone angles)
2. `src/Lighting/LightManager.h/.cpp` — 광원 목록 관리, 활성 광원 개수 추적
3. GPU 전달: Structured Buffer 또는 CB 배열 (register b1)
4. PBR.hlsl 확장: 다중 광원 루프, 타입별 분기 (Directional/Point/Spot)
5. Spot Light: smoothstep 원뿔 페이드
6. 기존 Phase 01 PointLight와 호환 유지
7. 런타임 광원 추가/제거/편집

**완료 기준**: 여러 타입의 광원이 동시에 PBR 오브젝트를 비추고, 셰이더에서 합산 렌더링

### Phase 18: Shadow Mapping
**목표**: 광원별 Shadow Map 생성, PCF 적용

1. Shadow Map 리소스: D32_FLOAT 텍스처, DSV + SRV 동시 생성
2. `src/Shaders/ShadowDepth.hlsl` — VS: position 변환만, PS: 없음
3. Shadow Depth Pass: 광원 시점 depth-only 렌더링
4. Light-View-Projection 행렬 계산 (Directional: Ortho, Spot: Perspective)
5. Depth Bias 설정 (shadow acne 방지)
6. PCF 구현: 3×3 커널 `SampleCmpLevelZero` + Comparison Sampler
7. PBR.hlsl에 shadow factor 통합: `Σ shadowFactor × (diffuse + specular)`
8. 광원별 독립 Shadow Map (최대 8장, register t6~t13)

**완료 기준**: 그림자가 정확하게 생성되고, PCF로 부드러운 그림자 경계 표시

### Phase 19: 씬 파일 로딩 UI + 카메라 네비게이션
**목표**: 메뉴 기반 씬 로딩, 마우스 + 키보드 카메라 조작

1. Win32Menu에 "File" 메뉴 추가: "Open Scene..." (GetOpenFileName, 필터: `*.gltf;*.glb;*.fbx`)
2. 씬 로딩 워크플로우: 기존 씬 해제 → Assimp 파싱 → SceneNode/Material/Texture 구축
3. 카메라 배치: 씬 파일 내 카메라 있으면 사용, 없으면 Fit to Scene
4. 드래그 앤 드롭: WM_DROPFILES 처리
5. 카메라 마우스 네비게이션:
   - 우클릭 드래그: Yaw/Pitch 회전
   - 마우스 휠: 전진/후진 (돌리 줌)
   - 중클릭 드래그: 패닝
6. 카메라 키보드 네비게이션:
   - WASD: 카메라 시선 방향 기준 전진/후퇴/좌/우 이동
   - Q/E: 월드 Y축 기준 상/하 이동
   - +/-: FOV 증가/감소
   - 이동 속도: 씬 바운딩 박스 크기에 비례 자동 조절
7. Fit to Scene: 씬 바운딩 박스 기반 카메라 자동 배치
8. 이동 속도 자동 조절: 씬 크기에 비례 (마우스/키보드 공통)

**완료 기준**: 메뉴에서 glTF/FBX 파일을 열어 씬이 교체되고, 마우스 + 키보드로 네비게이션 가능

### Phase 20: PBR 파이프라인 통합 + 렌더링 모드 선택
**목표**: 구현된 PBR 파이프라인을 Engine에 연결하여 텍스처가 정상 렌더링되도록 하고, 5단계 렌더링 모드 전환 구현

**Part A: PBR 파이프라인 통합 (텍스처 렌더링 활성화)**
> Phase 13-16에서 PBR 인프라(Vertex UV/tangent, PBR PSO, Root Signature, PBR.hlsl, DrawPrimitivesPBR, TextureCache)는 이미 구현됨.
> Engine에서 이들을 연결하는 마지막 통합 작업이 누락되어 텍스처가 렌더링되지 않음.

1. Engine에 `TextureCache` 생성 및 초기화
   - `Engine::Initialize()`에서 `TextureCache::Initialize(device, context)` 호출
   - `Renderer::SetTextureCache(textureCache)` 호출
2. `Engine::LoadScene()`에서 텍스처 로딩 연결
   - 각 Material의 텍스처 경로 → `TextureCache::GetOrLoad()` 호출
   - 반환된 Texture 포인터를 Material에 설정 (baseColorTexture, normalTexture 등)
   - 폴백 텍스처: 로드 실패 시 `TextureCache::GetFallback()` 사용
3. 씬 교체 시 `TextureCache::Clear()` 호출 (GPU 리소스 해제)
4. Alpha Mask/Blend 패스 구현
   - Renderer::RenderScene()에서 Material의 alphaMode별 렌더링 분리
   - Opaque → Alpha Mask (clip) → Alpha Blend (back-to-front 정렬)
   - Alpha Blend PSO: BlendEnable=true, DepthWriteMask=ZERO

**Part B: 렌더링 모드 선택**
5. `enum class RenderMode { Wireframe, Solid, BaseColorOnly, FullPBR, FullPBRShadows }`
6. Win32Menu에 "Render" 메뉴 추가 (5개 항목 + CheckMenuRadioItem)
7. `src/Shaders/Wireframe.hlsl` — 단색 셰이더
8. Wireframe PSO (FillMode = Wireframe)
9. Solid/BaseColorOnly/FullPBR: PBR 셰이더의 텍스처 플래그로 제어
10. FullPBRShadows: Shadow Depth Pass 수행 + Shadow Map 바인딩
11. Renderer에 `SetRenderMode()` + 모드별 PSO/패스 분기
12. DebugHUD에 현재 모드명 표시

**Part C: PBR 라이팅 버그 수정 (HLSL cbuffer 배열 패킹)**
13. `PBR.hlsl` — LightData 구조체의 `float _pad1[2]`를 `float2 _pad1`로 수정
    - HLSL cbuffer에서 `float arr[N]`은 각 원소가 16바이트 경계에 정렬됨
    - C++ 측 `float[2]` = 8바이트 vs HLSL `float[2]` = 32바이트 → 구조체 크기 불일치
    - 이로 인해 `numActiveLights`가 잘못된 오프셋에서 읽혀 0으로 판독 → 라이팅 미적용(검은 화면)
    - `float2` 벡터 타입으로 교체하여 C++/HLSL 레이아웃 일치

**Part D: 자동 3-포인트 라이팅 + PointLight 레거시 제거**
14. Phase 01의 `PointLight` 클래스 의존성 완전 제거
    - `src/Lighting/PointLight.h` 더 이상 사용하지 않음 (vcxproj 참조 제거)
    - Engine.h/cpp: `m_pointLight`, `m_lightSphereMesh`, `m_lightSphereVB/IB` 멤버 삭제
    - Renderer.h/cpp: `RenderLightIndicator()` 삭제, `RenderScene()` 시그니처에서 `PointLight*` 제거
15. 광원 인디케이터 구 렌더링 삭제
16. 방향키(Arrow/PgUp/PgDn) 광원 위치 이동 기능 삭제
17. Win32Menu에서 "Reset Position" 메뉴 항목 + 콜백 삭제
18. 기본 씬: LightManager에 3-포인트 라이팅 자동 배치
    - Key Light (warm, 밝음): position=(2,2.5,-2), intensity=12, color=(1.0, 0.95, 0.9)
    - Fill Light (cool, 부드러움): position=(-2.5,1.5,1.5), intensity=6, color=(0.8, 0.85, 1.0)
    - Back Light (rim): position=(0,3,2.5), intensity=8, color=(1,1,1)
    - 감쇠: Kc=1.0, Kl=0.027, Kq=0.005
19. 씬 로드 시: 바운딩 박스 기반 3-포인트 라이팅 자동 배치
    - 씬 중심 + 대각선 50% 반경으로 Key/Fill/Back 배치
    - 감쇠 계수를 씬 크기에 비례하여 자동 스케일링
20. 색상 변경 메뉴: LightManager의 모든 광원 색상 일괄 변경
21. DebugHUD 광원 정보: LightManager에서 직접 조회

**Part E: glTF/GLB 좌표계 변환 + GLB 임베딩 텍스처 로딩**
22. Assimp 임포트 플래그에 `aiProcess_ConvertToLeftHanded` 추가
    - glTF는 우수 좌표계(RH, +Z=뷰어 방향), DirectX는 좌수 좌표계(LH, +Z=화면 안쪽)
    - `aiProcess_ConvertToLeftHanded` = `MakeLeftHanded | FlipUVs | FlipWindingOrder`:
      · `MakeLeftHanded`: 정점/노말/탄젠트의 Z축 반전 + 노드 변환 행렬 조정 + UV.y 반전
      · `FlipUVs`: UV.y 재반전 (MakeLeftHanded의 UV 반전과 상쇄 → 원래 UV 유지)
      · `FlipWindingOrder`: CCW→CW (D3D12 `FrontCounterClockwise=FALSE` 규칙 일치)
    - `stbi_set_flip_vertically_on_load(false)`: glTF UV 원점(top-left)이 D3D12와 동일하므로 이미지 반전 불필요
23. Transform에 `SetLocalMatrix()` 추가 — Assimp 행렬을 직접 저장하여 TRS 분해→재합성 round-trip 손실 방지
24. GLB 임베딩 텍스처 로딩 지원
    - SceneData에 `embeddedTextures` 맵 추가 (키: `*N`, 값: 압축 이미지 바이트)
    - SceneLoader에서 `aiTexture::pcData` 추출 (compressed: PNG/JPG 바이트, raw: ARGB 변환)
    - TextureCache에 `GetOrLoadFromMemory()` 추가 — `stbi_load_from_memory()`로 디코딩
    - Engine::LoadScene()에서 `*N` 경로 감지 시 임베딩 데이터 → `GetOrLoadFromMemory()` 호출

**Part F: PBR 머티리얼 추출 파이프라인 강화**

25. Metallic/Roughness factor 항상 할당
    - `mat->Get()` 성공 여부와 무관하게 `result->metallicFactor`/`result->roughnessFactor`에 값 할당
    - Assimp 조회 실패 시에도 glTF 스펙 기본값(1.0)이 적용됨
26. Normal map 텍스처 폴백 타입 추가
    - 기존: `aiTextureType_NORMALS`만 조회
    - 추가: `aiTextureType_HEIGHT` → `aiTextureType_NORMAL_CAMERA` 순 폴백
    - 익스포터별 다른 타입 매핑에 대응
27. Occlusion 텍스처 조회 순서 수정
    - 변경 전: `LIGHTMAP` → `AMBIENT_OCCLUSION`
    - 변경 후: `AMBIENT_OCCLUSION` → `LIGHTMAP` (glTF 표준 타입 우선)

**Part G: 궤도 회전 광원 + 4-광원 밸런싱**

28. 궤도 회전 포인트 라이트 추가
    - 카메라 시선 축 주변을 원형 궤도로 회전하는 4번째 광원
    - 궤도 중심: 카메라에서 시선 방향 30% 지점, 반지름: 씬 대각선 25%
    - 회전 속도 ~0.8 rad/s (약 8초/바퀴), 애니메이션 토글과 독립
    - Engine.h에 `m_orbitLightIndex`, `m_orbitLightAngle` 멤버 추가
    - Engine::Update()에서 매 프레임 궤도 위치 계산 (카메라 시선 수직 좌표축 기반)
29. 4-광원 체제 intensity 밸런싱
    - Key: 12→8, Fill: 6→3, Back: 8→4, Orbit: 10→6 (Initialize + LoadScene 동일 적용)

**완료 기준**: glTF/GLB 씬 로드 시 텍스처가 정상 렌더링됨, GLB 임베딩 텍스처 정상 로드, 좌표계 변환으로 모델 방향/텍스처 매핑 정상, Alpha Mask/Blend 오브젝트 정상 표시, 메뉴에서 5단계 렌더링 모드를 즉시 전환 가능, 씬 로드 시 4-광원 라이팅 자동 배치 (3-포인트 + 궤도), PointLight 레거시 코드 완전 제거, PBR 머티리얼 파라미터/텍스처가 다양한 glTF 익스포터에서 올바르게 추출됨

### Phase 21: 레거시 코드 정리 + 테스트 재정비
**목표**: Phase 02 마이그레이션 과정에서 남은 레거시 코드를 제거하고, 테스트를 현재 API에 맞게 업데이트하여 전체 테스트 통과

1. `src/Lighting/PointLight.h` 삭제 — Phase 01의 단일 포인트 라이트 클래스, Light + LightManager로 완전 대체됨
2. `tests/smoke/test_EngineInit.cpp` 수정
   - `#include "Lighting/PointLight.h"` → `#include "Lighting/LightManager.h"` + `#include "Lighting/Light.h"`
   - `RRE::PointLight light;` → `RRE::LightManager lightManager;` + 기본 광원 추가
   - `renderer.RenderScene(sceneGraph, camera, &light, aspectRatio)` → `renderer.RenderScene(sceneGraph, camera, aspectRatio, &lightManager)`
3. `src/RREngine.vcxproj.filters` 재생성 — Phase 02에서 추가된 파일들 반영
   - Asset/ (SceneLoader, Material, Texture, TextureCache)
   - Lighting/ (Light.h, LightManager)
   - 새 셰이더 (PBR.hlsl, ShadowDepth.hlsl, Wireframe.hlsl)
   - RHI/D3D12 추가 파일 (CBPool, DescriptorHeap 등)
4. `src/RREngine.vcxproj`에서 PointLight.h 참조 제거 (있을 경우)
5. 전체 테스트 빌드 및 실행 — 모든 유닛/스모크 테스트 통과 확인

**완료 기준**: PointLight 레거시 코드 완전 제거, 모든 테스트가 현재 API로 빌드 및 통과, vcxproj.filters가 최신 파일 구조 반영

### Phase 22: 초기 씬 제거 + Object/Animation 메뉴 삭제 + 프리미티브 SceneNode 분리 + Per-Mesh AABB

**목표**: 앱 시작 시 빈 화면으로 대기하도록 변경 (초기 프로시저럴 씬 제거, Object/Animation 메뉴 삭제). glTF/GLB 씬 로딩 시 서브 프리미티브를 개별 SceneNode로 분리하고 각 Mesh에 AABB를 추가.

#### A. 초기 씬 제거 + Object/Animation 메뉴 삭제

1. **앱 시작 시 빈 씬**: `Engine::Initialize()`에서 초기 큐브/물체 씬 구성 코드 제거
   - MeshFactory 4종 Mesh 생성 제거 (m_sphereMesh, m_tetrahedronMesh, m_cubeMesh, m_cylinderMesh)
   - 초기 SceneNode 트리 구성 제거 (m_parentNode, m_orbitPivotNode, m_childNode)
   - 관련 멤버 변수 제거 (Engine.h)
2. **Object 메뉴 삭제**:
   - `Win32Menu`에서 "Object" 팝업 메뉴 생성/처리 코드 제거
   - `MeshType` enum, `MeshCallback` typedef, `SetMeshCallback()`, `m_meshCallback`, `m_objectMenu` 제거
   - `ID_OBJECT_SPHERE/TETRAHEDRON/CUBE/CYLINDER` 메뉴 ID 제거
   - `Engine::OnMeshTypeChanged()` 메서드 제거
3. **Animation 메뉴 삭제**:
   - `Win32Menu`에서 "Animation" 팝업 메뉴 생성/처리 코드 제거
   - `AnimCallback` typedef, `SetAnimCallback()`, `UpdateAnimCheckMark()`, `m_animCallback`, `m_animMenu` 제거
   - `ID_ANIM_PLAY/PAUSE` 메뉴 ID 제거
   - `Engine::OnAnimationToggle()` 메서드 제거
   - Space 키 애니메이션 토글 제거
   - `m_isAnimating`, `m_rotationAngle`, `m_orbitAngle`, `m_childRotationAngle` 멤버 제거
   - `Engine::Update()`에서 애니메이션 회전 갱신 코드 제거
4. **광원 설정 업데이트**: 초기 씬이 없으므로 `Initialize()`에서 3-포인트 광원 자동 배치 제거 (씬 로드 시에만 배치)

#### B. 프리미티브 → SceneNode 분리 + Per-Mesh AABB

5. **Mesh에 AABB 추가**: `Renderer/Mesh.h`에 `DirectX::BoundingBox aabb` 멤버 추가
6. **SceneLoader 프리미티브 분리 확인/강화**:
   - `ProcessNode()`에서 aiNode가 여러 aiMesh를 참조할 때 각각 별도 SceneNode 자식으로 생성
   - Sponza: 단일 aiNode에 103개 aiMesh → 103개 SceneNode로 분리
7. **Per-Mesh AABB 계산**: `ConvertMesh()` 끝에서 `BoundingBox::CreateFromPoints()`로 로컬 AABB 생성
8. **SceneNode 월드 AABB 캐싱**:
   - `SceneNode`에 `BoundingBox m_worldAABB`, `bool m_aabbDirty = true` 추가
   - `GetWorldAABB()`: dirty면 Mesh 로컬 AABB를 WorldMatrix로 변환 후 캐시
9. **DebugHUD 확장**: 총 SceneNode 수, 총 Mesh 수를 HUD에 표시
10. **유닛 테스트**: AABB 계산 정확성, 프리미티브 분리 후 노드 수 검증

#### C. 미사용 코드 완전 제거 (A 작업 완료 후 파생)

11. **파일 삭제**:
    - `src/Renderer/MeshFactory.h/.cpp` — MeshFactory 전체 삭제 (더 이상 호출 없음)
    - `src/Renderer/FaceColorPalette.h` — MeshFactory에서만 사용, 함께 삭제
    - `tests/unit/test_FaceColoring.cpp` — MeshFactory/FaceColorPalette에 전적으로 의존, 삭제
12. **테스트 파일 정리** (`tests/smoke/test_EngineInit.cpp`):
    - `#include "Renderer/MeshFactory.h"` 제거
    - `MeshTypeChangeUpdatesSceneNodes()` 테스트 삭제 (메시 타입 전환 기능 제거됨)
    - `SceneGraphWithRendererOneCycle()` 등 MeshFactory를 사용하는 테스트: 간단한 수동 Mesh 생성으로 교체
13. **프로젝트 파일 업데이트**:
    - `src/RREngine.vcxproj`: MeshFactory.cpp, FaceColorPalette.h, MeshFactory.h 항목 제거
    - `src/RREngine.vcxproj.filters`: 동일 항목 제거
    - `tests/RREngineTests.vcxproj`: MeshFactory.cpp 항목 제거
    - `tests/RREngineTests.vcxproj.filters`: test_FaceColoring.cpp 항목 제거

**완료 기준**: 앱 시작 시 빈 화면, Object/Animation 메뉴 없음, glTF 씬 로드 후 정상 렌더링, Sponza 로딩 시 103개+ SceneNode 분리, 각 노드에 유효한 AABB, MeshFactory/FaceColorPalette 파일 삭제, 잔여 테스트 전체 통과

### Phase 23: 렌더링 최적화 — Culling + LOD + Light Culling
**목표**: Frustum/Occlusion Culling, LOD 시스템 (자동 LOD 생성 포함), 광원 컬링

1. `src/Renderer/FrustumCuller.h/.cpp` — AABB vs 6-plane 교차 검사
   - `BoundingFrustum::CreateFromMatrix(proj)` + `Transform(invView)` 로 월드 공간 Frustum 생성
   - `IsVisible(const BoundingBox&)` → `m_frustum.Intersects(aabb)` 활용
2. `DirectX::BoundingFrustum` + `BoundingBox::Intersects()` 활용
3. `src/Renderer/OcclusionCuller.h/.cpp` — P0 스텁: 항상 false 반환 (보수적 판정)
4. Occluded 오브젝트: CB 갱신 + Draw 모두 스킵
5. `src/Renderer/LODSelector.h/.cpp` — 거리 기반 LOD 선택
   - `struct LODMesh { Mesh* meshLODs[3]; float switchDistances[3]; uint32 lodCount; }`
   - `std::async`로 비동기 LOD 생성; `std::atomic<bool> lodsReady` 로 스레드 안전 접근
   - `#define NOMINMAX` 필수: Windows SDK min/max 매크로와 `std::min/max` 충돌 방지
6. LODMesh 구조체: Mesh 배열 + 전환 거리
7. glTF/FBX LOD 매핑 (MSFT_lod 확장)
8. 자동 LOD 생성: 씬 파일에 LOD 데이터가 없으면 그리드 기반 버텍스 클러스터링으로 LOD 1(~50%), LOD 2(~25%) 자동 생성. 백그라운드 스레드에서 비동기 수행. 완료 전까지 원본(LOD 0)으로 렌더링
9. `src/Renderer/LightCuller.h/.cpp` — 광원 컬링
   - 거리 기반: Point/Spot 광원의 유효 범위(BoundingSphere) vs Frustum 교차 검사
   - 기여도 기반: 광원~카메라 거리 및 강도로 화면 기여도 추정, 임계값 이하 제외
   - Directional Light는 항상 포함
10. `LightManager::BuildFilteredLightConstants(activeIndices)` — 컬링된 활성 광원만 GPU LightCB로 빌드
11. `CullStats` 구조체 (Renderer.h):
    - `visibleNodes`, `frustumCulledNodes`, `occlusionCulledNodes`
    - `activeLights`, `culledLights`
    - `renderedPolygons` — Culling + LOD 적용 후 실제 제출된 삼각형 수 (Pass 1 + Pass 2 합산)
12. DebugHUD 확장:
    - `Polys (scene):    N` — 씬 전체 폴리곤 수 (Culling/LOD 미적용)
    - `Polys (rendered): M` — Culling + LOD 후 실제 렌더링된 폴리곤 수
    - `Poly/sec: X.XM` — 초당 렌더링 폴리곤 (rendered 기준)
    - `Visible: N  Culled: M` — 노드 컬링 통계
    - `Lights: N active  M culled` — 광원 컬링 통계
13. 유닛 테스트: `test_FrustumCuller.cpp` (8개), `test_LightCuller.cpp` (7개)
14. **Optimization 메뉴** (Win32Menu.h/.cpp, Engine.cpp):
    - "Optimization" 메뉴바 추가; 기능별 체크 가능한 하위 항목으로 런타임 on/off
    - **Frustum Culling** (`ID_OPTIM_FRUSTUM_CULL = 8002`): 비활성 시 Frustum 교차 검사 스킵 → 씬 전체 렌더링
    - **Light Culling** (`ID_OPTIM_LIGHT_CULL = 8003`): 비활성 시 전체 광원을 GPU에 전달
    - **LOD** (`ID_OPTIM_LOD = 8001`): 비활성 시 항상 LOD 0(원본) 메시 사용
    - Occlusion Culling은 P0 스텁(항상 false)이므로 메뉴 항목 없음
    - LOD 전환 거리: `sceneDiagonal × 2.0` (LOD 1), `sceneDiagonal × 6.0` (LOD 2) — 충분히 원거리에서만 전환
    - `Renderer::SetFrustumCullingEnabled()`, `SetLightCullingEnabled()`, `SetLODEnabled()` API 제공

**완료 기준**: Frustum 밖 오브젝트 culled, Occluded 오브젝트 스킵, 거리별 LOD 전환 (자동 생성 포함), 원거리/저기여 광원 컬링, DebugHUD에 씬 전체 폴리곤 수와 렌더링된 폴리곤 수 각각 표시, Optimization 메뉴에서 각 기능 on/off 가능, 91/91 테스트 통과

### Phase 24: HLSL 경고 수정 + Shadow Map 자동 크기 조정 + Sponza 빠른 로드 ✅
**목표**: PBR.hlsl X4000 경고 최소화, Shadow Map 해상도·투영 범위·카메라 배치를 씬 크기에 맞게 자동 조정, Sponza 전용 빠른 로드 메뉴 추가

1. **PBR.hlsl — SampleShadowMap 구조 개선** (X4000 경고 최소화)
   - `[branch] switch` → `float result = 1.0f; if/else-if` 체인으로 교체
   - `ShadowMap` X4000 경고 제거 (`result = 1.0f` 명시적 초기화)
   - `CalcShadow`: `shadowIdx = min(shadowIdx, MAX_SHADOW_MAPS - 1)` 인덱스 범위 보장
   - `shadow += saturate(SampleShadowMap(...))` — PCF 누적 값 범위 명시
   - 잔존 X4000 경고 1건: FXC 컴파일러 고유 한계 (비교 샘플러 + 동적 cbuffer 인덱스 조합), Phase 32에서 재검토
2. **PBR.hlsl — ShadowCB 확장**
   - `shadowTexelSize` 필드 추가 (b3): `1.0 / shadowMapResolution` (CPU에서 계산)
   - PCF 루프의 하드코딩 `1.0f / 1024.0f` → `shadowTexelSize` cbuffer 값으로 교체
3. **D3D12Context — 런타임 Shadow Map 해상도 지원**
   - `SHADOW_MAP_SIZE` 상수 제거 → `m_shadowMapSize = 1024` 런타임 멤버로 교체
   - `SetShadowMapSize(uint32)`: [512, 4096] 범위 2의 제곱수로 스냅
   - `RecreateShadowMaps()`: 해상도 변경 후 GPU 리소스 재생성
   - `GetShadowMapSize()`: 현재 해상도 조회
   - `ShadowConstants` 구조체에 `shadowTexelSize` 필드 추가
4. **Renderer — 씬 대각선 기반 Shadow 투영 스케일링**
   - `SetSceneDiagonal(float)` + `m_sceneDiagonal = 10.0f` 멤버 추가
   - Directional 그림자 Ortho 범위: 고정 `20×20m` → `sceneDiagonal × 1.5f` 자동 계산
   - Directional/Spot far plane: 고정 `100.0f` → `sceneDiagonal × 3.0f`
   - Directional near plane: `sceneDiagonal × 0.5f` (소형 씬에서 z-fighting 방지)
   - Spot near plane: `sceneDiagonal × 0.05f` (was hardcoded 0.1f)
   - Shadow 카메라 배치: `shadowCamPos = sceneCenter - dir*(farPlane*0.5f)` — farPlane 절반 거리에서 씬 중심을 향하도록 위치시켜 씬 전체가 깊이 범위에 포함되도록 보장
   - Shadow Normal Bias: `shadowNormalBiasWorld = (sceneDiagonal × 1.5f) / shadowMapSize × 2.0f` — 씬 크기와 해상도에 비례한 world-space bias
   - Shadow pass 전 `shadowConst.shadowTexelSize = 1.0f / GetShadowMapSize()`
5. **Engine::LoadScene() — 씬 로드 후 자동 연결**
   - `Renderer::SetSceneDiagonal(m_sceneDiagonal)` 호출
   - 씬 크기 기반 해상도 선택: ≤ 10m → 1024, ≤ 100m → 2048, > 100m → 4096
   - `D3D12Context::SetShadowMapSize()` + `RecreateShadowMaps()` 호출
6. **Orbit Light → Directional + castShadow**
   - Orbit Light 타입 변경: `LightType::Point` → `LightType::Directional`, `castShadow = true`
   - `Engine::Update()`: `position` 갱신 → `direction` 갱신으로 변경
     - 월드 Y축 기준 회전, 45° 앙각 고정 (카메라 독립)
     - `kElevRad = π/4`, `cosElev = cos(45°) ≈ 0.707`, `sinElev = sin(45°) ≈ 0.707`
     - `lightDir = { -cosElev·cos(θ), -sinElev, -cosElev·sin(θ) }` (이미 단위 벡터, 항상 원점 향함)
   - Directional 광원이므로 매 프레임 Shadow Depth Pass 1회 실행 → 회전하는 그림자 효과
7. **Sponza 빠른 로드 — File 메뉴 "Sponza!" 항목 추가**
   - `Win32Menu`: `ID_FILE_OPEN_SPONZA = 6002`, `AppendMenuW(m_fileMenu, MF_STRING, ID_FILE_OPEN_SPONZA, L"Sponza!")` 추가
   - `Win32Menu::HandleCommand`: `case ID_FILE_OPEN_SPONZA → m_fileSponzaCallback()` 처리
   - `FileSponzaCallback` + `SetFileSponzaCallback()` 추가
   - `Engine::LoadSponzaScene()` 구현:
     - 파일 열기 다이얼로그로 씬 선택 → `LoadScene()` 호출 (표준 로딩 절차 동일)
     - 카메라를 Sponza 전용 위치로 재배치: position `{10, 4.5, 4}`, lookAt `{0, 0, 0}`, FOV 60°
     - Sponza 전용 조명 레이아웃 설정: 기존 3-point + Orbit 라이트를 교체
       - Key Light: Directional (태양, warm `{1.0, 0.95, 0.8}`, intensity=10, castShadow=true), direction `normalize({-0.3, -1, 0.5})`
       - Fill Light: Point (하늘 간접광, cool `{0.4, 0.5, 0.7}`, intensity=1.75, position `{-6, 10, 0}`)
       - Torch ×4: Point (횃불 `{1.0, 0.45, 0.08}`, intensity=8, 빠른 감쇠 Kl=0.7/Kq=1.8), 코너 4곳 배치
     - `m_orbitLightIndex = SIZE_MAX` — Sponza에서는 Orbit 조명 비활성화
   - `Engine::Initialize()`: `m_menu->SetFileSponzaCallback([this](){ LoadSponzaScene(); })` 연결
8. **Sponza 태양 방향 토글 — L 키**
   - `Engine.h`: `m_isSponzaScene`, `m_sponzaSunAltMode`, `m_sponzaSunToggleKeyWasDown`, `m_sponzaSunKeyIndex` 추가
   - `Engine::LoadSponzaScene()`: `m_isSponzaScene = true`, `m_sponzaSunAltMode = false`, `m_sponzaSunKeyIndex = 0` 설정
   - `Engine::LoadScene()`: `m_isSponzaScene = false` (일반 씬 로드 시 토글 비활성화)
   - `Engine::Update()`: `m_isSponzaScene && L 키 에지` 감지 시 방향 토글
     - 기본: `normalize({-0.3, -1.0, 0.5})` → 앙각 ≈ 60°
     - Alt: `normalize({-0.3, -1.5, 0.3})` → 앙각 ≈ 74° (1층까지 더 깊이 조명)
   - `Sponza!` 메뉴로 로드된 경우에만 동작, 일반 `LoadScene()`으로 열면 비활성

**완료 기준**: Shadow Map 해상도가 씬 크기에 맞게 자동 선택됨, Shadow 카메라가 씬 전체 깊이 범위를 커버, nearPlane·spotNear·shadowNormalBiasWorld가 sceneDiagonal 기반으로 스케일링, ShadowTexelSize가 GPU로 동적 전달, Orbit Directional Light 회전 그림자 정상 렌더링, "File > Sponza!" 메뉴 항목에서 Sponza 씬 로드 및 전용 카메라/조명 자동 적용, L 키로 태양 방향 60°↔74° 토글 (Sponza 전용), 빌드 오류 0건 (경고 1건 잔존 — FXC 컴파일러 한계)

### Phase 25: Bistro 씬 분석 + glTF 에셋 준비
**목표**: Bistro glTF 변환본(`niagara_bistro`)을 에셋으로 준비하고, 씬 스케일·카메라·광원·Shadow Map
추천 세팅을 분석하여 SceneSettings.md에 문서화한다. (구현 없음, 설계·에셋 준비 단계)

1. **glTF 변환본 선택 및 다운로드**: `niagara_bistro` (github.com/zeux/niagara_bistro, MIT)
   - 포맷: `bistro.gltf` + `bistro.bin` + `textures/*.png` (DDS→PNG 변환 완료)
   - NVIDIA ORCA Bistro 원본을 lightly edit, glTF 카메라 내장, Vulkan niagara 렌더러에서 검증
   - `git clone https://github.com/zeux/niagara_bistro assets/test-models/Bistro`
2. **씬 스케일 분석** → SceneSettings.md "Bistro Exterior" 섹션 추가:
   - Exterior: ~2,832,120 삼각형 (NVIDIA ORCA 공식 수치)
   - Interior: ~1,046,609 삼각형 (Interior with wine: ~1,293,691)
   - Exterior 예상 크기: ~40m × 25m × 15m, diagonal ≈ 50m (glTF 단위: m)
   - Assimp 로드 시 scale 변환 불필요 (glTF 기본 단위 m, Sponza의 0.008 factor 없음)
3. **카메라 추천 세팅** → SceneSettings.md 기록 (정면 거리 기준)
4. **광원 추천 세팅** → SceneSettings.md 기록 (저녁 Directional + 가로등 Point)
5. **Shadow Map 자동 설정 예측** → diagonal ≈ 50m 기준, 현재 엔진 자동 계산값 기록

**완료 기준**: `assets/test-models/Bistro/bistro.gltf` 준비 완료, SceneSettings.md Bistro 섹션 작성 완료

### Phase 26: Bistro! 빠른 로드 메뉴 + 씬 전용 설정
**목표**: File 메뉴에 "Bistro!" 항목을 추가하고, `Engine::LoadBistroScene()`에서 Phase 25에서
정리한 카메라·광원 세팅을 자동 적용한다. Phase 24의 Sponza! 구현을 참조한다.

1. **Win32Menu 확장**:
   - `ID_FILE_OPEN_BISTRO = 6003`, File 메뉴에 "Bistro!" 항목 추가
   - `WM_COMMAND → m_fileBistroCallback()` 처리
   - `FileBistroCallback` + `SetFileBistroCallback()` 추가
2. **`Engine::LoadBistroScene()` 구현**:
   - 파일 열기 다이얼로그(bistro.gltf 선택) → `LoadScene()` 호출 (표준 로딩)
   - 카메라: Bistro 전용 프리셋 (SceneSettings.md 기준)
   - `m_lightManager->Clear()` 후 Bistro 전용 광원 배치:
     - Directional "Evening Sun" (warm `{1.0, 0.85, 0.6}`, intensity≈6, castShadow=true)
     - Point "Street Lamp" × N (주황 가로등 `{1.0, 0.9, 0.6}`, 거리 감쇠)
     - Point "Café Fill" (실내 누출광 `{0.9, 0.8, 0.5}`)
   - `m_orbitLightIndex = SIZE_MAX` — Bistro에서 Orbit 조명 비활성
3. **`Engine::Initialize()`**: `m_menu->SetFileBistroCallback([this](){ LoadBistroScene(); })` 연결

**완료 기준**: "File > Bistro!" 메뉴 항목으로 bistro.gltf 로드 및 전용 카메라·광원 자동 적용, Shadow Map 자동 설정(diagonal≈50m → 2048×2048) 정상 동작

### Phase 27: Texture Streaming + Mip-Mapping
**목표**: 필요 Mip만 GPU 로드, 가시성/거리 기반 우선순위

1. `src/Asset/TextureStreamer.h/.cpp` — Mip 레벨 기반 스트리밍
2. 텍스처 우선순위: `priority = isVisible ? (1/distance) : 0`
3. 초기 로드: 하위 Mip만 → 필요 시 상위 Mip 비동기 로딩
4. Mip chain 생성: `floor(log2(max(w,h))) + 1`
5. Sampler: Anisotropic (MaxAnisotropy = 16)
6. 메모리 예산: VRAM 모니터링, LRU + 거리 기반 해제

**완료 기준**: 카메라 거리에 따라 Mip 레벨 동적 로딩/해제, Anisotropic 필터링 적용

### Phase 28: Instanced Rendering + 멀티스레드 로딩
**목표**: 동일 Mesh+Material 인스턴싱, 병렬 리소스 로딩

1. `src/Renderer/InstanceBatcher.h/.cpp` — 동일 Mesh+Material 그룹핑
2. Instance Buffer: InstanceData (World Matrix) per-instance 슬롯
3. DrawIndexedInstanced 호출
4. `src/Core/ThreadPool.h/.cpp` — CPU 코어 수 기반 워커 스레드
5. 텍스처 디코딩 병렬화: 스레드 풀에 태스크 제출
6. Copy Queue: Graphics Queue와 병렬 업로드

**완료 기준**: 동일 메시 인스턴싱으로 드로우콜 감소, 멀티스레드 텍스처 디코딩

### Phase 29: GPU 메모리 최적화
**목표**: CB 풀링, VRAM 적응, Shared Material CB, Dirty Flag, Front-to-Back

1. CBPool: Upload Heap 풀링, 256바이트 정렬, 링 버퍼
2. VRAM 모니터링: `IDXGIAdapter3::QueryVideoMemoryInfo`
3. 적응적 CB 갱신: VRAM > Budget의 80% 시 저우선순위 오브젝트 갱신 빈도 감소
4. Shared Material CB (PerObjectCB + PerMaterialCB 분리)
5. Dirty Flag: Transform/Material/Light 미변경 시 갱신 스킵
6. Opaque Front-to-Back 정렬 (Early-Z rejection)
7. DebugHUD: VRAM 사용량, 스트리밍 리소스 수/대역폭, 렌더 통계

**완료 기준**: CB 풀에서 슬롯 할당, VRAM 예산 초과 시 적응적 동작, Dirty Flag 갱신 스킵

### Phase 30: Phase 02 통합 & 최종 검증
**목표**: 전체 Phase 02 기능 통합, 대형 씬 벤치마크

1. 전체 렌더 파이프라인 통합 (12단계):
   Scene Graph 순회 → Frustum Culling → Occlusion Culling → LOD(자동 LOD 포함) → Light Culling →
   Instance Batching → Texture Streaming → CB 갱신 → Material 정렬 → Front-to-Back → Shadow Pass → Main Pass
2. **Shadow Depth Pass Frustum Culling**: 광원 시점의 BoundingFrustum을 생성하여 FrustumCuller를
   Shadow Depth Pass에도 적용 — 광원 시야 밖 오브젝트의 shadow draw call 스킵
3. 대형 씬 벤치마크: Sponza, Bistro 등 로딩 및 렌더링 확인
4. 5단계 렌더링 모드 전체 동작 확인
5. DebugHUD 전체 항목: FPS, 해상도, 폴리곤, culled/occluded 수, 드로우콜, VRAM, 스트리밍, 렌더모드
6. 모든 유닛 테스트 + 스모크 테스트 통과
7. 성능 프로파일링 및 최적화 조정

**완료 기준**: Sponza급 씬을 PBR+Shadow+최적화로 60fps 이상 렌더링, 모든 테스트 통과

### Phase 31: RRScenePreprocessor — 오프라인 씬 전처리 도구 + 백그라운드 자동 생성
**목표**: glTF/GLB/FBX 씬을 처리하여 엔진 전용 바이너리(`.rrscene`)로 저장하는 파이프라인을 구현한다.
두 가지 진입점을 제공한다: ① 독립 CLI 도구(`RRScenePreprocessor.exe`), ② 표준 경로 로딩 완료 후
렌더링 앱 내 백그라운드 자동 생성. 렌더링 앱은 `.rrscene`을 직접 로드하여 GPU 업로드만 수행한다.

1. **전처리 파이프라인 구현** (`src/Asset/ScenePreprocessor.h/.cpp`, CLI와 엔진이 공유):
   - `ScenePreprocessor::Generate(sourcePath, outputPath)`: 동기 전처리 (CLI 도구용)
   - `ScenePreprocessor::GenerateAsync(sourcePath)`: `std::async`로 백그라운드 실행, `std::future<bool>` 반환 (엔진 내 자동 생성용)
   - 내부 파이프라인: Assimp 파싱 → Vertex/Index 변환 + Tangent → 프리미티브 분리 → 메시별 AABB → Auto-LOD(QEM, LOD1=50%/LOD2=25%) → 이미지 디코딩(stb_image) → Mip chain(CPU box filter) → 씬 직렬화
   - 원자적 파일 쓰기: 임시 파일(`.rrscene.tmp`) 완성 후 원본 경로로 rename (부분 파일 방지)

2. **VS 프로젝트 `RRScenePreprocessor` 추가** (솔루션 내 Console Application):
   - `ScenePreprocessor` (엔진 헤더 공유)를 호출하는 얇은 CLI 래퍼
   - 진입점: `main(argc, argv)` — 입력 파일 경로를 인수로 받아 `Generate()` 호출
   - 출력: `bin/Debug/RRScenePreprocessor.exe`

3. **`.rrscene` 바이너리 포맷 정의** (`src/Asset/RRSceneFormat.h`, 공용 헤더):
   - Header: magic("RRSC"), version(uint32), sourceHash(uint64, 크기^수정시각), 섹션 오프셋 테이블
   - Scene Section: 노드 계층(부모-자식 인덱스, 이름, 로컬 TRS), 씬 AABB, 카메라 초기 상태
   - Mesh Section: 메시별 — Vertex 배열(position/color/normal/texCoord/tangent) raw dump, Index 배열, AABB, LOD 데이터(LOD 0~2 Vertex/Index + 전환 거리)
   - Material Section: PBR factor 값, AlphaMode, doubleSided, 텍스처 인덱스, sRGB/Linear 포맷 힌트
   - Texture Section: 텍스처별 — 너비/높이/Mip 수, DXGI_FORMAT, 전체 Mip chain 픽셀 데이터(연속 배치)
   - Light Section: 타입/색상/강도/위치/방향/감쇠/원뿔각/castShadow/BoundingSphere radius

4. **렌더링 앱 이중 로딩 경로 추가** (`src/Asset/SceneLoader`):
   - **고속 경로**: `.rrscene` 발견 + 해시 일치 → 바이너리 직접 읽기 → GPU 업로드(VB/IB/텍스처)만 수행
   - **표준 경로**: `.rrscene` 없거나 해시 불일치 → Assimp 런타임 파싱 → 로딩 완료 후 항목 5 실행
   - 자동 감지: 원본 파일과 동일 디렉토리에 동일 이름 `.rrscene` 존재 → 자동 선택
   - DebugHUD에 로딩 경로 표시: "Fast (.rrscene)" / "Standard (Assimp)"

5. **표준 경로 로딩 후 백그라운드 자동 전처리** (`Engine::LoadScene()`):
   - 표준 경로(Assimp) 로딩 완료 직후: `ScenePreprocessor::GenerateAsync(sourcePath)` 호출
   - 렌더링을 블로킹하지 않고 백그라운드 스레드에서 전처리 파이프라인 실행
   - DebugHUD에 진행 상태 표시: "Preprocessing scene..." (완료 후 사라짐)
   - 완료 시: `.rrscene` 파일 원자적 저장, 콘솔 로그 출력 ("Sponza.rrscene saved")
   - 다음 로딩 시 자동으로 고속 경로 사용

**완료 기준**: 신규 씬 첫 로딩 시 표준 경로 + 백그라운드 자동 생성 동작 확인, 두 번째 로딩 시 자동으로 고속 경로 사용 확인(1~3초), CLI 도구(`RRScenePreprocessor.exe`)로도 동일한 `.rrscene` 생성 가능, 렌더링 결과 동일

---

### Phase 32: 코드 리뷰, 최적화, 버그 수정 & 아키텍처 문서화
**목표**: 전체 코드 품질 점검, 성능 최적화, 버그 수정, ARCHITECTURE.md 작성

1. **전체 코드 리뷰**:
   - 모든 소스 파일을 순회하며 코드 품질 점검
   - 사용되지 않는 코드(dead code), 불필요한 include, 중복 로직 제거
   - 네이밍 컨벤션 일관성 검증 (PascalCase/camelCase/UPPER_SNAKE_CASE)
   - OWASP 취약점 점검: 버퍼 오버플로우, 범위 초과 접근, null 역참조 등
   - COM 객체/GPU 리소스 해제 누락 검사 (Fence 대기 후 해제 보장)
   - 스마트 포인터/ComPtr 사용 일관성 검증

2. **성능 최적화**:
   - GPU 프로파일링: PIX 또는 타임스탬프 쿼리로 병목 구간 식별
   - CPU 프로파일링: 핫 루프, 불필요한 메모리 할당, 과도한 복사 제거
   - D3D12 Warning/Error 메시지 전수 확인 (Debug Layer)
   - 셰이더 최적화: 불필요한 분기 제거, 상수 폴딩, 레지스터 압력 점검
   - 드로우콜 수, 상태 전환 횟수 최소화 확인
   - 메모리 누수 점검 (D3D12 Live Object 리포트)

3. **UX 개선**:
   - **드래그 앤 드롭**: Win32 `WM_DROPFILES` 처리 → 파일 경로 추출 → `LoadScene()` 호출
   - **Camera 중클릭 패닝**: 중클릭 드래그 시 right·up 벡터 기준 카메라 평행 이동

4. **버그 수정 및 엣지 케이스 처리**:
   - 모든 유닛 테스트 + 스모크 테스트 재실행, 실패 항목 수정
   - 윈도우 리사이즈/모드 전환 중 안정성 확인
   - 빈 씬(메시 0개), Material 없는 Mesh, 텍스처 없는 Material 등 엣지 케이스 처리
   - 대형 씬 로딩 중 메모리 부족 시 graceful 처리
   - 멀티스레드 race condition 검증 (ThreadSanitizer 또는 수동 검증)

5. **ARCHITECTURE.md 작성**:
   - 프로젝트 전체 디렉토리 구조 + 각 파일의 역할 설명
   - 모듈 간 의존성 다이어그램 (텍스트 기반)
   - 엔진 초기화 → 메인 루프 → 종료까지의 동작 흐름
   - 프레임당 렌더링 파이프라인 흐름 (11단계 상세)
   - 주요 클래스 관계도 (Engine, Renderer, SceneGraph, RHI, Asset 등)
   - D3D12 리소스 라이프사이클 (생성 → 사용 → 해제)
   - 데이터 흐름: CPU(SceneLoader) → Memory(SceneGraph/Material/Texture) → GPU(CB/VB/IB/SRV)
   - 스레딩 모델: 메인 스레드 vs 워커 스레드 vs Copy Queue
   - 셰이더 파이프라인: 입력 레이아웃, CB 레지스터 맵, 텍스처 바인딩 포인트
   - 기존 PRD/PLAN/CLAUDE와의 참조 관계 명시

**완료 기준**: 전체 코드 리뷰 완료, 모든 테스트 통과, D3D12 Debug Layer 경고 0건, PBR.hlsl X4000 잔존 경고 제거, ARCHITECTURE.md 작성 완료

---

## Phase 03: 고급 렌더링 기법

> Phase 02 완료 코드 위에 GPU-Driven 컬링, 고급 섀도잉, 스켈레탈 애니메이션,
> 지연 렌더링(Deferred Shading), 포스트 프로세싱, 레이 트레이싱, 신경망 업스케일링 등
> 최신 실시간 렌더링 기법을 단계적으로 추가한다.

**포함 Phase**: Phase 33 ~ Phase 49

---

### Phase 33: Occlusion Culling — Hi-Z GPU
**목표**: 현재 P0 스텁(항상 false)인 `OcclusionCuller`를 GPU Hi-Z 방식으로 완전 구현.
CPU Readback 간이 방식을 거치지 않고 바로 Hi-Z로 구현한다.
현재 엔진에 Compute Shader 인프라가 없으므로, 먼저 인프라를 구축한 뒤 Hi-Z를 구현한다.

0. **Phase 02 Backup 생성** *(Phase 33 구현 시작 전 최초 1회)*:
   - 프로젝트 루트에 `Phase 02 Backup/` 폴더를 생성하고, 루트의 전체 소스(`src/`, `tests/`, `assets/`, `shaders/` 등)를 복사한다.
   - `Phase 01 Backup/` 폴더는 복사 대상에서 제외한다 (이중 백업 방지).
   - `bin/`, `.git/`, `*.user`, `*.suo`, `ipch/` 등 빌드 산출물 및 IDE 캐시는 제외한다.
   - 백업 완료 후 `Phase 02 Backup/README.md`에 백업 일시, Phase 02 최종 완료 상태를 기록한다.
   - **`Phase 02 Backup/` 폴더 안의 파일은 백업 후 절대 수정하지 않는다. 이후 어떠한 Phase 구현에서도 이 폴더를 참조만 하고 절대 건드리지 않는다.**

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
   - Optimization 메뉴 항목 추가: `ID_OPTIM_OCCLUSION_CULL = 8004`

**완료 기준**: Sponza에서 Occlusion Culling 활성화 시 드로우콜 절감 수치 DebugHUD 확인, GPU stall 없이 1프레임 레이턴시로 동작

---

### Phase 34: Point Light Cube Map Shadowing
**목표**: `castShadow = true`인 Point Light에 대해 6면 Cube Map 기반 Omnidirectional Shadow Map 구현.

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

**완료 기준**: Point light `castShadow = true` 설정 시 구면 그림자 정상 렌더링, PCF 적용으로 경계 부드러움, Sponza 횃불 위치에 그림자 확인 가능

---

### Phase 35: Skeletal Animation
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
   - `Vertex` 구조체에 `XMUSHORT4 joints` (JOINTS_0) + `XMFLOAT4 weights` (WEIGHTS_0) 추가
   - D3D12 Input Layout, HLSL 입력 구조체, `static_assert` 갱신

6. **GPU Skinning**:
   - Joint matrix palette CB: `cbuffer SkinCB : register(b4)` — 최대 128개 bone matrix
   - `src/Shaders/PBR.hlsl`: `#define SKINNING` 조건부 컴파일로 스킨드/비스킨드 분기
     - `float4 skinnedPos = Σ(weights[i] * mul(jointMatrices[joints[i]], localPos))`
     - Normal, Tangent도 동일 변환 적용
   - AnimationController::Update() 후 현재 bone world matrix 계산 → GPU 업로드

**완료 기준**: glTF 애니메이션 파일(예: CesiumMan.glb, RiggedFigure.glb)에서 노드 TRS 애니메이션 및 스킨 메시 애니메이션이 정상 재생, 모든 테스트 통과

---

### Phase 36: RRScenePreprocessor 확장 — Skeletal Animation 지원
**목표**: Phase 33에서 추가된 Skeleton/Skin/Animation 데이터를 `.rrscene` 포맷에 통합하여
전처리기와 렌더러 양쪽을 확장한다. 애니메이션 씬도 고속 로딩 경로를 사용할 수 있게 된다.

1. **`.rrscene` 포맷 버전 업 (v2)**:
   - Header의 version 필드: 1 → 2
   - Vertex 포맷 확장: `joints(XMUINT4)` + `weights(XMFLOAT4)` 필드 추가 (스킨 메시용)
   - **Skeleton Section 추가**: 본 수, 본별 이름/parentIndex/inverseBindMatrix, Skin → joint 인덱스 배열
   - **Animation Section 추가**: 클립 수, 클립별 — 이름, 재생 시간, 채널 수, 채널별 — target 노드 인덱스/Property(TRS)/Interpolation/키프레임 배열(시간+값)
   - 하위 호환: v1(비애니메이션 씬) 파일도 계속 로딩 가능

2. **RRScenePreprocessor 확장**:
   - Assimp `aiMesh::mBones` → Skeleton/Skin 직렬화 (per-vertex joint/weight 포함)
   - Assimp `aiAnimation` → AnimationClip 직렬화 (TRS 키프레임, 보간 타입 포함)
   - 스킨 메시의 Vertex에 joints/weights 필드 포함하여 `.rrscene` Mesh Section 저장
   - 비스킨 메시는 joints/weights 필드 생략 (플래그로 구분)

3. **렌더링 앱 고속 경로 확장**:
   - `.rrscene` v2 로딩: Skeleton/Skin → `Skeleton`/`Skin` 객체 생성, Animation → `AnimationClip` 객체 생성
   - AnimationController에 클립 등록, 자동 재생 시작 (씬에 클립이 있을 때)
   - 스킨 Vertex 데이터 → GPU VB(joint/weight 포함) 업로드

4. **버전 감지 및 마이그레이션**:
   - v1 파일 로딩 시 Skeleton/Animation Section 없음 → 해당 객체 미생성으로 처리
   - 원본 파일 해시 불일치 → 재전처리 안내 메시지 출력

**완료 기준**: CesiumMan.glb를 RRScenePreprocessor로 전처리 후 렌더링 앱에서 `.rrscene` 고속 로딩으로 스켈레탈 애니메이션 정상 재생 확인, 비애니메이션 `.rrscene`(v1)과의 하위 호환 유지

---

### Phase 37: Deferred Rendering — G-Buffer 기반 렌더링 파이프라인
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

### Phase 38: HDR Pipeline + Tone Mapping
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

### Phase 39: SSAO (Screen Space Ambient Occlusion)
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

### Phase 40: Bloom + Post-Processing 파이프라인
**목표**: Bloom 효과 구현 및 Ping-Pong Buffer 기반 Post-Processing 프레임워크 구축.

1. **Ping-Pong Buffer 프레임워크**: HDR RT 2개 교대 사용, `PostProcessor::AddPass(shader)` 패스 등록
2. **Bright Pass**: Luminance 임계값(기본 1.0) 이상 픽셀 추출
3. **Gaussian Blur Pyramid**: 6단계 다운샘플 → 업샘플 합성 (Dual Kawase Blur 활용)
4. **Bloom Composite**: Bloom 레이어를 HDR RT에 Additive Blend
5. **파이프라인 순서 확정**: Bloom → Tone Mapping → TAA → sRGB 출력
6. **메뉴/HUD**: Bloom on/off, 임계값·Intensity 조정

**완료 기준**: Bloom 효과 동작, Post-Processing 프레임워크 완성

---

### Phase 41: TAA (Temporal Anti-Aliasing)
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

### Phase 42: Motion Blur + Depth of Field
**목표**: Per-Object Motion Blur와 Bokeh DoF로 영화적 품질 향상.

1. **Motion Blur** (Phase 41 Velocity Buffer 활용):
   - Tile-based Max Velocity: N×N 타일 내 최대 속도 계산
   - 속도 방향으로 N샘플 평균, Soft-Edge 처리, 셔터 속도 시뮬레이션
2. **Depth of Field**:
   - CoC(Circle of Confusion): Depth → CoC 반경 계산(Focus Distance, F-Number 파라미터)
   - Bokeh Blur: CoC 가변 반경 Gather Blur (기본: Separable Gaussian, 품질: Hexagonal Bokeh)
   - Near/Far Field 분리 처리
   - F-Number, Focal Length 메뉴 조정

**완료 기준**: Motion Blur per-object 동작, DoF CoC 기반 블러, 메뉴 파라미터 조정

---

### Phase 43: SSR (Screen Space Reflections) + Refraction
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

### Phase 44: Screen Space Subsurface Scattering (SSSSS)
**목표**: 피부·밀랍·대리석 등 반투명 재질의 광 산란 효과.

1. **Subsurface Material**: `subsurfaceColor`(XMFLOAT3) + `scatterWidth`(float) 파라미터 추가
2. **SSS Pass (Separable)**:
   - Stencil 마스크로 SSS/비-SSS 픽셀 분리
   - 6-weight Gaussian Kernel × 3채널(R > G > B 확산 폭): 수평→수직 2패스 분리
   - R 채널 가장 넓게 산란 (붉은 피부 효과)
3. **참고 모델**: 피부 재질 테스트 모델 또는 ProceduralSphere + SSS 재질

**완료 기준**: SSS on/off 비교, RGB 채널별 확산 폭 조절, Stencil 마스크 동작

---

### Phase 45: Global Illumination — DDGI (Dynamic Diffuse GI)
**목표**: 씬 전역 동적 간접광 시뮬레이션. Irradiance Probe 기반 DDGI 구현.

1. **Probe Grid**: 씬 AABB 내 3D Grid(예: 8×4×8 = 256 Probe) 배치
2. **Probe Update**:
   - DXR 사용 가능 시: 각 Probe에서 구면 방향으로 Radiance Ray 발사 (Phase 46 연동)
   - DXR 미지원 시: 정적 Reflection Capture Probe로 폴백
   - Irradiance(L0) + Visibility(Depth) → Probe Texture(Octahedral Map) 저장
3. **Probe Sampling**: 픽셀 위치 → 3D Grid → 8코너 Probe 삼선형 보간, SH2/SH3 Irradiance 샘플링
4. **Lighting Pass 통합**: Indirect Diffuse += Probe Irradiance × Albedo (기존 Fill Light 대체/보완)
5. **디버그 뷰**: Probe 위치·Irradiance 시각화

**완료 기준**: Probe Grid 배치, Irradiance 업데이트, 씬 간접광 표현, 디버그 시각화

---

### Phase 46: DXR Hybrid Ray Tracing
**목표**: DirectX 12 Raytracing(DXR) API로 Ray-Traced Shadow·Reflection·GI 구현.
Rasterization과 Ray Tracing을 Hybrid 방식으로 결합, PCF Shadow/SSR 대비 최고 품질 달성.

1. **DXR 인프라**:
   - `ID3D12Device5::CreateStateObject()` — DXR PSO (RayGen/ClosestHit/Miss/AnyHit 셰이더)
   - BLAS: 메시별 생성 및 GPU 빌드 (정적/동적 BLAS 분리)
   - TLAS: 씬 전체 인스턴스 행렬 기반 매 프레임 갱신
   - ShaderTable: RayGen/Miss/HitGroup 테이블 빌드 및 업로드
2. **Ray-Traced Shadow**: Directional/Point/Spot 광원별 Shadow Ray, 반투명 AnyHit, PCF 대체
3. **Ray-Traced Reflection**: G-Buffer Normal+Roughness → 반사 Ray, Cone Sampling, 재귀 1~2레벨
4. **GI 연동**: Phase 45 DDGI Probe Update에 DXR Ray 활용
5. **Denoiser 연동**: Phase 48 Neural Denoiser 또는 Temporal Accumulation Denoiser
6. **하드웨어 감지 및 폴백**: DXR Tier 1.1 미지원 시 PCF Shadow/SSR로 자동 폴백

**완료 기준**: TLAS/BLAS 빌드, RT Shadow 동작, RT Reflection 동작, DXR/Raster Hybrid 전환

---

### Phase 47: Nanite-style Virtual Geometry
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

### Phase 48: Neural Upscaling (DLSS/FSR) + Neural Denoising
**목표**: AI/ML 기반 업스케일링으로 저해상도 렌더링 + 고품질 출력. Ray-Traced 노이즈 제거.

1. **FSR 3 (AMD FidelityFX Super Resolution)**:
   - FidelityFX SDK 연동: Color Buffer + Depth + Motion Vector → FSR3 업스케일 출력
   - Quality Mode 메뉴: Quality / Balanced / Performance / Ultra Performance
   - 렌더 해상도: 출력 해상도의 50%/67%/75%로 설정 가능
2. **DLSS 3 (NVIDIA DLSS) — 선택적**:
   - NVIDIA Streamline SDK 연동 (RTX 하드웨어 전용), 미지원 시 FSR로 자동 폴백
3. **Neural Denoising**:
   - 옵션 A: NRD (NVIDIA Real-time Denoising) SDK 연동 (Relax/Reblur 알고리즘)
   - 옵션 B: 자체 Temporal Accumulation Denoiser (모멘트 기반 분산 추정 + Bilateral Filter)
4. **DebugHUD**: Upscaling 모드, 렌더/출력 해상도, Denoiser 종류 표시

**완료 기준**: FSR 3 업스케일 동작, Quality Mode 전환, Denoiser 적용 (RT 결과 또는 독립 노이즈 입력)

---

### Phase 49: Phase 03 코드 리뷰, 최적화, 버그 수정 & 아키텍처 문서화
**목표**: Phase 33~48에서 추가된 모든 고급 렌더링 기법의 코드 품질 점검, 성능 최적화,
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
   - 전체 렌더 파이프라인 다이어그램 (Phase 01 ~ Phase 48 누적 아키텍처)
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
Phase 32 (Phase 02 완료: 코드 리뷰 + ARCHITECTURE.md)
    │
    ├── Phase 33 (Occlusion Culling: Hi-Z GPU + Compute 인프라) ──┐
    ├── Phase 34 (Point Light Cube Map Shadowing) ─────────────────┤
    ├── Phase 35 (Skeletal Animation) ───────────────────────────────┤
    │       └── Phase 36 (RRScenePreprocessor 확장: Skeletal 지원) ──┤
    │                                                               │
    └──────────────────────────────────── Phase 37 (Deferred Rendering) ─┘
                                                         │
                                          Phase 38 (HDR Pipeline + Tone Mapping)
                                                         │
                                ┌───────── Phase 39 (SSAO) ──────────────────┐
                                │         Phase 40 (Bloom + PP 파이프라인)    │
                                │         Phase 41 (TAA) ─────────────────────┤
                                │         Phase 42 (Motion Blur + DoF) ───────┤
                                │         Phase 43 (SSR + Refraction) ─────────┤
                                │         Phase 44 (SSSSS) ────────────────────┤
                                │                                              │
                                └──────────────────────── Phase 45 (DDGI/GI) ──┤
                                                                   │           │
                                                    Phase 46 (DXR Hybrid RT) ──┘
                                                                   │
                                                    Phase 47 (Nanite: Virtual Geometry)
                                                                   │
                                                    Phase 48 (Neural Upscaling + Denoising)
                                                                   │
                                                    Phase 49 (코드 리뷰 + 최적화 + 버그 수정 + ARCHITECTURE.md)
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

## Phase 02 의존성 그래프

```
Phase 11 (Phase 01 완료)
    │
    ├── Phase 12 (SceneLoader) ──┐
    │       └── Phase 13 (Vertex+Material) ──┐
    │               └── Phase 14 (Texture) ──┤
    │                                        ├── Phase 16 (셰이더: PBR) ──┐
    ├── Phase 15 (RHI 확장) ─────────────────┤                           │
    │                                        ├── Phase 17 (다중 광원) ───┤
    │                                        │       └── Phase 18 (Shadow) ──┐
    │                                        │                               │
    │                                        ├── Phase 19 (씬 UI+카메라) ────┤
    │                                        │                               │
    │                                        └── Phase 20 (렌더 모드) ───────┤
    │                                                                        │
    ├── Phase 21 (레거시 정리+테스트) ────────────────────────────────────────┤
    ├── Phase 22 (프리미티브 분리+AABB) ────────────────────────────────────┤
    │       └── Phase 23 (Culling+LOD) ────────────────────────────────────┤
    │               └── Phase 24 (HLSL 경고+Shadow 자동 크기) ✅ ───────────┤
    ├── Phase 25 (Bistro 씬 분석+에셋 준비) ─────────────────────────────────┤
    │       └── Phase 26 (Bistro! 메뉴+씬 전용 설정) ────────────────────────┤
    ├── Phase 27 (Texture Streaming) ────────────────────────────────────────┤
    ├── Phase 28 (Instancing+멀티스레드) ────────────────────────────────────┤
    ├── Phase 29 (GPU 메모리 최적화) ────────────────────────────────────────┤
    │                                                                        │
    └────────────────────────────────────────────────────── Phase 30 (통합) ─┘
                                                                             │
                                                            Phase 31 (RRScenePreprocessor: .rrscene 전처리 도구) ─┘
                                                                             │
                                                            Phase 32 (코드 리뷰 + ARCHITECTURE.md)
                                                                             ↓
                                                                     Phase 03 시작 (Phase 33~48)
```

## Phase 02 리스크 & 대응

| 리스크 | 대응 |
|--------|------|
| Assimp 파싱 성능 (대형 씬) | Assimp 파싱은 메인 스레드, 후처리(VB/IB/텍스처)를 워커 스레드에 분배 |
| 텍스처 메모리 초과 (Sponza 등) | Texture Streaming + VRAM 예산 모니터링으로 Mip 동적 해제 |
| Shadow Map 품질 (계단, acne) | Depth Bias 튜닝 + PCF 커널 크기 조정 (3×3 → 5×5) |
| 다중 PSO 관리 복잡도 | PSO 캐시 (Material 속성 조합 → PSO 매핑), 상태 정렬로 전환 최소화 |
| CB 풀 크기 부족 (대형 씬) | 초기 4MB, 부족 시 자동 확장 (새 Upload Heap 추가), VRAM 예산 내에서 |
| 멀티스레드 race condition | GPU 업로드는 메인 스레드에서만, 교체는 원자적, 상태 플래그로 동기화 |
| Gamma Correction 이중 적용 | SRGB 텍스처/렌더타겟 설정을 정확히 구분, pow 수동 변환과 혼용 금지 |
| 렌더 모드 전환 시 깜빡임 | PSO/CB를 미리 준비, 모드 전환은 프레임 경계에서만 수행 |
