# PROMPT: 단계별 구현 프롬프트

> Phase 01, 02 (Prompt 1~31)는 구현 완료. 아카이브: `PROMPT_Phase01-02_Archive.md`

---

## Prompt 32: Occlusion Culling — Hi-Z GPU ✅

```
PRD.md, PLAN.md(Phase 32), CLAUDE.md를 참조하여 Phase 32를 구현하라.
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
     · Win32Menu에 ID_OPTIM_OCCLUSION_CULL = 8005 추가 (8004는 MipMap 토글에서 사용 중)
     · "Occlusion Culling" 체크 토글 항목 (Optimization 메뉴)
     · Engine 콜백 → Renderer::SetOcclusionCullingEnabled(bool) 연결

5. 성능을 검증한다.
   - Sponza 씬에서 Hi-Z Occlusion Culling 활성화 시 드로우콜 수 감소 확인
   - DebugHUD에서 occlusionCulledNodes 수치 및 FPS 개선 확인
   - CPU readback 방식 대비 GPU stall 감소 확인

빌드하여 모든 테스트가 통과하고, Hi-Z Occlusion Culling이 Sponza에서 정상 동작하는지 확인하라.
```

---

## Prompt 33: Shadow Quality — Cube Map Shadow + CSM + PCSS (전부 완료 ✅)

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 33 섹션을 참조하여 Phase 33을 구현하라.
이 단계는 세 가지 그림자 품질 개선을 함께 구현한다:
Part A — Point Light Cube Map Shadow, Part B — CSM, Part C — PCSS.

=== Part A: Point Light Cube Map Shadow ===

※ Part A 작업 시 아래 두 항목도 함께 처리한다 (Part C 미완 사항):
  A-추가1. PCSS Blocker Search 반경을 perspective-aware 공식으로 교체한다.
           searchWidth = lightSize × (receiver_depth - nearPlane) / receiver_depth
           (현재 고정값 lightSize * shadowTexelSize → Point light perspective에 맞게 개선)
  A-추가2. PCSS 기본값을 ON으로 변경한다.
           (Point light shadow 추가 후 contact hardening 효과가 명확해지므로)

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

=== Part B: CSM (Cascaded Shadow Maps) ===

7. Cascade Frustum을 분할한다.
   - 카메라 Frustum을 N=3 구간으로 분할한다.
   - Practical Split Scheme 적용: split_i = λ·log_split + (1-λ)·uniform_split (λ=0.5)
     · log_split_i     = near × (far/near)^(i/N)
     · uniform_split_i = near + (far-near) × (i/N)
   - 각 cascade에 m_shadowMaps[0~2] 할당 (기존 Directional Shadow Map 슬롯 재활용)
   - m_shadowMaps[3~]: Spot/Point Cube Map 용도 유지

8. Cascade별 Shadow Depth Pass를 구현한다.
   - Directional Light 1개당 cascade 3 pass 실행 (기존 1 pass → 3 pass로 확장)
   - 각 cascade의 Ortho 범위:
     · cascade frustum 코너 8개를 광원 뷰 공간으로 변환 → AABB min/max 계산
     · Ortho width/height = AABB 범위, near/far = AABB Z 범위 + slack
   - ShadowConstants.lightViewProj[0~2]에 cascade별 lightViewProj 저장
   - ShadowCB(b3)에 cascade split depth 배열 추가: float cascadeSplits[3] (view-space Z 경계값)

9. HLSL PBR.hlsl에 Cascade 선택 로직을 추가한다.
   - 픽셀의 view-space depth를 cascadeSplits[0~2]와 비교하여 cascade 인덱스 결정
   - 해당 cascade의 lightViewProj로 shadow UV 계산 후 m_shadowMaps[cascadeIdx] 샘플링
   - Blend band (cascade 경계 10% 구간): 인접 cascade 간 PCF 비율 보간으로 경계선 제거
   - 디버그 뷰: Render 메뉴 옵션으로 cascade 색상 시각화 (0=적, 1=녹, 2=청)

