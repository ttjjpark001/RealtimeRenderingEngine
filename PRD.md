# PRD: 실시간 렌더링 엔진 (Realtime Rendering Engine)

> **Phase 01 완료 및 백업 안내**
> Phase 01에서 구현한 모든 내용은 `Phase 01 Backup/` 폴더에 백업되었다.
> 해당 폴더의 파일은 참조하거나 수정하지 않는다.
> 이후 작업은 프로젝트 루트의 `src/`, `tests/` 등 현재 디렉토리에서 진행한다.

---

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

### 3.7 초기 씬 없음 (Phase 22에서 변경)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| S-01 | 앱 시작 시 빈 화면을 표시하며 씬 로드를 대기한다 | P0 |
| S-02 | "File → Open Scene..." 메뉴 또는 드래그앤드롭으로 glTF/GLB/FBX 씬을 로드한다 | P0 |

> ~~3.7 오브젝트 선택 메뉴 (M-01~M-05)~~: Phase 22에서 삭제. 초기 프로시저럴 씬과 Object 메뉴 제거.
> ~~3.8 애니메이션 제어 (A-01~A-05)~~: Phase 22에서 삭제. 프로시저럴 오브젝트 회전 애니메이션 제거.

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
| CAM-13 | 마우스 우클릭 드래그로 카메라 시선 방향(Yaw/Pitch)을 회전할 수 있다 | P0 |
| CAM-14 | 마우스 휠로 카메라를 전진/후진(돌리) 이동할 수 있다 | P0 |
| CAM-15 | 마우스 중클릭(휠 클릭) 드래그로 카메라를 상하좌우 패닝할 수 있다 | P1 |
| CAM-16 | 로드된 씬의 바운딩 박스를 기반으로 카메라 이동 속도가 자동 조절된다 | P1 |
| CAM-17 | "Camera" 메뉴에서 "Fit to Scene"을 선택하면 카메라가 씬 전체를 볼 수 있는 위치로 재배치된다 | P0 |

### 3.12 테스트
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| Q-01 | DirectXMath 래퍼/유틸리티에 대한 유닛 테스트를 작성한다 | P0 |
| Q-02 | Scene Graph 조작에 대한 유닛 테스트를 작성한다 | P0 |
| Q-03 | 엔진 초기화~렌더링 루프 1프레임 실행의 스모크 테스트를 작성한다 | P0 |
| Q-04 | RHI 백엔드 초기화/해제 스모크 테스트를 작성한다 | P0 |
| Q-05 | MeshFactory의 면 색상 인접 규칙 위반 여부를 검증하는 유닛 테스트를 작성한다 | P0 |

---

## Phase 02: glTF 2.0 씬 로딩 및 PBR 렌더링

