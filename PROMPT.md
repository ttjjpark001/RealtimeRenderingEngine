# PROMPT: 단계별 구현 프롬프트

> Phase 01, 02 (Prompt 1~31)는 구현 완료. 아카이브: `PROMPT_Phase01-02_Archive.md`

---

## Prompt 32: RRScenePreprocessor — 오프라인 씬 전처리 도구 + 백그라운드 자동 생성

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 32 섹션과 GoodToPreprocess.md를 참조하여 Phase 32를 구현하라.
이 단계는 .rrscene 전처리 파이프라인을 공용 클래스로 구현하고,
CLI 도구와 렌더링 앱 내 백그라운드 자동 생성의 두 진입점을 제공한다.

1. .rrscene 바이너리 포맷을 정의한다 (src/Asset/RRSceneFormat.h, 공용 헤더).
   헤더:
     · char magic[4] = "RRSC"
     · uint32 version = 1
     · uint64 sourceHash  (원본 파일 크기 ^ 수정 시각, 변경 감지용)
     · uint32 sectionCount
     · SectionEntry[] { SectionType type; uint64 offset; uint64 size; }
   섹션 타입: Scene / Mesh / Material / Texture / Light
   각 섹션 세부 구조:
   - Scene: 노드 수, 노드별(부모 인덱스, 이름, 로컬 TRS 행렬, meshIndex, materialIndex), 씬 AABB, 카메라 초기(position/yaw/pitch/fov)
   - Mesh: 메시 수, 메시별(vertex 수, index 수, Vertex 배열 raw dump, Index 배열 raw dump, AABB, LOD 수, LOD별 vertex/index + 전환 거리)
   - Material: 재질 수, 재질별(PBR factor, AlphaMode, doubleSided, textureIndex 참조 5개, sRGB 플래그)
   - Texture: 텍스처 수, 텍스처별(width, height, mipLevels, DXGI_FORMAT, 전체 Mip chain 픽셀 데이터 연속 배치)
   - Light: 광원 수, 광원별(type, color, intensity, position, direction, Kc/Kl/Kq, innerCone, outerCone, castShadow, bsRadius)

2. 전처리 파이프라인을 공용 클래스로 구현한다 (src/Asset/ScenePreprocessor.h/.cpp).
   - static bool Generate(const std::string& sourcePath, const std::string& outputPath):
     동기 실행, CLI 도구와 엔진에서 모두 호출 가능
     a. Assimp 파싱: aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded
     b. Vertex/Index 변환: aiMesh → 엔진 Vertex 구조체
        · Tangent 없으면 Gram-Schmidt 재직교화로 생성
     c. 프리미티브 분리: aiNode의 복수 aiMesh → 각각 SceneNode로 분리
     d. 메시별 AABB: BoundingBox::CreateFromPoints()
     e. Auto-LOD 생성:
        · LOD 1: 원본 삼각형 50% (QEM Edge Collapse)
        · LOD 2: 원본 삼각형 25%
        · 전환 거리: sceneDiagonal × 2.0f (LOD 1), × 6.0f (LOD 2)
     f. 이미지 디코딩: stb_image로 PNG/JPEG → RGBA 픽셀 버퍼
        · baseColor/emissive: sRGB 플래그 설정
        · normal/metallicRoughness/occlusion: Linear 플래그 설정
     g. Mip chain 생성: CPU box filter, floor(log2(max(w,h))) + 1 레벨
     h. 씬 구조 직렬화: 노드 계층, 씬 AABB, 카메라 초기 배치, Material, Light(BoundingSphere 포함)
     i. 원자적 파일 쓰기: 임시 파일(.rrscene.tmp) 완성 후 최종 경로로 rename
   - static std::future<bool> GenerateAsync(const std::string& sourcePath):
     std::async로 백그라운드 스레드에서 Generate() 실행, future 반환

3. CLI 도구 프로젝트를 추가한다 (RRScenePreprocessor, Console Application).
   - ScenePreprocessor::Generate()를 호출하는 얇은 래퍼
   - main(argc, argv): 입력 파일 경로 인수 받음, 출력 경로 = 입력과 동일 디렉토리 + .rrscene 확장자
   - 출력: bin/Debug/RRScenePreprocessor.exe

4. 렌더링 앱에 이중 로딩 경로를 추가한다 (src/Asset/SceneLoader).
   - SceneLoader::LoadScene(path):
     a. 동일 디렉토리에 path.rrscene 존재 여부 확인
     b. 존재하면: LoadRRScene(rrscenePath) 시도
        · Header magic/version 검증
        · sourceHash와 원본 파일 해시 비교 → 불일치 시 표준 경로로 폴백 + 로그
        · 검증 통과 시: 섹션 순서대로 SceneNode/Mesh/Material/Texture/Light 객체 생성
        · GPU 업로드(VB/IB/Texture)만 수행 (Assimp 파싱 없음)
     c. 없거나 실패 시: 기존 Assimp 표준 경로 사용 → 로딩 완료 후 항목 5 실행
   - DebugHUD에 로딩 경로 표시: "Fast (.rrscene)" 또는 "Standard (Assimp)"

