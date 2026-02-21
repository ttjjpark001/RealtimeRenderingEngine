# PRD: 실시간 렌더링 엔진 (Realtime Rendering Engine)

## 1. 개요

Win32 API 기반의 실시간 렌더링 엔진을 C++로 개발한다. 하드웨어 추상화 계층(RHI)을 통해 렌더링 백엔드를 분리하고, Scene Graph 기반의 오브젝트 관리 체계를 갖춘다.

## 2. 목표

- 독립적인 Win32 윈도우 애플리케이션으로 동작
- 하드웨어 종속적인 렌더링 코드를 추상화하여 백엔드 교체 가능
- Scene Graph를 통한 계층적 오브젝트 관리
- Transform Matrix 기반 오브젝트 회전
- 테스트 자동화를 통한 품질 보증

## 3. 기능 요구사항

### 3.1 윈도우 시스템
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| W-01 | Win32 API로 메인 윈도우를 생성한다 | P0 |
| W-02 | 뷰포트를 마우스 드래그로 자유롭게 리사이즈할 수 있다 | P0 |
| W-03 | 리사이즈 시 종횡비가 자유롭게 변경된다 | P0 |
| W-04 | 리사이즈 시 렌더링 컨텍스트가 올바르게 갱신된다 | P0 |
| W-05 | 메뉴를 통해 윈도우 크기를 프리셋 해상도로 변경할 수 있다 | P0 |
| W-06 | 프리셋 해상도: 800x450 (윈도우 모드), 960x540 (윈도우 모드) | P0 |
| W-07 | 메뉴를 통해 전체 화면(Full Screen) 모드로 전환할 수 있다 | P0 |
| W-08 | 전체 화면에서 윈도우 모드로 복귀할 수 있다 (Esc 키 또는 메뉴) | P0 |
| W-09 | 화면 모드 변경 시 렌더링 컨텍스트(SwapChain, RTV 등)가 올바르게 갱신된다 | P0 |
| W-10 | 프리셋 해상도 변경과 마우스 드래그 리사이즈는 독립적으로 모두 동작한다 | P0 |
| W-11 | 전체 화면 전환은 Borderless Windowed 방식(WS_POPUP 스타일 + 모니터 전체 크기 SetWindowPos)으로 구현한다. DXGI SetFullscreenState는 사용하지 않는다 | P0 |

### 3.2 Rendering Hardware Interface (RHI)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| R-01 | 렌더링 API 호출을 추상화하는 RHI 인터페이스를 정의한다 | P0 |
| R-02 | RHI는 디바이스 초기화, 프레임 시작/종료, 드로우 콜을 포함한다 | P0 |
| R-03 | DirectX 12를 첫 번째 RHI 백엔드로 구현한다 | P0 |
| R-04 | 버텍스 버퍼, 인덱스 버퍼를 RHI를 통해 관리한다 | P0 |
| R-05 | 렌더링 시 Depth Stencil Buffer를 생성하고 DSV(Depth Stencil View)로 바인딩하여 깊이 테스트를 수행한다 | P0 |
| R-06 | 뷰포트 리사이즈 시 Depth Stencil Buffer를 재생성한다 | P0 |
| R-07 | Transform Matrix(World/View/Projection)를 매 프레임 GPU에 전달하기 위해 ID3D12DescriptorHeap(CBV 힙) 및 Upload Buffer 기반 Constant Buffer 관리 로직을 구현한다 | P0 |

### 3.3 Scene Graph
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| S-01 | 트리 구조의 Scene Graph를 구현한다 | P0 |
| S-02 | 오브젝트는 부모-자식 계층 구조(Object Hierarchy)를 갖는다 | P0 |
| S-03 | 자식 오브젝트는 부모의 Transform을 상속받는다 | P0 |
| S-04 | Scene Graph 순회를 통해 렌더링 순서를 결정한다 | P0 |

### 3.4 Transform & Rotation
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| T-01 | 4x4 Transform Matrix를 사용하여 오브젝트 변환을 표현한다 | P0 |
| T-02 | 오브젝트별 회전(Rotation)을 Transform Matrix로 적용한다 | P0 |
| T-03 | Translation, Rotation, Scale 조합을 지원한다 | P1 |
| T-04 | 부모-자식 간 Transform 연쇄(concatenation)를 지원한다 | P0 |

### 3.5 Vertex Data Structure
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| V-01 | Vertex는 Position(x, y, z) 속성을 갖는다 | P0 |
| V-02 | Vertex는 Color(r, g, b, a) 속성을 갖는다 | P0 |
| V-03 | Vertex는 Normal(nx, ny, nz) 속성을 갖는다 (라이팅 연산용) | P0 |
| V-04 | Vertex 데이터를 메모리에 연속적으로 배치한다 | P0 |
| V-05 | Vertex 구조체 멤버의 바이트 오프셋은 D3D12 Input Layout 선언과 정확히 일치해야 하며, `static_assert`로 빌드 타임 검증한다 | P0 |