### 3.13 3D 씬 파일 로딩
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| G-01 | glTF 2.0 (.gltf + .bin) 파일을 로딩할 수 있다 | P0 |
| G-02 | GLB (.glb) 바이너리 포맷을 로딩할 수 있다 | P0 |
| G-02a | FBX (.fbx) 파일을 로딩할 수 있다 | P0 |
| G-02b | 파일 다이얼로그 필터에 FBX 포맷을 포함한다 (*.gltf;*.glb;*.fbx) | P0 |
| G-03 | Assimp 라이브러리를 사용하여 glTF/FBX 파싱을 수행한다 (Assimp의 통합 씬 구조로 포맷 차이를 추상화) | P0 |
| G-04 | Mesh 데이터를 로딩한다: position, normal, texcoord(UV), tangent, index | P0 |
| G-05 | glTF node hierarchy를 엔진의 SceneNode 트리로 변환한다 | P0 |
| G-06 | glTF material 정보를 추출하여 엔진의 Material 객체로 변환·저장한다 (baseColor, metallic, roughness, normal, emissive 등 모든 PBR 파라미터) | P0 |
| G-07 | Material이 참조하는 텍스처(albedo, normal, roughness, metallic 등)를 비동기로 로드하여 엔진 전용 GPU 리소스(Texture 객체)로 변환·저장한다 | P0 |
| G-07a | 비동기 텍스처 로딩 중에도 렌더링이 중단되지 않는다 (폴백 텍스처 또는 factor 값으로 렌더링) | P0 |
| G-07b | embedded(glTF 내장) 및 external(외부 파일) 텍스처 이미지를 모두 지원한다 (PNG, JPEG 등) | P0 |
| G-08 | Node transform 애니메이션(translation, rotation, scale 키프레임)을 로딩한다 | Phase 35 |
| G-09 | Skeletal animation(bone/skin)을 로딩한다 | Phase 35 |
| G-10 | 대형 씬(Sponza, Bistro 등)을 로딩하여 렌더링할 수 있다 | P0 |
| G-11 | "File" 메뉴의 "Open Scene..." 항목을 선택하면 파일 다이얼로그(GetOpenFileName)가 열린다 | P0 |
| G-12 | 파일 다이얼로그에서 glTF/GLB 파일을 선택하면 해당 씬을 로드하여 화면에 렌더링한다 | P0 |
| G-13 | 씬 로드 시 기존 씬(Phase 01 데모 오브젝트 포함)을 해제하고 새 씬으로 교체한다 | P0 |
| G-14 | 씬 로드 후 씬 파일에 카메라 시작 위치가 지정되어 있으면 해당 위치/방향으로 카메라를 배치한다 | P0 |
| G-14a | 씬 파일에 카메라 정보가 없으면 씬의 바운딩 박스를 기반으로 씬 전체를 조망할 수 있는 위치에 카메라를 자동 배치한다 (Fit to Scene) | P0 |
| G-15 | 로드된 씬을 카메라 네비게이션(마우스 + 키보드 WASD/QE)으로 자유롭게 탐색할 수 있다 | P0 |
| G-16 | 드래그 앤 드롭으로 glTF/GLB 파일을 윈도우에 놓아도 씬이 로드된다 | P1 |

### 3.14 Material 시스템
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| MAT-01 | PBR metallic-roughness 워크플로우를 지원하는 Material 클래스를 구현한다 | P0 |
| MAT-02 | Base Color (텍스처 + factor)를 지원한다 | P0 |
| MAT-03 | Metallic-Roughness (텍스처 + factor)를 지원한다 | P0 |
| MAT-04 | Normal Map 텍스처를 지원한다 | P0 |
| MAT-05 | Emissive (텍스처 + factor)를 지원한다 | P1 |
| MAT-06 | Occlusion 텍스처를 지원한다 | P1 |
| MAT-07 | Alpha 모드를 지원한다: Opaque, Mask(alphaCutoff), Blend | P0 |
| MAT-08 | Double-sided 렌더링 플래그를 지원한다 | P0 |
| MAT-09 | Material이 없는 Mesh는 기존 vertex-color 방식으로 폴백 렌더링한다 | P0 |

### 3.15 Texture 시스템
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| TEX-01 | 이미지 데이터로부터 D3D12 텍스처 리소스(Texture2D)를 생성한다 | P0 |
| TEX-02 | 텍스처에 대한 Shader Resource View(SRV)를 생성하고 바인딩한다 | P0 |
| TEX-03 | Sampler 상태(Linear, Point, Anisotropic 등)를 관리한다 | P0 |
| TEX-04 | 동일 텍스처의 중복 로딩을 방지하는 캐싱을 구현한다 | P0 |
| TEX-05 | SRGB 포맷 텍스처를 올바르게 처리한다 (baseColor = SRGB, normal/roughness/metallic = Linear) | P0 |
| TEX-06 | 텍스처 로딩을 별도 스레드에서 비동기로 수행한다 (메인 렌더 루프 블로킹 방지) | P0 |
| TEX-07 | 비동기 로딩 완료 전까지 1x1 폴백 텍스처(white)를 바인딩한다 | P0 |
| TEX-08 | 비동기 로딩 완료 시 GPU 리소스를 메인 스레드에서 안전하게 교체한다 | P0 |

