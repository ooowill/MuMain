///////////////////////////////////////////////////////////////////////////////
// MainScene.cpp - Main game scene implementation
///////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cwchar>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "Engine/Object/EditObjects.h"
#include "UI/Chat/Chat.h"
#include "MainScene.h"
#include "SceneCommon.h"
#include "Camera/CameraUtility.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Sprites/GlobalBitmap.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Engine/Object/ZzzInterface.h"
#include "Input/Selection.h"
#include "Render/Effects/ZzzEffect.h"
#include "World/MapInfra/MapManager.h"
#include "UI/Legacy/UIMng.h"
#include "UI/NewUI/NewUISystem.h"
#include "GameLogic/Social/PartyManager.h"
#include "GameLogic/Events/Cinematic/CDirection.h"
#include "GameLogic/Pets/w_PetProcess.h"
#include "Core/Utilities/Log/muConsoleDebug.h"
#include "Core/Utilities/FrameProfiler.h"
#include "Network/Server/WSclient.h"
#include "Network/Reconnect/ReconnectManager.h"
#include "MUHelper/MuHelper.h"
#include "Character/AccountCharacterList.h"
#include "Character/KillNotificationClient.h"
#include "Engine/AI/GOBoid.h"
#include "GameLogic/Items/PersonalShopTitleImp.h"
#include "Network/Server/ServerListManager.h"
#include "UI/Legacy/UIManager.h"
#include "Engine/Object/ZzzInventory.h"
#include "World/MapInfra/PortalMgr.h"
#include "Guild/GuildCache.h"
#include "UI/Legacy/UIMapName.h"
#include "Camera/CameraProjection.h"
#include "Camera/CameraManager.h"
#include "Camera/CameraMode.h"
#ifdef _EDITOR
#include "Camera/FrustumRenderer.h"
#include "Camera/CameraDebugLog.h"
#endif

// External declarations
#ifdef _EDITOR
extern "C" bool DevEditor_IsDebugVisualizationEnabled();
// DevEditor render toggle functions
extern "C" bool DevEditor_ShouldRenderTerrain();
extern "C" bool DevEditor_ShouldRenderStaticObjects();
extern "C" bool DevEditor_ShouldRenderEffects();
extern "C" bool DevEditor_ShouldRenderDroppedItems();
extern "C" bool DevEditor_ShouldRenderItemLabels();
extern "C" bool DevEditor_ShouldRenderEquippedItems();
extern "C" bool DevEditor_ShouldRenderWeatherEffects();
extern "C" bool DevEditor_ShouldRenderUI();
extern "C" bool DevEditor_IsCameraFogOverrideEnabled(const char* cameraName);
extern "C" bool DevEditor_GetCameraFogOverrideValue(const char* cameraName);
#endif

extern HWND g_hWnd;
extern CErrorReport g_ErrorReport;
extern float EarthQuake;
extern int CheckSkill;
extern int MouseY;
extern int LoadingWorld;
extern DWORD g_dwKeyFocusUIID;
extern int ErrorMessage;
extern int HeroTile;
extern CHARACTER* Hero;
extern CUIManager* g_pUIManager;
extern CUIMapName* g_pUIMapName;
extern int MouseX;
extern bool MouseLButtonPush;
extern bool MouseLButton;
extern vec3_t MouseTarget;
extern int EditFlag;

static bool RequireLeavesEffect()
{
    return (gMapManager.WorldActive == WD_0LORENCIA && HeroTile != 4) ||
           (gMapManager.WorldActive == WD_2DEVIAS && HeroTile != 3 && HeroTile < 10) ||
           gMapManager.WorldActive == WD_3NORIA ||
           gMapManager.WorldActive == WD_7ATLANSE ||
           gMapManager.InDevilSquare() ||
           gMapManager.WorldActive == WD_10HEAVEN ||
           gMapManager.InChaosCastle() ||
           gMapManager.InBattleCastle() ||
           M31HuntingGround::IsInHuntingGround() ||
           M33Aida::IsInAida() ||
           M34CryWolf1st::IsCyrWolf1st() ||
           gMapManager.WorldActive == WD_42CHANGEUP3RD_2ND ||
           IsIceCity() ||
           IsSantaTown() ||
           gMapManager.IsPKField() ||
           IsDoppelGanger2() ||
           gMapManager.IsEmpireGuardian1() ||
           gMapManager.IsEmpireGuardian2() ||
           gMapManager.IsEmpireGuardian3() ||
           gMapManager.IsEmpireGuardian4() ||
           IsUnitedMarketPlace();
}

static bool ShouldRenderLeaves()
{
    return (gMapManager.WorldActive == WD_2DEVIAS && HeroTile != 3 && HeroTile < 10) ||
           IsIceCity() ||
           IsSantaTown() ||
           gMapManager.IsPKField() ||
           IsDoppelGanger2() ||
           gMapManager.IsEmpireGuardian1() ||
           gMapManager.IsEmpireGuardian2() ||
           gMapManager.IsEmpireGuardian3() ||
           gMapManager.IsEmpireGuardian4() ||
           IsUnitedMarketPlace();
}

namespace
{
    struct LorenciaBandObjVertex
    {
        float X;
        float Y;
        float Z;
    };

    struct LorenciaBandObjTexCoord
    {
        float U;
        float V;
    };

    struct LorenciaBandObjTriangleVertex
    {
        int VertexIndex;
        int TexCoordIndex;
    };

    struct LorenciaBandObjModel
    {
        bool TriedLoad = false;
        bool Loaded = false;
        bool TriedTexture = false;
        bool TextureLoaded = false;
        GLuint TextureIndex = BITMAP_UNKNOWN;
        float MinY = 0.f;
        float MaxY = 1.f;
        std::vector<LorenciaBandObjVertex> Vertices;
        std::vector<LorenciaBandObjTexCoord> TexCoords;
        std::vector<std::array<LorenciaBandObjTriangleVertex, 3>> Triangles;
    };

    enum class LorenciaBandObjMotion
    {
        Musician,
        JewelSeller,
        Priest,
    };

    struct LorenciaBandObjInstance
    {
        const char* ObjPath;
        const wchar_t* TexturePath;
        int World;
        float TileX;
        float TileY;
        float Rotation;
        float Scale;
        float AnimationPhase;
        float MotionStrength;
        LorenciaBandObjMotion Motion;
        bool UseJewelSellerAnchor;
        LorenciaBandObjModel Model;
    };

    constexpr float kLorenciaCustomNpcScale = 86.4f;
    constexpr float kLorenciaJewelSellerAnchorX = 121.5f;
    constexpr float kLorenciaJewelSellerAnchorY = 123.5f;
    constexpr float kLorenciaJewelSellerAnchorRadius = 7.f;
    constexpr float kDeviasPriestTileX = 205.f;
    constexpr float kDeviasPriestTileY = 30.f;
    constexpr float kDeviasPriestRotation = 45.f;
    constexpr float kDeviasPriestScale = 86.4f;
    constexpr float kDeviasPriestHearRadius = 18.f;
    constexpr double kDeviasPriestFirstSpeechDelayMs = 2500.0;
    constexpr double kDeviasPriestSpeechIntervalMs = 180000.0;
    constexpr double kDeviasPriestBubbleDurationMs = 15000.0;