5. 표준 경로 로딩 후 백그라운드 자동 전처리를 구현한다 (Engine::LoadScene()).
   - 표준 경로(Assimp) 로딩 완료 직후: ScenePreprocessor::GenerateAsync(sourcePath) 호출
     · 반환된 std::future<bool>을 Engine 멤버(m_preprocessFuture)에 저장
   - 렌더링 블로킹 없이 백그라운드 스레드에서 전처리 파이프라인 실행
   - DebugHUD에 진행 상태 표시:
     · 진행 중: "Preprocessing scene..." (m_preprocessFuture가 유효한 동안)
     · 완료 후: 메시지 사라짐
   - 매 프레임 Engine::Update()에서 future 완료 여부 폴링:
     · future.wait_for(0ms) == ready → 결과 확인, 성공 시 콘솔 로그 출력
       ("Sponza.rrscene saved — next load will use fast path")
     · m_preprocessFuture 초기화(reset)
   - 씬 교체 시 이전 전처리 future가 실행 중이면 detach(취소 불가) 후 진행

6. 동작을 검증한다.
   - Sponza.gltf 첫 로딩: 표준 경로(Assimp) 사용 + DebugHUD "Preprocessing scene..." 표시 확인
   - 전처리 완료 후: Sponza.rrscene 파일 생성 확인, 콘솔 로그 확인
   - Sponza.gltf 두 번째 로딩: 자동으로 고속 경로 사용(~90% 단축) 확인
   - CLI 도구로 동일한 .rrscene 생성 후 렌더링 앱에서 고속 로딩 확인
   - 원본 파일 변경 후 로딩: 해시 불일치 감지 → 표준 경로 폴백 + 재전처리 시작 확인

빌드하여 첫 로딩 시 백그라운드 자동 생성이 동작하고, 두 번째 로딩부터 고속 경로가 사용되는지 확인하라.
```

---

## Prompt 33: Occlusion Culling — Hi-Z GPU

```
PRD.md, PLAN.md(Phase 33), CLAUDE.md를 참조하여 Phase 33을 구현하라.
이 단계는 현재 P0 스텁(항상 false)인 OcclusionCuller를 GPU Hi-Z 방식으로 완전 구현한다.
CPU Readback 간이 방식을 거치지 않고 바로 Hi-Z로 구현한다.
현재 엔진에 Compute Shader 인프라가 없으므로, 먼저 인프라를 구축한다.

1. Compute Shader 인프라를 구축한다.
   - src/RHI/D3D12/D3D12ComputePipeline.h/.cpp를 신규 생성한다.
     · CS 전용 Root Signature 생성 (UAV, SRV, CBV 슬롯 정의)
     · ID3D12PipelineState (ComputePipelineStateDesc) 생성/관리
   - D3D12Context에 Dispatch(x, y, z) 메서드를 추가한다.
   - CBV_SRV_UAV DescriptorHeap을 UAV 슬롯이 포함되도록 확장한다.

2. Hi-Z (Hierarchical-Z) Buffer를 생성한다.
   - 이전 프레임 Depth Buffer(DXGI_FORMAT_D32_FLOAT)를 DXGI_FORMAT_R32_FLOAT로 복사한다.
     · CopyTextureRegion 또는 Compute Shader를 사용하여 복사한다.
   - 복사된 텍스처를 시작으로 반씩 축소하는 Mip chain을 Compute Shader로 생성한다.
     · 각 Mip 단계: UAV(write) 바인딩, SRV(read) 바인딩 교차
     · 축소 필터: max(depth) — 보수적 occlusion 판정을 위해 최대값 사용
     · 최대 floor(log2(max(width, height))) 단계 생성
   - Hi-Z HLSL 파일: src/Shaders/HiZDownsample.hlsl

3. GPU-side AABB depth 비교 Compute Shader를 구현한다.
   - src/Shaders/OcclusionTest.hlsl을 신규 생성한다.
   - 입력: SceneNode AABB(center + extents) 배열 StructuredBuffer, ViewProj 행렬 CBV
   - 처리:
     a. AABB 8개 코너를 NDC로 변환
     b. screen-space min/max (UV 공간) 계산
     c. 최적 Mip 레벨 계산: floor(log2(maxExtent_pixels))
     d. Hi-Z Mip 텍스처에서 해당 영역 depth 샘플링
     e. AABB 근거리 Z와 비교 → RWByteAddressBuffer에 결과(0/1) 기록
   - 출력 결과를 Readback Buffer로 복사, 1프레임 레이턴시로 CPU에서 읽는다.

