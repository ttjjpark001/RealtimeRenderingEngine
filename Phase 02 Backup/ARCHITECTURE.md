# ARCHITECTURE.md — Realtime Rendering Engine

> Phase 30 완료 시점 (2026-03) 기준 아키텍처 문서
> 구현 범위: Phase 01 ~ Phase 30 (Phase 02 완전 통합)

---

## 1. 전체 디렉토리 구조

```
RealtimeRenderingEngine/
├── src/
│   ├── Asset/               — 씬 로딩 & 리소스 관리
│   │   ├── SceneLoader.h/.cpp       — Assimp glTF/GLB/FBX → SceneNode 트리 변환
│   │   ├── Material.h/.cpp          — PBR Material (baseColor/metallic/roughness/normal/emissive/occlusion)
│   │   ├── Texture.h/.cpp           — D3D12 텍스처 리소스 (2D, Mip chain)
│   │   ├── TextureCache.h/.cpp      — 경로 기반 중복 방지 캐시 + 폴백 텍스처
│   │   └── TextureStreamer.h/.cpp   — VRAM 모니터링 + Mip 우선순위 추적
│   │
│   ├── Core/                — 엔진 루프 & 공용 타입
│   │   ├── Engine.h/.cpp            — 메인 루프, 씬 로딩, 카메라/조명 설정, 콜백 연결
│   │   ├── Types.h                  — DirectXMath 별칭 (Vector3, Vector4, Matrix4x4)
│   │   └── ThreadPool.h/.cpp        — CPU 코어 기반 워커 스레드 풀 (비동기 텍스처 디코딩)
│   │
│   ├── Lighting/            — 조명 시스템
│   │   ├── Light.h                  — LightType enum + Light 구조체
│   │   └── LightManager.h/.cpp      — 최대 16개 광원 목록 관리
│   │
│   ├── Math/                — 수학 유틸리티
│   │   └── MathUtil.h               — DirectXMath 헬퍼 함수
│   │
│   ├── Platform/Win32/      — Win32 플랫폼 레이어
│   │   ├── Win32Window.h/.cpp       — 창 생성, WndProc, 마우스/키보드/드래그앤드롭 입력
│   │   └── Win32Menu.h/.cpp         — 메뉴 ID 정의, 콜백 디스패치
│   │
│   ├── RHI/                 — Rendering Hardware Interface (추상화 계층)
│   │   ├── IRHIDevice.h             — 순수 가상 device 인터페이스
│   │   ├── IRHIBuffer.h             — 순수 가상 buffer 인터페이스
│   │   ├── IRHIContext.h            — 순수 가상 context 인터페이스
│   │   └── D3D12/                  — DirectX 12 백엔드 구현
│   │       ├── D3D12Device.h/.cpp   — ID3D12Device, SwapChain, DXGI 어댑터
│   │       ├── D3D12Context.h/.cpp  — 커맨드 리스트, 드로우콜, 프레임 관리
│   │       ├── D3D12Buffer.h/.cpp   — Vertex/Index Buffer (Upload + Default heap)
│   │       ├── D3D12CBPool.h/.cpp   — Constant Buffer 링 버퍼 풀 (256B 정렬)
│   │       ├── D3D12DescriptorHeap.h/.cpp — CBV/SRV/UAV + DSV 힙 관리
│   │       └── D3D12PipelineState.h/.cpp  — Root Signature + 전체 PSO 생성
│   │
│   ├── Renderer/            — 렌더링 파이프라인 오케스트레이션
│   │   ├── Renderer.h/.cpp          — RenderScene(): Frustum/Light Culling → Shadow → Opaque → AlphaBlend
│   │   ├── DebugHUD.h/.cpp          — D2D1+DirectWrite HUD 오버레이
│   │   ├── FrustumCuller.h/.cpp     — BoundingFrustum vs AABB 교차 검사
│   │   ├── OcclusionCuller.h/.cpp   — P0 스텁 (항상 visible 반환, Hi-Z는 Phase 33)
│   │   ├── LODSelector.h/.cpp       — 거리 기반 LOD 단계 선택 + Auto-LOD QEM 생성
│   │   ├── LightCuller.h/.cpp       — Frustum 밖/저기여 Point/Spot 광원 제거
│   │   ├── InstanceBatcher.h/.cpp   — 동일 Mesh+Material → DrawIndexedInstanced 그룹핑
│   │   ├── Mesh.h/.cpp              — Vertex 배열 + Index 배열 + AABB
│   │   ├── MeshFactory.h/.cpp       — 기하 프리미티브 생성
│   │   ├── Vertex.h                 — Vertex struct + D3D12 Input Layout 정의
│   │   └── FaceColorPalette.h/.cpp  — 8색 팔레트 그래프 컬러링
│   │
│   ├── Scene/               — Scene Graph & Camera
│   │   ├── SceneGraph.h/.cpp        — 트리 루트, DFS 순회, 총 폴리곤 수
│   │   ├── SceneNode.h/.cpp         — Transform + Mesh* + Material* + WorldAABB 캐싱
│   │   ├── Transform.h/.cpp         — 위치/회전/스케일 → TRS 행렬
│   │   └── Camera.h/.cpp            — Perspective/Orthographic, WASD+마우스 네비게이션
│   │
│   ├── Shaders/             — HLSL 소스 파일
│   │   ├── BasicColor.hlsl          — 단순 vertex-color + 단일 광원 (Phase 01 호환)
│   │   ├── PBR.hlsl                 — Cook-Torrance BRDF, 멀티 광원, Shadow PCF 3×3
│   │   ├── ShadowDepth.hlsl         — Shadow Depth pass VS (PS 없음, depth-only)
│   │   └── Wireframe.hlsl           — 단색 Wireframe VS+PS
│   │
│   └── Tools/               — 오프라인 도구 (Phase 32+)
│       └── (RRScenePreprocessor 소스: Phase 32에서 추가)
│
├── tests/
│   ├── unit/                — DirectXMath, SceneGraph, FaceColoring, Camera 단위 테스트
│   └── smoke/               — 엔진 초기화, RHI 백엔드 스모크 테스트 (WARP)
│
├── assets/test-models/      — Khronos glTF-Sample-Assets + 대형 씬
├── bin/
│   ├── Debug/               — 디버그 빌드 출력 (RREngine.exe + Shaders/*.cso)
│   └── Release/             — 릴리즈 빌드 출력
├── PRD.md                   — 제품 요구사항
├── PLAN.md                  — Phase별 구현 계획
├── CLAUDE.md                — 코딩 가이드 (이 문서와 함께 읽을 것)
└── ARCHITECTURE.md          — 이 문서
```