### 3.16 광원 시스템 확장 (Multi-Light)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| ML-01 | 광원 타입으로 Directional Light를 추가한다 (방향, 색상, 강도) | P0 |
| ML-02 | 광원 타입으로 Spot Light를 추가한다 (위치, 방향, 색상, 내부/외부 원뿔각, 감쇠) | P0 |
| ML-03 | 기존 Point Light는 그대로 유지한다 (위치, 색상, 감쇠) | P0 |
| ML-04 | 셰이더에서 최대 8개 이상의 광원을 동시에 처리할 수 있다 | P0 |
| ML-05 | 각 광원은 타입(Directional/Point/Spot), 색상, 강도, 위치, 방향, 감쇠 파라미터를 갖는다 | P0 |
| ML-06 | Structured Buffer 또는 Constant Buffer 배열로 다중 광원 데이터를 GPU에 전달한다 | P0 |
| ML-07 | 활성 광원 개수를 프레임마다 셰이더에 전달한다 | P0 |
| ML-08 | Spot Light의 조명 감쇠는 내부/외부 원뿔각 사이에서 smoothstep으로 페이드한다 | P0 |
| ML-09 | 광원 추가/제거/편집을 런타임에 수행할 수 있다 | P1 |

### 3.17 Shadow Mapping
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| SM-01 | Shadow Mapping을 이용하여 그림자를 생성한다 | P0 |
| SM-02 | Directional Light: Orthographic 투영 기반 Shadow Map을 생성한다 | P0 |
| SM-03 | Spot Light: Perspective 투영 기반 Shadow Map을 생성한다 | P0 |
| SM-04 | Point Light: Cube Map(6면) 기반 Omnidirectional Shadow Map을 생성한다 | P1 |
| SM-05 | Shadow Map 해상도: 기본 1024×1024 (설정 가능) | P0 |
| SM-06 | Shadow Map은 Depth-only 패스로 렌더링한다 (별도 렌더 패스) | P0 |
| SM-07 | Shadow Map을 SRV로 바인딩하여 라이팅 셰이더에서 그림자 판정에 사용한다 | P0 |
| SM-08 | 그림자 acne 방지를 위해 depth bias를 적용한다 | P0 |
| SM-09 | Percentage Closer Filtering(PCF)을 적용하여 그림자 계단 현상을 완화한다 | P0 |
| SM-10 | PCF 커널 크기: 최소 3×3 (설정 가능) | P0 |
| SM-11 | PCF는 Shadow Map 텍셀 오프셋 기반으로 주변 샘플을 비교·평균한다 | P0 |
| SM-12 | 그림자가 있는 영역은 해당 광원의 diffuse+specular 기여가 차단된다 (ambient는 유지) | P0 |
| SM-13 | 여러 광원의 그림자가 독립적으로 계산된다 (광원별 Shadow Map) | P0 |

### 3.18 셰이더 확장 (Cook-Torrance BRDF)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| SH-01 | Cook-Torrance BRDF 모델을 HLSL로 구현하여 물리 기반 렌더링을 수행한다 | P0 |
| SH-02 | Cook-Torrance Specular: D(GGX/Trowbridge-Reitz) × G(Smith-Schlick) × F(Fresnel-Schlick) / (4·NdotL·NdotV) | P0 |
| SH-03 | Diffuse 항은 Lambertian diffuse (albedo / π)를 사용한다 | P0 |
| SH-04 | Metallic 파라미터로 diffuse/specular 비율을 결정한다 (metallic=1이면 diffuse=0, F0=albedo) | P0 |
| SH-05 | Roughness 파라미터로 표면 거칠기를 제어한다 (GGX alpha = roughness²) | P0 |
| SH-06 | Albedo, Normal, Metallic, Roughness 텍스처를 샘플링하여 BRDF 입력으로 사용한다 | P0 |
| SH-07 | Vertex에 UV 좌표(TEXCOORD) 및 Tangent 입력을 지원한다 | P0 |
| SH-08 | Normal Map을 탄젠트 공간(TBN 행렬)에서 월드 공간으로 변환하여 적용한다 | P0 |
| SH-09 | 기존 vertex-color 전용 셰이더(BasicColor)와 공존한다 (Phase 01 오브젝트 호환) | P0 |
| SH-10 | Alpha Test(Mask 모드)와 Alpha Blend를 셰이더에서 처리한다 | P0 |
| SH-11 | 텍스처가 바인딩되지 않은 채널은 Material의 factor 값으로 폴백한다 | P0 |
| SH-12 | 픽셀 셰이더에서 다중 광원(최대 8개 이상)을 루프로 순회하며 각 광원의 기여를 합산한다 | P0 |
| SH-13 | 광원 타입(Directional/Point/Spot)에 따라 조명 방향, 감쇠, 원뿔 페이드를 분기 계산한다 | P0 |
| SH-14 | 각 광원에 대해 Shadow Map을 샘플링하여 그림자 여부를 판정한다 | P0 |
| SH-15 | PCF(Percentage Closer Filtering)를 적용하여 부드러운 그림자 경계를 생성한다 | P0 |
| SH-16 | Shadow depth 패스용 간소화 셰이더를 구현한다 (VS: position 변환만, PS: 없음 또는 depth 출력만) | P0 |
| SH-17 | 최종 픽셀 출력에 Gamma Correction을 적용한다. 셰이더 내부 라이팅 연산은 리니어 공간에서 수행하고, 최종 결과에 `pow(color, 1/2.2)` 변환을 적용하여 sRGB 출력한다 (SRGB 렌더 타겟 또는 셰이더 수동 변환) | P0 |