4. OcclusionCuller P0 스텁을 Hi-Z 방식으로 교체한다.
   - src/Renderer/OcclusionCuller.h/.cpp를 수정한다.
   - IsOccluded()가 GPU Hi-Z 결과 버퍼의 값을 반환하도록 구현한다.
   - occlusionCulledNodes 통계를 CullStats에 반영하고 DebugHUD에 표시한다.
   - Optimization 메뉴 항목을 추가한다:
     · Win32Menu에 ID_OPTIM_OCCLUSION_CULL = 8004 추가
     · "Occlusion Culling" 체크 토글 항목 (Optimization 메뉴)
     · Engine 콜백 → Renderer::SetOcclusionCullingEnabled(bool) 연결

5. 성능을 검증한다.
   - Sponza 씬에서 Hi-Z Occlusion Culling 활성화 시 드로우콜 수 감소 확인
   - DebugHUD에서 occlusionCulledNodes 수치 및 FPS 개선 확인
   - CPU readback 방식 대비 GPU stall 감소 확인

빌드하여 모든 테스트가 통과하고, Hi-Z Occlusion Culling이 Sponza에서 정상 동작하는지 확인하라.
```

---

## Prompt 34: Point Light Cube Map Shadowing

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 34 섹션을 참조하여 Phase 34를 구현하라.
이 단계는 castShadow = true인 Point Light에 대해 Omnidirectional Shadow Map(TextureCube)을 구현한다.

1. TextureCube D3D12 리소스를 생성한다.
   - D3D12Context에 Point light 전용 Cube Shadow Map 리소스를 추가한다.
     · ID3D12Resource: TEXTURE2D_ARRAY (ArraySize=6, DXGI_FORMAT_D32_FLOAT)
     · 각 면에 대해 DSV 6개 (depth write용) 생성
     · SRV 1개 (TextureCube로 전체 6면 샘플링) 생성
     · 최대 MAX_POINT_SHADOW_LIGHTS = 4개 Point light shadow 지원
   - CreateCubeShadowMaps(), RecreateCubeShadowMaps() 메서드 추가

2. 6-pass Shadow Depth 렌더링을 구현한다.
   - Renderer::RenderScene()의 Shadow Depth Pass 루프를 확장한다.
   - Point light 분기에서 기존 'continue' 스킵을 제거하고 6-pass를 실행한다.
     · 6면 방향: +X(-Z up), -X(-Z up), +Y(-X up), -Y(+X up), +Z(-Z up), -Z(-Z up)
       (D3D12 TextureCube 면 순서: +X, -X, +Y, -Y, +Z, -Z)
     · 각 면 View 행렬: XMMatrixLookAtLH(lightPos, lightPos+faceDir, faceUp)
     · Projection: XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, nearPlane, farPlane)
       (XM_PIDIV2 = 90°, aspect=1.0, farPlane = m_sceneDiagonal * 3.0f)
   - D3D12Context::BeginShadowPass(shadowIdx, faceIndex)와
     EndShadowPass(shadowIdx, faceIndex) 오버로드 또는 별도 BeginCubeShadowPass() 추가

3. HLSL PBR.hlsl을 확장한다.
   - TextureCube 바인딩 추가:
     TextureCube PointShadowMap0 : register(t13);
     TextureCube PointShadowMap1 : register(t14);
     TextureCube PointShadowMap2 : register(t15);
     TextureCube PointShadowMap3 : register(t16);
   - SamplePointShadow(uint idx, float3 lightToPixel, float depth) 함수 구현:
     · lightToPixel = normalize(pixelWorldPos - lightPos)
     · TextureCube에서 SampleCmpLevelZero 또는 Sample + 수동 depth 비교
     · depth = length(pixelWorldPos - lightPos) / farPlane (정규화)
   - CalcShadow()를 Point light 타입에서 SamplePointShadow()를 호출하도록 분기

4. LightConstants와 LightData를 확장한다.
   - Light.h: shadowType 필드 추가 (0=Texture2D, 1=TextureCube)
   - LightManager::BuildLightConstants(): Point light castShadow에 pointShadowIdx 할당
   - HLSL LightData 구조체: shadowType 필드 추가 (uint)

5. Root Signature를 확장한다.
   - D3D12Context의 Root Signature에 t13~t16 SRV 슬롯 추가
   - 라이팅 패스에서 Cube Shadow Map SRV 바인딩

6. 성능을 관리한다.
   - 최대 4개 Point light shadow 허용 (6pass × 4 = 24 depth pass/frame)
   - LightCuller와 연동: shadow casting Point light도 거리 기반 culling 적용
   - DebugHUD에 Cube Shadow Pass 수 표시

빌드하여 모든 테스트가 통과하고, Sponza 씬에서 횃불 위치(castShadow=true Point light)의
구면 그림자가 정상 렌더링되는지 확인하라.
```

---

## Prompt 35: Skeletal Animation

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 35 섹션을 참조하여 Phase 35를 구현하라.
Part A(Node Transform Animation)를 먼저 완성한 뒤 Part B(Skeletal Animation)를 구현한다.