---

## 2. 모듈 의존성 그래프

```
[Platform/Win32]          [Core/ThreadPool]
      │                          │
      ↓                          ↓
  [Engine] ←──────────── [Asset: SceneLoader, TextureCache, TextureStreamer]
      │                          │
      ├──→ [Scene: SceneGraph, SceneNode, Camera]
      │         │
      │         ↓
      ├──→ [Lighting: LightManager]
      │
      ├──→ [Renderer]
      │        ├── FrustumCuller
      │        ├── OcclusionCuller  (P0 stub)
      │        ├── LODSelector
      │        ├── LightCuller
      │        └── InstanceBatcher
      │
      └──→ [RHI/D3D12]
               ├── D3D12Device      ← DXGI + ID3D12Device
               ├── D3D12Context     ← Command List, Draw Calls, Shadow Maps
               ├── D3D12PipelineState ← Root Signature + PSOs
               ├── D3D12CBPool      ← Constant Buffer 링 버퍼
               └── D3D12DescriptorHeap ← CBV/SRV/DSV 힙
```

**의존 방향 규칙:**
- `Engine`은 RHI 인터페이스(`IRHIDevice`, `IRHIContext`)만 참조; 구체 D3D12 타입은 `static_cast`로만 접근
- `Renderer`는 `D3D12Context`를 직접 참조 (Draw 호출 성능 때문에 인터페이스 우회)
- `Asset`/`Scene`/`Lighting`은 D3D12에 의존하지 않음 (플랫폼 독립)
- `Platform/Win32`는 다른 모듈에 의존하지 않음; 콜백으로 Engine에 이벤트 전달