=== Part C: PCSS (Percentage Closer Soft Shadows) ===

10. Blocker Search를 구현한다.
    - 픽셀의 shadow UV 주변을 searchWidth 반경으로 샘플링한다.
      searchWidth = lightSize × (receiver_depth - cascadeNear) / receiver_depth
    - 차폐 텍셀(shadow map depth < receiver_depth)의 평균 depth(d_blocker) 계산
    - 차폐 텍셀이 없으면 shadow factor = 1.0으로 early-out

11. Penumbra Width를 계산하고 가변 커널 PCF를 수행한다.
    - penumbraWidth = (receiver_depth - d_blocker) / d_blocker × lightSize
      lightSize 기본값: sceneDiagonal × 0.02
    - penumbraWidth에 비례하는 반경으로 PCF 수행:
      · 16~32개 Poisson Disk 샘플 (블루노이즈 회전: noise texture 또는 frame counter 기반)
      · 최소 반경: PCF 3×3 기존 텍셀 오프셋 크기, 최대: shadow texel 9개 반경
    - CSM 각 cascade에 동일 PCSS 로직 적용

12. PCSS 성능 제어를 추가한다.
    - Optimization 메뉴에 "PCSS" on/off 토글 추가 (ID_OPTIM_PCSS 신규 정의)
      off 시: 기존 PCF 3×3 고정 커널 폴백
    - DebugHUD에 Shadow Mode 표시 (PCF 3×3 / PCSS)
    - lightSize 파라미터를 Optimization 메뉴에 Small(0.5×) / Normal(1.0×) / Large(2.0×) 프리셋으로 노출
      (별도 Shadow 메뉴 대신 Optimization 메뉴에 통합)

빌드하여 모든 테스트가 통과하고 다음을 확인하라:
- Sponza: 횃불 위치(castShadow=true Point light) 구면 그림자 정상 렌더링
- Bistro: CSM cascade 색상 디버그 뷰에서 3단계 구분 확인, 원거리 그림자 품질 개선
- PCSS on/off 전환 시 접촉 경화(Contact Hardening) 그림자 차이 확인
```

---

## Prompt 33D: Spot Light Shadow ✅

```
PRD.md, PLAN.md(Phase 33 Part D), CLAUDE.md를 참조하여 Phase 33 Part D를 구현하라.
이 단계는 castShadow=true인 Spot Light에 대해 Perspective 투영 기반 Shadow Map을 구현한다.
기존 Directional/Point Shadow 인프라를 최대한 재사용한다.

1. Spot Light Shadow Depth Pass를 구현한다.
   - castShadow=true인 Spot Light에 대해 기존 Texture2D Shadow Map 슬롯을 할당한다.
   - Projection: XMMatrixPerspectiveFovLH(outerConeAngle * 2, 1.0f, nearPlane, farPlane)
   - View 행렬: XMMatrixLookAtLH(lightPos, lightPos + lightDir, upVector)
   - 기존 BeginShadowPass / DrawShadowDepth / EndShadowPass 패턴을 재사용한다.

2. LightManager 연동을 확인한다.
   - Spot Light castShadow=true 시 shadowMapIndex가 올바르게 할당되는지 확인한다.
   - BuildLightConstants()에서 Spot Light의 lightViewProj 행렬이 GPU에 전달되는지 확인한다.

3. HLSL(PBR.hlsl)을 확인한다.
   - Spot Light는 shadowType==0 (Texture2D) 경로를 사용하므로 기존 PCF/PCSS가 그대로 적용된다.
   - 별도 HLSL 수정 없이 동작하는지 확인한다.

빌드하여 Spot Light castShadow=true 설정 시 그림자가 정상 렌더링되는지 확인하라.
```

---

## Prompt 34: Skeletal Animation

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 34 섹션을 참조하여 Phase 34를 구현하라.
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

## Prompt 34 Part C: Yup2Zup 애니메이션 쿼터니언 좌표계 버그 수정

```
PLAN.md의 Phase 34 Part C를 참조하여 Yup2Zup 애니메이션 쿼터니언 버그를 수정하라.