=== Part A: Node Transform Animation (G-08) ===

1. Animation 데이터 구조를 만든다.
   - src/Asset/Animation.h를 신규 생성한다.
     · struct Keyframe<T> { float time; T value; }
     · struct AnimationChannel {
           SceneNode* targetNode;
           enum Property { Translation, Rotation, Scale } property;
           std::vector<Keyframe<XMFLOAT3>> posKeys;   // Translation/Scale
           std::vector<Keyframe<XMFLOAT4>> rotKeys;   // Rotation (quaternion)
           enum Interpolation { Linear, Step, CubicSpline } interpolation;
       }
     · struct AnimationClip { std::string name; float duration; std::vector<AnimationChannel> channels; }
   - 보간 함수 구현:
     · Linear: XMVectorLerp / XMQuaternionSlerp
     · Step: 현재 시간 이하의 마지막 키프레임 값 반환
     · CubicSpline: glTF cubic spline 공식 적용 (in-tangent, value, out-tangent 삼중 구조)

2. SceneLoader에 애니메이션 로딩을 추가한다.
   - SceneLoader::LoadAnimations(const aiScene*, SceneGraph*) 메서드 신규 추가
   - aiAnimation → AnimationClip 변환:
     · aiNodeAnim::mPositionKeys → Translation 채널
     · aiNodeAnim::mRotationKeys → Rotation 채널 (aiQuaternion → XMFLOAT4)
     · aiNodeAnim::mScalingKeys → Scale 채널
   - 채널 target name(aiNodeAnim::mNodeName) → SceneNode* 매핑
     (SceneGraph에서 name으로 노드 검색)
   - Engine::LoadScene() 에서 LoadAnimations() 호출

3. AnimationController를 구현한다.
   - src/Core/AnimationController.h/.cpp를 신규 생성한다.
   - AnimationController::Update(float dt):
     · m_currentTime += dt * m_playbackSpeed
     · 루프: m_currentTime >= clip.duration 시 0으로 리셋
     · 각 채널의 보간값 계산 → SceneNode의 Transform 갱신
       (SetLocalTranslation / SetLocalRotation / SetLocalScale 또는 SetLocalMatrix)
   - Play(), Pause(), SetClip(AnimationClip*), SetPlaybackSpeed(float) 메서드
   - Engine::Update()에서 AnimationController::Update(dt) 호출
   - "Animation" 메뉴에서 클립 선택 가능 (씬 로드 후 클립 목록 동적 생성)

=== Part B: Skeletal Animation (G-09) ===

4. Skeleton / Skin 데이터 구조를 만든다.
   - src/Asset/Skeleton.h를 신규 생성한다.
     · struct Bone { std::string name; int parentIndex; XMFLOAT4X4 inverseBindMatrix; }
     · struct Skeleton { std::vector<Bone> bones; int FindBone(const std::string& name) const; }
     · struct Skin { Skeleton* skeleton; std::vector<int> jointIndices; }
   - SceneLoader에서 aiMesh::mBones 배열을 순회하여 Skeleton/Skin 생성:
     · aiBone::mName → Bone.name
     · aiBone::mOffsetMatrix → Bone.inverseBindMatrix (Assimp 전치 주의)
     · aiBone::mWeights → per-vertex joint index + weight 저장
   - Mesh 구조체에 Skin* skin 포인터 추가

5. Vertex 포맷을 확장한다.
   - Vertex 구조체에 추가:
     · XMUINT4 joints  (JOINTS_0)  — 영향을 주는 본 인덱스 최대 4개
     · XMFLOAT4 weights (WEIGHTS_0) — 각 본의 가중치 (합=1.0)
   - D3D12 Input Layout에 슬롯 추가:
     · JOINTS_0:  R8G8B8A8_UINT,  offset = sizeof(이전 필드까지)
     · WEIGHTS_0: R32G32B32A32_FLOAT
   - HLSL VSInput 구조체에 동일 필드 추가
   - static_assert로 sizeof(Vertex) 및 각 멤버 오프셋 검증 갱신

6. GPU Skinning 셰이더를 구현한다.
   - PBR.hlsl에 Skinning 지원 추가:
     · cbuffer SkinCB : register(b4) { float4x4 jointMatrices[128]; uint jointCount; }
     · VSInput에 uint4 joints : JOINTS_0; float4 weights : WEIGHTS_0; 추가
     · VSMain 내 스키닝 계산:
       float4x4 skinMatrix =
           weights.x * jointMatrices[joints.x] +
           weights.y * jointMatrices[joints.y] +
           weights.z * jointMatrices[joints.z] +
           weights.w * jointMatrices[joints.w];
       float4 skinnedPos = mul(float4(input.position, 1.0f), skinMatrix);
       float4 worldPos = mul(skinnedPos, World);
       (Normal, Tangent도 동일 skin matrix로 변환)
     · 스킨 메시 여부: SkinCB의 jointCount > 0 이면 스키닝 적용
   - Renderer: 스킨 메시 드로우콜 전 SkinCB 바인딩 (jointCount=0이면 identity 바인딩)

