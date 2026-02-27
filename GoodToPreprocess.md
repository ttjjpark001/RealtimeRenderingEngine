# GoodToPreprocess.md — 씬 데이터 로딩 후 내부 가공 자료 목록

> 씬 파일(glTF/GLB/FBX)에서 읽어들인 원본 데이터를 앱 내부에서 가공하여
> 새로운 파생 자료로 구성하는 항목들의 목록이다.
> 각 항목은 **원본 데이터 → 가공 처리 → 파생 자료** 흐름으로 정리한다.

---

## 1. Mesh 관련

### 1-1. Vertex 포맷 변환
- **원본**: Assimp의 aiMesh (position, normal, texcoord, tangent, index)
- **가공**: Assimp 타입 → 엔진 Vertex 구조체로 변환 (XMFLOAT3/2/4 정렬, 연속 배치)
- **파생 자료**: `std::vector<Vertex>` + `std::vector<uint32>` (엔진 전용 Mesh 객체)
- **위치**: `src/Asset/SceneLoader.cpp`
- **Phase**: 12 (SceneLoader 기본)

### 1-2. Tangent 벡터 보정/생성
- **원본**: aiMesh의 탄젠트 데이터 (없을 수도 있음)
- **가공**: 탄젠트가 없으면 UV + Normal로부터 계산, Gram-Schmidt 재직교화
- **파생 자료**: Vertex의 `XMFLOAT4 tangent` (w = handedness)
- **위치**: `src/Asset/SceneLoader.cpp`, PBR.hlsl VS
- **Phase**: 13 (Vertex 포맷 확장)

### 1-3. GPU 버퍼 (VB/IB) 생성
- **원본**: CPU 메모리의 Vertex/Index 배열
- **가공**: Upload Heap → Default Heap 복사, ID3D12Resource 생성
- **파생 자료**: GPU 상주 Vertex Buffer + Index Buffer (`MeshBuffers` in Renderer 캐시)
- **위치**: `src/Renderer/Renderer.cpp` (UploadMesh)
- **Phase**: 12

### 1-4. 메시별 AABB (Bounding Box) 계산
- **원본**: Mesh의 전체 정점 위치 + SceneNode의 World Transform
- **가공**: 모든 정점의 Min/Max 좌표를 구해 축 정렬 바운딩 박스 생성
- **파생 자료**: `DirectX::BoundingBox` (center + extents)
- **위치**: `src/Scene/SceneNode.h/.cpp`, `src/Renderer/FrustumCuller.h/.cpp`
- **Phase**: 21 (Frustum Culling)

### 1-5. 자동 LOD 메시 생성 (Auto-LOD)
- **원본**: 원본 메시 (LOD 0, 씬 파일에 LOD 데이터가 없는 경우)
- **가공**: Edge Collapse + QEM(Quadric Error Metrics) 기반 메시 심플리피케이션
  - LOD 1: 원본 삼각형의 ~50%
  - LOD 2: 원본 삼각형의 ~25%
  - 정점 위치, 법선, UV 보존
- **파생 자료**: `LODMesh` 구조체 (Mesh 포인터 배열 + 전환 거리 배열)
- **위치**: `src/Renderer/LODSelector.h/.cpp`
- **Phase**: 21 (Culling + LOD)
- **비고**: 백그라운드 스레드에서 비동기 수행, 완료 전까지 LOD 0으로 렌더링

### 1-6. Face Coloring (Phase 01 전용)
- **원본**: MeshFactory가 생성한 프로시저럴 메시의 면 인접 정보
- **가공**: Greedy 그래프 컬러링 알고리즘으로 인접면에 서로 다른 색상 배정
- **파생 자료**: 면별 색상이 적용된 Vertex 배열 (flat shading용 정점 중복)
- **위치**: `src/Renderer/FaceColorPalette.h`, `src/Renderer/MeshFactory.cpp`
- **Phase**: 01 (Phase 4)

---

## 2. Texture 관련

### 2-1. 이미지 디코딩
- **원본**: PNG/JPEG 파일 (glTF 내장 또는 외부 참조)
- **가공**: stb_image 또는 WIC로 압축 해제 → RGBA 8bit 픽셀 배열
- **파생 자료**: CPU 메모리의 디코딩된 이미지 버퍼 (`uint8*`)
- **위치**: `src/Asset/Texture.cpp`
- **Phase**: 14 (Texture 시스템)
- **비고**: 워커 스레드에서 비동기 수행

### 2-2. Mip Chain 생성
- **원본**: 디코딩된 원본 해상도 이미지
- **가공**: CPU box filter (또는 GPU compute)로 단계별 1/2 축소 반복
  - Mip 레벨 수 = `floor(log2(max(width, height))) + 1`