=== 버그 요약 ===

aiProcess_ConvertToLeftHanded 사용 시 Yup2Zup 루트 노드를 포함한 glTF 모델에서
자식 노드 애니메이션 쿼터니언의 회전축이 월드 공간에서 틀어지는 버그.

재현 모델: assets/test-models/CesiumMilkTruck/CesiumMilkTruck.glb
- Yup2Zup 루트 노드 회전: {x=0.5, y=-0.5, z=0.5, w=0.5}
- 바퀴 회전 채널이 올바른 축 대신 틀어진 축으로 재생됨

CesiumMan.glb는 Yup2Zup 노드가 없어 영향 없음 — 회귀 기준 모델로 사용.

=== 수정 범위 ===

src/Asset/SceneLoader.cpp — LoadAnimations() 내부

1. 채널 target 노드에서 루트 방향으로 계층을 순회하여 Yup2Zup 패턴 노드를 감지한다.
   - 노드 이름이 "Yup2Zup"이거나, 회전값이 {±0.5, ±0.5, ±0.5, ±0.5} 패턴인 경우 감지.

2. 감지된 경우 해당 자식 계층에 속하는 AnimationChannel의 rotation 키프레임에
   역보정 쿼터니언을 적용한다.
   - ConvertToLeftHanded가 Assimp 내부에서 쿼터니언에 어떤 변환을 적용하는지
     (Z분량 부호 반전 여부 등) Assimp 소스를 확인한 뒤 올바른 역보정 수식을 도출한다.

3. translation / scale 키프레임의 Z 부호 처리 필요 여부도 함께 검토하여 수정한다.

4. 기존 모델(DamagedHelmet, CesiumMan, Sponza 등)에서 회귀가 없음을 확인한다.

=== 테스트 ===

- CesiumMilkTruck.glb를 로드하여 바퀴 애니메이션이 올바른 축으로 회전하는지 확인한다.
- CesiumMan.glb를 로드하여 기존 애니메이션이 그대로 정상 재생되는지 확인한다.
- 버그 재현 조건을 커버하는 유닛 테스트를 추가한다.
- 전체 유닛·스모크 테스트가 통과해야 한다.
```

---

## Prompt 35: Deferred Rendering — G-Buffer 기반 렌더링 파이프라인

```
PRD.md, PLAN.md(Phase 35), CLAUDE.md를 참조하여 Phase 35를 구현하라.
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

## Prompt 36: RRScenePreprocessor — 오프라인 씬 전처리 도구 + Deferred Rendering 통합 지원