7. AnimationController를 Part B 연동으로 확장한다.
   - Part A에서 각 채널이 SceneNode Transform을 갱신한 뒤,
     SceneGraph를 순회하여 bone world matrix 배열(joint palette)을 계산:
     · boneWorldMatrix[i] = parentBoneWorldMatrix * bone[i].localTRS
     · jointMatrix[i] = inverseBindMatrix[i] * boneWorldMatrix[i]
   - 계산된 jointMatrix 배열을 SkinCB Upload Buffer에 복사

빌드하여 CesiumMan.glb 또는 RiggedFigure.glb에서 노드 TRS 애니메이션과
스킨 메시 애니메이션이 정상 재생되는지 확인하라.
모든 유닛·스모크 테스트가 통과해야 한다.
```

---

## Prompt 36: RRScenePreprocessor 확장 — Skeletal Animation 지원

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 36 섹션과 GoodToPreprocess.md를 참조하여 Phase 36를 구현하라.
이 단계는 Phase 33에서 추가된 Skeleton/Skin/Animation 데이터를 .rrscene 포맷에 통합하여
전처리기와 렌더링 앱 고속 로딩 경로를 모두 확장한다.

1. .rrscene 포맷을 v2로 버전 업한다 (src/Asset/RRSceneFormat.h 수정).
   - Header.version: 1 → 2
   - Vertex 구조체에 joints(uint32×4), weights(float×4) 필드 추가
     · 스킨 메시: isSkinned 플래그 = true, 해당 필드 포함
     · 비스킨 메시: isSkinned = false, 해당 필드 생략 (파일 크기 절약)
   - Skeleton Section 추가 (SectionType::Skeleton):
     · uint32 boneCount
     · 본별: 이름(문자열), parentIndex(int32), inverseBindMatrix(float 4×4)
     · Skin 수, Skin별: skeletonIndex, jointIndices 배열
   - Animation Section 추가 (SectionType::Animation):
     · uint32 clipCount
     · 클립별: 이름, 재생 시간(float), 채널 수
     · 채널별: targetNodeIndex, Property(TRS enum), Interpolation enum,
               키프레임 수, 키프레임 배열(float time + float3/float4 value)
   - 하위 호환: version == 1 파일 로딩 시 Skeleton/Animation 섹션 미존재 → 관련 객체 미생성

2. RRScenePreprocessor를 확장한다.
   - Assimp aiMesh::mBones 순회:
     · aiBone::mName, mOffsetMatrix → Bone 생성 (Assimp 전치 주의)
     · aiBone::mWeights → per-vertex joint index + weight 기록
     · 스킨 메시 Vertex에 joints/weights 기록 후 Skeleton Section 직렬화
   - Assimp aiAnimation 순회:
     · aiNodeAnim::mPositionKeys → Translation 키프레임 (XMFLOAT3 + time)
     · aiNodeAnim::mRotationKeys → Rotation 키프레임 (XMFLOAT4 quaternion + time)
     · aiNodeAnim::mScalingKeys → Scale 키프레임 (XMFLOAT3 + time)
     · Interpolation: Assimp aiAnimBehaviour → Linear/Step/CubicSpline 매핑
     · target name → 노드 인덱스 매핑 후 Animation Section 직렬화
   - 버전 2 헤더와 전체 섹션 목록(Scene/Mesh/Material/Texture/Light/Skeleton/Animation) 기록

3. 렌더링 앱 고속 로딩 경로를 확장한다.
   - SceneLoader::LoadRRScene() 내 version 분기:
     · version == 2: Skeleton/Animation 섹션 파싱
       - Skeleton Section → Skeleton/Skin 객체 생성 → Mesh.skin 포인터 연결
       - Animation Section → AnimationClip 배열 생성 → AnimationController 등록
     · version == 1: 기존 로직 유지 (Skeleton/Animation 없이 로딩)
   - 스킨 Vertex(joints/weights 포함) → GPU VB 업로드 (Input Layout v2 사용)
   - AnimationController에 클립 자동 등록, 첫 번째 클립 자동 재생 시작

4. 동작을 검증한다.
   - CesiumMan.glb를 RRScenePreprocessor에 입력 → CesiumMan.rrscene(v2) 생성
   - 렌더링 앱에서 CesiumMan.gltf 열기 → v2 고속 경로로 스켈레탈 애니메이션 재생 확인
   - 비애니메이션 씬의 v1 .rrscene 파일도 계속 정상 로딩되는지 확인 (하위 호환)
   - v1과 v2 rrscene 모두 Assimp 표준 경로 렌더링 결과와 동일한지 비교

빌드하여 모든 테스트가 통과하고, CesiumMan.rrscene에서 스켈레탈 애니메이션이
표준 로딩 경로와 동일하게 재생되는지 확인하라. v1 파일 하위 호환을 유지해야 한다.
```