### 3.19 프리미티브 분리 + Per-Mesh AABB

| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| PRIM-01 | glTF/GLB 씬 로딩 시, 단일 aiNode에 연결된 복수의 aiMesh(서브 프리미티브)를 각각 별도 SceneNode로 분리하여 노드 단위 Culling/LOD/Instancing이 적용될 수 있게 한다 | P0 |
| PRIM-02 | 각 Mesh에 로컬 AABB(BoundingBox)를 계산하여 저장한다. `BoundingBox::CreateFromPoints()`를 사용한다 | P0 |
| PRIM-03 | SceneNode에 월드 공간 AABB를 캐싱한다. Transform 변경 시 dirty flag를 설정하고, GetWorldAABB() 호출 시 재계산한다 | P0 |

### 3.20 렌더링 최적화

#### Frustum Culling & Occlusion Culling
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| OPT-01 | Frustum Culling을 구현하여 카메라 시야(View Frustum) 밖의 오브젝트를 렌더링에서 제외한다 | P0 |
| OPT-02 | 오브젝트의 AABB(Axis-Aligned Bounding Box)와 View Frustum의 6개 평면을 교차 검사한다 | P0 |
| OPT-03 | Frustum Culling은 Shadow Depth Pass에도 적용한다 (광원 frustum 기준) | P1 |
| OPT-04 | Occlusion Culling을 구현하여 다른 오브젝트에 완전히 가려진(occluded) 오브젝트의 상수 버퍼 업데이트 및 드로우콜을 스킵한다 | P0 |
| OPT-05 | Hi-Z(Hierarchical-Z) 기반 또는 이전 프레임 depth buffer를 활용한 소프트웨어 Occlusion Culling을 구현한다 | P1 |
| OPT-06 | Occluded 판정된 오브젝트는 Constant Buffer 갱신, 텍스처 바인딩, DrawCall 모두를 스킵한다 | P0 |

#### LOD (Level of Detail)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| OPT-07 | LOD(Level of Detail) 시스템을 구현하여 카메라 거리에 따라 메시 디테일을 전환한다 | P0 |
| OPT-08 | LOD 단계: 최소 2단계 이상 (High, Low 또는 High, Medium, Low) | P0 |
| OPT-09 | LOD 전환 거리 기준값을 오브젝트별 또는 전역으로 설정할 수 있다 | P1 |
| OPT-10 | glTF/FBX 파일에 LOD 메시가 포함되어 있으면 자동으로 LOD 단계에 매핑한다 | P0 |
| OPT-10a | LOD 메시가 씬 파일에 없는 경우, Edge Collapse(QEM) 기반 메시 심플리피케이션으로 LOD 메시를 자동 생성한다 | P0 |
| OPT-10b | 자동 LOD 생성은 백그라운드 스레드에서 비동기로 수행하며, 생성 완료 전까지 원본 메시(LOD 0)로 렌더링한다 | P0 |
| OPT-10c | 자동 LOD 생성 시 LOD 1은 원본 삼각형의 ~50%, LOD 2는 ~25%로 축소한다 | P0 |
| OPT-11 | 자동 LOD 생성이 실패하거나 메시가 이미 충분히 간단한 경우 단일 LOD로 동작한다 (폴백) | P0 |

