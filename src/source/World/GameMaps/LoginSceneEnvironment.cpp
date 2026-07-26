#include "stdafx.h"

#include "World/GameMaps/LoginSceneEnvironment.h"
#include "Data/DataHandler/LoadData.h"
#include "Render/Effects/ZzzEffect.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "World/MapInfra/MapManager.h"

namespace LoginSceneEnvironment
{
namespace
{
    constexpr wchar_t LoginModelDirectory[] = L"Data\\Object74\\";
    constexpr wchar_t LoginTextureDirectory[] = L"Object74\\";
    constexpr wchar_t CharacterModelDirectory[] = L"Data\\Object75\\";
    constexpr wchar_t CharacterTextureDirectory[] = L"Object75\\";


    constexpr float CharacterFountainMinX = 9000.0f;
    constexpr float CharacterFountainMaxX = 10000.0f;
    constexpr float CharacterFountainMinY = 12600.0f;
    constexpr float CharacterFountainMaxY = 13100.0f;

    constexpr float CharacterRearFeatureMinX = 13700.0f;
    constexpr float CharacterRearFeatureMaxX = 14250.0f;
    constexpr float CharacterRearFeatureMinY = 12500.0f;
    constexpr float CharacterRearFeatureMaxY = 13400.0f;

    constexpr int LoginModelSlots[] = {
        2, 3, 4, 6, 8, 9, 11, 12, 13, 14,
        19, 20, 21, 30, 39, 55, 56, 57, 81,
    };

    constexpr int CharacterModelSlots[] = {
        1, 3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, 16, 17, 18,
        25, 26, 27, 31, 32, 34, 35, 37, 39, 40, 41, 42, 43, 44,
        45, 55, 56, 57, 58, 61, 62, 70, 71, 72, 74, 76, 81, 82, 83,
    };

    template <size_t SlotCount>
    void LoadSceneModels(
        const int (&slots)[SlotCount],
        const wchar_t* modelDirectory,
        const wchar_t* textureDirectory)
    {
        for (const int fileSlot : slots)
        {
            const int model = MODEL_WORLD_OBJECT + fileSlot - 1;
            gLoadData.AccessModel(model, modelDirectory, L"Object", fileSlot);
            gLoadData.OpenTexture(model, textureDirectory);
        }
    }

    bool IsInvisibleMarker(const int type)
    {
        if (gMapManager.WorldActive == WD_73NEW_LOGIN_SCENE)
        {
            switch (type)
            {
            case 38:
            case 54:
                return true;
            default:
                return false;
            }
        }

        switch (type)
        {
        case 14:
        case 15:
        case 38:
        case 39:
        case 40:
        case 54:
        case 55:
        case 56:
            return true;
        default:
            return false;
        }
    }

    bool TransformBonePosition(BMD* model, const int bone, vec3_t position)
    {
        if (model == nullptr || bone < 0 || bone >= model->NumBones)
        {
            return false;
        }

        vec3_t relative;
        Vector(0.0f, 0.0f, 0.0f, relative);
        model->TransformPosition(BoneTransform[bone], relative, position, false);
        return true;
    }

    void RenderLeafLights(OBJECT* object, BMD* model)
    {
        const float pulse = sinf(WorldTime * 0.0025f + object->Position[0] * 0.01f) * 0.15f + 0.85f;
        vec3_t light;
        Vector(0.28f * pulse, 0.62f * pulse, 1.0f * pulse, light);

        const int firstBone = object->Type == 31 ? 6 : object->Type == 33 ? 1 : 5;
        const int lastBone = object->Type == 31 ? 8 : object->Type == 33 ? 3 : model->NumBones - 1;
        const int boneStep = object->Type == 71 ? 7 : 1;
        const float baseScale = object->Type == 71 ? 0.36f : 0.55f;

        for (int bone = firstBone; bone <= lastBone; bone += boneStep)
        {
            vec3_t position;
            if (!TransformBonePosition(model, bone, position))
            {
                continue;
            }

            CreateSprite(BITMAP_LIGHT, position, baseScale + pulse * 0.12f, light, object);
            CreateSprite(
                BITMAP_SHINY + 1,
                position,
                baseScale * 0.55f + pulse * 0.06f,
                light,
                object,
                WorldTime * 0.035f + bone * 120.0f);
        }
    }