### 3.6 면 색상 규칙 (Face Coloring)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| C-01 | 도형의 각 면에 사용할 색상 팔레트: Red, Green, Blue, Cyan, Magenta, Yellow, Black, White (8색) | P0 |
| C-02 | 이웃한 면(edge를 공유하는 면)에는 서로 다른 색상을 적용한다 (그래프 컬러링) | P0 |
| C-03 | 면 색상은 도형 생성 시 자동으로 결정된다 (MeshFactory에서 처리) | P0 |
| C-04 | 같은 면을 구성하는 모든 Vertex에 동일한 색상을 지정한다 (flat shading) | P0 |

### 3.7 오브젝트 선택 메뉴
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| M-01 | Win32 메뉴바를 통해 화면에 표시할 3D 오브젝트를 선택할 수 있다 | P0 |
| M-02 | 선택 가능한 오브젝트: 구(Sphere), 정사면체(Tetrahedron), 정육면체(Cube), 실린더(Cylinder) | P0 |
| M-03 | 메뉴에서 오브젝트를 선택하면 즉시 화면의 물체가 교체된다 | P0 |
| M-04 | 현재 선택된 오브젝트에 체크 표시가 된다 | P0 |
| M-05 | 기본 선택 오브젝트는 정육면체(Cube)이다 | P0 |

### 3.8 애니메이션 제어
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| A-01 | 오브젝트 회전 애니메이션을 시작/멈춤(토글) 할 수 있다 | P0 |
| A-02 | 메뉴바의 "Animation" 메뉴에서 "Play / Pause" 항목으로 토글한다 | P0 |
| A-03 | Space 키로도 애니메이션 시작/멈춤을 토글할 수 있다 | P0 |
| A-04 | 멈춤 상태에서는 오브젝트의 현재 회전 각도가 유지된다 | P0 |
| A-05 | 시작 시 기본 상태는 애니메이션 재생(Play)이다 | P0 |

### 3.9 포인트 광원 (Point Light)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| L-01 | 장면에 포인트 광원(Point Light) 1개가 존재한다 | P0 |
| L-02 | 광원은 색상(RGB)과 위치(x, y, z) 속성을 갖는다 | P0 |
| L-03 | 광원에 의한 diffuse 라이팅이 오브젝트에 적용된다 | P0 |
| L-04 | 광원의 속성(색상, 위치)이 화면에 텍스트로 표시된다 | P0 |
| L-05 | 광원 정보 표시를 메뉴에서 on/off 토글할 수 있다 | P0 |
| L-06 | 메뉴를 통해 광원의 색상을 변경할 수 있다 (White, Red, Green, Blue, Yellow, Cyan, Magenta 중 선택) | P0 |
| L-07 | 키보드(방향키 + PgUp/PgDn)로 광원의 위치를 이동할 수 있다 | P0 |
| L-08 | 광원 위치 변경 시 라이팅 결과가 실시간 갱신된다 | P0 |
| L-09 | 라이팅은 픽셀 셰이더(Pixel Shader)에서 픽셀 단위(Per-Pixel Lighting)로 계산한다 | P0 |
| L-10 | 광원 감쇠는 거리 기반 수식 `attenuation = 1 / (Kc + Kl·d + Kq·d²)` 를 적용한다 (Kc: 상수 계수, Kl: 선형 계수, Kq: 이차 계수) | P0 |

### 3.10 상태 표시 HUD (On-Screen Debug Info)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| H-01 | 화면 왼쪽 상단에 디버그 정보를 텍스트로 오버레이 표시한다 | P0 |
| H-02 | 현재 프레임레이트(FPS)를 표시한다 | P0 |
| H-03 | 현재 렌더링 해상도(Width x Height)를 표시한다 | P0 |
| H-04 | 현재 가로세로비(Aspect Ratio)를 표시한다 | P0 |
| H-05 | 장면 내 전체 폴리곤(삼각형) 개수를 표시한다 | P0 |
| H-06 | 초당 폴리곤 처리 속도(Polygons/sec)를 표시한다 | P0 |
| H-07 | 리사이즈 시 해상도/종횡비 값이 실시간 갱신된다 | P0 |

