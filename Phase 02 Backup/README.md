# Phase 02 Backup

> 백업 일시: 2026-03-14
> Phase 01 ~ Phase 30 완전 완료 상태 기준

---

## 백업 목적

Phase 03 (Phase 32~49) 진행 전 안전망 확보.
**이 폴더의 파일은 어떠한 경우에도 수정하지 않는다. 참조만 가능.**

---

## Phase 02 최종 구현 기능 목록

### Phase 01 — 기본 렌더링 엔진

- Win32 API + DirectX 12 + C++17 기반 실시간 렌더링 엔진
- RHI(Rendering Hardware Interface) 추상화 계층 (IRHIDevice/IRHIContext/IRHIBuffer)
- Scene Graph (트리 구조, DFS 순회, WorldMatrix 계층 전파)
- DirectXMath 기반 수학 시스템 (SIMD, row-major/column-major 전치 규칙)
- Vertex/Index Buffer (Upload → Default Heap), PSO, Command List 관리
- Face Color Palette (8색 그래프 컬러링, 인접 면 구별)
- DebugHUD (D3D11On12 + D2D1 + DirectWrite 텍스트 오버레이)
- 화면 모드 전환 (Windowed 프리셋 / Borderless Fullscreen, Esc 복귀)
- 카메라 (Perspective/Orthographic, WASD+QE, 마우스 우클릭 회전, 휠 줌, 중클릭 패닝)
- 유닛·스모크 테스트 인프라 (Google Test + WARP 어댑터)

### Phase 02 — glTF/PBR 렌더링 파이프라인

**씬 로딩 & Asset 시스템**
- glTF 2.0 / GLB / FBX 씬 로딩 (Assimp, aiProcess_ConvertToLeftHanded)
- 단일 aiNode의 복수 aiMesh → 개별 SceneNode 분리 (Sponza 103개 서브프리미티브)
- Per-Mesh AABB (BoundingBox) 계산 및 WorldAABB 캐싱 (dirty flag)
- Material 시스템 (PBR factor + 텍스처 5채널: albedo/normal/metalRough/emissive/occlusion)
- Texture 시스템 (CPU Mip chain 생성, Upload Heap → Default Heap, SRGB/Linear 구분)
- TextureCache (경로 기반 중복 방지, 폴백 1×1 white 텍스처)
- 드래그 앤 드롭 씬 로딩 (WM_DROPFILES)
- Fit to Scene 자동 카메라 배치 (씬 바운딩 박스 기반)

**PBR 셰이더 (PBR.hlsl)**
- Cook-Torrance BRDF: GGX NDF + Smith-Schlick G + Schlick Fresnel
- 멀티 광원 루프 (최대 16개, Directional / Point / Spot 타입 분기)
- PCF 3×3 Shadow Mapping (SampleCmpLevelZero, Comparison Sampler)
- Reinhard Tone Mapping + Gamma Correction (pow 1/2.2)
- SV_IsFrontFace 기반 양면 법선 반전 (CullMode=NONE PSO 전용)

**광원 시스템**
- 씬 로드 시 3-포인트 조명 자동 배치 (Key/Fill/Back PointLight + Orbit Directional)
- Orbit Light: 월드 Y축 궤도 회전 (0.8 rad/s), 45° 앙각 고정, castShadow=true
- Sponza 전용 태양 방향 토글 (L키, 두 방향 프리셋)
- Bistro 씬: Orbit Light 비활성화, 별도 씬 설정 적용
- 거리 기반 감쇠 (Kc/Kl/Kq), Spot 원뿔 페이드 (smoothstep)

**Shadow Mapping**
- Directional Light: Orthographic 투영, 씬 크기 기반 자동 범위
- Spot Light: Perspective 투영 (outerConeAngle × 2)
- Shadow Map 해상도 자동 선택: ≤10m → 1024, ≤100m → 2048, >100m → 4096
- Shadow Normal Bias (Karan-Hanrahan, 씬·해상도 비례 world-space bias)
- Shadow Depth Pass Frustum Culling (광원 시점 BoundingFrustum 적용)