    void RenderCastleLights(OBJECT* object, BMD* model)
    {
        const float pulse = sinf(WorldTime * 0.002f + object->Position[1] * 0.01f) * 0.18f + 0.82f;
        vec3_t light;
        Vector(0.22f * pulse, 0.66f * pulse, 1.0f * pulse, light);

        for (int bone = 2; bone <= 9; ++bone)
        {
            vec3_t position;
            if (!TransformBonePosition(model, bone, position))
            {
                continue;
            }

            CreateSprite(BITMAP_LIGHT, position, object->Scale * (1.45f + pulse * 0.25f), light, object);
            CreateSprite(
                BITMAP_SHINY + 1,
                position,
                object->Scale * 0.58f,
                light,
                object,
                WorldTime * 0.025f + bone * 45.0f);
        }

        vec3_t firePosition;
        if (TransformBonePosition(model, 1, firePosition))
        {
            CreateSprite(BITMAP_LIGHT, firePosition, object->Scale * 1.8f, light, object);
            if (rand_fps_check(2))
            {
                CreateParticle(
                    BITMAP_TRUE_BLUE,
                    firePosition,
                    object->Angle,
                    light,
                    0,
                    object->Scale * 0.9f,
                    object);
            }
        }
    }

    void RenderAdditiveSceneMesh(OBJECT* object, BMD* model, const vec3_t light)
    {
        model->BeginRender(1.0f);

        const bool lightEnabled = model->LightEnable;
        const int streamMesh = model->StreamMesh;
        vec3_t bodyLight;
        VectorCopy(model->BodyLight, bodyLight);

        model->LightEnable = false;
        model->StreamMesh = 0;
        VectorCopy(light, model->BodyLight);
        model->RenderMesh(
            0,
            RENDER_TEXTURE | RENDER_BRIGHT,
            object->Alpha,
            object->BlendMesh,
            object->BlendMeshLight,
            object->BlendMeshTexCoordU,
            object->BlendMeshTexCoordV);

        VectorCopy(bodyLight, model->BodyLight);
        model->StreamMesh = streamMesh;
        model->LightEnable = lightEnabled;
        model->EndRender();
    }

    bool IsCharacterFountainRegion(const OBJECT* object)
    {
        return object->Position[0] > CharacterFountainMinX
            && object->Position[0] < CharacterFountainMaxX
            && object->Position[1] > CharacterFountainMinY
            && object->Position[1] < CharacterFountainMaxY;
    }

    bool IsCharacterRearFeatureRegion(const OBJECT* object)
    {
        return object->Position[0] > CharacterRearFeatureMinX
            && object->Position[0] < CharacterRearFeatureMaxX
            && object->Position[1] > CharacterRearFeatureMinY
            && object->Position[1] < CharacterRearFeatureMaxY;
    }

    bool IsCharacterCentralRockRegion(const OBJECT* object)
    {
        return object->Type >= 41
            && object->Type <= 44
            && object->Position[0] > 8400.0f
            && object->Position[0] < 11200.0f
            && object->Position[1] > 12000.0f
            && object->Position[1] < 14100.0f;
    }

    void RenderLitSceneMesh(OBJECT* object, BMD* model, const vec3_t light)
    {
        model->BeginRender(1.0f);

        const bool lightEnabled = model->LightEnable;
        const int streamMesh = model->StreamMesh;
        vec3_t bodyLight;
        VectorCopy(model->BodyLight, bodyLight);

        model->LightEnable = false;
        model->StreamMesh = 0;
        VectorCopy(light, model->BodyLight);
        model->RenderMesh(
            0,
            RENDER_TEXTURE,
            object->Alpha,
            object->BlendMesh,
            object->BlendMeshLight,
            object->BlendMeshTexCoordU,
            object->BlendMeshTexCoordV);

        VectorCopy(bodyLight, model->BodyLight);
        model->StreamMesh = streamMesh;
        model->LightEnable = lightEnabled;

        model->EndRender();
    }

