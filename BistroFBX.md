# Bistro FBX 로딩 이슈 및 대응 방안

Amazon Lumberyard Bistro (NVIDIA ORCA 배포판) FBX 원본을 현재 구현된 SceneLoader로 로딩할 때
발생하는 이슈와 대응 방안을 정리한다.

> **결론**: FBX 원본 직접 로딩은 해결해야 할 이슈가 많다.
> **권장 대안**: glTF 변환본 사용 — `niagara_bistro` 또는 `bevy_bistro_scene` 참고.

---

## 이슈 목록

| # | 이슈 | 심각도 | 상태 |
|---|------|--------|------|
| 1 | DDS 텍스처 포맷 미지원 | **Blocker** | 미해결 |
| 2 | FBX 절대 경로 텍스처 참조 | Major | 미해결 |
| 3 | FBX 단위 스케일 미적용 | Major | 미해결 |
| 4 | Phong/Lambert → PBR 변환 없음 | Major | 미해결 |
| 5 | Exterior + Interior 분리 파일 구조 | Major | 미해결 |
| 6 | 씬 바운딩 박스 로컬 좌표 문제 | Major | 미해결 |
| 7 | 대용량 파일 로딩 성능 | Minor | 미해결 |

---

## 이슈 상세

### 이슈 1 — DDS 텍스처 포맷 미지원 (Blocker)

**현상:**
Bistro FBX의 텍스처 파일은 대부분 `.dds` 형식(BC1/BC3/BC5/BC7 블록 압축)이다.
현재 TextureLoader는 `stb_image`를 사용하는데, `stb_image`는 DDS 포맷을 지원하지 않는다.
텍스처 로딩이 전부 실패하여 폴백 텍스처(1×1 white)로만 렌더링된다.

**원인:**
- DDS는 GPU 네이티브 압축 포맷 (BCn 블록 압축)
- `stb_image`는 PNG/JPG/BMP/TGA 등의 비압축/범용 포맷만 지원
- BCn 압축 데이터를 CPU에서 디코딩하려면 별도 라이브러리 필요

**대응 방안:**
- **단기**: DDS 파일을 PNG/JPG로 일괄 변환 후 FBX 경로 수정 (texconv.exe 활용)
- **장기**: `DirectXTex` 라이브러리 통합 (Microsoft 공식, DDS + BCn 완전 지원)
  - `DDSTextureLoader12.h/.cpp`를 `src/Asset/`에 추가
  - TextureLoader에서 확장자가 `.dds`이면 DirectXTex 경로로 분기
  - vcpkg: `directxtex:x64-windows`

```cpp
// TextureLoader 분기 예시
if (ext == ".dds")
    return LoadDDS(path, device, commandList);   // DirectXTex
else
    return LoadSTB(path, device, commandList);   // stb_image
```

---

### 이슈 2 — FBX 절대 경로 텍스처 참조 (Major)

**현상:**
NVIDIA ORCA 배포 FBX 파일의 텍스처 경로가 원본 작업자의 로컬 절대 경로
(예: `C:\AmazonLumberyard\Projects\Bistro\...`)로 하드코딩되어 있다.
다른 환경에서 로딩 시 Assimp이 텍스처를 찾지 못하여 폴백 처리된다.

**원인:**
FBX 포맷은 텍스처 경로를 파일 내부에 임베딩하며, 절대/상대 경로 혼용이 발생한다.
Assimp은 절대 경로를 그대로 사용하여 파일 탐색을 시도하지만 존재하지 않으면 누락 처리한다.

**대응 방안:**
1. **경로 재매핑**: Assimp `IOSystem` 커스텀 구현으로 절대 경로 → 상대 경로 변환
2. **수동 재매핑**: SceneLoader에서 `aiMaterial::GetTexture()`로 얻은 경로를 `FBX 파일 디렉토리 + 파일명`으로 재조립
3. **Autodesk FBX SDK**: FBX 파일 자체를 수정하여 상대 경로로 변환 후 재저장

```cpp
// 경로 재매핑 예시 (SceneLoader)
std::string FixTexturePath(const std::string& embeddedPath, const std::string& fbxDir) {
    // 절대 경로 또는 접근 불가 시 파일명만 추출하여 fbxDir에서 탐색
    if (!std::filesystem::exists(embeddedPath)) {
        auto filename = std::filesystem::path(embeddedPath).filename();
        return (std::filesystem::path(fbxDir) / filename).string();
    }
    return embeddedPath;
}
```

---

### 이슈 3 — FBX 단위 스케일 미적용 (Major)

