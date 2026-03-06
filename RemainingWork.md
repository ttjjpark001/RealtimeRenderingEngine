# 잔여 구현 항목 정리

> 최종 업데이트: 2026-03-06 (Phase 23.5 → Phase 30 통합 결정 반영)
> Phase 24까지 구현된 내용을 바탕으로, 이후 필요한 작업을 정리한다.

---

## 현재 구현 완료 범위 (Phase 24)

| 항목 | 상태 |
|------|------|
| FrustumCuller (AABB vs Frustum 교차) | ✅ 완료 |
| LightCuller (거리·기여도 기반 광원 컬링) | ✅ 완료 |
| LODSelector (거리 기반 + 비동기 자동 LOD 생성) | ✅ 완료 |
| OcclusionCuller | ⚠️ P0 스텁 (항상 false, 실제 컬링 없음) |
| CullStats + DebugHUD 이중 폴리곤 표시 | ✅ 완료 |
| Optimization 메뉴 (Frustum / Light / LOD 토글) | ✅ 완료 |
| Shadow Mapping (Directional/Spot, PCF) | ✅ 완료 |
| PBR 셰이더 + 텍스처 로딩 | ✅ 완료 |
| 프리미티브 → SceneNode 분리 + Per-Mesh AABB | ✅ 완료 |
| Shadow Map 해상도 자동 선택 (씬 크기 기반) | ✅ 완료 |
| Shadow Ortho/Perspective 범위 자동 계산 | ✅ 완료 |
| shadowTexelSize GPU 동적 전달 | ✅ 완료 |
| PBR.hlsl HLSL X4000 경고 최소화 시도 | ⚠️ 부분 완료 (CalcShadow 경고 1건 잔존) |
| Orbit Light → Directional + castShadow=true | ✅ 완료 |

---

## 잔여 구현 항목

### Phase 25 — Texture Streaming + Mip-Mapping

현재 모든 텍스처가 최고 해상도(단일 Mip)로 메인 스레드에서 한 번에 로드된다.

**구현 항목**

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| `TextureStreamer.h/.cpp` | 미존재 | 가시성·거리 기반 우선순위 큐, Mip 레벨 동적 로딩/해제 |
| Mip chain 생성 | `MipLevels = 1` | `floor(log2(max(w,h))) + 1` 전체 Mip 생성 |
| Anisotropic Sampler | Linear Wrap 사용 중 | `D3D12_FILTER_ANISOTROPIC`, MaxAnisotropy = 16 |
| 스트리밍 우선순위 | 없음 | `priority = isVisible ? (1/distance) : 0` |
| VRAM 예산 연동 | 없음 | VRAM 초과 시 LRU + 거리 기반 Mip 해제 |

---

### Phase 26 — Instanced Rendering + 멀티스레드 로딩

**Instanced Rendering**

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| `InstanceBatcher.h/.cpp` | 미존재 | 동일 Mesh+Material 그룹핑, Instance Buffer 생성 |
| `DrawIndexedInstanced` | 미사용 | 현재 동일 Mesh라도 노드마다 개별 Draw 호출 |
| per-instance Input Layout | 없음 | slot 1: `INSTANCE_WORLD` (4×float4, per-instance) |
| HLSL `SV_InstanceID` | 없음 | VS에서 Instance Buffer World Matrix 인덱싱 |

**멀티스레드 로딩**

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| `ThreadPool.h/.cpp` | 미존재 | CPU 코어 수 기반 워커 스레드 |
| 텍스처 디코딩 병렬화 | 순차 실행 | 씬 로드 중 메인 스레드 프리즈 원인 |
| Copy Queue | 없음 | Graphics Queue와 병렬 GPU 업로드 (Phase 26) |

---

### Phase 27 — GPU 메모리 최적화

**CB 풀링**

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| `D3D12CBPool.h/.cpp` | 미존재 | Upload Heap 풀, 256바이트 정렬, 링 버퍼 (프레임 0/1 영역 분리) |
| Per-Object / Per-Material CB 분리 | 없음 | `register b0` (오브젝트), `register b2` (Material) |