```
PRD.md, PLAN.md, CLAUDE.md의 Phase 36 섹션을 참조하여 Phase 36을 구현하라.
Phase 35에서 확정된 Deferred Rendering G-Buffer 레이아웃과 PSO 구조를 기반으로,
glTF/GLB/FBX 씬을 엔진 전용 바이너리(.rrscene)로 저장하는 파이프라인을 구현한다.
PSO 타입 사전 분류, G-Buffer 채널 매핑 플래그, BC 포맷 텍스처 압축을 포함한다.

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
   - Mesh: 메시 수, 메시별(uint8 psoType, vertex 수, index 수, Vertex 배열 raw dump, Index 배열 raw dump, AABB, LOD 수, LOD별 vertex/index + 전환 거리)
   - Material: 재질 수, 재질별(PBR factor, AlphaMode, doubleSided, textureIndex 참조 5개, sRGB 플래그, uint16 gBufferChannelMask)
   - Texture: 텍스처 수, 텍스처별(width, height, mipLevels, DXGI_FORMAT(BC 포맷), 전체 Mip chain BC 압축 블록)
   - Light: 광원 수, 광원별(type, color, intensity, position, direction, Kc/Kl/Kq, innerCone, outerCone, castShadow, bsRadius)

2. 전처리 파이프라인을 공용 클래스로 구현한다 (src/Asset/ScenePreprocessor.h/.cpp).
   - static bool Generate(const std::string& sourcePath, const std::string& outputPath)
   - static std::future<bool> GenerateAsync(const std::string& sourcePath)
   내부 파이프라인:
   a. Assimp 파싱: aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded
   b. Vertex/Index 변환 + Tangent 생성 (없으면 Gram-Schmidt 재직교화) → 프리미티브 분리 → 메시별 AABB
   c. Auto-LOD (QEM Edge Collapse, LOD1=50%/LOD2=25%, 전환 거리 sceneDiagonal×2/×6)
   d. PSO 타입 사전 분류: Material.AlphaMode + Material.doubleSided → uint8 psoType
      · 0=DeferredOpaque_CullBack (Opaque/AlphaMask + doubleSided=false)
      · 1=DeferredOpaque_CullNone (Opaque/AlphaMask + doubleSided=true)
      · 2=ForwardBlend_CullBack  (AlphaBlend + doubleSided=false)
      · 3=ForwardBlend_CullNone  (AlphaBlend + doubleSided=true)
   e. G-Buffer 채널 매핑 플래그: Phase 35 G-Buffer 레이아웃 기준 비트마스크 계산
      · bit0: hasAlbedoMap(RT0 RGB), bit1: metallicFromMetalRoughTex(RT0 A)
      · bit2: hasNormalMap(RT1 RGB), bit3: roughnessFromMetalRoughTex(RT1 A)
      · bit4: hasEmissiveMap(RT2 RGB), bit5: hasOcclusionMap(RT2 A)
      → uint16 gBufferChannelMask로 Material Section에 저장
   f. BC 포맷 텍스처 압축 (DirectXTex):
      · Albedo/Emissive (sRGB): BC7
      · Normal map (RG only): BC5
      · MetallicRoughness/AO: BC4
      · 전체 Mip chain BC 블록으로 압축 후 저장
   g. 씬 구조 직렬화 (노드 계층, 씬 AABB, 카메라, Material, Light)
   h. 원자적 파일 쓰기: .rrscene.tmp 완성 후 최종 경로로 rename

3. CLI 도구 프로젝트를 추가한다 (RRScenePreprocessor, Console Application).
   - main(argc, argv): 입력 파일 경로 인수 → Generate() 호출
   - 출력: bin/Debug/RRScenePreprocessor.exe

4. 렌더링 앱에 이중 로딩 경로를 추가한다 (src/Asset/SceneLoader).
   - 고속 경로: .rrscene 발견 + sourceHash 일치 → 섹션 순서대로 객체 생성, psoType·gBufferChannelMask 직접 사용 → GPU 업로드(BC 텍스처 블록)만 수행
   - 표준 경로: .rrscene 없거나 해시 불일치 → Assimp 파싱 → 항목 5 실행
   - DebugHUD: "Fast (.rrscene)" / "Standard (Assimp)"

5. 표준 경로 로딩 후 백그라운드 자동 전처리를 구현한다 (Engine::LoadScene()).
   - Assimp 로딩 완료 직후: GenerateAsync(sourcePath) 호출, future를 m_preprocessFuture에 저장
   - DebugHUD: "Preprocessing scene..." 표시, 완료 후 "Sponza.rrscene saved" 콘솔 로그
   - 매 프레임 future 완료 여부 폴링 → 완료 시 m_preprocessFuture 초기화
   - 씬 교체 시 이전 future가 실행 중이면 detach 후 진행

6. 동작을 검증한다.
   - Sponza.gltf 첫 로딩: 표준 경로 + "Preprocessing scene..." 확인
   - 전처리 완료 후: Sponza.rrscene 생성 확인
   - 두 번째 로딩: 고속 경로(~90% 단축) 확인
   - 원본 파일 변경 후 로딩: 해시 불일치 → 표준 경로 폴백 + 재전처리 시작 확인
   - psoType·gBufferChannelMask가 G-Buffer Fill Pass에서 올바르게 참조됨 확인
   - BC 압축 텍스처 GPU 업로드 후 렌더링 결과 동일 확인

빌드하여 첫 로딩 시 백그라운드 자동 생성, 두 번째 로딩부터 고속 경로가 동작하는지 확인하라.
```