- **파생 자료**: 전체 Mip chain 데이터 (각 레벨별 픽셀 배열)
- **위치**: `src/Asset/Texture.cpp`, `src/Asset/TextureStreamer.cpp`
- **Phase**: 22 (Texture Streaming + Mip-Mapping)

### 2-3. GPU 텍스처 리소스 + SRV 생성
- **원본**: 디코딩된 이미지 버퍼 (+ Mip chain)
- **가공**:
  - ID3D12Resource (TEXTURE2D) 생성 (Default Heap)
  - Upload Buffer를 통해 CPU → GPU 복사
  - 리소스 상태 전이: COPY_DEST → PIXEL_SHADER_RESOURCE
  - SRV 디스크립터 생성
- **파생 자료**: GPU 상주 텍스처 + SRV 핸들
- **위치**: `src/Asset/Texture.cpp`
- **Phase**: 14

### 2-4. sRGB / Linear 포맷 분류
- **원본**: 텍스처의 용도 (Material 내 역할)
- **가공**: 용도에 따라 GPU 텍스처 포맷 결정
  - baseColor(Albedo) → `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB` (자동 sRGB→Linear 변환)
  - Normal, MetallicRoughness, Occlusion → `DXGI_FORMAT_R8G8B8A8_UNORM` (Linear)
- **파생 자료**: 올바른 색 공간의 GPU 텍스처 리소스
- **위치**: `src/Asset/Texture.cpp`
- **Phase**: 14

### 2-5. 폴백 텍스처 생성
- **원본**: 없음 (앱 초기화 시 프로그래밍으로 생성)
- **가공**: 1×1 white 픽셀로 GPU 텍스처 생성
- **파생 자료**: 플레이스홀더 Texture 객체 (비동기 로딩 완료 전 바인딩용)
- **위치**: `src/Asset/TextureCache.cpp`
- **Phase**: 14

### 2-6. 텍스처 캐시 (중복 방지)
- **원본**: 동일 파일 경로를 참조하는 복수 Material
- **가공**: 파일 경로 기반 `unordered_map` 조회 → 이미 로딩된 텍스처 재사용
- **파생 자료**: 공유 Texture 포인터 (1회 로딩, 다수 Material이 참조)
- **위치**: `src/Asset/TextureCache.cpp`
- **Phase**: 14

---

## 3. Scene 구조 관련

### 3-1. 노드 계층 변환
- **원본**: Assimp의 aiNode 트리 (이름, 로컬 트랜스폼, 부모-자식 관계)
- **가공**: aiNode 재귀 순회 → 엔진의 SceneNode 트리로 변환
- **파생 자료**: `SceneGraph` (루트 노드 아래 SceneNode 트리)
- **위치**: `src/Asset/SceneLoader.cpp`
- **Phase**: 12

### 3-2. 씬 바운딩 박스 계산
- **원본**: 씬 내 모든 Mesh의 정점 위치
- **가공**: 전체 씬에 대한 Min/Max AABB 계산
- **파생 자료**: 씬 전체 AABB (카메라 Fit to Scene, 이동 속도 자동 조절에 사용)
- **위치**: `src/Asset/SceneLoader.cpp`
- **Phase**: 12

### 3-3. 카메라 초기 배치 추출
- **원본**: aiCamera 노드 (있는 경우) 또는 씬 바운딩 박스 (없는 경우)
- **가공**:
  - 카메라 노드 있음 → 위치/방향/FOV 추출
  - 없음 → 바운딩 박스 대각선 기반 거리 산출 (Fit to Scene)
- **파생 자료**: Camera 초기 상태 (position, yaw, pitch, FOV)
- **위치**: `src/Asset/SceneLoader.cpp`, `src/Scene/Camera.cpp`
- **Phase**: 19

---

## 4. Material 관련

### 4-1. PBR Material 파라미터 추출
- **원본**: aiMaterial (Assimp의 재질 데이터)
- **가공**: glTF metallic-roughness 워크플로우 파라미터 추출
- **파생 자료**: `Material` 객체
  - Factor 값: baseColorFactor, metallicFactor, roughnessFactor, emissiveFactor
  - 텍스처 포인터: baseColor, metallicRoughness, normal, emissive, occlusion
  - 렌더 상태: AlphaMode (Opaque/Mask/Blend), alphaCutoff, doubleSided
- **위치**: `src/Asset/SceneLoader.cpp`, `src/Asset/Material.h`
- **Phase**: 13