---

## Prompt 37: Deferred Rendering — G-Buffer 기반 렌더링 파이프라인

```
PRD.md, PLAN.md(Phase 03), CLAUDE.md를 참조하여 Phase 37을 구현하라.
기존 Forward Rendering 파이프라인을 Deferred Shading으로 전환한다.
Alpha Blend 오브젝트는 Forward 패스를 유지하는 Hybrid 구조를 적용한다.

1. G-Buffer MRT 생성 (D3D12Context):
   - RT0: R8G8B8A8_UNORM_SRGB — Albedo(RGB) + Metallic(A)
   - RT1: R16G16B16A16_FLOAT — World Normal(XYZ) + Roughness(A)
   - RT2: R8G8B8A8_UNORM — Emissive(RGB) + AO(A)
   - Depth: D32_FLOAT (SRV 겸용), OMSetRenderTargets MRT 바인딩

2. Geometry Pass: Opaque → G-Buffer Fill, Shadow Pass 선행, Alpha Mask clip() 적용

3. Lighting Pass: Full-Screen Quad, G-Buffer SRV + Shadow SRV 바인딩, Cook-Torrance BRDF, HDR RT 출력

4. Forward+ 투명 패스: Alpha Blend 메시는 기존 Forward로 HDR RT 합성

5. G-Buffer 디버그 뷰 (Render 메뉴): Albedo / Normal / MetalRoughness / Depth 시각화

빌드하여 G-Buffer MRT, Deferred Lighting Pass, Alpha Blend 합성을 확인하라.
```

---

## Prompt 38: HDR Pipeline + Tone Mapping

```
PRD.md, PLAN.md(Phase 38), CLAUDE.md를 참조하여 Phase 38을 구현하라.

1. HDR Render Target: DXGI_FORMAT_R16G16B16A16_FLOAT (Lighting Pass 출력)
2. Tone Mapping Pass: Reinhard 또는 ACES Filmic — Render 메뉴 선택
3. Auto-Exposure: Compute Shader로 평균 Luminance → EV 자동 조절
4. sRGB 출력: R8G8B8A8_UNORM_SRGB SwapChain
5. DebugHUD: Tone Mapping 모드, Luminance, EV 표시

빌드하여 HDR RT, Tonemapping 전환, Auto-Exposure를 확인하라.
```

---

## Prompt 39: SSAO (Screen Space Ambient Occlusion)

```
PRD.md, PLAN.md(Phase 39), CLAUDE.md를 참조하여 Phase 39를 구현하라.

1. SSAO Buffer: R8_UNORM 렌더 타겟
2. SSAO Pass: Hemisphere Sample Kernel(16~64개) + 노이즈 텍스처 랜덤화
   - Depth → View-Space Position, G-Buffer Normal → View-Space
   - 반구형 샘플로 주변 깊이 비교 → Raw AO
3. Blur Pass: Bilateral Blur (Depth/Normal 경계 보존), 수평→수직 2패스
4. Lighting Pass 통합: AO × Ambient Light
5. Optimization 메뉴: SSAO on/off, AO Buffer 시각화

빌드하여 SSAO Buffer, Blur, Lighting 통합, on/off 비교를 확인하라.
```

---

## Prompt 40: Bloom + Post-Processing 파이프라인

```
PRD.md, PLAN.md(Phase 40), CLAUDE.md를 참조하여 Phase 40을 구현하라.

1. Ping-Pong Buffer 프레임워크: PostProcessor 클래스, HDR RT 2개 교대
2. Bright Pass: Luminance 임계값 이상 픽셀 추출
3. Gaussian Blur Pyramid: 6단계 다운샘플→업샘플 (Dual Kawase Blur)
4. Bloom Composite: Additive Blend
5. 파이프라인 순서: Lighting → SSAO → Bloom → Tone Mapping → TAA → sRGB
6. 메뉴: Bloom on/off, Threshold, Intensity

빌드하여 Bloom 효과, Post-Processing 프레임워크를 확인하라.
```

---

## Prompt 41: TAA (Temporal Anti-Aliasing)

```
PRD.md, PLAN.md(Phase 41), CLAUDE.md를 참조하여 Phase 41을 구현하라.

1. Jitter Matrix: 8~16프레임 Halton Sequence로 투영 행렬 서브픽셀 오프셋
2. Motion Vector Buffer: R16G16_FLOAT, 정적(카메라)/동적(WorldMatrix) Reprojection
3. History Buffer: 이전 프레임 TAA 출력 SRV
4. TAA Resolve: Current + History 블렌딩(α≈0.1~0.15)
   - Variance Clipping(3×3 AABB clip), Velocity 기반 가중치 감소
5. 메뉴: TAA / MSAA / None

빌드하여 TAA on/off, 고스팅 억제, 정적 씬 품질을 확인하라.
```