---

## 3. 렌더 파이프라인 흐름

```
Engine::Render()
  └─ D3D12Context::BeginFrame()
       └─ Reset CommandAllocator, CBPool, InstancePool
  └─ D3D12Context::Clear()  (cobalt blue background)
  └─ Renderer::RenderScene(SceneGraph, Camera, aspectRatio, LightManager)
       │
       ├─ [1] 뷰 행렬/투영 행렬 계산 → ViewProj 저장
       ├─ [2] FrustumCuller::Build(ViewProj)  — 6-plane frustum 추출
       ├─ [3] LightCuller::CullLights()       — Frustum 밖/저기여 광원 제거
       ├─ [4] D3D12Context::SetViewProjection/SetPBRLights/SetRenderMode
       │
       ├─ [5] Shadow Depth Pass  (FullPBRShadows 모드 한정)
       │       └─ 그림자 광원별:
       │            ├─ Light VP 행렬 계산 (Directional=Ortho, Spot=Perspective)
       │            ├─ D3D12Context::BeginShadowPass(idx)
       │            ├─ SceneGraph 순회 → DrawShadowDepthInstanced (Frustum Cull 적용)
       │            └─ D3D12Context::EndShadowPass(idx)
       │
       ├─ [6] D3D12Context::RestoreMainRenderTarget()
       │       └─ ShadowConstants 갱신 (텍셀 크기, 노멀 바이어스)
       │
       ├─ [7] Wireframe 모드: SceneGraph 순회 → DrawPrimitivesWireframe
       │
       └─ [8] PBR 모드 (Solid / BaseColorOnly / FullPBR / FullPBRShadows):
               ├─ Pass A: Opaque + AlphaMask 수집
               │    └─ 각 노드: FrustumCull → OcclusionCull(stub) → LOD 선택
               │         → InstanceBatcher에 추가 (Mesh+Material 키로 그룹핑)
               ├─ Front-to-back 정렬 (Early-Z rejection 최대화)
               ├─ DrawPrimitivesPBRInstanced (Opaque 배치)
               ├─ DrawPrimitivesPBRInstanced (AlphaMask 배치)
               └─ Pass B: AlphaBlend (back-to-front 정렬 → 단일 드로우콜)
  │
  └─ DebugHUD::Render()   (D2D1 텍스트 오버레이)
  └─ D3D12Context::EndFrame()
       └─ ExecuteCommandLists → FlushD2DText → Present → WaitForGPU(Fence)
```

---

## 4. PSO (Pipeline State Object) 목록

| PSO 이름 | CullMode | Blend | Depth Write | 용도 |
|----------|----------|-------|-------------|------|
| `BasicColor` | BACK | None | YES | Phase 01 vertex-color 오브젝트 |
| `PBR` | **BACK** | None | YES | 일반 불투명 머티리얼 (`doubleSided=false`) |
| `PBRDoubleSided` | **NONE** | None | YES | 양면 불투명 머티리얼 (`doubleSided=true`) |
| `PBRAlphaBlend` | **BACK** | SrcAlpha/InvSrcAlpha | NO | 일반 반투명 머티리얼 |
| `PBRAlphaBlendDoubleSided` | **NONE** | SrcAlpha/InvSrcAlpha | NO | 양면 반투명 머티리얼 |
| `ShadowDepth` | BACK | None | YES (D32_FLOAT) | 깊이 전용 Shadow 패스 |
| `Wireframe` | NONE | None | YES | 와이어프레임 모드 |