#### Texture Streaming & Mip-Mapping
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| OPT-12 | Texture Streaming을 구현하여 필요한 Mip 레벨만 GPU 메모리에 로드한다 | P0 |
| OPT-13 | 텍스처 로딩 우선순위를 카메라 Frustum 내 가시 여부와 카메라 거리로 결정한다 (가시 + 가까울수록 높은 우선순위) | P0 |
| OPT-14 | Frustum 밖의 텍스처는 로딩 우선순위를 최하위로 내리거나 스트리밍을 일시 중단한다 | P0 |
| OPT-15 | 카메라 거리에 따라 요구 Mip 레벨을 결정한다 (가까울수록 고해상도 Mip 요구) | P0 |
| OPT-16 | 상위 Mip(고해상도)은 필요 시 비동기로 스트리밍 로드하고, 로드 전까지 하위 Mip으로 렌더링한다 | P0 |
| OPT-17 | GPU 메모리 사용량 예산을 설정하고, 예산 초과 시 불필요한 상위 Mip을 해제한다 | P0 |
| OPT-18 | Mip-Mapping을 지원하여 텍스처 생성 시 Mip chain을 자동 생성한다 | P0 |
| OPT-19 | D3D12 텍스처 리소스 생성 시 MipLevels를 적절히 설정한다 (전체 Mip chain 또는 지정 레벨) | P0 |
| OPT-20 | Sampler에서 Mip 필터링(Trilinear 또는 Anisotropic)을 활성화한다 | P0 |

#### Light Culling (광원 컬링)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| LC-01 | 카메라 Frustum 밖에 위치한 Point/Spot 광원을 라이팅 계산에서 제외한다 | P0 |
| LC-02 | Point/Spot 광원의 유효 범위(감쇠로 기여도가 임계값 이하가 되는 거리)를 BoundingSphere로 계산한다 | P0 |
| LC-03 | 광원의 BoundingSphere와 카메라 Frustum의 교차 검사로 가시 여부를 판정한다 | P0 |
| LC-04 | 광원~카메라 거리 및 강도 기반으로 화면 기여도를 추정하여, 임계값(0.01) 이하인 광원을 제외한다 | P0 |
| LC-05 | Directional Light는 무한 거리이므로 항상 활성 상태를 유지한다 (컬링 대상 아님) | P0 |
| LC-06 | 컬링된 광원 수를 DebugHUD에 표시한다 | P1 |

#### Instanced Rendering
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| OPT-21 | Instanced Rendering을 구현하여 동일 메시를 여러 위치에 단일 드로우콜로 렌더링한다 | P0 |
| OPT-22 | 인스턴스 데이터(World Matrix 등)를 Instance Buffer로 GPU에 전달한다 | P0 |
| OPT-23 | `DrawIndexedInstanced`를 사용하여 인스턴스 수만큼 한 번에 렌더링한다 | P0 |
| OPT-24 | 씬 내 동일 Mesh+Material 조합을 자동으로 인스턴싱 후보로 수집한다 | P1 |

#### 멀티스레드 리소스 로딩
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| OPT-25 | 씬 로딩 시 Mesh/Material/Texture 파싱을 멀티스레드로 병렬 수행한다 | P0 |
| OPT-26 | 스레드 풀(thread pool)을 구현하여 워커 스레드 수를 CPU 코어 수에 맞게 관리한다 | P0 |
| OPT-27 | 텍스처 이미지 디코딩(CPU 작업)을 복수 워커 스레드에 분배하여 병렬 디코딩한다 | P0 |
| OPT-28 | GPU 업로드(Upload Buffer → Default Heap 복사)는 메인 스레드 또는 Copy Queue에서 수행한다 | P0 |
| OPT-29 | D3D12 Copy Queue를 활용하여 Graphics Queue와 병렬로 리소스 업로드를 수행한다 | P1 |
| OPT-30 | 멀티스레드 로딩 중에도 렌더 루프가 중단되지 않는다 (폴백 리소스로 렌더링) | P0 |