### 4-2. Per-Material Constant Buffer 구성
- **원본**: Material 객체의 PBR 파라미터
- **가공**: PerMaterialCB 구조체로 패킹 (GPU 정렬)
  - 텍스처 존재 여부 플래그: hasAlbedoMap, hasNormalMap, hasMetallicRoughnessMap 등
  - Dirty Flag 기반: 파라미터 변경 시에만 갱신
- **파생 자료**: GPU Constant Buffer 슬롯 (register b2)
- **위치**: `src/RHI/D3D12/D3D12Context.cpp`
- **Phase**: 16

---

## 5. Light 관련

### 5-1. 광원 데이터 추출
- **원본**: aiLight 노드 (glTF/FBX 내 광원 정보, 있는 경우)
- **가공**: 타입(Directional/Point/Spot), 위치, 방향, 색상, 강도, 감쇠 계수, 원뿔각 추출
- **파생 자료**: `Light` 구조체 → `LightManager`에 등록
- **위치**: `src/Asset/SceneLoader.cpp`, `src/Lighting/LightManager.cpp`
- **Phase**: 17

### 5-2. 광원 유효 범위 (BoundingSphere) 계산
- **원본**: Point/Spot 광원의 감쇠 계수 (Kc, Kl, Kq) + 강도
- **가공**: 감쇠로 기여도가 임계값(0.01) 이하가 되는 거리 계산 → BoundingSphere 생성
- **파생 자료**: `DirectX::BoundingSphere` (position + radius)
- **위치**: `src/Renderer/LightCuller.cpp`
- **Phase**: 21

### 5-3. Light-View-Projection 행렬 계산
- **원본**: 그림자 생성 광원의 위치, 방향, 타입
- **가공**:
  - Directional → `LookAtLH × OrthographicLH`
  - Spot → `LookAtLH × PerspectiveFovLH`
  - `XMMatrixTranspose()` 적용 (GPU 전달용)
- **파생 자료**: `XMFLOAT4X4 lightViewProj` (ShadowConstants에 저장)
- **위치**: `src/Renderer/Renderer.cpp`
- **Phase**: 18

### 5-4. Shadow Map 생성
- **원본**: 씬 전체 지오메트리 + 광원 LVP 행렬
- **가공**: 광원 시점에서 Depth-only 렌더 패스 수행
  - Shadow Depth PSO (depth bias 적용, color write 없음)
  - D32_FLOAT 포맷 텍스처에 깊이 기록
- **파생 자료**: Shadow Map 텍스처 (최대 8장, 1024×1024, `ID3D12Resource`)
- **위치**: `src/RHI/D3D12/D3D12Context.cpp`
- **Phase**: 18
- **비고**: 매 프레임 재생성 (동적 그림자)

---

## 6. 렌더링 최적화 파생 자료

### 6-1. View Frustum 추출
- **원본**: 카메라 View-Projection 행렬
- **가공**: VP 행렬로부터 6개 절두체 평면(Left, Right, Top, Bottom, Near, Far) 추출
- **파생 자료**: `DirectX::BoundingFrustum`
- **위치**: `src/Renderer/FrustumCuller.cpp`
- **Phase**: 21

### 6-2. Hi-Z (Hierarchical-Z) 버퍼
- **원본**: 이전 프레임의 Depth Buffer
- **가공**: 2×2 max reduction으로 Mip chain 생성 (depth 보수적 축소)
- **파생 자료**: Hi-Z 텍스처 피라미드 (Occlusion Culling 판정용)
- **위치**: `src/Renderer/OcclusionCuller.cpp`
- **Phase**: 21

### 6-3. 인스턴스 그룹핑 + Instance Buffer
- **원본**: SceneGraph 노드들 (동일 Mesh+Material 조합)
- **가공**:
  - Mesh+Material 기준 그룹핑
  - 각 인스턴스의 World Matrix를 InstanceData 배열로 수집
  - GPU Instance Buffer (per-instance VB slot 1) 생성
- **파생 자료**: `InstanceData[]` (XMFLOAT4X4 world, 전치 적용) + Instance Buffer
- **위치**: `src/Renderer/InstanceBatcher.cpp`
- **Phase**: 23

### 6-4. Constant Buffer Pool 슬롯 할당
- **원본**: Per-Object/Per-Material/Per-Light CB 데이터
- **가공**:
  - 큰 Upload Heap에서 256바이트 정렬 슬롯 동적 할당
  - 링 버퍼 방식 (프레임 0/1 영역 번갈아 사용)
- **파생 자료**: `CBAllocation` (GPU 가상 주소 + 오프셋 + 크기)
- **위치**: `src/RHI/D3D12/D3D12CBPool.cpp`
- **Phase**: 24