---

## Prompt 42: Motion Blur + Depth of Field

```
PRD.md, PLAN.md(Phase 42), CLAUDE.md를 참조하여 Phase 42를 구현하라.

1. Motion Blur: Tile-based Max Velocity (Compute) → 속도 방향 N샘플 평균, 셔터 속도 스케일
2. Depth of Field:
   - CoC: Depth → CoC 반경 (Focus Distance, F-Number)
   - Bokeh Blur: Separable Gaussian 또는 Hexagonal Bokeh, Near/Far 분리
3. Camera 메뉴: F-Number, Focal Length, Focus Distance, Motion Blur/DoF on/off

빌드하여 Motion Blur per-object, DoF CoC 블러, 메뉴 파라미터를 확인하라.
```

---

## Prompt 43: SSR (Screen Space Reflections) + Refraction

```
PRD.md, PLAN.md(Phase 43), CLAUDE.md를 참조하여 Phase 43을 구현하라.

1. SSR: G-Buffer Normal+Depth → 반사 Ray, Hi-Z Raymarching, Fresnel, Roughness 블러, Envmap Fallback
2. Refraction: Alpha Blend 오브젝트에 IOR 기반 UV 오프셋, Depth 비교로 penetration 방지
3. Material: IOR 파라미터 추가
4. Optimization 메뉴: SSR on/off

빌드하여 SSR 반사, Roughness 블러, Fresnel, Refraction을 확인하라.
```

---

## Prompt 44: Screen Space Subsurface Scattering (SSSSS)

```
PRD.md, PLAN.md(Phase 44), CLAUDE.md를 참조하여 Phase 44를 구현하라.

1. Material 확장: subsurfaceColor(XMFLOAT3) + scatterWidth(float)
2. SSS Pass: Stencil 마스크, 6-weight Gaussian × 3채널(R>G>B 확산 폭), 수평→수직 2패스
3. Lighting Pass 통합: SSS 결과를 Diffuse에 합성
4. Optimization 메뉴: SSSSS on/off, scatterWidth 조정

빌드하여 SSS on/off, RGB 채널 확산 폭, Stencil 마스크를 확인하라.
```

---

## Prompt 45: Global Illumination — DDGI (Dynamic Diffuse GI)

```
PRD.md, PLAN.md(Phase 45), CLAUDE.md를 참조하여 Phase 45를 구현하라.

1. Probe Grid: 씬 AABB 내 3D Grid (8×4×8=256 Probe), Octahedral Map 텍스처
2. Probe Update: DXR 가능 시 Radiance Ray, 미지원 시 정적 Reflection Capture Fallback
3. Probe Sampling: 삼선형 보간, SH2 Irradiance 샘플링
4. Lighting Pass: Indirect Diffuse += Probe Irradiance × Albedo / π
5. 디버그 뷰: Probe 위치·Irradiance 시각화

빌드하여 Probe 배치, 간접광 표현, 디버그 시각화를 확인하라.
```

---

## Prompt 46: DXR Hybrid Ray Tracing

```
PRD.md, PLAN.md(Phase 46), CLAUDE.md를 참조하여 Phase 46을 구현하라.
DXR Tier 1.1 미지원 시 PCF Shadow/SSR로 자동 폴백해야 한다.

1. DXR 인프라: Feature 감지, DXR PSO(RayGen/ClosestHit/Miss/AnyHit), BLAS(정적/동적), TLAS(매 프레임), ShaderTable
2. Ray-Traced Shadow: 광원별 Shadow Ray, Alpha AnyHit, PCF 대체 (메뉴 토글)
3. Ray-Traced Reflection: Normal+Roughness → Cone Sampling, 재귀 1~2레벨
4. GI 연동: DDGI Probe Update에 DXR Ray 활용
5. Denoiser 연동: Phase 48 Denoiser 또는 Temporal Accumulation
6. 폴백: DXR 미지원 시 PCF/SSR/DDGI Static

빌드하여 TLAS/BLAS, RT Shadow, RT Reflection, Hybrid 전환을 확인하라.
```

---

## Prompt 47: Nanite-style Virtual Geometry

```
PRD.md, PLAN.md(Phase 47), CLAUDE.md를 참조하여 Phase 47을 구현하라.
Mesh Shader 미지원 시 기존 DrawIndexedInstanced + LODSelector로 폴백해야 한다.

1. Meshlet 분할: ~128 삼각형, 바운딩 스피어 + 노말 Cone
2. Mesh Shader 파이프라인: Amplification(Frustum/Back-face Culling) + Mesh Shader(삼각형 출력)
3. Cluster LOD Hierarchy: 심플리피케이션으로 LOD 트리, GPU Projected Error 기준 전환
4. GPU-Driven: Compute → DrawArgs Buffer → ExecuteIndirect()
5. 디버그 뷰: Meshlet 색상, LOD 레벨 시각화
6. 폴백: Mesh Shader 미지원 시 기존 DrawIndexedInstanced

빌드하여 Meshlet 시각화, Amplification+Mesh Shader, GPU-Driven IndirectDraw를 확인하라.
```