#### GPU 메모리 관리 (Constant Buffer 풀링)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| OPT-31 | 개별 오브젝트마다 Constant Buffer를 생성하지 않고, 하나의 큰 Upload Heap을 풀링(pooling)하여 CB 슬롯을 동적 할당한다 | P0 |
| OPT-32 | 프레임마다 풀의 오프셋을 리셋하여 링 버퍼 방식으로 CB 슬롯을 재사용한다 | P0 |
| OPT-33 | CB 풀 크기는 VRAM 가용량을 고려하여 설정하며, 대형 씬에서도 충분한 슬롯을 확보한다 | P0 |
| OPT-34 | VRAM 사용량을 `IDXGIAdapter3::QueryVideoMemoryInfo`로 모니터링한다 | P0 |
| OPT-35 | VRAM 사용량이 OS가 보고하는 현재 가용 전용 비디오 메모리(Dedicated Video Memory)의 80%를 초과하면 우선순위가 낮은 오브젝트의 CB 갱신 빈도를 낮춘다 (N프레임마다 1회). 기준값은 `IDXGIAdapter3::QueryVideoMemoryInfo(DXGI_MEMORY_SEGMENT_GROUP_LOCAL)`의 `Budget` 필드의 80%로 한다 | P0 |
| OPT-36 | CB 갱신 우선순위: 카메라 가까운 오브젝트 > 먼 오브젝트, 화면 차지 비율 큰 오브젝트 > 작은 오브젝트 | P0 |

#### 재질 공유 상수 버퍼 (Shared Material CB)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| OPT-37 | PBR 재질 파라미터(metallic, roughness, baseColorFactor, emissiveFactor 등)를 재질별 공유 Constant Buffer로 묶어 전송한다 | P0 |
| OPT-38 | 동일 Material을 사용하는 모든 오브젝트는 같은 Material CB를 참조하여 중복 전송을 제거한다 | P0 |
| OPT-39 | Material CB는 재질 파라미터가 변경될 때만 갱신한다 (dirty flag 기반) | P0 |
| OPT-40 | Per-Object CB(World Matrix 등)와 Per-Material CB를 분리하여 각각 독립적인 갱신 주기로 관리한다 | P0 |

#### Opaque 패스 Front-to-Back 정렬
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| OPT-41 | Opaque 패스에서 오브젝트를 카메라 거리 기준 앞→뒤(front-to-back) 순으로 정렬하여 Early-Z rejection을 극대화한다 | P0 |

#### Dirty Flag 기반 CB 갱신 스킵
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| OPT-42 | 오브젝트의 Transform이 변경되지 않았으면 해당 프레임의 Per-Object CB 갱신을 스킵한다 | P0 |
| OPT-43 | 광원 데이터가 변경되지 않았으면 Light CB 갱신을 스킵한다 | P0 |

#### DebugHUD 최적화 통계
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| OPT-44 | DebugHUD에 최적화 통계를 표시한다: frustum culled 수, occlusion culled 수, 드로우콜 수, 인스턴스 수, VRAM 사용량(Used/Budget) | P1 |
| OPT-45 | DebugHUD에 현재 스트리밍 중인 리소스 개수와 남은 스트리밍 대역폭(MB/s 또는 큐 잔량)을 표시한다 | P1 |

#### CB 슬롯 하드웨어 정렬
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| OPT-46 | Constant Buffer 슬롯 할당 시 D3D12 하드웨어 정렬 요구사항(256바이트 경계)을 준수한다. CBPool의 `Allocate()` 호출마다 반환 오프셋이 256의 배수임을 보장한다 | P0 |
| OPT-47 | CB 데이터 크기가 256바이트 미만이더라도 다음 슬롯까지 256바이트 단위로 패딩한다 (`alignedSize = (size + 255) & ~255`) | P0 |