    std::array<LorenciaBandObjInstance, 5> g_lorenciaBandObjInstances = { {
        { "Data\\Custom\\LorenciaBand\\alaude_01\\alaude_01.obj",           L"Data\\Custom\\LorenciaBand\\alaude_01\\alaude_01.tga",           WD_0LORENCIA, 121.f, 132.f, 90.f, kLorenciaCustomNpcScale, 0.00f, 1.00f, LorenciaBandObjMotion::Musician, false, {} },
        { "Data\\Custom\\LorenciaBand\\alaude_02\\alaude_02.obj",           L"Data\\Custom\\LorenciaBand\\alaude_02\\alaude_02.tga",           WD_0LORENCIA, 121.f, 133.f, 90.f, kLorenciaCustomNpcScale, 1.25f, 0.85f, LorenciaBandObjMotion::Musician, false, {} },
        { "Data\\Custom\\LorenciaBand\\pandeiro_01\\pandeiro_01.obj",       L"Data\\Custom\\LorenciaBand\\pandeiro_01\\pandeiro_01.tga",       WD_0LORENCIA, 121.f, 131.f, 90.f, kLorenciaCustomNpcScale, 2.40f, 1.15f, LorenciaBandObjMotion::Musician, false, {} },
        { "Data\\Custom\\LorenciaBand\\vendedor_joias\\vendedor_joias.obj", L"Data\\Custom\\LorenciaBand\\vendedor_joias\\vendedor_joias.tga", WD_0LORENCIA, 121.f, 123.f, 45.f, kLorenciaCustomNpcScale, 0.65f, 1.00f, LorenciaBandObjMotion::JewelSeller, true, {} },
        { "Data\\Custom\\DeviasChurch\\padre\\padre.obj",                  L"Data\\Custom\\DeviasChurch\\padre\\padre.tga",                  WD_2DEVIAS, kDeviasPriestTileX, kDeviasPriestTileY, kDeviasPriestRotation, kDeviasPriestScale, 1.85f, 0.55f, LorenciaBandObjMotion::Priest, false, {} },
    } };

    std::vector<std::wstring> g_deviasPriestExternalVerses;
    bool g_deviasPriestTriedLoadVerses = false;
    double g_deviasPriestNextSpeechTime = 0.0;
    double g_deviasPriestSpeechUntil = 0.0;
    int g_deviasPriestLastVerse = -1;
    std::wstring g_deviasPriestSpeech;
    std::wstring g_deviasPriestSpeechLine1;
    std::wstring g_deviasPriestSpeechLine2;

    bool IsPvpServerSelectedForCustomNpcs()
    {
        return g_ServerListManager != nullptr && g_ServerListManager->IsSelectedPvpServer();
    }

    constexpr std::array<const wchar_t*, 10> kDeviasPriestFallbackVerses = { {
        L"Mateus 5:9 - Bem-aventurados os pacificadores.",
        L"Joao 14:6 - Cristo e o caminho, a verdade e a vida.",
        L"Romanos 12:12 - Alegrem-se na esperanca e perseverem na oracao.",
        L"1 Corintios 13:13 - Permanecem a fe, a esperanca e o amor.",
        L"Filipenses 4:4 - Regozijai-vos sempre no Senhor.",
        L"1 Tessalonicenses 5:17 - Orai sem cessar.",
        L"Tiago 1:5 - Se falta sabedoria, peca-a a Deus.",
        L"1 Pedro 5:7 - Lancem sobre Deus toda ansiedade.",
        L"Apocalipse 22:21 - A graca do Senhor seja convosco.",
        L"Mateus 11:28 - Cristo chama os cansados ao descanso.",
    } };

    int ParseLorenciaBandObjIndex(const std::string& token, int vertexCount)
    {
        if (token.empty())
            return -1;

        const int index = std::atoi(token.c_str());
        if (index > 0)
            return index - 1;
        if (index < 0)
            return vertexCount + index;
        return -1;
    }

    LorenciaBandObjTriangleVertex ParseLorenciaBandObjFaceToken(const std::string& token, int vertexCount, int texCoordCount)
    {
        LorenciaBandObjTriangleVertex result{ -1, -1 };

        const std::string::size_type firstSlash = token.find('/');
        const std::string vertexText = firstSlash == std::string::npos ? token : token.substr(0, firstSlash);
        result.VertexIndex = ParseLorenciaBandObjIndex(vertexText, vertexCount);

        if (firstSlash != std::string::npos)
        {
            const std::string::size_type secondSlash = token.find('/', firstSlash + 1);
            const std::string texCoordText = secondSlash == std::string::npos
                ? token.substr(firstSlash + 1)
                : token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
            result.TexCoordIndex = ParseLorenciaBandObjIndex(texCoordText, texCoordCount);
        }

        return result;
    }

    bool LoadLorenciaBandObjModel(LorenciaBandObjInstance& instance)
    {
        LorenciaBandObjModel& model = instance.Model;
        if (model.TriedLoad)
            return model.Loaded;

        model.TriedLoad = true;

        std::ifstream objFile(instance.ObjPath);
        if (!objFile)
            return false;

        std::string line;
        while (std::getline(objFile, line))
        {
            if (line.size() < 2)
                continue;

            if (line[0] == 'v' && line[1] == ' ')
            {
                std::istringstream stream(line.substr(2));
                LorenciaBandObjVertex vertex{};
                stream >> vertex.X >> vertex.Y >> vertex.Z;
                if (stream)
                {
                    if (model.Vertices.empty())
                    {
                        model.MinY = vertex.Y;
                        model.MaxY = vertex.Y;
                    }
                    else
                    {
                        model.MinY = std::min(model.MinY, vertex.Y);
                        model.MaxY = std::max(model.MaxY, vertex.Y);
                    }
                    model.Vertices.push_back(vertex);
                }
                continue;
            }

            if (line[0] == 'v' && line[1] == 't' && (line.size() == 2 || line[2] == ' '))
            {
                std::istringstream stream(line.substr(2));
                LorenciaBandObjTexCoord texCoord{};
                stream >> texCoord.U >> texCoord.V;
                if (stream)
                    model.TexCoords.push_back(texCoord);
                continue;
            }

            if (line[0] == 'f' && line[1] == ' ')
            {
                std::istringstream stream(line.substr(2));
                std::vector<LorenciaBandObjTriangleVertex> faceVertices;
                std::string token;
                while (stream >> token)
                {
                    const LorenciaBandObjTriangleVertex faceVertex = ParseLorenciaBandObjFaceToken(
                        token,
                        static_cast<int>(model.Vertices.size()),
                        static_cast<int>(model.TexCoords.size()));
                    if (faceVertex.VertexIndex >= 0 && faceVertex.VertexIndex < static_cast<int>(model.Vertices.size()))
                        faceVertices.push_back(faceVertex);
                }

                for (size_t i = 1; i + 1 < faceVertices.size(); ++i)
                {
                    model.Triangles.push_back({
                        faceVertices[0],
                        faceVertices[i],
                        faceVertices[i + 1],
                    });
                }
            }
        }

        model.Loaded = !model.Vertices.empty() && !model.Triangles.empty();
        return model.Loaded;
    }

    bool EnsureLorenciaBandObjTexture(LorenciaBandObjInstance& instance)
    {
        LorenciaBandObjModel& model = instance.Model;
        if (model.TriedTexture)
            return model.TextureLoaded;

        model.TriedTexture = true;
        model.TextureIndex = Bitmaps.LoadImage(instance.TexturePath, GL_LINEAR, GL_REPEAT);
        model.TextureLoaded = model.TextureIndex != BITMAP_UNKNOWN;
        return model.TextureLoaded;
    }

    float SmoothLorenciaBandAmount(float value)
    {
        value = std::clamp(value, 0.f, 1.f);
        return value * value * (3.f - (2.f * value));
    }

    bool IsLorenciaJewelSellerWanderingNpc(const CHARACTER* character)
    {
        if (character == nullptr || !character->Object.Live || !character->Object.Visible)
            return false;

        if (gMapManager.WorldActive != WD_0LORENCIA)
            return false;

        switch (character->MonsterIndex)
        {
        case MONSTER_WANDERING_MERCHANT_MARTIN:
        case MONSTER_WANDERING_MERCHANT_HAROLD:
        case MONSTER_WANDERING_MERCHANT_ZYRO:
            break;
        default:
            return false;
        }

        const float tileX = character->Object.Position[0] / TERRAIN_SCALE;
        const float tileY = character->Object.Position[1] / TERRAIN_SCALE;
        const float dx = tileX - kLorenciaJewelSellerAnchorX;
        const float dy = tileY - kLorenciaJewelSellerAnchorY;
        return (dx * dx) + (dy * dy) <= kLorenciaJewelSellerAnchorRadius * kLorenciaJewelSellerAnchorRadius;
    }