---

## Prompt 37: HDR Pipeline + Tone Mapping

```
PRD.md, PLAN.md(Phase 37), CLAUDE.md를 참조하여 Phase 37을 구현하라.

1. HDR Render Target: DXGI_FORMAT_R16G16B16A16_FLOAT (Lighting Pass 출력)
2. Tone Mapping Pass: Reinhard 또는 ACES Filmic — Render 메뉴 선택
3. Auto-Exposure: Compute Shader로 평균 Luminance → EV 자동 조절
4. sRGB 출력: R8G8B8A8_UNORM_SRGB SwapChain
5. DebugHUD: Tone Mapping 모드, Luminance, EV 표시

빌드하여 HDR RT, Tonemapping 전환, Auto-Exposure를 확인하라.
```

---

## Prompt 38: SSAO (Screen Space Ambient Occlusion)

```
PRD.md, PLAN.md(Phase 38), CLAUDE.md를 참조하여 Phase 38을 구현하라.

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

## Prompt 39: Bloom + Post-Processing 파이프라인

```
PRD.md, PLAN.md(Phase 39), CLAUDE.md를 참조하여 Phase 39를 구현하라.

1. Ping-Pong Buffer 프레임워크: PostProcessor 클래스, HDR RT 2개 교대
2. Bright Pass: Luminance 임계값 이상 픽셀 추출
3. Gaussian Blur Pyramid: 6단계 다운샘플→업샘플 (Dual Kawase Blur)
4. Bloom Composite: Additive Blend
5. 파이프라인 순서: Lighting → SSAO → Bloom → Tone Mapping → TAA → sRGB
6. 메뉴: Bloom on/off, Threshold, Intensity

빌드하여 Bloom 효과, Post-Processing 프레임워크를 확인하라.
```

---

## Prompt 40: TAA (Temporal Anti-Aliasing)

```
PRD.md, PLAN.md(Phase 40), CLAUDE.md를 참조하여 Phase 40을 구현하라.

1. Jitter Matrix: 8~16프레임 Halton Sequence로 투영 행렬 서브픽셀 오프셋
2. Motion Vector Buffer: R16G16_FLOAT, 정적(카메라)/동적(WorldMatrix) Reprojection
3. History Buffer: 이전 프레임 TAA 출력 SRV
4. TAA Resolve: Current + History 블렌딩(α≈0.1~0.15)
   - Variance Clipping(3×3 AABB clip), Velocity 기반 가중치 감소
5. 메뉴: TAA / MSAA / None

빌드하여 TAA on/off, 고스팅 억제, 정적 씬 품질을 확인하라.
```

---

## Prompt 41: Motion Blur + Depth of Field

```
PRD.md, PLAN.md(Phase 41), CLAUDE.md를 참조하여 Phase 41을 구현하라.

1. Motion Blur: Tile-based Max Velocity (Compute) → 속도 방향 N샘플 평균, 셔터 속도 스케일
2. Depth of Field:
   - CoC: Depth → CoC 반경 (Focus Distance, F-Number)
   - Bokeh Blur: Separable Gaussian 또는 Hexagonal Bokeh, Near/Far 분리
3. Camera 메뉴: F-Number, Focal Length, Focus Distance, Motion Blur/DoF on/off

빌드하여 Motion Blur per-object, DoF CoC 블러, 메뉴 파라미터를 확인하라.
```

---

## Prompt 42: SSR (Screen Space Reflections) + Refraction

```
PRD.md, PLAN.md(Phase 42), CLAUDE.md를 참조하여 Phase 42를 구현하라.

1. SSR: G-Buffer Normal+Depth → 반사 Ray, Hi-Z Raymarching, Fresnel, Roughness 블러, Envmap Fallback
2. Refraction: Alpha Blend 오브젝트에 IOR 기반 UV 오프셋, Depth 비교로 penetration 방지
3. Material: IOR 파라미터 추가
4. Optimization 메뉴: SSR on/off