### 3.21 렌더링 모드 선택 (Render Mode)
| ID | 요구사항 | 우선순위 |
|----|----------|----------|
| RM-01 | "Render" 메뉴에서 렌더링 모드를 선택하여 단계별로 렌더링 복잡도를 전환할 수 있다 | P0 |
| RM-02 | **Wireframe** 모드: 메시의 엣지만 와이어프레임으로 표시한다 (래스터라이저 FillMode = Wireframe, 라이팅/텍스처 미적용) | P0 |
| RM-03 | **Solid (No Texture)** 모드: 텍스처 없이 Material의 factor 값(baseColorFactor 등)과 라이팅만 적용하여 렌더링한다 | P0 |
| RM-04 | **Base Color Only** 모드: Base Color(Albedo) 텍스처만 적용하고, metallic/roughness/normal 등 기타 PBR 텍스처는 기본값(factor)으로 렌더링한다 | P0 |
| RM-05 | **Full PBR** 모드: 모든 PBR 텍스처(albedo, normal, metallic, roughness, emissive, occlusion)를 적용한 완전한 PBR 렌더링을 수행한다 (그림자 미적용) | P0 |
| RM-06 | **Full PBR + Shadows** 모드: Full PBR에 Shadow Mapping(PCF)까지 적용한 최종 렌더링을 수행한다 (기본 모드) | P0 |
| RM-07 | 현재 선택된 렌더링 모드에 체크 표시를 한다 (CheckMenuRadioItem) | P0 |
| RM-08 | 렌더링 모드 전환 시 PSO(Pipeline State Object) 및 셰이더 바인딩을 즉시 교체한다 | P0 |
| RM-09 | DebugHUD에 현재 렌더링 모드 이름을 표시한다 | P1 |
| RM-10 | 기본 렌더링 모드는 "Full PBR + Shadows"이다 | P0 |

## 4. 비기능 요구사항