    CHARACTER* FindLorenciaJewelSellerAnchor()
    {
        CHARACTER* bestCharacter = nullptr;
        float bestDistanceSq = kLorenciaJewelSellerAnchorRadius * kLorenciaJewelSellerAnchorRadius;

        for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
        {
            CHARACTER* character = &CharactersClient[i];
            if (!IsLorenciaJewelSellerWanderingNpc(character))
                continue;

            const float tileX = character->Object.Position[0] / TERRAIN_SCALE;
            const float tileY = character->Object.Position[1] / TERRAIN_SCALE;
            const float dx = tileX - kLorenciaJewelSellerAnchorX;
            const float dy = tileY - kLorenciaJewelSellerAnchorY;
            const float distanceSq = (dx * dx) + (dy * dy);
            if (distanceSq <= bestDistanceSq)
            {
                bestCharacter = character;
                bestDistanceSq = distanceSq;
            }
        }

        return bestCharacter;
    }

    struct LorenciaBandObjPlacement
    {
        float WorldX = 0.f;
        float WorldY = 0.f;
        float WorldZ = 0.f;
        float Rotation = 0.f;
    };

    bool ResolveLorenciaBandObjPlacement(const LorenciaBandObjInstance& instance, LorenciaBandObjPlacement& placement)
    {
        if (instance.UseJewelSellerAnchor)
        {
            const CHARACTER* anchor = FindLorenciaJewelSellerAnchor();
            if (anchor == nullptr)
                return false;

            placement.WorldX = anchor->Object.Position[0];
            placement.WorldY = anchor->Object.Position[1];
            placement.WorldZ = RequestTerrainHeight(placement.WorldX, placement.WorldY) + 3.f;
            placement.Rotation = anchor->Object.Angle[2] + instance.Rotation;
            return true;
        }

        placement.WorldX = (instance.TileX * TERRAIN_SCALE) + (TERRAIN_SCALE * 0.5f);
        placement.WorldY = (instance.TileY * TERRAIN_SCALE) + (TERRAIN_SCALE * 0.5f);
        placement.WorldZ = RequestTerrainHeight(placement.WorldX, placement.WorldY) + 3.f;
        placement.Rotation = instance.Rotation;
        return true;
    }

    LorenciaBandObjVertex GetLorenciaBandRenderVertex(
        const LorenciaBandObjInstance& instance,
        const LorenciaBandObjModel& model,
        const LorenciaBandObjVertex& vertex)
    {
        const float heightRange = std::max(0.001f, model.MaxY - model.MinY);
        const float height01 = std::clamp((vertex.Y - model.MinY) / heightRange, 0.f, 1.f);
        LorenciaBandObjVertex result{
            vertex.X,
            -vertex.Z,
            vertex.Y - model.MinY,
        };

        const float time = (static_cast<float>(WorldTime) * 0.006f) + instance.AnimationPhase;
        const float upperBody = SmoothLorenciaBandAmount((height01 - 0.30f) / 0.50f);
        const float head = SmoothLorenciaBandAmount((height01 - 0.72f) / 0.20f);
        const float midHeight = SmoothLorenciaBandAmount((height01 - 0.32f) / 0.18f) *
            (1.f - SmoothLorenciaBandAmount((height01 - 0.78f) / 0.12f));
        const float sideReach = SmoothLorenciaBandAmount((std::abs(vertex.X) - 0.08f) / 0.28f);
        const float hands = midHeight * sideReach;

        if (instance.Motion == LorenciaBandObjMotion::Priest)
        {
            const float torso = SmoothLorenciaBandAmount((height01 - 0.24f) / 0.34f) *
                (1.f - SmoothLorenciaBandAmount((height01 - 0.82f) / 0.12f));
            const float handLift = midHeight * sideReach;

            result.X += std::sin(time * 0.55f) * 0.005f * torso;
            result.Y += std::cos(time * 0.50f) * 0.004f * torso;
            result.X += std::sin(time * 0.72f) * 0.010f * head;
            result.Z -= (0.016f + std::sin(time * 0.90f) * 0.006f) * head;
            result.Z += (0.018f + std::sin(time * 1.15f) * 0.008f) * handLift * instance.MotionStrength;
            result.Y += std::cos(time * 1.05f) * 0.010f * handLift * instance.MotionStrength;
            return result;
        }

        if (instance.Motion == LorenciaBandObjMotion::JewelSeller)
        {
            const float torso = SmoothLorenciaBandAmount((height01 - 0.22f) / 0.32f) *
                (1.f - SmoothLorenciaBandAmount((height01 - 0.82f) / 0.12f));
            const float boxHand = midHeight * SmoothLorenciaBandAmount((vertex.X - 0.05f) / 0.26f);
            const float pointingHand = midHeight * SmoothLorenciaBandAmount((-vertex.X - 0.02f) / 0.28f);

            result.X += std::sin(time * 0.45f) * 0.006f * torso;
            result.Y += std::cos(time * 0.40f) * 0.004f * torso;
            result.X += 0.018f * head;
            result.Y += 0.016f * head;
            result.Z -= (0.030f + (std::sin(time * 0.85f) * 0.005f)) * head;
            result.X += 0.052f * pointingHand;
            result.Y += 0.018f * pointingHand;
            result.Z -= 0.020f * pointingHand;
            result.X += 0.010f * boxHand;
            result.Z += std::sin(time * 1.25f) * 0.006f * boxHand;

            return result;
        }

        result.X += std::sin(time) * 0.014f * upperBody * instance.MotionStrength;
        result.Y += std::cos(time * 0.85f) * 0.010f * upperBody * instance.MotionStrength;
        result.X += std::sin((time * 1.35f) + vertex.Z) * 0.010f * head * instance.MotionStrength;
        result.Y += std::cos((time * 1.15f) + vertex.X) * 0.008f * head * instance.MotionStrength;
        result.X += std::cos((time * 2.80f) + (vertex.Z * 4.f)) * 0.018f * hands * instance.MotionStrength;
        result.Z += std::sin((time * 3.20f) + (vertex.X * 5.f)) * 0.020f * hands * instance.MotionStrength;

        return result;
    }

    void SetLorenciaBandVertexColor(const LorenciaBandObjModel& model, const LorenciaBandObjVertex& vertex)
    {
        const float heightRange = std::max(0.001f, model.MaxY - model.MinY);
        const float height01 = std::clamp((vertex.Y - model.MinY) / heightRange, 0.f, 1.f);
        if (height01 > 0.72f)
        {
            glColor3f(0.86f, 0.72f, 0.48f);
        }
        else if (std::abs(vertex.X) > 0.24f)
        {
            glColor3f(0.40f, 0.23f, 0.11f);
        }
        else if (height01 < 0.18f)
        {
            glColor3f(0.18f, 0.16f, 0.14f);
        }
        else
        {
            glColor3f(0.62f, 0.50f, 0.36f);
        }
    }

    void RenderLorenciaBandObjInstance(LorenciaBandObjInstance& instance)
    {
        if (!LoadLorenciaBandObjModel(instance))
            return;

        const LorenciaBandObjModel& model = instance.Model;
        LorenciaBandObjPlacement placement;
        if (!ResolveLorenciaBandObjPlacement(instance, placement))
            return;

        glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT | GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT | GL_LIGHTING_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);

        const bool textured = EnsureLorenciaBandObjTexture(instance);
        if (textured)
        {
            glEnable(GL_TEXTURE_2D);
            BindTexture(static_cast<int>(model.TextureIndex));
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glColor4f(1.f, 1.f, 1.f, 1.f);
        }
        else
        {
            glDisable(GL_TEXTURE_2D);
        }

        glPushMatrix();
        glTranslatef(placement.WorldX, placement.WorldY, placement.WorldZ);
        glRotatef(placement.Rotation, 0.f, 0.f, 1.f);
        glScalef(instance.Scale, instance.Scale, instance.Scale);

        glBegin(GL_TRIANGLES);
        for (const auto& triangle : model.Triangles)
        {
            for (const auto& corner : triangle)
            {
                const auto& vertex = model.Vertices[corner.VertexIndex];
                if (textured && corner.TexCoordIndex >= 0 && corner.TexCoordIndex < static_cast<int>(model.TexCoords.size()))
                {
                    const auto& texCoord = model.TexCoords[corner.TexCoordIndex];
                    glTexCoord2f(texCoord.U, 1.f - texCoord.V);
                }
                else if (!textured)
                {
                    SetLorenciaBandVertexColor(model, vertex);
                }

                const LorenciaBandObjVertex renderVertex = GetLorenciaBandRenderVertex(instance, model, vertex);
                glVertex3f(renderVertex.X, renderVertex.Y, renderVertex.Z);
            }
        }
        glEnd();