**현상:**
Bistro FBX의 기본 단위는 **센티미터(cm)**이다.
현재 SceneLoader는 단위 스케일을 고려하지 않으므로, 씬이 100배 크게 로딩된다.
- 카메라 Fit to Scene이 씬 크기에 비례해 지나치게 멀리 배치됨
- 이동 속도 자동 조절이 의도한 것보다 100배 빠르게 설정됨

**원인:**
Assimp은 `aiScene::mMetaData`에 `UnitScaleFactor`를 저장한다(cm이면 0.01).
현재 SceneLoader는 이 메타데이터를 읽지 않아 스케일 보정이 없다.

**대응 방안:**
SceneLoader에서 FBX 로딩 시 `UnitScaleFactor` 메타데이터를 읽어 루트 노드에 스케일 적용:

```cpp
// SceneLoader에서 FBX 단위 스케일 적용
double unitScale = 1.0;
if (scene->mMetaData) {
    scene->mMetaData->Get("UnitScaleFactor", unitScale);  // cm → 0.01, m → 1.0
}
// 루트 SceneNode의 스케일에 unitScale 반영
rootNode->GetTransform().SetScale({ (float)unitScale, (float)unitScale, (float)unitScale });
```

또는 Assimp 임포트 단계에서 `AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY`를 사용:

```cpp
importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 0.01f);  // cm → m
importer.ReadFile(path, flags | aiProcess_GlobalScale);
```

---

### 이슈 4 — Phong/Lambert → PBR 변환 없음 (Major)

**현상:**
Bistro FBX의 재질은 Phong 또는 Lambert 셰이딩 모델 기반이다.
현재 Material 시스템은 PBR(metallic/roughness) 파라미터만 사용하므로,
FBX의 Phong 파라미터(`Shininess`, `Diffuse`, `Specular` 등)를 PBR로 변환하는 로직이 없다.
결과적으로 metallic=0, roughness=1의 획일적인 마감으로 렌더링된다.

**원인:**
- glTF: `pbrMetallicRoughness` 구조체에 metallic/roughness가 직접 기술됨
- FBX: `AI_MATKEY_SHININESS`(Phong 지수), `AI_MATKEY_COLOR_DIFFUSE/SPECULAR`로 표현
- Assimp은 FBX Phong을 PBR로 자동 변환하지 않음

**대응 방안:**
SceneLoader의 Material 변환 로직에 Phong → PBR 근사 변환 추가:

```cpp
// Phong shininess → PBR roughness 변환 (Dimitar Lazarov 공식)
float shininess = 0.0f;
mat->Get(AI_MATKEY_SHININESS, shininess);
// shininess 범위: 0~1000 (Phong), PBR roughness: 0~1
float roughness = std::sqrt(2.0f / (shininess + 2.0f));
roughness = std::clamp(roughness, 0.0f, 1.0f);

// Phong specular 강도로 metallic 근사 (비금속이면 0, 금속이면 1)
aiColor3D specular;
mat->Get(AI_MATKEY_COLOR_SPECULAR, specular);
float specularIntensity = (specular.r + specular.g + specular.b) / 3.0f;
float metallic = std::clamp(specularIntensity - 0.04f, 0.0f, 1.0f);  // F0=0.04 비금속 기준

pbrMaterial->metallicFactor = metallic;
pbrMaterial->roughnessFactor = roughness;
```

**주의**: 이 변환은 근사값이므로 Bistro 원본의 비주얼을 완전히 재현하지 못한다.
정확한 재현이 필요하면 glTF 변환본 사용 권장.

---

### 이슈 5 — Exterior + Interior 분리 파일 구조 (Major)

**현상:**
NVIDIA ORCA 배포판의 Bistro는 두 개의 별도 FBX 파일로 분리되어 있다:
- `Bistro_Exterior.fbx` — 실외 건물 외관, 거리
- `Bistro_Interior.fbx` — 실내 인테리어

현재 SceneLoader는 단일 파일 로딩만 지원하므로, 두 파일을 동시에 로딩하여
하나의 SceneGraph로 합치는 기능이 없다.

**대응 방안:**
1. **임시**: 두 파일 중 하나만 로딩하여 별도 테스트
2. **장기**: SceneLoader에 `AppendScene(path)` 기능 추가 — 기존 SceneGraph에 새 파일의 노드를 병합
   - 충돌하는 노드 이름 처리 (prefix 추가)
   - 공유 Material/Texture 중복 방지 (TextureCache로 자연스럽게 해결됨)