| ID | 요구사항 |
|----|----------|
| NF-01 | C++17 이상 표준 사용 |
| NF-02 | Visual Studio 2022 Solution (v143 툴셋) |
| NF-03 | 외부 라이브러리 의존성 최소화 (Win32 API + DirectX 12 + 표준 라이브러리 + Assimp 중심) |
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
| 3D 에셋 로더 | Assimp (vcpkg) |
| 렌더링 백엔드 | DirectX 12 |
| 링크 라이브러리 | d3d12.lib, dxgi.lib, d3dcompiler.lib, dxguid.lib, assimp-vc143-mt.lib |

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
| glTF 2.0 | GL Transmission Format. Khronos 표준 3D 에셋 포맷. JSON(.gltf) + 바이너리(.bin) 또는 단일 바이너리(.glb) |
| PBR | Physically Based Rendering. 물리 기반 렌더링. metallic-roughness 워크플로우 사용 |
| Assimp | Open Asset Import Library. 다양한 3D 포맷을 로딩하는 오픈소스 라이브러리 |
| SRV | Shader Resource View. D3D12에서 텍스처를 셰이더에 바인딩하기 위한 디스크립터 |
| Normal Map | 표면의 법선 벡터를 텍스처로 저장하여 세밀한 조명 효과를 표현하는 기법 |
| Alpha Mask | 텍스처의 알파 값이 임계값(alphaCutoff) 이하이면 픽셀을 버리는 렌더링 모드 |
| Cook-Torrance BRDF | 물리 기반 Specular 반사 모델. D(법선 분포) × G(기하 감쇠) × F(프레넬) / (4·NdotL·NdotV) |
| GGX (Trowbridge-Reitz) | Cook-Torrance의 법선 분포 함수(NDF). roughness 기반 미세면 분포를 모델링 |
| Fresnel-Schlick | 프레넬 효과 근사식. F0 + (1-F0)·(1-cosθ)^5. 시야각에 따른 반사율 변화 |
| Smith-Schlick GGX | 기하 감쇠 함수. 미세면 간 상호 차폐(masking/shadowing)를 모델링 |
| Directional Light | 무한 거리에서 평행하게 비추는 광원. 태양광 모사. 위치 없이 방향만 존재 |
| Spot Light | 원뿔 형태로 비추는 광원. 위치, 방향, 내부/외부 원뿔각, 감쇠를 가짐 |
| Shadow Map | 광원 시점에서 장면의 깊이를 렌더링한 텍스처. 그림자 판정에 사용 |
| Shadow Mapping | Shadow Map을 이용하여 픽셀이 그림자 안에 있는지 판정하는 기법 |
| PCF (Percentage Closer Filtering) | Shadow Map의 주변 텍셀을 다중 샘플링하여 그림자 경계를 부드럽게 만드는 필터링 기법 |
| Depth Bias | Shadow Map 렌더링 시 깊이 값에 미세 오프셋을 추가하여 shadow acne(자기 그림자 노이즈)를 방지하는 기법 |
| Frustum Culling | 카메라 시야(View Frustum) 밖의 오브젝트를 렌더링에서 제외하는 최적화 기법 |
| AABB | Axis-Aligned Bounding Box. 축 정렬 바운딩 박스. 오브젝트를 감싸는 최소 직육면체 |
| LOD (Level of Detail) | 카메라 거리에 따라 메시의 디테일 수준을 전환하여 렌더링 부하를 줄이는 기법 |
| Texture Streaming | 필요한 텍스처 Mip 레벨만 GPU 메모리에 동적으로 로드/해제하는 기법 |
| Mip-Mapping | 텍스처의 축소 버전(Mip chain)을 미리 생성하여 원거리 렌더링 품질과 성능을 개선하는 기법 |
| Instanced Rendering | 동일 메시를 여러 위치에 한 번의 드로우콜로 렌더링하는 기법. DrawIndexedInstanced 사용 |
| FBX | Autodesk 독점 3D 교환 포맷. 메시, 머티리얼, 애니메이션, 본 등을 포함. Assimp으로 로딩 |
| Occlusion Culling | 다른 오브젝트에 완전히 가려진 오브젝트를 렌더링에서 제외하는 최적화 기법 |
| Hi-Z (Hierarchical-Z) | Depth buffer의 축소 Mip chain을 이용한 빠른 가시성 판정 기법 |
| Upload Heap Pooling | 개별 CB를 따로 할당하지 않고 하나의 큰 Upload Heap에서 슬롯을 동적 할당하는 메모리 관리 기법 |
| Shared Material CB | 동일 재질의 PBR 파라미터를 하나의 CB로 공유하여 GPU 전송 횟수를 줄이는 기법 |
| Dirty Flag | 데이터가 변경되었는지 추적하는 플래그. 변경 시에만 GPU로 재전송하여 불필요한 갱신을 방지 |
| Copy Queue | D3D12의 별도 커맨드 큐. Graphics Queue와 병렬로 리소스 업로드를 수행 |
| Early-Z Rejection | GPU가 픽셀 셰이더 실행 전에 depth test로 가려진 픽셀을 미리 제거하는 하드웨어 최적화 |
| Thread Pool | 미리 생성된 워커 스레드 집합. 작업을 큐에 넣으면 유휴 스레드가 처리하는 병렬 실행 패턴 |
| Gamma Correction | 리니어 공간에서 계산한 색상을 sRGB 감마 곡선(pow 1/2.2)으로 변환하여 모니터에 올바른 밝기로 출력하는 과정 |
| CB Alignment (256-byte) | D3D12 Constant Buffer는 256바이트 경계 정렬이 필수. 할당 오프셋과 크기 모두 256의 배수여야 한다 |
| Light Culling | 카메라 시야 밖이거나 기여도가 미미한 광원을 라이팅 계산에서 제외하는 최적화 기법 |
| Auto-LOD (Mesh Simplification) | 씬 파일에 LOD 메시가 없을 때 Edge Collapse(QEM) 알고리즘으로 간략화된 LOD 메시를 자동 생성하는 기법 |
| QEM (Quadric Error Metrics) | Edge Collapse 시 어떤 간선을 축소할지 결정하는 품질 메트릭. 기하학적 오차를 최소화하는 순서로 간선을 제거 |
| Render Mode | 렌더링 복잡도를 단계별로 전환하는 기능. Wireframe → Solid → Base Color → Full PBR → Full PBR + Shadows 순으로 복잡도 증가 |