    unsigned int HashShootingStarEvent(unsigned int value)
    {
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        value ^= value >> 16;
        return value;
    }

    int GetCharacterShootingStarTexture()
    {
        BMD& model = Models[MODEL_WORLD_OBJECT + 4];
        if (model.NumMeshs <= 0 || model.Meshs == nullptr || model.IndexTexture == nullptr)
        {
            return BITMAP_UNKNOWN;
        }

        const int textureSlot = model.Meshs[0].Texture;
        if (textureSlot < 0 || textureSlot >= model.NumMeshs)
        {
            return BITMAP_UNKNOWN;
        }

        return model.IndexTexture[textureSlot];
    }

    void RenderShootingStar(
        const int texture,
        const float cycle,
        const float activeTime,
        const float offset,
        const unsigned int salt)
    {
        const float elapsed = fmodf(static_cast<float>(WorldTime) + offset, cycle);
        if (elapsed > activeTime)
        {
            return;
        }

        const unsigned int event = static_cast<unsigned int>(
            (static_cast<float>(WorldTime) + offset) / cycle);
        const unsigned int seed = HashShootingStarEvent(event + salt);
        const float progress = elapsed / activeTime;
        const float fadeIn = std::min(elapsed / 480.0f, 1.0f);
        const float fadeOut = std::min((activeTime - elapsed) / 820.0f, 1.0f);
        const float brightness = std::max(0.0f, fadeIn * fadeOut);

        const float startX = 215.0f + static_cast<float>(seed % 145u);
        const float startY = 52.0f + static_cast<float>((seed >> 8) % 66u);
        const float travelX = 5.0f + static_cast<float>((seed >> 16) % 4u);
        const float travelY = 3.0f + static_cast<float>((seed >> 24) % 3u);
        const float x = startX - progress * travelX;
        const float y = startY + progress * travelY;
        const float width = 16.0f + static_cast<float>((seed >> 12) % 5u);
        const float angle = 34.0f + static_cast<float>((seed >> 20) % 5u);

        glColor4f(
            0.64f * brightness,
            0.38f * brightness,
            1.0f * brightness,
            brightness);
        RenderBitmapRotate(texture, x, y, width, width * 0.45f, angle);
    }

}

void LoadModels()
{
    if (gMapManager.WorldActive == WD_73NEW_LOGIN_SCENE)
    {
        LoadSceneModels(LoginModelSlots, LoginModelDirectory, LoginTextureDirectory);
        return;
    }

    LoadSceneModels(CharacterModelSlots, CharacterModelDirectory, CharacterTextureDirectory);
}

void Release()
{
}

void CreateObject(OBJECT* object)
{
    object->CollisionRange = -300.0f;
    object->LightEnable = true;

    if (gMapManager.WorldActive == WD_74NEW_CHARACTER_SCENE
        && object->Type >= 24
        && object->Type <= 26)
    {
        object->BlendMesh = 0;
        object->BlendMeshLight = 1.0f;
    }

    if (IsInvisibleMarker(object->Type))
    {
        object->HiddenMesh = -2;
    }
}

bool MoveObject(OBJECT* object)
{
    object->LightEnable = true;

    if (IsInvisibleMarker(object->Type))
    {
        object->HiddenMesh = -2;
        return true;
    }

    const bool loginWater = gMapManager.WorldActive == WD_73NEW_LOGIN_SCENE
        && (object->Type == 1 || object->Type == 7 || object->Type == 18);
    const bool characterWater = gMapManager.WorldActive == WD_74NEW_CHARACTER_SCENE
        && object->Type >= 24
        && object->Type <= 26;
    if (loginWater)
    {
        object->BlendMeshTexCoordV += 0.002f;
    }
    else if (characterWater)
    {
        object->BlendMesh = 0;
        object->BlendMeshLight = 1.0f;
        object->BlendMeshTexCoordV -= 0.002f;
    }

    return true;
}

void RenderCharacterSceneSkyEffects()
{
    if (gMapManager.WorldActive != WD_74NEW_CHARACTER_SCENE)
    {
        return;
    }

}

void RenderCharacterFountainEffects()
{
    if (gMapManager.WorldActive != WD_74NEW_CHARACTER_SCENE)
    {
        return;
    }

    // Fountain water and spray are rendered from the map's real meshes and markers.
}

bool ShouldRenderBeyondBlockCulling(const int type)
{
    if (gMapManager.WorldActive != WD_74NEW_CHARACTER_SCENE)
    {
        return false;
    }

    switch (type)
    {
    case 20:
    case 21:
    case 22:
    case 23:
    case 30:
    case 31:
    case 33:
    case 34:
    case 36:
    case 38:
    case 39:
    case 40:
    case 24:
    case 25:
    case 26:
    case 41:
    case 42:
    case 43:
    case 44:
    case 54:
    case 55:
    case 56:
    case 70:
    case 71:
    case 73:
    case 75:
    case 81:
        return true;
    default:
        return false;
    }
}

bool ShouldDeferUntilOpaqueObjectsRendered(const int type)
{
    return gMapManager.WorldActive == WD_74NEW_CHARACTER_SCENE
        && type >= 24
        && type <= 26;
}

bool RenderObjectVisual(OBJECT* object, BMD* model)
{
    if (gMapManager.WorldActive != WD_74NEW_CHARACTER_SCENE)
    {
        return true;
    }

    vec3_t light;
    Vector(1.0f, 1.0f, 1.0f, light);

    switch (object->Type)
    {
    case 31:
    case 33:
    case 71:
        RenderLeafLights(object, model);
        break;
    case 38:
        if (rand_fps_check(6))
        {
            vec3_t cloudLight;
            Vector(0.02f, 0.02f, 0.05f, cloudLight);
            CreateParticle(
                BITMAP_CLOUD,
                object->Position,
                object->Angle,
                cloudLight,
                20,
                object->Scale,
                nullptr);
        }
        break;
    case 39:
    case 40:
        break;
    case 54:
        CreateParticleFpsChecked(
            BITMAP_WATERFALL_5,
            object->Position,
            object->Angle,
            light,
            0);
        break;
    case 55:
        CreateParticleFpsChecked(
            BITMAP_WATERFALL_3,
            object->Position,
            object->Angle,
            light,
            8,
            object->Scale,
            object);
        break;
    case 56:
        if (rand_fps_check(8))
        {
            CreateParticle(
                BITMAP_WATERFALL_2,
                object->Position,
                object->Angle,
                light,
                4,
                object->Scale,
                object);
        }
        break;
    case 73:
        RenderCastleLights(object, model);
        break;
    }

    return true;
}

bool RenderObject(OBJECT* object, BMD* model)
{
    if (gMapManager.WorldActive != WD_74NEW_CHARACTER_SCENE)
    {
        return false;
    }

    if (IsCharacterCentralRockRegion(object))
    {
        vec3_t light;
        Vector(0.92f, 0.95f, 1.0f, light);
        RenderLitSceneMesh(object, model, light);
        return true;
    }

    if (object->Type == 30
        || object->Type == 31
        || object->Type == 33
        || object->Type == 36
        || object->Type == 70)
    {
        vec3_t light;
        Vector(0.38f, 0.48f, 0.72f, light);
        RenderAdditiveSceneMesh(object, model, light);
        return true;
    }

    if (object->Type < 24
        || object->Type > 26
        || (!IsCharacterFountainRegion(object)
            && !IsCharacterRearFeatureRegion(object)))
    {
        return false;
    }

    model->BeginRender(1.0f);
    const bool lightEnabled = model->LightEnable;
    model->LightEnable = false;

    const float textureV = -fmodf(static_cast<float>(WorldTime), 2001.0f) * 0.0005f;
    model->RenderMesh(
        0,
        RENDER_TEXTURE | RENDER_BRIGHT,
        object->Alpha,
        object->BlendMesh,
        object->BlendMeshLight,
        object->BlendMeshTexCoordU,
        textureV);

    model->LightEnable = lightEnabled;
    model->EndRender();
    return true;
}
}