**PSO 선택 로직** (`DrawPrimitivesPBRInstanced`):
```cpp
bool isBlend = material && material->alphaMode == AlphaMode::Blend;
bool isDS    = material && material->doubleSided;
// isBlend × isDS 조합으로 4가지 PSO 중 선택
```

**모든 PBR PSO는 동일한 Root Signature를 공유:**
- Param 0: CBV b0 (PerObjectPBR: ViewProj + CameraPos)
- Param 1: SRV t0~t4 (Material 텍스처 5채널: albedo/normal/metalRough/emissive/occlusion)
- Param 2: CBV b1 (LightsCB: 최대 16개 광원)
- Param 3: CBV b2 (PerMaterialCB: factor + 텍스처 플래그)
- Param 4: SRV t5~t12 (Shadow Maps 8개)
- Param 5: CBV b3 (ShadowCB: lightViewProj[8] + bias)
- Static Sampler s0: Anisotropic Wrap (텍스처)
- Static Sampler s1: Comparison LessEqual Border-White (PCF shadow)

---

## 5. Constant Buffer 레이아웃

| CB 레지스터 | 구조체 | 크기 (정렬 후) | 갱신 빈도 |
|-------------|--------|----------------|-----------|
| `b0` | `PerObjectPBR` (ViewProj + CamPos) | 80 → 256 B | 드로우 배치당 |
| `b1` | `LightConstants` (16 광원 배열 + 개수) | 1296 → 1536 B | 프레임당 |
| `b2` | `PerMaterialConstants` (factor + 플래그) | 80 → 256 B | 드로우 배치당 |
| `b3` | `ShadowConstants` (LVP[8] + bias) | 528 → 768 B | 프레임당 (Shadow Pass 전) |
| `b0 (Shadow)` | `ShadowPassConstants` (lightViewProj) | 64 → 256 B | Shadow 드로우당 |
| `b0 (Wireframe)` | `WireframeConstants` (World + ViewProj + CamPos) | 144 → 256 B | 드로우당 |

**CBPool 구조 (`D3D12CBPool`):**
- 단일 Upload Heap (4 MB, 영구 맵핑)
- 더블 버퍼링: Frame 0 / Frame 1 영역을 번갈아 사용
- `Allocate(size)`: `alignedSize = (size + 255) & ~255` → 256B 정렬 보장
- `ResetFrame(frameIndex)`: 프레임 시작 시 해당 영역 오프셋 리셋

---

## 6. Descriptor Heap 레이아웃

단일 `CBV_SRV_UAV` 힙 (shader-visible):

```
[0 ... 2047]   Persistent 영역 (절대 리셋 안 함)
  - 텍스처 SRV (TextureCache::GetOrLoad() 시 AllocatePersistent())
  - Shadow Map SRV (CreateShadowMaps() 최초 1회 AllocatePersistent(), 이후 재사용)
  - 폴백 텍스처 SRV (엔진 초기화 시)

[2048 ... 34815]  Transient 영역 (BeginFrame()마다 리셋)
  - 드로우콜마다 CBV 디스크립터 (AllocateTransient() × 4)
  - 드로우콜마다 Material SRV 블록 5개 (AllocateTransient() × 5)
  - 드로우콜마다 Shadow SRV 블록 8개 (AllocateTransient() × 8)
```

---

## 7. Shadow Map 시스템

**리소스:**
- `m_shadowMaps[8]`: `DXGI_FORMAT_R32_TYPELESS` Texture2D (Default Heap)
- DSV (`D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL`): 깊이 쓰기용
- SRV (`DXGI_FORMAT_R32_FLOAT`): 라이팅 패스 샘플링용
- `m_shadowSrvsAllocated` 플래그: SRV 슬롯 최초 1회만 `AllocatePersistent()`
  → `RecreateShadowMaps()` 시 기존 핸들 재사용 (descriptor heap leak 방지)

**해상도 자동 선택** (씬 대각선 기준):
| 씬 크기 | Shadow Map 해상도 |
|---------|-----------------|
| ≤ 10 m | 1024 × 1024 |
| ≤ 100 m | 2048 × 2048 |
| > 100 m | 4096 × 4096 |