**갱신 최적화**

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| Transform Dirty Flag | 없음 | 변경 없으면 CB 갱신 스킵 |
| Material Dirty Flag | 없음 | Material 파라미터 미변경 시 갱신 스킵 |
| Light CB Dirty Flag | 없음 | 광원 데이터 미변경 시 갱신 스킵 |
| VRAM 모니터링 | 없음 | `IDXGIAdapter3::QueryVideoMemoryInfo`, DebugHUD 표시 |
| 적응적 CB 갱신 | 없음 | VRAM > Budget 80% 시 저우선순위 오브젝트 갱신 빈도 감소 |

**정렬 최적화**

| 작업 | 현재 상태 | 설명 |
|------|-----------|------|
| Opaque Front-to-Back 정렬 | 없음 | Early-Z rejection 극대화 |
| Material 기반 드로우콜 정렬 | 없음 | PSO/텍스처 상태 전환 최소화 |

---

### Phase 28 — 통합 & 벤치마크

Phase 25~27 완료 후 전체 파이프라인 연결 및 검증.

| 작업 | 설명 |
|------|------|
| 12단계 파이프라인 통합 | Scene Graph 순회 → Frustum → Occlusion → LOD → Light Cull → Instance Batching → Texture Streaming → CB 갱신 → Material 정렬 → Front-to-Back → Shadow Pass → Main Pass |
| DebugHUD 전체 항목 | VRAM 사용량, 스트리밍 리소스 수, 드로우콜 수 추가 |
| 대형 씬 벤치마크 | Sponza, Bistro 로딩 + Full PBR + Shadows + 모든 최적화 활성 상태에서 60fps 목표 |
| 5단계 렌더링 모드 전체 확인 | Wireframe / Solid / Base Color / Full PBR / Full PBR+Shadows |
| 전체 테스트 통과 확인 | 유닛 + 스모크 통과 |

---

### Phase 29 — 코드 리뷰 & 문서화

| 작업 | 설명 |
|------|------|
| 전체 코드 리뷰 | Dead code 제거, include 정리, 네이밍 일관성 검증 |
| PBR.hlsl CalcShadow X4000 경고 | FXC 컴파일러 한계 (비교 샘플러 + 동적 cbuffer 인덱스). Phase 24에서 최소화 시도 후 1건 잔존. Texture2DArray로의 리팩터링 또는 FXC 업데이트로 재검토 |
| GPU 리소스 해제 누락 검사 | Fence 대기 후 해제 보장, ComPtr 사용 일관성 |
| PIX / 타임스탬프 쿼리 프로파일링 | 병목 구간 식별 및 최적화 |
| `ARCHITECTURE.md` 작성 | 전체 엔진 구조, 모듈 간 의존성, 렌더 파이프라인 다이어그램 |

---

### Phase 30 — Occlusion Culling (Hi-Z GPU)

현재 `OcclusionCuller::IsOccluded()`는 항상 `false`를 반환하는 스텁이다.
CPU Readback 간이 방식을 거치지 않고 GPU Hi-Z 방식으로 바로 구현한다.
Compute Shader 파이프라인 인프라 구축이 선행 조건이다.

| 작업 | 설명 |
|------|------|
| Compute Shader 인프라 | `D3D12ComputePipeline.h/.cpp` 신규, `D3D12Context::Dispatch()` 추가, UAV descriptor 관리 |
| Hi-Z Buffer 생성 | 이전 프레임 Depth → `R32_FLOAT` SRV 복사 후 Compute로 Mip chain(UAV) 생성 (max 필터) |
| GPU-side AABB 비교 | AABB 8코너 → NDC → screen-space min/max, 최적 Mip 레벨 샘플링, 근거리 Z 비교 |
| Readback + Fence 동기화 | GPU 판정 결과 → Readback Buffer → CPU 읽기 (1프레임 레이턴시) |
| `occlusionCulledNodes` 통계 | CullStats 반영, DebugHUD 표시 |
| Optimization 메뉴 항목 추가 | `ID_OPTIM_OCCLUSION_CULL = 8004` (Win32Menu + Engine 콜백 연결) |

---

### Phase 31 — Point Light Cube Map Shadowing

`castShadow = true`인 Point Light에 대해 6면 TextureCube 기반 Omnidirectional Shadow Map 구현.