        glPopMatrix();
        glPopAttrib();
    }

    bool IsHeroNearDeviasPriest()
    {
        if (IsPvpServerSelectedForCustomNpcs() || gMapManager.WorldActive != WD_2DEVIAS || Hero == nullptr)
            return false;

        const float heroTileX = Hero->Object.Position[0] / TERRAIN_SCALE;
        const float heroTileY = Hero->Object.Position[1] / TERRAIN_SCALE;
        const float dx = heroTileX - kDeviasPriestTileX;
        const float dy = heroTileY - kDeviasPriestTileY;
        return (dx * dx) + (dy * dy) <= kDeviasPriestHearRadius * kDeviasPriestHearRadius;
    }

    std::wstring Utf8ToWideString(const std::string& text)
    {
        if (text.empty())
            return {};

        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (length <= 0)
            return std::wstring(text.begin(), text.end());

        std::wstring result(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), length);
        return result;
    }

    void TrimDeviasPriestLine(std::string& line)
    {
        if (line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF)
        {
            line.erase(0, 3);
        }

        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();

        size_t first = 0;
        while (first < line.size() && (line[first] == ' ' || line[first] == '\t'))
            ++first;

        if (first > 0)
            line.erase(0, first);
    }

    const std::vector<std::wstring>& GetDeviasPriestExternalVerses()
    {
        if (g_deviasPriestTriedLoadVerses)
            return g_deviasPriestExternalVerses;

        g_deviasPriestTriedLoadVerses = true;

        std::ifstream file("Data\\Custom\\DeviasChurch\\padre\\new_testament_bkj.txt", std::ios::binary);
        if (!file)
            return g_deviasPriestExternalVerses;

        std::string line;
        while (std::getline(file, line))
        {
            TrimDeviasPriestLine(line);
            if (line.empty() || line[0] == '#')
                continue;

            std::wstring verse = Utf8ToWideString(line);
            if (verse.size() > 220)
            {
                verse.resize(217);
                verse += L"...";
            }

            if (!verse.empty())
                g_deviasPriestExternalVerses.push_back(verse);
        }

        return g_deviasPriestExternalVerses;
    }

    int GetDeviasPriestVerseCount()
    {
        const auto& externalVerses = GetDeviasPriestExternalVerses();
        if (!externalVerses.empty())
            return static_cast<int>(externalVerses.size());

        return static_cast<int>(kDeviasPriestFallbackVerses.size());
    }

    std::wstring GetDeviasPriestVerse(int index)
    {
        const auto& externalVerses = GetDeviasPriestExternalVerses();
        if (!externalVerses.empty())
            return externalVerses[static_cast<size_t>(index) % externalVerses.size()];

        return kDeviasPriestFallbackVerses[static_cast<size_t>(index) % kDeviasPriestFallbackVerses.size()];
    }

    int PickDeviasPriestVerseIndex()
    {
        const int verseCount = GetDeviasPriestVerseCount();
        if (verseCount <= 0)
            return -1;

        unsigned int seed = static_cast<unsigned int>(WorldTime);
        if (Hero != nullptr)
        {
            seed ^= static_cast<unsigned int>(Hero->PositionX << 8);
            seed ^= static_cast<unsigned int>(Hero->PositionY << 16);
        }

        int index = static_cast<int>(seed % static_cast<unsigned int>(verseCount));
        if (verseCount > 1 && index == g_deviasPriestLastVerse)
            index = (index + 1) % verseCount;

        return index;
    }

    void SplitDeviasPriestSpeech(const std::wstring& speech)
    {
        constexpr size_t kMaxLineLength = 38;
        g_deviasPriestSpeechLine1.clear();
        g_deviasPriestSpeechLine2.clear();

        if (speech.size() <= kMaxLineLength)
        {
            g_deviasPriestSpeechLine1 = speech;
            return;
        }

        size_t split = speech.rfind(L' ', kMaxLineLength);
        if (split == std::wstring::npos || split < 20)
            split = kMaxLineLength;

        g_deviasPriestSpeechLine1 = speech.substr(0, split);
        size_t secondStart = split;
        while (secondStart < speech.size() && speech[secondStart] == L' ')
            ++secondStart;

        g_deviasPriestSpeechLine2 = speech.substr(secondStart);
        if (g_deviasPriestSpeechLine2.size() > kMaxLineLength)
        {
            g_deviasPriestSpeechLine2.resize(kMaxLineLength - 3);
            g_deviasPriestSpeechLine2 += L"...";
        }
    }

    int MeasureDeviasPriestSpeechWidth(const wchar_t* text)
    {
        if (text == nullptr || text[0] == L'\0')
            return 0;

        SIZE size{};
        if (!GetTextExtentPoint32(g_pRenderText->GetFontDC(), text, lstrlen(text), &size))
            return 0;

        const float rate = std::max(0.001f, g_fScreenRate_x);
        return static_cast<int>(std::ceil(static_cast<float>(size.cx) / rate));
    }

    void UpdateDeviasPriestSpeech()
    {
        if (IsPvpServerSelectedForCustomNpcs())
        {
            g_deviasPriestNextSpeechTime = 0.0;
            g_deviasPriestSpeechUntil = 0.0;
            return;
        }

        if (!IsHeroNearDeviasPriest())
        {
            g_deviasPriestNextSpeechTime = 0.0;
            g_deviasPriestSpeechUntil = 0.0;
            return;
        }

        if (g_deviasPriestNextSpeechTime <= 0.0)
            g_deviasPriestNextSpeechTime = WorldTime + kDeviasPriestFirstSpeechDelayMs;

        if (WorldTime < g_deviasPriestNextSpeechTime)
            return;

        const int verseIndex = PickDeviasPriestVerseIndex();
        if (verseIndex < 0)
            return;

        g_deviasPriestLastVerse = verseIndex;
        g_deviasPriestSpeech = GetDeviasPriestVerse(verseIndex);
        SplitDeviasPriestSpeech(g_deviasPriestSpeech);
        g_deviasPriestSpeechUntil = WorldTime + kDeviasPriestBubbleDurationMs;
        g_deviasPriestNextSpeechTime = WorldTime + kDeviasPriestSpeechIntervalMs;

        if (g_pChatListBox != nullptr)
            g_pChatListBox->AddText(L"Padre", g_deviasPriestSpeech, SEASON3B::TYPE_CHAT_MESSAGE);
    }

    void RenderDeviasPriestSpeech()
    {
        if (IsPvpServerSelectedForCustomNpcs() || gMapManager.WorldActive != WD_2DEVIAS || g_deviasPriestSpeechUntil <= WorldTime || g_deviasPriestSpeechLine1.empty())
            return;

        const float worldX = (kDeviasPriestTileX * TERRAIN_SCALE) + (TERRAIN_SCALE * 0.5f);
        const float worldY = (kDeviasPriestTileY * TERRAIN_SCALE) + (TERRAIN_SCALE * 0.5f);

        vec3_t position;
        Vector(worldX, worldY, RequestTerrainHeight(worldX, worldY) + 185.f, position);

        int screenX = 0;
        int screenY = 0;
        CameraProjection::WorldToScreen(g_Camera, position, &screenX, &screenY);

        g_pRenderText->SetFont(g_hFont);

        constexpr int kMinBoxWidth = 72;
        constexpr int kMaxBoxWidth = 170;
        constexpr int kHorizontalPadding = 8;
        const int textWidth = std::max({
            MeasureDeviasPriestSpeechWidth(L"Padre"),
            MeasureDeviasPriestSpeechWidth(g_deviasPriestSpeechLine1.c_str()),
            MeasureDeviasPriestSpeechWidth(g_deviasPriestSpeechLine2.c_str()),
        });
        const int boxWidth = std::clamp(textWidth + (kHorizontalPadding * 2), kMinBoxWidth, kMaxBoxWidth);
        const int lineCount = g_deviasPriestSpeechLine2.empty() ? 2 : 3;
        const int boxHeight = 8 + (lineCount * 12);
        const int left = std::clamp(screenX - (boxWidth / 2), 4, REFERENCE_WIDTH - boxWidth - 4);
        const int top = std::clamp(screenY - boxHeight, 4, REFERENCE_HEIGHT - boxHeight - 44);

        EnableAlphaBlend3();
        RenderColor(static_cast<float>(left), static_cast<float>(top), static_cast<float>(boxWidth), static_cast<float>(boxHeight), 0.54f, 1);
        EndRenderColor();

        g_pRenderText->SetBgColor(0);
        g_pRenderText->SetTextColor(150, 255, 240, 255);
        g_pRenderText->RenderText(left, top + 3, L"Padre", boxWidth, 0, RT3_SORT_CENTER);

        g_pRenderText->SetTextColor(230, 220, 200, 255);
        g_pRenderText->RenderText(left + kHorizontalPadding, top + 15, g_deviasPriestSpeechLine1.c_str(), boxWidth - (kHorizontalPadding * 2), 0, RT3_SORT_CENTER);

        if (!g_deviasPriestSpeechLine2.empty())
            g_pRenderText->RenderText(left + kHorizontalPadding, top + 27, g_deviasPriestSpeechLine2.c_str(), boxWidth - (kHorizontalPadding * 2), 0, RT3_SORT_CENTER);
    }

    void RenderLorenciaBandMembers()
    {
        if (Hero == nullptr || IsPvpServerSelectedForCustomNpcs())
            return;

        for (auto& instance : g_lorenciaBandObjInstances)
        {
            if (instance.World == gMapManager.WorldActive)
                RenderLorenciaBandObjInstance(instance);
        }
    }

    bool IsHudCommandBlockedByForegroundWindow()
    {
        if (g_pNewUISystem == nullptr)
        {
            return false;
        }

        static constexpr DWORD kBlockingInterfaces[] =
        {
            SEASON3B::INTERFACE_INVENTORY,
            SEASON3B::INTERFACE_INVENTORY_EXT,
            SEASON3B::INTERFACE_STORAGE,
            SEASON3B::INTERFACE_STORAGE_EXT,
            SEASON3B::INTERFACE_CHARACTER,
            SEASON3B::INTERFACE_NPCSHOP,
            SEASON3B::INTERFACE_MIXINVENTORY,
            SEASON3B::INTERFACE_TRADE,
            SEASON3B::INTERFACE_MYSHOP_INVENTORY,
            SEASON3B::INTERFACE_PURCHASESHOP_INVENTORY,
            SEASON3B::INTERFACE_WINDOW_MENU,
            SEASON3B::INTERFACE_OPTION,
            SEASON3B::INTERFACE_MOVEMAP,
            SEASON3B::INTERFACE_INGAMESHOP,
        };

        for (const DWORD interfaceKey : kBlockingInterfaces)
        {
            if (g_pNewUISystem->IsVisible(interfaceKey))
            {
                return true;
            }
        }

        return false;
    }

    void RenderHudCommandButton(float x, float y, float width, float height, const wchar_t* label, const wchar_t* command)
    {
        const bool isHover = CheckMouseIn(x, y, width, height);

        EnableAlphaBlend3();
        RenderColor(x, y, width, height, isHover ? 0.82f : 0.62f, 1);
        EndRenderColor();

        g_pRenderText->SetBgColor(0);
        g_pRenderText->SetTextColor(isHover ? CLRDW_BR_YELLOW : CLRDW_WHITE);
        g_pRenderText->RenderText(
            static_cast<int>(x),
            static_cast<int>(y + 4.f),
            label,
            static_cast<int>(width),
            0,
            RT3_SORT_CENTER);

        if (isHover && MouseLButtonPush)
        {
            wchar_t text[128] = {};
            std::wcsncpy(text, command, (sizeof(text) / sizeof(text[0])) - 1);
            SendMacroChat(text);
            MouseLButtonPush = false;
            MouseLButton = false;
        }
    }

    float Clamp01(float value)
    {
        return std::clamp(value, 0.f, 1.f);
    }

    float SmoothStep(float value)
    {
        value = Clamp01(value);
        return value * value * (3.f - (2.f * value));
    }

    bool IsDayNightCycleEnabledForCurrentMap()
    {
        return gMapManager.WorldActive == WD_0LORENCIA ||
               gMapManager.WorldActive == WD_2DEVIAS ||
               gMapManager.WorldActive == WD_3NORIA;
    }

    float GetDayNightCyclePhase()
    {
        constexpr double kCycleMilliseconds = 10.0 * 60.0 * 1000.0;
        double timeInCycle = std::fmod(WorldTime, kCycleMilliseconds);
        if (timeInCycle < 0.0)
        {
            timeInCycle += kCycleMilliseconds;
        }

        return static_cast<float>(timeInCycle / kCycleMilliseconds);
    }

    float GetNightStrength(float phase)
    {
        if (phase < 0.15f)
        {
            return 1.f - SmoothStep(phase / 0.15f);
        }

        if (phase < 0.55f)
        {
            return 0.f;
        }

        if (phase < 0.70f)
        {
            return SmoothStep((phase - 0.55f) / 0.15f);
        }

        return 1.f;
    }

    float GetTwilightStrength(float phase)
    {
        if (phase < 0.15f)
        {
            const float dawn = phase / 0.15f;
            return 1.f - std::abs((dawn * 2.f) - 1.f);
        }

        if (phase >= 0.55f && phase < 0.70f)
        {
            const float dusk = (phase - 0.55f) / 0.15f;
            return 1.f - std::abs((dusk * 2.f) - 1.f);
        }

        return 0.f;
    }

    struct DayNightMapTone
    {
        float BlackAlpha;
        float BlueRed;
        float BlueGreen;
        float BlueBlue;
        float BlueAlpha;
        float MoonRed;
        float MoonGreen;
        float MoonBlue;
        float MoonAlpha;
        float TwilightAlpha;
    };

    DayNightMapTone GetDayNightMapTone()
    {
        if (gMapManager.WorldActive == WD_2DEVIAS)
        {
            return { 0.18f, 0.03f, 0.13f, 0.34f, 0.40f, 0.12f, 0.25f, 0.55f, 0.08f, 0.10f };
        }

        return { 0.40f, 0.02f, 0.05f, 0.18f, 0.24f, 0.06f, 0.10f, 0.24f, 0.03f, 0.14f };
    }

    struct DayNightLightSource
    {
        int World;
        float TileX;
        float TileY;
        float Height;
        float RadiusX;
        float RadiusY;
        float Red;
        float Green;
        float Blue;
        float Alpha;
    };

    constexpr std::array<DayNightLightSource, 10> kDayNightLightSources = { {
        { WD_2DEVIAS,   206.f,  58.f, 155.f, 175.f, 110.f, 1.00f, 0.42f, 0.12f, 0.52f },
        { WD_2DEVIAS,   204.f,  62.f,  90.f, 105.f,  72.f, 1.00f, 0.36f, 0.10f, 0.34f },
        { WD_2DEVIAS,   210.f,  62.f,  90.f, 105.f,  72.f, 1.00f, 0.36f, 0.10f, 0.34f },
        { WD_2DEVIAS,   216.f,  65.f,  80.f, 120.f,  82.f, 0.95f, 0.40f, 0.14f, 0.30f },
        { WD_0LORENCIA, 123.f, 132.f,  95.f, 135.f,  92.f, 1.00f, 0.45f, 0.14f, 0.38f },
        { WD_0LORENCIA, 130.f, 128.f,  95.f, 135.f,  92.f, 1.00f, 0.45f, 0.14f, 0.38f },
        { WD_0LORENCIA, 142.f, 126.f,  95.f, 120.f,  82.f, 0.95f, 0.38f, 0.12f, 0.28f },
        { WD_3NORIA,    173.f, 124.f,  95.f, 130.f,  88.f, 1.00f, 0.46f, 0.16f, 0.36f },
        { WD_3NORIA,    180.f, 116.f,  95.f, 130.f,  88.f, 1.00f, 0.46f, 0.16f, 0.36f },
        { WD_3NORIA,    166.f, 117.f,  95.f, 115.f,  78.f, 0.95f, 0.40f, 0.14f, 0.28f },
    } };

    void RenderDayNightTintQuad(float width, float height, float red, float green, float blue, float alpha)
    {
        if (alpha <= 0.001f || width <= 0.f || height <= 0.f)
        {
            return;
        }

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(red, green, blue, alpha);

        glBegin(GL_QUADS);
        glVertex2f(0.f, 0.f);
        glVertex2f(width, 0.f);
        glVertex2f(width, height);
        glVertex2f(0.f, height);
        glEnd();
    }

    void RenderDayNightLightDisc(float x, float y, float radiusX, float radiusY, float red, float green, float blue, float alpha)
    {
        if (alpha <= 0.001f || radiusX <= 0.f || radiusY <= 0.f)
        {
            return;
        }

        constexpr int kSegments = 36;
        constexpr float kTwoPi = Q_PI * 2.f;

        glBegin(GL_TRIANGLE_FAN);
        glColor4f(red, green, blue, alpha);
        glVertex2f(x, y);

        glColor4f(red, green, blue, 0.f);
        for (int i = 0; i <= kSegments; ++i)
        {
            const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(kSegments);
            glVertex2f(x + (std::cos(angle) * radiusX), y + (std::sin(angle) * radiusY));
        }
        glEnd();
    }

    bool ProjectDayNightLightSource(const DayNightLightSource& source, float& outX, float& outY)
    {
        const float worldX = (source.TileX * TERRAIN_SCALE) + (TERRAIN_SCALE * 0.5f);
        const float worldY = (source.TileY * TERRAIN_SCALE) + (TERRAIN_SCALE * 0.5f);

        if (Hero != nullptr)
        {
            vec3_t sourcePosition;
            vec3_t range;
            Vector(worldX, worldY, Hero->Object.Position[2], sourcePosition);
            VectorSubtract(Hero->Object.Position, sourcePosition, range);
            const float maxDistance = 34.f * TERRAIN_SCALE;
            if ((range[0] * range[0]) + (range[1] * range[1]) > (maxDistance * maxDistance))
            {
                return false;
            }
        }

        vec3_t position;
        Vector(worldX, worldY, RequestTerrainHeight(worldX, worldY) + source.Height, position);

        int screenX = 0;
        int screenY = 0;
        CameraProjection::WorldToScreen(g_Camera, position, &screenX, &screenY);

        outX = static_cast<float>(screenX) * static_cast<float>(WindowWidth) / static_cast<float>(REFERENCE_WIDTH);
        outY = static_cast<float>(screenY) * static_cast<float>(WindowHeight) / static_cast<float>(REFERENCE_HEIGHT);

        return outX > -source.RadiusX && outX < static_cast<float>(WindowWidth) + source.RadiusX &&
               outY > -source.RadiusY && outY < static_cast<float>(WindowHeight) + source.RadiusY;
    }

    void RenderDayNightLightSources(float night)
    {
        if (night <= 0.001f)
        {
            return;
        }

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glShadeModel(GL_SMOOTH);

        const float flicker = 0.88f + (std::sin(static_cast<float>(WorldTime) * 0.012f) * 0.08f) +
                              (std::sin(static_cast<float>(WorldTime) * 0.031f) * 0.04f);

        for (const DayNightLightSource& source : kDayNightLightSources)
        {
            if (source.World != gMapManager.WorldActive)
            {
                continue;
            }

            float x = 0.f;
            float y = 0.f;
            if (!ProjectDayNightLightSource(source, x, y))
            {
                continue;
            }

            const float alpha = source.Alpha * night * Clamp01(flicker);
            RenderDayNightLightDisc(x, y, source.RadiusX, source.RadiusY, source.Red, source.Green, source.Blue, alpha);
            RenderDayNightLightDisc(x, y, source.RadiusX * 0.45f, source.RadiusY * 0.45f,
                1.f, 0.70f, 0.28f, alpha * 0.55f);
        }
    }

    void RenderDayNightCycleOverlay()
    {
        if (!IsDayNightCycleEnabledForCurrentMap())
        {
            return;
        }

        const float phase = GetDayNightCyclePhase();
        const float night = GetNightStrength(phase);
        const float twilight = GetTwilightStrength(phase);
        if (night <= 0.001f && twilight <= 0.001f)
        {
            return;
        }

        const float width = static_cast<float>(WindowWidth);
        const float height = static_cast<float>(WindowHeight);
        const DayNightMapTone tone = GetDayNightMapTone();

        BeginBitmap();
        glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_CURRENT_BIT | GL_TEXTURE_BIT | GL_LIGHTING_BIT);

        RenderDayNightTintQuad(width, height, 0.00f, 0.00f, 0.00f, night * tone.BlackAlpha);
        RenderDayNightTintQuad(width, height, tone.BlueRed, tone.BlueGreen, tone.BlueBlue, night * tone.BlueAlpha);
        RenderDayNightTintQuad(width, height, tone.MoonRed, tone.MoonGreen, tone.MoonBlue, night * tone.MoonAlpha);
        RenderDayNightTintQuad(width, height, 0.95f, 0.42f, 0.10f, twilight * tone.TwilightAlpha);
        RenderDayNightLightSources(night);

        glPopAttrib();
        EndBitmap();
    }
}