**렌더링 최적화**
- Frustum Culling (카메라·광원 시점, BoundingBox::Intersects)
- Light Culling (Frustum 밖 + 기여도 임계값 이하 Point/Spot 제거)
- LOD 시스템 (Auto-LOD QEM, 거리 2×/6× sceneDiagonal 전환, VRAM 압박 시 공격적 전환)
- Instance Batching (동일 Mesh+Material → DrawIndexedInstanced 1회)
- Texture Streaming (VRAM 모니터링 IDXGIAdapter3, 가시성·거리 기반 Mip 우선순위)
- Constant Buffer 풀링 (링 버퍼 4MB, 256B 정렬, 더블 버퍼링)
- Dirty Flag 기반 CB 갱신 스킵 (Transform 미변경 시 memcpy 스킵)
- Opaque Front-to-Back 정렬 (Early-Z rejection 극대화)
- Alpha Blend Back-to-Front 정렬

**PSO 구성 (7종)**
- BasicColor (vertex-color, Phase 01 호환)
- PBR (CullMode=BACK, Opaque)
- PBRDoubleSided (CullMode=NONE, Opaque)
- PBRAlphaBlend (CullMode=BACK, 반투명)
- PBRAlphaBlendDoubleSided (CullMode=NONE, 반투명)
- ShadowDepth (depth-only, DepthBias 적용)
- Wireframe

**D3D12 인프라**
- CBV_SRV_UAV Descriptor Heap (Persistent 2048 + Transient 32768 분리 관리)
- Shadow Map SRV 최초 1회 AllocatePersistent + 재사용 (descriptor leak 방지)
- Copy Queue (비동기 텍스처 업로드, Graphics Queue와 병렬)
- ThreadPool (CPU 코어 기반 워커 — 이미지 디코딩, Auto-LOD QEM)

**UX / 메뉴**
- 5단계 렌더링 모드 (Wireframe / Solid / Base Color / Full PBR / Full PBR+Shadows)
- Optimization 메뉴 (Frustum Culling / Light Culling / LOD / MipMapping 런타임 토글)
- Camera 메뉴 (투영 전환, FOV, Reset, Fit to Scene)
- Light 메뉴 (색상 일괄 적용, 광원 정보 HUD 토글)
- DebugHUD: FPS, 해상도, 폴리곤, Culled/Occluded, VRAM, 스트리밍, 드로우콜, 렌더모드

**코드 품질**
- 전체 코드 리뷰 완료 (dead code 제거, include 정리, 네이밍 일관성)
- 유닛 + 스모크 테스트 91/91 통과 (WARP 어댑터, Google Test)
- ARCHITECTURE.md 작성 완료 (디렉토리 구조, 모듈 의존성, 파이프라인, PSO, CB, 셰이더 등)

---

## 파일 구성

| 경로 | 내용 |
|------|------|
| `src/` | 엔진 소스 (Asset, Core, Lighting, Math, Platform, RHI, Renderer, Scene, Shaders) |
| `tests/` | 유닛 + 스모크 테스트 (unit/, smoke/) |
| `assets/` | glTF/GLB 테스트 모델 (DamagedHelmet, Sponza, FlightHelmet 등) |
| `RealtimeRenderingEngine.sln` | Visual Studio 2022 솔루션 |
| `ARCHITECTURE.md` | Phase 30 기준 아키텍처 문서 |
| `PLAN.md` | Phase별 구현 계획 |
| `CLAUDE.md` | 코딩 가이드 |
| `RemainingWork.md` | Phase 30 완료 시점 잔여 작업 목록 |
| `vcpkg.json` | 패키지 의존성 (assimp, gtest 등) |

---

## 빌드 방법

Visual Studio 2022에서 `RealtimeRenderingEngine.sln` 열고 `RREngine` 프로젝트 빌드.
- Debug|x64 또는 Release|x64
- vcpkg integrate install 필요 (assimp:x64-windows, gtest:x64-windows)
