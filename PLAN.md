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
4. 드래그 앤 드롭: WM_DROPFILES 처리 (P1)
5. 카메라 마우스 네비게이션:
   - 우클릭 드래그: Yaw/Pitch 회전
   - 마우스 휠: 전진/후진 (돌리 줌)
   - 중클릭 드래그: 패닝 (P1)
6. 카메라 키보드 네비게이션:
   - WASD: 카메라 시선 방향 기준 전진/후퇴/좌/우 이동
   - Q/E: 월드 Y축 기준 상/하 이동
   - +/-: FOV 증가/감소
   - 이동 속도: 씬 바운딩 박스 크기에 비례 자동 조절
7. Fit to Scene: 씬 바운딩 박스 기반 카메라 자동 배치
8. 이동 속도 자동 조절: 씬 크기에 비례 (마우스/키보드 공통, P1)

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

### Phase 24: HLSL 경고 수정 + Shadow Map 자동 크기 조정 ✅
**목표**: PBR.hlsl X4000 경고 최소화, Shadow Map 해상도 및 투영 범위를 씬 크기에 맞게 자동 조정

1. **PBR.hlsl — SampleShadowMap 구조 개선** (X4000 경고 최소화)
   - `[branch] switch` → `float result = 1.0f; if/else-if` 체인으로 교체
   - `ShadowMap` X4000 경고 제거 (`result = 1.0f` 명시적 초기화)
   - `CalcShadow`: `shadowIdx = min(shadowIdx, MAX_SHADOW_MAPS - 1)` 인덱스 범위 보장
   - `shadow += saturate(SampleShadowMap(...))` — PCF 누적 값 범위 명시
   - 잔존 X4000 경고 1건: FXC 컴파일러 고유 한계 (비교 샘플러 + 동적 cbuffer 인덱스 조합), Phase 29에서 재검토
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
   - Spot/Directional far plane: 고정 `100.0f` → `sceneDiagonal × 3.0f`
   - Shadow pass 전 `shadowConst.shadowTexelSize = 1.0f / GetShadowMapSize()`
5. **Engine::LoadScene() — 씬 로드 후 자동 연결**
   - `Renderer::SetSceneDiagonal(m_sceneDiagonal)` 호출
   - 씬 크기 기반 해상도 선택: ≤ 10m → 1024, ≤ 100m → 2048, > 100m → 4096
   - `D3D12Context::SetShadowMapSize()` + `RecreateShadowMaps()` 호출

**완료 기준**: Shadow Map 해상도가 씬 크기에 맞게 자동 선택됨, Shadow Ortho/Perspective 범위가 sceneDiagonal 기반으로 스케일링, ShadowTexelSize가 GPU로 동적 전달, 빌드 오류 0건 (경고 1건 잔존 — FXC 컴파일러 한계)

### Phase 25: Texture Streaming + Mip-Mapping
**목표**: 필요 Mip만 GPU 로드, 가시성/거리 기반 우선순위

1. `src/Asset/TextureStreamer.h/.cpp` — Mip 레벨 기반 스트리밍
2. 텍스처 우선순위: `priority = isVisible ? (1/distance) : 0`
3. 초기 로드: 하위 Mip만 → 필요 시 상위 Mip 비동기 로딩
4. Mip chain 생성: `floor(log2(max(w,h))) + 1`
5. Sampler: Anisotropic (MaxAnisotropy = 16)
6. 메모리 예산: VRAM 모니터링, LRU + 거리 기반 해제

**완료 기준**: 카메라 거리에 따라 Mip 레벨 동적 로딩/해제, Anisotropic 필터링 적용

### Phase 26: Instanced Rendering + 멀티스레드 로딩
**목표**: 동일 Mesh+Material 인스턴싱, 병렬 리소스 로딩

1. `src/Renderer/InstanceBatcher.h/.cpp` — 동일 Mesh+Material 그룹핑
2. Instance Buffer: InstanceData (World Matrix) per-instance 슬롯
3. DrawIndexedInstanced 호출
4. `src/Core/ThreadPool.h/.cpp` — CPU 코어 수 기반 워커 스레드
5. 텍스처 디코딩 병렬화: 스레드 풀에 태스크 제출
6. Copy Queue (P1): Graphics Queue와 병렬 업로드

**완료 기준**: 동일 메시 인스턴싱으로 드로우콜 감소, 멀티스레드 텍스처 디코딩

### Phase 27: GPU 메모리 최적화
**목표**: CB 풀링, VRAM 적응, Shared Material CB, Dirty Flag, Front-to-Back

1. CBPool: Upload Heap 풀링, 256바이트 정렬, 링 버퍼
2. VRAM 모니터링: `IDXGIAdapter3::QueryVideoMemoryInfo`
3. 적응적 CB 갱신: VRAM > Budget의 80% 시 저우선순위 오브젝트 갱신 빈도 감소
4. Shared Material CB (PerObjectCB + PerMaterialCB 분리)
5. Dirty Flag: Transform/Material/Light 미변경 시 갱신 스킵
6. Opaque Front-to-Back 정렬 (Early-Z rejection)
7. DebugHUD: VRAM 사용량, 스트리밍 리소스 수/대역폭, 렌더 통계

**완료 기준**: CB 풀에서 슬롯 할당, VRAM 예산 초과 시 적응적 동작, Dirty Flag 갱신 스킵

### Phase 28: Phase 02 통합 & 최종 검증
**목표**: 전체 Phase 02 기능 통합, 대형 씬 벤치마크

1. 전체 렌더 파이프라인 통합 (12단계):
   Scene Graph 순회 → Frustum Culling → Occlusion Culling → LOD(자동 LOD 포함) → Light Culling →
   Instance Batching → Texture Streaming → CB 갱신 → Material 정렬 → Front-to-Back → Shadow Pass → Main Pass
2. 대형 씬 벤치마크: Sponza, Bistro 등 로딩 및 렌더링 확인
3. 5단계 렌더링 모드 전체 동작 확인
4. DebugHUD 전체 항목: FPS, 해상도, 폴리곤, culled/occluded 수, 드로우콜, VRAM, 스트리밍, 렌더모드
5. 모든 유닛 테스트 + 스모크 테스트 통과
6. 성능 프로파일링 및 최적화 조정

**완료 기준**: Sponza급 씬을 PBR+Shadow+최적화로 60fps 이상 렌더링, 모든 테스트 통과

### Phase 29: 코드 리뷰, 최적화, 버그 수정 & 아키텍처 문서화
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

3. **버그 수정 및 엣지 케이스 처리**:
   - 모든 유닛 테스트 + 스모크 테스트 재실행, 실패 항목 수정
   - 윈도우 리사이즈/모드 전환 중 안정성 확인
   - 빈 씬(메시 0개), Material 없는 Mesh, 텍스처 없는 Material 등 엣지 케이스 처리
   - 대형 씬 로딩 중 메모리 부족 시 graceful 처리
   - 멀티스레드 race condition 검증 (ThreadSanitizer 또는 수동 검증)

4. **ARCHITECTURE.md 작성**:
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
    ├── Phase 25 (Texture Streaming) ────────────────────────────────────────┤
    ├── Phase 26 (Instancing+멀티스레드) ────────────────────────────────────┤
    ├── Phase 27 (GPU 메모리 최적화) ────────────────────────────────────────┤
    │                                                                        │
    └────────────────────────────────────────────────────── Phase 28 (통합) ─┘
                                                                             │
                                                            Phase 29 (코드 리뷰 + ARCHITECTURE.md) ─┘
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