/**
 * @brief Performs one-time initialization when entering the main game scene.
 *
 * This function is called once when transitioning from character selection to the main game.
 * It performs the following tasks:
 * - Sends character selection to the game server
 * - Initializes UI systems (chat, party, guild, etc.)
 * - Sets up camera and input configuration
 * - Clears previous scene state and prepares for gameplay
 *
 * @note This function should only be called once per main scene entry.
 */
static void InitializeMainScene()
{
    g_pMainFrame->ResetSkillHotKey();

    const wchar_t* selectedCharacterName = CharacterAttribute->Name;
    if ((selectedCharacterName == nullptr || selectedCharacterName[0] == L'\0')
        && SelectedHero >= 0
        && SelectedHero < AccountCharacterList::NativeVisibleSlots)
    {
        selectedCharacterName = CharactersClient[SelectedHero].ID;
    }

    g_pMainFrame->LoadSkillHotKeysLocal();

    if (selectedCharacterName == nullptr || selectedCharacterName[0] == L'\0')
    {
        g_ErrorReport.Write(L"[InitializeMainScene] missing selected character name, selected=%d\r\n", SelectedHero);
        CurrentProtocolState = RECEIVE_JOIN_SERVER_SUCCESS;
        SceneFlag = CHARACTER_SCENE;
        return;
    }

    g_ConsoleDebug->Write(MCD_NORMAL, L"Join the game with the following character: %ls", selectedCharacterName);
    g_ErrorReport.Write(L"> Character selected <%d> \"%ls\"\r\n", SelectedHero + 1, selectedCharacterName);

    InitMainScene = true;

    g_ConsoleDebug->Write(MCD_SEND, L"SendRequestJoinMapServer");

    CurrentProtocolState = REQUEST_JOIN_MAP_SERVER;
    SocketClient->ToGameServer()->SendSelectCharacter(selectedCharacterName);

    // Remember which character is in play so auto-reconnect can re-select it.
    ReconnectManager::Instance().CacheCharacter(selectedCharacterName);

    CUIMng::Instance().CreateMainScene();

    g_Camera.Angle[2] = -45.f;

    ClearInput();
    InputEnable = false;
    TabInputEnable = false;
    InputTextWidth = 256;
    InputTextMax[0] = 42;
    InputTextMax[1] = 10;
    InputNumber = 2;
    for (int i = 0; i < MAX_WHISPER; i++)
    {
        g_pChatListBox->AddText(L"", L"", SEASON3B::TYPE_WHISPER_MESSAGE);
    }

    g_GuildNotice[0][0] = '\0';
    g_GuildNotice[1][0] = '\0';

    g_pPartyManager->Create();

    g_pChatListBox->ClearAll();
    g_pSystemLogBox->ClearAll();

    g_pSlideHelpMgr->Init();
    g_pUIMapName->Init();
    if (!ReconnectManager::Instance().IsActive())
    {
        MUHelper::g_MuHelper.ResetSessionState(true);
    }
    g_pNewUIMuHelper->LoadCachedOrReset();

    g_GuildCache.Reset();
    g_PortalMgr.Reset();

    ClearAllObjectBlurs();

    SetFocus(g_hWnd);

    g_ErrorReport.Write(L"> Main Scene init success. ");
    g_ErrorReport.WriteCurrentTime();

    g_ConsoleDebug->Write(MCD_NORMAL, L"MainScene Init Success");
}