**Shadow 카메라 배치 (Directional):**
```
shadowCamPos = sceneCenter - dir * (farPlane * 0.5f)
orthoSize    = diagonal * 1.5f
far          = diagonal * 3.0f
near         = diagonal * 0.5f
```

**PCF 3×3 필터 (PBR.hlsl `CalcShadow()`):**
```hlsl
for (int y = -1; y <= 1; y++)
    for (int x = -1; x <= 1; x++)
        shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler,
            uv + float2(x,y) * shadowTexelSize, depth);
shadow /= 9.0f;
```

---

## 8. 씬 로딩 흐름

```
Engine::LoadScene(filePath)
  1. WaitForGPU()  — GPU 완료 대기
  2. Renderer::ClearMeshCache() + TextureCache::Clear() + TextureStreamer::Clear()
  3. SceneLoader::LoadScene(filePath)  — Assimp 파싱
       ├─ aiProcess_Triangulate | GenNormals | CalcTangentSpace | ConvertToLeftHanded
       ├─ aiNode 트리 → SceneNode 트리 (각 aiMesh → 별도 SceneNode)
       ├─ 씬 로컬 공간 바운딩 박스 계산
       └─ 임베딩/외부 텍스처 경로 수집
  4. TextureCache::GetOrLoad() × N  — GPU 텍스처 업로드 (Upload Heap → Default Heap)
       ├─ baseColor/emissive: DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
       └─ normal/metallicRoughness/occlusion: DXGI_FORMAT_R8G8B8A8_UNORM (linear)
  5. SceneGraph::SetRoot(data.rootNode)
  6. 월드 공간 바운딩 박스 재계산 (node WorldMatrix × mesh local AABB)
  7. Camera 배치 (씬 내 Camera 노드 → Fit to Scene 폴백)
  8. LightManager 3-포인트 조명 자동 배치 (씬 크기 비례)
  9. Renderer::RegisterMeshesForLOD()  — 비동기 Auto-LOD QEM 생성
  10. Renderer::SetSceneDiagonal/SetSceneCenter
  11. D3D12Context::RecreateShadowMaps()  — 씬 크기 기반 해상도 재설정
```

**좌표계 변환 (glTF RH → DirectX LH):**
```
aiProcess_ConvertToLeftHanded = MakeLeftHanded | FlipUVs | FlipWindingOrder
```
- `MakeLeftHanded`: 정점 Z 반전 + UV.y 반전
- `FlipUVs`: UV.y 재반전 (상쇄 → 원본 UV 유지)
- `FlipWindingOrder`: CCW → CW (DirectX FrontCounterClockwise=FALSE와 일치)

---

## 9. 최적화 시스템

### Frustum Culling (`FrustumCuller`)
- `DirectX::BoundingFrustum` (ViewProj 행렬로부터 추출)
- 각 SceneNode의 WorldAABB vs 6-평면 교차 검사
- `BoundingBox::Intersects(BoundingFrustum)` 활용

### LOD 시스템 (`LODSelector`)
- LOD 전환 거리: `2.0× sceneDiagonal` (Medium), `6.0× sceneDiagonal` (Low)
- VRAM 압박 시 전환 거리 0.5× 스케일링 (더 공격적 LOD)
- Auto-LOD QEM: 백그라운드 스레드에서 원본 메시 50%/25% 삼각형으로 자동 생성

### Light Culling (`LightCuller`)
- Directional Light: 항상 활성
- Point/Spot Light: 유효 반경 BoundingSphere vs Camera Frustum 교차 + 기여도 임계값

### Instance Batching (`InstanceBatcher`)
- 동일 `(Mesh*, Material*)` 키로 그룹핑
- Per-instance Upload Buffer에 World Matrix 배치 복사
- `DrawIndexedInstanced(indexCount, instanceCount, ...)` 1회 호출