3. **씬 설정 파일**: JSON/YAML 형식의 씬 디스크립터로 여러 파일을 하나의 씬으로 정의

```json
// 씬 디스크립터 예시 (향후 지원)
{
  "name": "Bistro",
  "files": [
    { "path": "Bistro_Exterior.fbx", "offset": [0, 0, 0] },
    { "path": "Bistro_Interior.fbx", "offset": [0, 0, 0] }
  ]
}
```

---

### 이슈 6 — 씬 바운딩 박스 로컬 좌표 문제 (Major)

**현상:**
SceneLoader에서 씬 바운딩 박스를 계산할 때, 각 노드의 Vertex 좌표를
월드 변환 없이 로컬 공간에서만 집계할 경우 바운딩 박스가 부정확하다.
FBX의 경우 노드 계층이 복잡하고 변환 행렬이 다양하므로 이 문제가 더 두드러진다.

**결과:**
- Camera Fit to Scene이 잘못된 위치에 카메라를 배치
- 이동 속도 자동 조절이 잘못된 스케일 기준으로 설정

**대응 방안:**
씬 바운딩 박스 계산 시 SceneGraph 순회로 월드 AABB를 수집:

```cpp
// 씬 AABB 수집 (SceneGraph 순회, 월드 공간)
BoundingBox sceneBounds;
bool first = true;
sceneGraph->Traverse([&](SceneNode* node) {
    if (!node->GetMesh()) return;
    BoundingBox worldAABB = node->GetWorldAABB();  // Phase 22에서 구현
    if (first) { sceneBounds = worldAABB; first = false; }
    else BoundingBox::CreateMerged(sceneBounds, sceneBounds, worldAABB);
});
```

> **참고**: Phase 22에서 `SceneNode::GetWorldAABB()`가 구현되면 이 문제가 자동으로 해결된다.

---

### 이슈 7 — 대용량 파일 로딩 성능 (Minor)

**현상:**
Bistro FBX 원본은 파일 크기가 약 1.8GB로, 로딩 중 수십 초 동안 UI가 응답하지 않는다.
현재 SceneLoader는 동기(블로킹) 방식으로 Assimp 파싱을 수행한다.

**대응 방안:**
1. **진행 상태 표시**: DebugHUD에 로딩 중 상태 메시지 출력 (`"Loading scene..."`)
2. **비동기 파싱**: 별도 스레드에서 Assimp 파싱 수행 (Assimp은 thread-safe하지 않으므로 단일 워커)
3. **점진적 로딩**: 노드 단위로 파싱 완료 시 즉시 렌더링 시작
4. **Assimp FBX 최적화**: `aiProcess_ImproveCacheLocality` 등 최적화 플래그 추가

```cpp
// 비동기 로딩 예시
m_loadFuture = std::async(std::launch::async, [this, path]() {
    return SceneLoader::LoadScene(path);  // 별도 스레드에서 파싱
});
// 메인 루프에서 완료 확인 후 SceneGraph 교체
```

---

## 권장 접근 전략

### 단기 (FBX 직접 로딩 목표)
이슈 해결 우선순위:
1. **이슈 3 (단위 스케일)**: 가장 간단, `AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY` 한 줄 추가
2. **이슈 2 (절대 경로)**: 텍스처 경로 재매핑 로직 추가
3. **이슈 4 (Phong→PBR)**: 변환 공식 적용
4. **이슈 1 (DDS)**: DirectXTex 통합 또는 텍스처 PNG 변환
5. **이슈 5 (분리 파일)**: 우선 Exterior만 테스트
6. **이슈 6 (바운딩 박스)**: Phase 22 완료 후 자동 해결

### 장기 (glTF 변환 사용 권장)
FBX 원본 대신 이미 변환된 glTF 버전을 사용하면 위 이슈 대부분이 해결된다:
- DDS → PNG/JPG (변환본에서 이미 처리)
- 경로 상대화 (glTF 표준)
- PBR 재질 (glTF pbr Metallic Roughness 네이티브)
- 단위 미터 기준

**추천 변환본:**
- `niagara_bistro`: 최적화된 glTF, 기하학 유지
- `bevy_bistro_scene`: 인스턴싱 최적화, 추가 최적화

---

## 참고 파일

| 파일 | 설명 |
|------|------|
| `src/Asset/SceneLoader.h/.cpp` | FBX/glTF 로딩 담당 |
| `src/Asset/TextureCache.h/.cpp` | 텍스처 로딩 + 캐싱 |
| `CLAUDE.md` — 외부 대형 씬 섹션 | Bistro 다운로드 링크 목록 |