/**
 * @brief Resets per-frame state variables at the start of each frame.
 *
 * Initializes frame-dependent state including:
 * - Earthquake effect damping
 * - Terrain lighting
 * - UI interaction flags (inventory, skill checks, mouse window state)
 *
 * @note Called every frame during the main scene update loop.
 */
static void InitializeSceneFrame()
{
    EarthQuake *= 0.2f;
    InitTerrainLight();

    CheckInventory = NULL;
    CheckSkill = -1;
    MouseOnWindow = false;
}

/**
 * @brief Updates user interface and processes player input.
 *
 * Handles all UI-related updates and input processing including:
 * - Party system updates
 * - New UI system updates
 * - Mouse and keyboard input handling
 * - Window focus management
 * - Interface movement and tournament interface updates
 *
 * @note Only processes input when not in top-view camera mode and loading is complete.
 * @note Skips processing if g_Camera.TopViewEnable is true or LoadingWorld >= 30.
 */
static void UpdateUIAndInput()
{
    if (g_Camera.TopViewEnable || LoadingWorld >= 30)
        return;

    if (MouseY >= (int)(REFERENCE_HEIGHT - 48))
        MouseOnWindow = true;

    g_pPartyManager->Update();
    g_pNewUISystem->Update();

    if (MouseLButton == true &&
        false == g_pNewUISystem->CheckMouseUse() &&
        g_dwMouseUseUIID == 0 &&
        g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHATINPUTBOX) == false)
    {
        g_pWindowMgr->SetWindowsEnable(FALSE);
        g_pFriendMenu->HideMenu();
        g_dwKeyFocusUIID = 0;
        if (GetFocus() != g_hWnd)
        {
            SaveIMEStatus();
            SetFocus(g_hWnd);
        }
    }

    MoveInterface();
    MoveTournamentInterface();

    if (ErrorMessage != MESSAGE_LOG_OUT)
        g_pUIManager->UpdateInput();
}

