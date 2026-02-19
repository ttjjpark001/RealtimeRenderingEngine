# CLAUDE.md — 실시간 렌더링 엔진 프로젝트 가이드

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
  Core/         — Engine 메인 루프, 공용 타입 (Types.h에 DirectXMath 별칭)
  Math/         — MathUtil.h (DirectXMath 헬퍼)
  Platform/     — Win32 윈도우/입력/메뉴 (플랫폼별 분리)
  RHI/          — 렌더링 하드웨어 추상화 인터페이스 (IRHIDevice, IRHIBuffer, IRHIContext)
    D3D12/      — DirectX 12 백엔드 (Device, Context, SwapChain, Buffer, PSO, DescriptorHeap)
  Renderer/     — Vertex, Mesh, FaceColorPalette, MeshFactory, Renderer, DebugHUD
  Scene/        — SceneNode, SceneGraph, Transform, Camera
  Lighting/     — PointLight (위치, 색상, 감쇠)
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
- HLSL 셰이더: Position + Color + Normal 입력, WVP 변환 + Diffuse 라이팅

### 수학 (DirectXMath)
- 저장 타입: XMFLOAT3, XMFLOAT4, XMFLOAT4X4 (멤버 변수용)
- 연산 타입: XMVECTOR, XMMATRIX (SIMD 레지스터, 로컬 연산용)
- 변환 패턴: XMLoadFloat3 → SIMD 연산 → XMStoreFloat3
- Types.h 별칭: Vector3 = XMFLOAT3, Vector4 = XMFLOAT4, Matrix4x4 = XMFLOAT4X4

### Scene Graph
- 트리 구조: 루트 노드 아래 부모-자식 계층
- 각 SceneNode는 Transform(위치/회전/스케일)과 Mesh 참조를 보유
- WorldMatrix = Parent.WorldMatrix × Local.TRS_Matrix
- 깊이 우선 순회로 렌더링

### 상태 표시 HUD (DebugHUD)
- 화면 왼쪽 상단에 렌더링 통계를 텍스트 오버레이로 표시
- 표시 항목: FPS, 해상도(WxH), 종횡비, 전체 폴리곤 수, 초당 폴리곤 처리 속도
- D2D/DirectWrite interop 또는 GDI interop으로 텍스트 렌더링

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

### 포인트 광원 (PointLight)
- 장면에 1개의 포인트 광원이 존재
- 속성: 위치(XMFLOAT3), 색상(XMFLOAT3), 감쇠 계수 (constant/linear/quadratic)
- HLSL Pixel Shader에서 Diffuse + Ambient 라이팅 계산
- Constant Buffer로 매 프레임 광원 데이터를 GPU에 전달
- 화면에 광원 정보(색상명, 위치) 표시 가능, "Light" 메뉴에서 on/off 토글
- 메뉴에서 광원 색상 선택 (White/Red/Green/Blue/Yellow/Cyan/Magenta)
- 방향키 + PgUp/PgDn으로 광원 위치 실시간 이동

### 카메라 (Camera)
- Scene/Camera.h/.cpp에 위치
- 투영 모드: Perspective (기본) / Orthographic 전환 가능
- 속성: 위치(XMFLOAT3), 시선 대상(lookAt), FOV, near/far plane
- GetViewMatrix(): XMMatrixLookAtLH로 뷰 행렬 생성
- GetProjectionMatrix(aspectRatio): 투영 모드에 따라 Perspective/Orthographic 행렬 생성
- WASD+QE 키로 카메라 위치 이동, +/- 키로 FOV 조절
- DebugHUD에 카메라 정보(투영 종류, 위치, 방향, FOV) 표시 가능, "Camera" 메뉴에서 on/off 토글
- 메뉴에서 투영 모드 전환, FOV 조절, Reset 가능

### Vertex 데이터
- `struct Vertex { XMFLOAT3 position; XMFLOAT4 color; XMFLOAT3 normal; }` — 연속 메모리 배치
- D3D12 Input Layout: POSITION (R32G32B32_FLOAT) + COLOR (R32G32B32A32_FLOAT) + NORMAL (R32G32B32_FLOAT)
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
d3d12.lib      — DirectX 12 core
dxgi.lib       — DXGI (SwapChain, Adapter)
d3dcompiler.lib — HLSL 런타임 컴파일
dxguid.lib     — DirectX GUIDs
```

## 테스트 규칙

- DirectXMath 유틸리티 변경 시: `tests/unit/test_MathUtil.cpp` 실행
- Scene Graph 변경 시: `tests/unit/test_SceneGraph.cpp` 실행
- MeshFactory/색상 변경 시: `tests/unit/test_FaceColoring.cpp` 실행
- Camera 변경 시: `tests/unit/test_Camera.cpp` 실행
- RHI/D3D12 변경 시: `tests/smoke/test_RHIBackend.cpp` 실행
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