| 작업 | 설명 |
|------|------|
| TextureCube 리소스 생성 | `TEXTURE2D_ARRAY` (ArraySize=6, D32_FLOAT), 6개 DSV + 1개 SRV, 최대 4광원 |
| 6-pass Shadow Depth | 광원 1개당 ±X/±Y/±Z 방향 6회 depth pass, FOV=90°, aspect=1.0 |
| HLSL TextureCube 샘플링 | `TextureCube PointShadowMap[]` 바인딩, `lightToPixel` 방향 벡터로 depth lookup |
| LightConstants 타입 구분 | Directional/Spot(Texture2D) vs Point(TextureCube) 구분 플래그 추가 |
| LightCuller 연동 | shadow casting Point light도 거리 기반 culling 적용 |
| DebugHUD | Cube Shadow Pass 수 표시 |

---

### Phase 32 — Skeletal Animation

glTF Node Transform 애니메이션(키프레임)과 Skeletal Animation(본/스킨) 구현.

**Part A: Node Transform Animation (G-08)**

| 작업 | 설명 |
|------|------|
| `Animation.h` | `AnimationClip`, `AnimationChannel`, `Keyframe<T>` 구조체 |
| 키프레임 보간 | LINEAR (Lerp/Slerp), STEP, CUBICSPLINE |
| SceneLoader 확장 | `aiAnimation` → `AnimationClip` 변환, target name → SceneNode 매핑 |
| `AnimationController` | `Update(dt)`, Play/Pause/Loop, 클립 목록 메뉴 |

**Part B: Skeletal Animation (G-09)**

| 작업 | 설명 |
|------|------|
| `Skeleton.h` | `Bone`, `Skeleton`, `Skin` 구조체, inverse bind matrix |
| SceneLoader 확장 | `aiMesh::mBones` → `Skeleton/Skin` 생성, per-vertex joint/weight 추출 |
| Vertex 포맷 확장 | `JOINTS_0` (XMUINT4) + `WEIGHTS_0` (XMFLOAT4) 추가, Input Layout/HLSL 갱신 |
| GPU Skinning | `SkinCB : register(b4)` (joint matrix palette 128개), PBR.hlsl VS 스키닝 계산 |
| Joint palette 업로드 | AnimationController::Update() 후 bone world matrix → GPU 복사 |

---

## 권장 구현 순서

```
Phase 24   완료 ✅  (HLSL 경고 + Shadow Map 자동 크기 조정)
    │
Phase 25    Texture Streaming + Mip-Mapping
    │           + Anisotropic Sampler 교체
    │
Phase 26    Instanced Rendering + 멀티스레드 로딩
    │
Phase 27    GPU 메모리 최적화
    │           (CBPool + Dirty Flag + Front-to-Back + VRAM 모니터링)
    │
Phase 28    통합 & 벤치마크 (Sponza 60fps 목표)
    │
Phase 29    코드 리뷰 + CalcShadow X4000 재검토 + ARCHITECTURE.md
    │
Phase 30    Occlusion Culling (Hi-Z GPU)
    │           CPU Readback 단계 없이 바로 Hi-Z GPU 구현
    │           Compute Shader 파이프라인 + Optimization 메뉴 항목 포함
    │
Phase 31    Point Light Cube Map Shadowing
    │           Omnidirectional Shadow Map (TextureCube, 6-pass depth)
    │
Phase 32    Skeletal Animation
            Part A: Node Transform Animation (TRS 키프레임)
            Part B: Skeletal Animation (본/스킨, GPU Skinning)
```

---

## 기타 메모

- **PBR.hlsl CalcShadow X4000 경고**: FXC 컴파일러 한계 — 비교 샘플러(`SamplerComparisonState`)와 동적 cbuffer 인덱스 조합에서 발생하는 고유 quirk. `SampleShadowMap`을 `if/else-if` 체인 + 명시적 초기화로 변경하여 SampleShadowMap 경고는 제거했지만 CalcShadow 경고 1건 잔존. Phase 29에서 `Texture2DArray` 방식으로 리팩터링 검토.
- **Occlusion Culling Optimization 메뉴 항목**: Phase 30 구현 시 `ID_OPTIM_OCCLUSION_CULL = 8004` 추가 (Phase 23.5 별도 구현 없이 Phase 30에서 통합)
- **Shadow Map SRV 누수**: `RecreateShadowMaps()` 호출 시 persistent descriptor heap에 이전 SRV 8개가 남음 (최대 1024개 중). 개발 엔진 용량 내 허용 범위이나 Phase 29에서 정리 권장.