/**
 * @brief Updates all game entities and visual effects.
 *
 * Performs per-frame updates for all game world entities:
 * - World objects and items
 * - Environmental effects (leaves, boids, fish)
 * - Chat messages and player shops
 * - Player hero and other characters
 * - Mounts and pets
 * - Visual effects (particles, joints, pointers)
 * - Direction indicators
 *
 * @note Some updates are conditional based on camera mode (e.g., items only update when not in top-view).
 * @note Includes editor object updates when ENABLE_EDIT is defined.
 */
static void UpdateGameEntities()
{
    MoveObjects();

    if (!g_Camera.TopViewEnable)
        MoveItems();

    if (RequireLeavesEffect())
    {
        MoveLeaves();
    }

    MoveBoids();
    MoveFishs();
    UI::Chat::MoveChat();
    UpdatePersonalShopTitleImp();
    MoveHero();
    MoveCharactersClient();
    MoveMounts();
    ThePetProcess().UpdatePets();
    MovePoints();
    MoveEffects();
    MoveJoints();
    MoveParticles();
    MovePointers();

    g_Direction.CheckDirection();

#ifdef ENABLE_EDIT
    Editor::EditObjects();
#endif //ENABLE_EDIT
}

/**
 * @brief Main update function for the game scene.
 *
 * This is the primary per-frame update loop for the main gameplay scene.
 * It orchestrates initialization, server connection waiting, and frame updates by calling:
 * 1. InitializeMainScene() - One-time setup (first call only)
 * 2. Server join synchronization - Waits for server response before enabling rendering
 * 3. InitializeSceneFrame() - Per-frame state reset
 * 4. UpdateUIAndInput() - UI and input processing
 * 5. UpdateGameEntities() - Game world and entity updates
 *
 * @note Returns early if EnableMainRender is false (waiting for server join).
 */
void MoveMainScene()
{
    if (!InitMainScene)
    {
        InitializeMainScene();
    }

    if (CurrentProtocolState == RECEIVE_JOIN_MAP_SERVER)
    {
        EnableMainRender = true;
    }

    if (EnableMainRender == false)
    {
        return;
    }

    InitializeSceneFrame();

    // While the reconnect dialog is up it's modal: block world clicks so they
    // don't move the hero and instead reach the dialog's Cancel button.
    if (ReconnectManager::Instance().IsActive())
        MouseOnWindow = true;

    UpdateUIAndInput();

    if (ErrorMessage != 0)
        MouseOnWindow = true;

    UpdateGameEntities();

    g_ConsoleDebug->UpdateMainScene();
}

/**
 * @brief Sets up OpenGL viewport and clear color for main scene.
 *
 * @param outWidth Output screen width
 * @param outHeight Output screen height
 * @param outByWaterMap Output water map flag (0=normal, 1=hellas water, 2=water terrain)
 * @param cameraPos Camera position for frustum
 */
static void SetupMainSceneViewport(int& outWidth, int& outHeight, BYTE& outByWaterMap, vec3_t cameraPos)
{
    outByWaterMap = 0;

    if (g_Camera.TopViewEnable == false)
    {
        // Use hardcoded value from original game (in 640×480 reference coordinates)
        // This is then scaled by BeginOpengl() to actual window size
        outHeight = REFERENCE_HEIGHT - 48;
    }
    else
    {
        outHeight = REFERENCE_HEIGHT;
    }

    outWidth = GetScreenWidth();

    // NOTE: Clear color is set by SceneManager::SetWorldClearColor() before this function is called
    // All background colors are now centralized in SceneManager.cpp

    BeginOpengl(0, 0, outWidth, outHeight);
    CreateFrustrum((float)outWidth / (float)REFERENCE_WIDTH, (float)outHeight / (float)REFERENCE_HEIGHT, cameraPos);

    // Setup fog for battle castle
    if (gMapManager.InBattleCastle())
    {
        if (battleCastle::InBattleCastle2(Hero->Object.Position))
        {
            vec3_t Color = { 0.f, 0.f, 0.f };
            battleCastle::StartFog(Color);
        }
        // Don't disable fog - let BeginOpengl() handle it based on FogEnable
    }
    CameraProjection::ScreenToWorldRay(g_Camera, MouseX, MouseY, MouseTarget);
}

/**
 * @brief Renders all 3D game entities (terrain, objects, characters, effects).
 *
 * @param byWaterMap Water map mode flag (passed by reference, may be modified)
 * @param width Screen width for water terrain rendering
 * @param height Screen height for water terrain rendering
 */