### Optimization 메뉴 (런타임 on/off)
| 항목 | 기본 | Renderer API |
|------|------|--------------|
| Frustum Culling | ON | `SetFrustumCullingEnabled(bool)` |
| Light Culling | ON | `SetLightCullingEnabled(bool)` |
| LOD | ON | `SetLODEnabled(bool)` |
| MipMapping | ON | `SetMipMappingEnabled(bool)` |

---

## 10. 스레딩 모델

| 스레드 | 역할 |
|--------|------|
| **메인 스레드** | Win32 메시지 루프, Update, Render, GPU 업로드 커맨드 기록 |
| **워커 스레드 (ThreadPool)** | 이미지 파일 디코딩 (stb_image), Auto-LOD QEM 계산 |
| **Copy Queue (D3D12)** | Upload Heap → Default Heap 전송 (Graphics Queue와 병렬) |

**동기화:**
- GPU 완료: `m_fence` + `WaitForGPU()` (씬 교체 전, Shutdown 전)
- Copy Queue 완료: `m_copyFence` + `WaitForCopyQueue()`
- 텍스처 업로드: `BeginUploadCommands()` / `EndUploadCommands()`

---

## 11. 셰이더 파이프라인

### PBR.hlsl (메인 렌더링)

```
VSMain:
  World    = per-instance rows (slot 1)
  worldPos = mul(float4(pos,1), World)
  clipPos  = mul(worldPos, ViewProj)
  TBN      = Gram-Schmidt 재직교화 후 월드 공간 TBN 행렬

PSMain(isFrontFace: SV_IsFrontFace):
  albedo          = AlbedoMap.Sample || baseColorFactor
  [AlphaMask]       clip(alpha - alphaCutoff)
  metallic,rough  = MetallicRoughnessMap (G=roughness, B=metallic)
  N               = NormalMap → TBN 변환 || vertex N
  [DoubleSided]     if (!isFrontFace) N = -N   // 뒷면 법선 반전
  V               = normalize(CameraPosition - worldPos)
  F0              = lerp(0.04, albedo, metallic)

  for each active light:
    L, attenuation  = compute per type (Dir/Point/Spot)
    BRDF            = Cook-Torrance (GGX NDF + Smith G + Schlick F)
    shadowFactor    = CalcShadow PCF 3×3 (if castShadow)
    Lo += (diffuse + specular) * lightColor * attenuation * NdotL * shadowFactor

  ambient = 0.15 * albedo * AO
  color   = ambient + Lo + emissive
  color   = Reinhard tone mapping
  color   = pow(color, 1/2.2)   // gamma correction
```

### ShadowDepth.hlsl (깊이 전용)
```
VSMain: clipPos = mul(float4(pos,1), World) × LightViewProj
PS: 없음 (depth write만)
```

---

## 12. 행렬 규칙 (Critical)

| 항목 | 방향 |
|------|------|
| DirectXMath 내부 | **Row-major** |
| HLSL cbuffer 기본 | **Column-major** |
| GPU 전달 시 | 반드시 `XMMatrixTranspose()` 적용 |
| Instance buffer World | SceneLoader에서 이미 전치된 상태로 `memcpy` |
| HLSL `row_major` 키워드 | **사용 금지** (CBV 방식에서 불안정) |

```cpp
// 패턴:
XMStoreFloat4x4(&dst, XMMatrixTranspose(worldMatrix));
// → memcpy to GPU Upload Buffer
```

---

## 13. HLSL cbuffer 패킹 규칙 (Critical)

HLSL cbuffer 내 배열 원소는 **16바이트 경계**에 정렬됨:

```hlsl
// 잘못된 예 — float[2]는 HLSL에서 각 원소가 16B → 합계 32B
float _pad[2];

// 올바른 예 — float2는 8B (C++ float[2]와 일치)
float2 _pad;
```

C++ 구조체는 대응 HLSL 구조체와 오프셋을 수동 검증해야 함.
`GPULightData._pad1[2]`는 C++ `float[2]`(8B)이나 HLSL에서는 `float2 _pad1`(8B)로 선언.