---

## Prompt 48: Neural Upscaling (DLSS/FSR) + Neural Denoising

```
PRD.md, PLAN.md(Phase 48), CLAUDE.md를 참조하여 Phase 48을 구현하라.

1. FSR 3: FidelityFX SDK 연동, Color+Depth+MotionVector → 업스케일, Quality Mode 메뉴
2. DLSS 3 (선택): Streamline SDK, RTX 감지, 미지원 시 FSR 폴백
3. Neural Denoising: NRD SDK 또는 자체 Temporal Accumulation Denoiser
4. DebugHUD: Upscaling 모드, 렌더/출력 해상도, Denoiser 종류

빌드하여 FSR 업스케일, Quality Mode 전환, Denoiser를 확인하라.
```

---

## Prompt 49: Phase 03 코드 리뷰, 최적화, 버그 수정 & 아키텍처 문서화

```
PRD.md, PLAN.md(Phase 49), CLAUDE.md를 참조하여 Phase 49를 수행하라.
Phase 33~48에서 추가된 모든 고급 렌더링 기법의 코드 품질을 점검하고,
성능을 최적화하며, 버그를 수정하고, ARCHITECTURE.md를 완성한다.

1. 코드 리뷰를 수행한다.
   - Dead code 제거, include 순서 정리, 네이밍 일관성 검증
   - G-Buffer MRT 바인딩 순서 및 포맷 일관성 확인
   - DXR ShaderTable 빌드 로직, BLAS/TLAS 갱신 주기 검토
   - Mesh Shader / Amplification Shader 경계 조건 검토
   - Neural Upscaling SDK 연동 초기화 순서 확인
   - D3D12 Debug Layer 경고 0건 목표 (리소스 상태 전이, lifetime 위반 등)

2. 성능 최적화를 수행한다.
   - PIX for Windows 또는 D3D12 Timestamp Query로 각 렌더 패스 비용 측정
   - G-Buffer 포맷 최적화 (RT1: R10G10B10A2 축소 검토)
   - SSAO 샘플 수 / TAA 블렌딩 계수 / Bloom 피라미드 단계 수 튜닝
   - Hi-Z Mip chain 생성 비용 측정 및 최적화
   - DXR TLAS Refit (정적 BLAS 재사용, 동적만 Rebuild)
   - Nanite Meshlet 크기 및 LOD Projected Error 임계값 튜닝
   - Denoiser Temporal 수렴 속도 vs 고스팅 트레이드오프 조정

3. 버그를 수정한다.
   - 렌더 패스 간 리소스 상태 전이 누락 (D3D12_RESOURCE_STATE_*)
   - TAA 씬 전환 직후 History Buffer 초기화 누락
   - SSR 화면 경계 아티팩트 (Fade 파라미터 튜닝)
   - DDGI Probe Irradiance 튀는 현상 (Hysteresis 파라미터)
   - DXR AnyHit 투명 오브젝트 투과율 계산 오류
   - FSR/DLSS Motion Vector 스케일 불일치

4. ARCHITECTURE.md를 완성한다.
   - 전체 렌더 파이프라인 다이어그램 (Phase 01~48 누적 아키텍처)
   - 렌더 패스 순서 및 리소스 의존성 다이어그램
     (Shadow → G-Buffer → Lighting → SSAO → SSR → Bloom → TAA → Tone Mapping → Upscale)
   - 주요 모듈 간 의존성 (Engine / Renderer / SceneGraph / RHI / Asset / Lighting)
   - G-Buffer 레이아웃, Descriptor Heap 구조, Root Signature 레지스터 맵
   - DXR 가속 구조 (BLAS/TLAS) 업데이트 주기 및 ShaderTable 구성
   - Meshlet / GPU-Driven 렌더링 흐름 (Compute → DrawArgs → ExecuteIndirect)
   - Neural Upscaling 렌더 해상도 관리 흐름
   - 스레딩 모델: 메인 렌더 스레드 / Compute Queue / Copy Queue / Worker Thread

5. 최종 벤치마크를 수행한다.
   - Sponza + Bistro: Full Phase 03 파이프라인(Deferred + SSAO + Bloom + TAA + SSR + DDGI) 60fps 목표
   - DXR 활성 시 RT Shadow + RT Reflection 포함 성능 측정
   - FSR 3 활성 시 (렌더 해상도 67%) 품질 vs 성능 비교
   - 모든 유닛 테스트 + 스모크 테스트 통과 확인

D3D12 Debug Layer 경고 0건, 주요 패스 타임스탬프 측정 완료,
ARCHITECTURE.md 작성 완료, Sponza+Bistro 벤치마크 결과 기록 후 Phase 03 완료 선언.
```