### 3.11 카메라 (Camera)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| CAM-01 | 장면에 카메라 1개가 존재한다 | P0 |
| CAM-02 | 카메라 투영 방식을 Perspective 또는 Orthographic으로 전환할 수 있다 | P0 |
| CAM-03 | 카메라의 위치(x, y, z)를 설정/변경할 수 있다 | P0 |
| CAM-04 | 카메라의 방향(Look Direction)을 설정/변경할 수 있다 | P0 |
| CAM-05 | Perspective 모드에서 FOV(Field of View)를 조절할 수 있다 | P0 |
| CAM-06 | Near/Far 클리핑 평면 값을 갖는다 | P1 |
| CAM-07 | 카메라 정보(투영 종류, 위치, 방향, FOV)가 화면에 텍스트로 표시된다 | P0 |
| CAM-08 | 카메라 정보 표시를 메뉴에서 on/off 토글할 수 있다 | P0 |
| CAM-09 | 메뉴를 통해 카메라의 투영 방식을 전환할 수 있다 | P0 |
| CAM-10 | 키보드(WASD + Q/E)로 카메라 위치를 이동할 수 있다 | P0 |
| CAM-11 | +/- 키로 FOV를 조절할 수 있다 | P0 |
| CAM-12 | 카메라 속성 변경 시 렌더링 결과가 실시간 갱신된다 | P0 |

### 3.12 테스트
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| Q-01 | DirectXMath 래퍼/유틸리티에 대한 유닛 테스트를 작성한다 | P0 |
| Q-02 | Scene Graph 조작에 대한 유닛 테스트를 작성한다 | P0 |
| Q-03 | 엔진 초기화~렌더링 루프 1프레임 실행의 스모크 테스트를 작성한다 | P0 |
| Q-04 | RHI 백엔드 초기화/해제 스모크 테스트를 작성한다 | P0 |
| Q-05 | MeshFactory의 면 색상 인접 규칙 위반 여부를 검증하는 유닛 테스트를 작성한다 | P0 |

## 4. 비기능 요구사항

| ID | 요구사항 |
|----|----------|
| NF-01 | C++17 이상 표준 사용 |
| NF-02 | Visual Studio 2022 Solution (v143 툴셋) |
| NF-03 | 외부 라이브러리 의존성 최소화 (Win32 API + DirectX 12 + 표준 라이브러리 중심) |
| NF-04 | 60fps 이상의 렌더 루프 유지 목표 |
| NF-05 | 테스트 프레임워크: Google Test |
| NF-06 | HLSL 셰이더는 앱 빌드 타임에 .cso(Compiled Shader Object) 파일로 사전 컴파일한다. 런타임 D3DCompileFromFile 호출은 사용하지 않는다 |

## 5. 기술 스택

| 구분 | 선택 |
|------|------|
| 언어 | C++17 |
| 플랫폼 | Windows 10/11 |
| 윈도우 | Win32 API |
| 빌드 | Visual Studio 2022 Solution (.sln) |
| 수학 라이브러리 | DirectXMath (Windows SDK 내장) |
| 테스트 | Google Test (vcpkg) |
| 렌더링 백엔드 | DirectX 12 |
| 링크 라이브러리 | d3d12.lib, dxgi.lib, d3dcompiler.lib, dxguid.lib |

## 6. 용어 정의

| 용어 | 설명 |
|------|------|
| RHI | Rendering Hardware Interface. 렌더링 하드웨어 추상화 계층 |
| Scene Graph | 장면을 트리 구조로 표현하는 데이터 구조 |
| Transform Matrix | 오브젝트의 위치/회전/스케일을 표현하는 4x4 행렬 |
| Vertex | 3D 공간의 한 점. Position과 Color 등의 속성을 가짐 |
| Smoke Test | 시스템의 기본 동작 여부를 확인하는 간단한 통합 테스트 |
| HUD | Head-Up Display. 화면 위에 오버레이되는 디버그/상태 정보 |
| Face Coloring | 면 색상 규칙. 이웃한 면이 같은 색을 갖지 않도록 색상을 배정하는 그래프 컬러링 |
| Color Palette | 면에 사용 가능한 8색: Red, Green, Blue, Cyan, Magenta, Yellow, Black, White |
| Point Light | 위치와 색상을 가진 전방향 점 광원. 거리에 따라 감쇠(attenuation)한다 |
| Diffuse Lighting | 광원 방향과 표면 법선의 내적으로 계산하는 기본 조명 모델 |
| Camera | 장면을 바라보는 시점. 위치, 방향, 투영 방식(Perspective/Orthographic)을 갖는다 |
| FOV (Field of View) | Perspective 카메라의 시야각(라디안). 값이 클수록 넓은 범위가 보인다 |
| Orthographic Projection | 원근감 없이 평행 투영하는 카메라 모드 |
| Perspective Projection | 원근법이 적용되는 카메라 모드 (기본) |
| Full Screen | 데스크톱 전체를 차지하는 전체 화면 모드. DXGI SetFullscreenState 또는 Borderless Windowed로 구현 |