---

## 요약 테이블

| # | 원본 데이터 | 가공 처리 | 파생 자료 | Phase |
|---|-----------|----------|----------|-------|
| 1-1 | aiMesh | 타입 변환 | Engine Vertex/Index 배열 | 12 |
| 1-2 | Mesh 정점/UV/Normal | Tangent 계산/보정 | Tangent 벡터 (w=handedness) | 13 |
| 1-3 | CPU Vertex/Index | GPU 업로드 | VB/IB (ID3D12Resource) | 12 |
| 1-4 | Mesh 정점 + World Transform | Min/Max 계산 | AABB (BoundingBox) | 21 |
| 1-5 | 원본 메시 | Edge Collapse (QEM) | LOD 1(50%), LOD 2(25%) 메시 | 21 |
| 1-6 | 면 인접 정보 | 그래프 컬러링 | 면별 색상 Vertex 배열 | 01 |
| 2-1 | PNG/JPEG 파일 | 이미지 디코딩 | RGBA 픽셀 버퍼 | 14 |
| 2-2 | 원본 이미지 | Box filter 축소 | Mip chain (전체 레벨) | 22 |
| 2-3 | 디코딩 이미지 | GPU 리소스 생성 | TEXTURE2D + SRV | 14 |
| 2-4 | 텍스처 용도 | 포맷 분류 | sRGB/Linear GPU 텍스처 | 14 |
| 2-5 | (없음) | 1×1 white 생성 | 폴백 텍스처 | 14 |
| 2-6 | 파일 경로 | 해시맵 조회 | 공유 Texture 포인터 | 14 |
| 3-1 | aiNode 트리 | 재귀 변환 | SceneNode 트리 (SceneGraph) | 12 |
| 3-2 | 전체 정점 | Min/Max 계산 | 씬 AABB | 12 |
| 3-3 | aiCamera / 씬 AABB | 위치/거리 산출 | Camera 초기 상태 | 19 |
| 4-1 | aiMaterial | PBR 파라미터 추출 | Material 객체 | 13 |
| 4-2 | Material 파라미터 | CB 구조체 패킹 | PerMaterialCB (GPU) | 16 |
| 5-1 | aiLight | 타입/속성 추출 | Light 구조체 | 17 |
| 5-2 | 감쇠 계수 + 강도 | 유효 거리 계산 | BoundingSphere | 21 |
| 5-3 | 광원 위치/방향/타입 | LVP 행렬 계산 | lightViewProj (전치) | 18 |
| 5-4 | 씬 지오메트리 + LVP | Depth-only 렌더 | Shadow Map 텍스처 | 18 |
| 6-1 | VP 행렬 | 평면 추출 | BoundingFrustum | 21 |
| 6-2 | 이전 Depth Buffer | Max reduction | Hi-Z 피라미드 | 21 |
| 6-3 | 동일 Mesh+Material 노드 | 그룹핑 + 행렬 수집 | Instance Buffer | 23 |
| 6-4 | CB 데이터 | 256B 정렬 할당 | CBPool 슬롯 | 24 |

---

## 전처리(Preprocess) vs 매 프레임(Per-Frame) 분류

### 씬 로딩 시 1회 전처리 (Load-time)
- 1-1 Vertex 포맷 변환
- 1-2 Tangent 생성/보정
- 1-3 GPU VB/IB 생성
- 1-4 메시별 AABB 계산 (정적 오브젝트)
- 1-5 자동 LOD 메시 생성 (비동기)
- 2-1 이미지 디코딩 (비동기)
- 2-2 Mip chain 생성
- 2-3 GPU 텍스처 + SRV 생성
- 2-4 sRGB/Linear 포맷 분류
- 2-5 폴백 텍스처 생성 (앱 초기화)
- 2-6 텍스처 캐시 등록
- 3-1 노드 계층 변환
- 3-2 씬 바운딩 박스
- 3-3 카메라 초기 배치
- 4-1 Material 파라미터 추출
- 5-1 광원 데이터 추출
- 5-2 광원 유효 범위 계산

### 매 프레임 갱신 (Per-Frame, 변경 시)
- 4-2 PerMaterialCB (Dirty Flag 기반, 변경 시만)
- 5-3 LVP 행렬 (광원/카메라 이동 시)
- 5-4 Shadow Map (동적 그림자)
- 6-1 View Frustum 추출 (카메라 이동 시)
- 6-2 Hi-Z 버퍼 (매 프레임)
- 6-3 Instance Buffer (씬 변경 시)
- 6-4 CB Pool 슬롯 할당 (매 프레임)