static void RenderGameWorld(BYTE& byWaterMap, int width, int height)
{
#ifdef _EDITOR
    // DevEditor render toggle checks
    bool renderTerrain = DevEditor_ShouldRenderTerrain();
    bool renderStatic = DevEditor_ShouldRenderStaticObjects();
    bool renderEffects = DevEditor_ShouldRenderEffects();
    bool renderDroppedItems = DevEditor_ShouldRenderDroppedItems();
    bool renderWeatherEffects = DevEditor_ShouldRenderWeatherEffects();
#else
    bool renderTerrain = true;
    bool renderStatic = true;
    bool renderEffects = true;
    bool renderDroppedItems = true;
    bool renderWeatherEffects = true;
#endif

    if (IsWaterTerrain() == false && renderTerrain)
    {
        if (gMapManager.WorldActive == WD_39KANTURU_3RD)
        {
            if (!g_Direction.m_CKanturu.IsMayaScene())
                { FRAME_PROFILE(Terrain); RenderTerrain(false); }
        }
        else
            if (gMapManager.WorldActive != WD_10HEAVEN && gMapManager.WorldActive != -1)
            {
                if ((gMapManager.IsPKField() || IsDoppelGanger2()) && renderStatic)
                {
                    FRAME_PROFILE(Objects); RenderObjects();
                }
                { FRAME_PROFILE(Terrain); RenderTerrain(false); }
            }
    }

    if (!gMapManager.IsPKField() && !IsDoppelGanger2() && renderStatic)
        { FRAME_PROFILE(Objects); RenderObjects(); }

    if (renderEffects)
    {
        RenderEffectShadows();
        RenderBoids();
    }

    { FRAME_PROFILE(Characters); RenderCharactersClient(); }
    RenderLorenciaBandMembers();

    if (EditFlag != EDIT_NONE && renderTerrain)
    {
        FRAME_PROFILE(Terrain); RenderTerrain(true);
    }
    if (!g_Camera.TopViewEnable && renderDroppedItems)
        { FRAME_PROFILE(Items); RenderItems(); }

    RenderFishs();
    RenderMount();

    if (renderWeatherEffects)
        RenderLeaves();

    if (!gMapManager.InChaosCastle())
        ThePetProcess().RenderPets();

    if (renderEffects)
        RenderBoids(true);

    if (renderStatic)
        { FRAME_PROFILE(Objects); RenderObjects_AfterCharacter(); }

    RenderJoints(byWaterMap);

    if (renderEffects)
    {
        FRAME_PROFILE(Effects);
        RenderEffects();
        RenderBlurs();
    }
    CheckSprites();
    BeginSprite();

    if (ShouldRenderLeaves())
    {
        RenderLeaves();
    }

    RenderSprites();
    RenderParticles();

    if (IsWaterTerrain() == false)
    {
        RenderPoints(byWaterMap);
    }

    EndSprite();

    RenderAfterEffects();

    if (IsWaterTerrain() == true)
    {
        byWaterMap = 2;

        EndOpengl();
        BeginOpengl(0, 0, width, height);
        RenderWaterTerrain();
        RenderJoints(byWaterMap);
        RenderEffects(true);
        RenderBlurs();
        CheckSprites();
        BeginSprite();

        if (gMapManager.WorldActive == WD_2DEVIAS && HeroTile != 3 && HeroTile < 10)
            RenderLeaves();

        RenderSprites(byWaterMap);
        RenderParticles(byWaterMap);
        RenderPoints(byWaterMap);

        EndSprite();
        EndOpengl();

        BeginOpengl(0, 0, width, height);
    }

    if (gMapManager.InBattleCastle())
    {
        if (battleCastle::InBattleCastle2(Hero->Object.Position))
        {
            battleCastle::EndFog();
        }
    }
}

/**
 * @brief Renders UI elements and overlays for the main scene.
 */
static void RenderMainSceneUI()
{
    Input::Selection::SelectObjects();
    UpdateDeviasPriestSpeech();

    BeginBitmap();
    // World HUD notifications belong below every legacy/NewUI window.
    KillNotificationClient::Render();
    RenderObjectDescription();

    if (g_Camera.TopViewEnable == false)
    {
        RenderInterface(true);
    }
    RenderTournamentInterface();
    EndBitmap();

    g_pPartyManager->Render();
    g_pNewUISystem->Render();

    BeginBitmap();
    RenderInfomation();
    RenderDeviasPriestSpeech();

#ifdef ENABLE_EDIT
    RenderDebugWindow();
#endif //ENABLE_EDIT

    EndBitmap();
    BeginBitmap();

    RenderCursor();

    EndBitmap();
}

/**
 * @brief Main rendering function for the game scene.
 *
 * Orchestrates the complete rendering pipeline:
 * 1. Determines camera position based on camera mode
 * 2. Sets up viewport and clear color
 * 3. Renders 3D world (terrain, objects, characters, effects)
 * 4. Renders UI and overlays
 *
 * @return true if rendering succeeded, false if rendering was skipped
 */
bool RenderMainScene()
{
    if (EnableMainRender == false)
    {
        return false;
    }

    if ((LoadingWorld) > 30)
    {
        return false;
    }

    // Per-camera fog default: Orbital uses fog (noticeable at longer view distances),
    // Default camera's fog zone sits at/beyond its far clip and reads as visual noise,
    // so fog is off by default for Default. DevEditor can override either below.
    if (ICamera* active = CameraManager::Instance().GetActiveCamera())
    {
        const char* name = active->GetName();
        if (strcmp(name, "Default") == 0)       FogEnable = false;
        else if (strcmp(name, "Orbital") == 0)  FogEnable = true;
    }

#ifdef _EDITOR
    // DevEditor override: allow forcing fog on/off for debugging.
    if (ICamera* active = CameraManager::Instance().GetActiveCamera())
    {
        const char* name = active->GetName();
        if (DevEditor_IsCameraFogOverrideEnabled(name))
            FogEnable = DevEditor_GetCameraFogOverrideValue(name);
    }
#endif

    vec3_t cameraPos;
    int width, height;
    BYTE byWaterMap;

    // Determine camera position
    if (MoveMainCamera() == true)
    {
        VectorCopy(Hero->Object.StartPosition, cameraPos);
    }
    else
    {
        g_pCatapultWindow->GetCameraPos(cameraPos);

        if (g_Direction.IsDirection() && g_Direction.m_bDownHero == false)
        {
            g_Direction.GetCameraPosition(cameraPos);
        }
    }

    SetupMainSceneViewport(width, height, byWaterMap, cameraPos);
    RenderGameWorld(byWaterMap, width, height);

#ifdef _EDITOR
    // Render spectated camera frustum wireframe when in FreeFly mode
    CameraMode cameraMode = CameraManager::Instance().GetCurrentMode();
    if (cameraMode == CameraMode::FreeFly)
    {
        ICamera* spectated = CameraManager::Instance().GetSpectatedCamera();
        if (spectated)
            RenderFrustumWireframe(spectated->GetFrustum());
    }

    // DEBUG: Render mouse ray as a visible line (magenta) from MousePosition to MouseTarget
    {
        GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
        GLboolean tex2d = glIsEnabled(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_TEXTURE_2D);
        glLineWidth(2.0f);
        glColor4f(1.0f, 0.0f, 1.0f, 1.0f);
        glBegin(GL_LINES);
        glVertex3fv(MousePosition);
        glVertex3fv(MouseTarget);
        glEnd();

        // Draw a small cross at MousePosition (green)
        constexpr float S = 30.0f;
        glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
        glBegin(GL_LINES);
        glVertex3f(MousePosition[0] - S, MousePosition[1], MousePosition[2]);
        glVertex3f(MousePosition[0] + S, MousePosition[1], MousePosition[2]);
        glVertex3f(MousePosition[0], MousePosition[1] - S, MousePosition[2]);
        glVertex3f(MousePosition[0], MousePosition[1] + S, MousePosition[2]);
        glEnd();

        glLineWidth(1.0f);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        if (depthTest) glEnable(GL_DEPTH_TEST);
        if (tex2d) glEnable(GL_TEXTURE_2D);
    }

    // DEBUG: Log ray state on left click (debounced to one log per click)
    {
        extern bool MouseLButtonPush;
        static bool wasPressed = false;
        if (MouseLButtonPush && !wasPressed)
        {
            wasPressed = true;
            extern int MouseX, MouseY;
            CAMERA_LOG("[RAY] Click: Mouse=(%d,%d) Pos=(%.0f,%.0f,%.0f) Target=(%.0f,%.0f,%.0f) "
                       "CamPos=(%.0f,%.0f,%.0f) PerspX=%.6f PerspY=%.6f CenterX=%d CenterY=%d FOV=%.1f ViewFar=%.0f",
                       MouseX, MouseY,
                       MousePosition[0], MousePosition[1], MousePosition[2],
                       MouseTarget[0], MouseTarget[1], MouseTarget[2],
                       g_Camera.Position[0], g_Camera.Position[1], g_Camera.Position[2],
                       g_Camera.PerspectiveX, g_Camera.PerspectiveY,
                       g_Camera.ScreenCenterX, g_Camera.ScreenCenterY,
                       g_Camera.FOV, g_Camera.ViewFar);
        }
        if (!MouseLButtonPush)
            wasPressed = false;
    }
#endif

    RenderDayNightCycleOverlay();

    RenderMainSceneUI();


    EndOpengl();

    return true;
}