빌드하여 SSR 반사, Roughness 블러, Fresnel, Refraction을 확인하라.
```

---

## Prompt 43: Screen Space Subsurface Scattering (SSSSS)

```
PRD.md, PLAN.md(Phase 43), CLAUDE.md를 참조하여 Phase 43을 구현하라.

1. Material 확장: subsurfaceColor(XMFLOAT3) + scatterWidth(float)
2. SSS Pass: Stencil 마스크, 6-weight Gaussian × 3채널(R>G>B 확산 폭), 수평→수직 2패스
3. Lighting Pass 통합: SSS 결과를 Diffuse에 합성
4. Optimization 메뉴: SSSSS on/off, scatterWidth 조정

빌드하여 SSS on/off, RGB 채널 확산 폭, Stencil 마스크를 확인하라.
```

---

## Prompt 44: Global Illumination — DDGI (Dynamic Diffuse GI)

```
PRD.md, PLAN.md(Phase 44), CLAUDE.md를 참조하여 Phase 44를 구현하라.

1. Probe Grid: 씬 AABB 내 3D Grid (8×4×8=256 Probe), Octahedral Map 텍스처
2. Probe Update: DXR 가능 시 Radiance Ray, 미지원 시 정적 Reflection Capture Fallback
3. Probe Sampling: 삼선형 보간, SH2 Irradiance 샘플링
4. Lighting Pass: Indirect Diffuse += Probe Irradiance × Albedo / π
5. 디버그 뷰: Probe 위치·Irradiance 시각화

빌드하여 Probe 배치, 간접광 표현, 디버그 시각화를 확인하라.
```

---

## Prompt 45: DXR Hybrid Ray Tracing

```
PRD.md, PLAN.md(Phase 45), CLAUDE.md를 참조하여 Phase 45를 구현하라.
DXR Tier 1.1 미지원 시 PCF Shadow/SSR로 자동 폴백해야 한다.

1. DXR 인프라: Feature 감지, DXR PSO(RayGen/ClosestHit/Miss/AnyHit), BLAS(정적/동적), TLAS(매 프레임), ShaderTable
2. Ray-Traced Shadow: 광원별 Shadow Ray, Alpha AnyHit, PCF 대체 (메뉴 토글)
3. Ray-Traced Reflection: Normal+Roughness → Cone Sampling, 재귀 1~2레벨
4. GI 연동: DDGI Probe Update에 DXR Ray 활용
5. Denoiser 연동: Phase 47 Denoiser 또는 Temporal Accumulation
6. 폴백: DXR 미지원 시 PCF/SSR/DDGI Static

빌드하여 TLAS/BLAS, RT Shadow, RT Reflection, Hybrid 전환을 확인하라.
```

---

## Prompt 46: Nanite-style Virtual Geometry

```
PRD.md, PLAN.md(Phase 46), CLAUDE.md를 참조하여 Phase 46을 구현하라.
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

## Prompt 47: Neural Upscaling (DLSS/FSR) + Neural Denoising

```
PRD.md, PLAN.md(Phase 47), CLAUDE.md를 참조하여 Phase 47을 구현하라.

1. FSR 3: FidelityFX SDK 연동, Color+Depth+MotionVector → 업스케일, Quality Mode 메뉴
2. DLSS 3 (선택): Streamline SDK, RTX 감지, 미지원 시 FSR 폴백
3. Neural Denoising: NRD SDK 또는 자체 Temporal Accumulation Denoiser
4. DebugHUD: Upscaling 모드, 렌더/출력 해상도, Denoiser 종류

빌드하여 FSR 업스케일, Quality Mode 전환, Denoiser를 확인하라.
```

---

## Prompt 48: Phase 03 코드 리뷰, 최적화, 버그 수정 & 아키텍처 문서화

```
PRD.md, PLAN.md(Phase 48), CLAUDE.md를 참조하여 Phase 48을 수행하라.
Phase 32~47에서 추가된 모든 고급 렌더링 기법의 코드 품질을 점검하고,
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
   - 전체 렌더 파이프라인 다이어그램 (Phase 01~47 누적 아키텍처)
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
