///////////////////////////////////////////////////////////////////////////////
// CharacterScene.cpp - Character selection scene implementation
///////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>#include "CharacterScene.h"
#include "SceneCore.h"
#include "Character/AccountCharacterList.h"
#include "Character/AccountCharacterPaging.h"
#include "Character/AccountCompanionClient.h"
#include "Character/CharacterManager.h"
#include "World/MapInfra/MapManager.h"
#include "World/GameMaps/LoginSceneEnvironment.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Sprites/GlobalBitmap.h"#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Engine/Object/ZzzInterface.h"
#include "Input/Selection.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Render/Effects/ZzzEffect.h"
#include "Engine/AI/GOBoid.h"
#include "GameLogic/Pets/w_PetProcess.h"
#include "UI/Legacy/UIMng.h"
#include "Core/Input/Input.h"
#include "Network/Server/WSclient.h"
#include "Network/Server/CSMapServer.h"
#include "Core/Utilities/Log/muConsoleDebug.h"
#include "UI/NewUI/NewUISystem.h"
#include "Audio/DSPlaySound.h"
#include "App/Platform/Windows/Winmain.h"
#include "SceneCommon.h"
#include "Camera/CameraUtility.h"
#include "Camera/CameraManager.h"
#include "Engine/Object/ZzzOpenData.h"
#include "LoginScene.h"
#include "Camera/CameraProjection.h"
#ifdef _EDITOR
#include "Camera/CameraMode.h"
#include "Camera/FrustumRenderer.h"
#include "Map/MapEditorSession.h"
#endif

#include <string>

// External declarations
extern EGameScene SceneFlag;
extern int ErrorMessage;
extern CHARACTER CharacterView;
extern int SelectedCharacter;
extern int g_iKeyPadEnable;
extern int g_iChatInputType;
extern CUITextInputBox* g_pSinglePasswdInputBox;
extern BOOL g_bIMEBlock;
extern DWORD g_dwBKConv;
extern DWORD g_dwBKSent;
extern HWND g_hWnd;
extern CErrorReport g_ErrorReport;
extern double WorldTime;
extern int MouseX;
extern int MouseY;
extern vec3_t MouseTarget;

// Forward declaration
BOOL Util_CheckOption(std::wstring lpszCommandLine, wchar_t cOption, std::wstring& lpszString);

void StartGame()
{
    {
        g_ErrorReport.Write(
            L"[StartGame] selected=%d currentPage=%d\r\n",
            SelectedHero,
            AccountCharacterPaging::GetCurrentPage() + 1);

        if (SelectedHero < 0 || SelectedHero >= AccountCharacterList::NativeVisibleSlots)
        {
            g_ErrorReport.Write(L"[StartGame] ignored invalid selected slot\r\n");
            return;
        }

        if (!CharactersClient[SelectedHero].Object.Live || CharactersClient[SelectedHero].ID[0] == L'\0')
        {
            g_ErrorReport.Write(
                L"[StartGame] ignored empty selected slot=%d live=%d\r\n",
                SelectedHero,
                CharactersClient[SelectedHero].Object.Live ? 1 : 0);
            return;
        }

        if (CTLCODE_01BLOCKCHAR & CharactersClient[SelectedHero].CtlCode)
        {
            g_ErrorReport.Write(
                L"[StartGame] blocked character name=\"%ls\"\r\n",
                CharactersClient[SelectedHero].ID);
            CUIMng::Instance().PopUpMsgWin(MESSAGE_BLOCKED_CHARACTER);
        }
        else
        {
            const bool isGuardSwitch = AccountCompanionClient::HasPendingSwitchTarget();
            AccountCompanionClient::ResetLocalState();
            CharacterAttribute->Level = CharactersClient[SelectedHero].Level;
            CharacterAttribute->Class = CharactersClient[SelectedHero].Class;
            CharacterAttribute->Skin = CharactersClient[SelectedHero].Skin;
            ::wcscpy_s(CharacterAttribute->Name, MAX_USERNAME_SIZE + 1, CharactersClient[SelectedHero].ID);

            ::ReleaseCharacterSceneData();
            InitLoading = false;
            SceneFlag = LOADING_SCENE;

            g_ErrorReport.Write(
                L"[StartGame] loading name=\"%ls\" level=%d class=%d\r\n",
                CharacterAttribute->Name,
                CharacterAttribute->Level,
                CharacterAttribute->Class);

            if (isGuardSwitch)
            {
                AccountCompanionClient::ClearPendingSwitchTarget();
            }
        }
    }
}

void CreateCharacterScene()
{
    g_pNewUIMng->ResetActiveUIObj();

    EnableMainRender = true;
    MouseOnWindow = false;
    ErrorMessage = 0;

    gMapManager.WorldActive = WD_74NEW_CHARACTER_SCENE;

    gMapManager.LoadWorld(gMapManager.WorldActive);
    OpenCharacterSceneData();

    CreateCharacterPointer(&CharacterView, MODEL_FACE + 1, 0, 0);
    CharacterView.Class = CLASS_KNIGHT;
    CharacterView.SkinIndex = gCharacterManager.GetSkinModelIndex(CLASS_KNIGHT);
    CharacterView.Object.Kind = 0;

    SelectedHero = -1;
    CUIMng::Instance().CreateCharacterScene();
    AccountCharacterPaging::RefreshVisibleCharacters();

    ClearInventory();
    CharacterAttribute->SkillNumber = 0;

    for (int i = 0; i < MAX_MAGIC; i++)
        CharacterAttribute->Skill[i] = AT_SKILL_UNDEFINED;

    for (int i = EQUIPMENT_WEAPON_RIGHT; i < EQUIPMENT_HELPER; i++)
        CharacterMachine->Equipment[i].Level = 0;

    g_pNewUISystem->HideAll();

    g_iKeyPadEnable = 0;
    GuildInputEnable = false;
    TabInputEnable = false;
    GoldInputEnable = false;
    InputEnable = true;
    ClearInput();
    InputIndex = 0;
    InputTextWidth = 90;
    InputNumber = 1;

    for (int i = 0; i < MAX_WHISPER; i++)
    {
        g_pChatListBox->AddText(L"", L"", SEASON3B::TYPE_WHISPER_MESSAGE);
    }

    HIMC hIMC = ImmGetContext(g_hWnd);
    DWORD Conversion, Sentence;

    Conversion = IME_CMODE_NATIVE;
    Sentence = IME_SMODE_NONE;

    g_bIMEBlock = FALSE;
    RestoreIMEStatus();
    ImmSetConversionStatus(hIMC, Conversion, Sentence);
    ImmGetConversionStatus(hIMC, &g_dwBKConv, &g_dwBKSent);
    SaveIMEStatus();
    ImmReleaseContext(g_hWnd, hIMC);
    g_bIMEBlock = TRUE;

    g_ErrorReport.Write(L"> Character scene init success.\r\n");
}

namespace
{
    bool TryAutoSelectPendingGuardSwitch()
    {
        wchar_t targetName[MAX_USERNAME_SIZE + 1] = {};
        if (!AccountCompanionClient::GetPendingSwitchTarget(targetName, MAX_USERNAME_SIZE + 1))
        {
            return false;
        }

        const int accountSlot = AccountCharacterList::FindSlotByName(targetName);
        if (accountSlot < 0)
        {
            return false;
        }

        if (!AccountCharacterPaging::IsSlotOnCurrentPage(accountSlot))
        {
            AccountCharacterPaging::SetPageForSlot(accountSlot);
            CUIMng::Instance().m_CharSelMainWin.UpdateDisplay();
            CUIMng::Instance().m_CharInfoBalloonMng.UpdateDisplay();
            return true;
        }

        const int visibleIndex = accountSlot - AccountCharacterPaging::GetFirstSlotOnCurrentPage();
        if (visibleIndex < 0 || visibleIndex >= AccountCharacterPaging::SlotsPerPage)
        {
            AccountCompanionClient::ClearPendingSwitchTarget();
            return false;
        }

        CHARACTER& character = CharactersClient[visibleIndex];
        if (!character.Object.Live || std::wcscmp(character.ID, targetName) != 0)
        {
            AccountCharacterPaging::RefreshVisibleCharacters();
            return true;
        }

        SelectedCharacter = visibleIndex;
        SelectedHero = visibleIndex;
        CUIMng::Instance().m_CharSelMainWin.UpdateDisplay();
        ::StartGame();
        return true;
    }
}

void NewMoveCharacterScene()
{
    if (CurrentProtocolState < RECEIVE_CHARACTERS_LIST)
    {
        return;
    }

    if (!InitCharacterScene)
    {
        InitCharacterScene = true;
        CreateCharacterScene();
        CUIMng::Instance().m_CharSelMainWin.UpdateDisplay();
        CUIMng::Instance().m_CharInfoBalloonMng.UpdateDisplay();
    }

    if (TryAutoSelectPendingGuardSwitch())
    {
        g_ConsoleDebug->UpdateMainScene();
        return;
    }

    InitTerrainLight();
    MoveObjects();
    MoveMounts();

    // Update camera BEFORE checking character visibility
    MoveCamera();

    // Now check character visibility with updated frustum
    MoveCharactersClient();
    MoveCharacterClient(&CharacterView);

    MoveEffects();
    MoveJoints();
    MoveParticles();
    MoveBoids();

    ThePetProcess().UpdatePets();

#ifdef _EDITOR
    g_MapEditorSession.Update();
#endif

#if defined _DEBUG || defined FOR_WORK
    std::wstring lpszTemp = { 0 };
    if (::Util_CheckOption(::GetCommandLine(), L'c', lpszTemp))
    {
        SelectedHero = ::_wtoi(lpszTemp.c_str());
        ::StartGame();
    }
#endif

    CInput& rInput = CInput::Instance();
    CUIMng& rUIMng = CUIMng::Instance();

    if (rInput.IsKeyDown(VK_RETURN))
    {
        if (!(rUIMng.m_MsgWin.IsShow() || rUIMng.m_CharMakeWin.IsShow()
            || rUIMng.m_SysMenuWin.IsShow() || rUIMng.m_OptionWin.IsShow())
            && SelectedHero > -1 && SelectedHero < AccountCharacterList::NativeVisibleSlots)
        {
            ::PlayBuffer(SOUND_CLICK01);

            if (SelectedCharacter >= 0)
                SelectedHero = SelectedCharacter;

            ::StartGame();
        }
    }
    // ESC menu toggle is handled by CUIMng::Update()

    if (rUIMng.IsCursorOnUI())
    {
        return;
    }

    if (rInput.IsLBtnDbl() && rUIMng.m_CharSelMainWin.IsShow())
    {
        if (SelectedCharacter < 0 || SelectedCharacter >= AccountCharacterList::NativeVisibleSlots)
        {
            return;
        }

        SelectedHero = SelectedCharacter;
        ::StartGame();
    }
    else if (rInput.IsLBtnDn())
    {
        if (SelectedCharacter < 0 || SelectedCharacter >= AccountCharacterList::NativeVisibleSlots)
            SelectedHero = -1;
        else
            SelectedHero = SelectedCharacter;
        rUIMng.m_CharSelMainWin.UpdateDisplay();
    }

    g_ConsoleDebug->UpdateMainScene();
}

/**
 * @brief Sets up viewport and character positioning for character selection scene.
 *
 * @param outWidth Output screen width
 * @param outHeight Output screen height
 */
static void SetupCharacterSceneViewport(int& outWidth, int& outHeight)
{
    constexpr float kSelectedCharacterHeight = 172.0f;

    MoveMainCamera();
    MoveCamera();

    glColor3f(1.f, 1.f, 1.f);
    outHeight = REFERENCE_HEIGHT;
    outWidth = GetScreenWidth();

    glClearColor(0.035f, 0.018f, 0.075f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    BeginOpengl(0, 0, REFERENCE_WIDTH, REFERENCE_HEIGHT);

    // Build global frustum arrays for TestFrustrum/TestFrustrum2D
    // Must be called after BeginOpengl (needs GL matrices) in every scene that renders terrain/objects
    {
        vec3_t cameraPos;
        VectorCopy(g_Camera.Position, cameraPos);
        CreateFrustrum((float)outWidth / (float)REFERENCE_WIDTH, 1.0f, cameraPos);
    }

    CameraProjection::ScreenToWorldRay(g_Camera, MouseX, MouseY, MouseTarget);

    // Reset character positions and lighting
    const bool showSelectedCharacter = !CUIMng::Instance().m_CharMakeWin.IsShow();
    for (int i = 0; i < AccountCharacterList::NativeVisibleSlots; i++)
    {
        CharactersClient[i].Object.Position[2] = kSelectedCharacterHeight;
        Vector(0.0f, 0.0f, 0.0f, CharactersClient[i].Object.Light);
        CharactersClient[i].Object.Visible = showSelectedCharacter && i == SelectedHero;
    }
}

/**
 * @brief Applies lighting to the currently selected character.
 */
static void ApplySelectedCharacterLighting()
{
    if (SelectedHero < 0 || SelectedHero >= AccountCharacterList::NativeVisibleSlots)
        return;

    OBJECT* o = &CharactersClient[SelectedHero].Object;
    if (!o->Live)
        return;

    EnableAlphaBlend();
    vec3_t Light;
    Vector(1.0f, 1.0f, 1.0f, Light);
    Vector(1.0f, 1.0f, 1.0f, o->Light);
    AddTerrainLight(o->Position[0], o->Position[1], Light, 1, PrimaryTerrainLight);
    DisableAlphaBlend();
}

/**
 * @brief Renders all 3D elements for the character selection scene.
 */
static void RenderCharacterScene3D()
{
    RenderTerrain(false);
    RenderObjects();
#ifdef _EDITOR
    g_MapEditorSession.RenderSelection();
#endif
    LoginSceneEnvironment::RenderCharacterFountainEffects();
    RenderCharactersClient();

    if (!CUIMng::Instance().IsCursorOnUI())
        Input::Selection::SelectObjects();

    RenderMount();
    RenderBlurs();
    RenderJoints();
    RenderEffects();
    ThePetProcess().RenderPets();
    RenderBoids();
    RenderObjects_AfterCharacter();
    CheckSprites();
}

/**
 * @brief Renders special effects for the selected character (aurora, particles).
 */
static void RenderSelectedCharacterEffects()
{
    if (CUIMng::Instance().m_CharMakeWin.IsShow())
        return;

    if (SelectedHero < 0 || SelectedHero >= AccountCharacterList::NativeVisibleSlots)
        return;

    OBJECT* o = &CharactersClient[SelectedHero].Object;
    if (!o->Live)
        return;

    // Aurora luminance pulses between (BASE - AMPLITUDE) and (BASE + AMPLITUDE)
    // with period ~2π / FREQUENCY ms.
    constexpr float AURORA_FREQUENCY = 0.0015f;
    constexpr float AURORA_AMPLITUDE = 0.3f;
    constexpr float AURORA_BASE_LUMINANCE = 0.5f;
    constexpr float AURORA_SOLE_OFFSET = 1.0f;

    vec3_t vLight;
    Vector(1.0f, 1.0f, 1.f, vLight);
    float fLumi = sinf(WorldTime * AURORA_FREQUENCY) * AURORA_AMPLITUDE + AURORA_BASE_LUMINANCE;
    Vector(fLumi * vLight[0], fLumi * vLight[1], fLumi * vLight[2], vLight);

    const float auraHeight =
        o->Position[2] - RequestTerrainHeight(o->Position[0], o->Position[1]) + AURORA_SOLE_OFFSET;

    EnableAlphaBlend();
    RenderTerrainAlphaBitmap(BITMAP_GM_AURORA, o->Position[0], o->Position[1], 1.8f, 1.8f, vLight, WorldTime * 0.01f, 1.0f, auraHeight);
    RenderTerrainAlphaBitmap(BITMAP_GM_AURORA, o->Position[0], o->Position[1], 1.2f, 1.2f, vLight, -WorldTime * 0.01f, 1.0f, auraHeight);
    DisableAlphaBlend();

    g_csMapServer.SetHeroID((wchar_t*)CharactersClient[SelectedHero].ID);
}

/**
 * @brief Renders UI elements for character selection scene.
 */
static void RenderCharacterSceneUI()
{
    BeginSprite();
    RenderSprites();
    RenderParticles();
    RenderPoints();
    EndSprite();

    BeginBitmap();
    LoginSceneEnvironment::RenderCharacterSceneSkyEffects();
    RenderInfomation();

#ifdef ENABLE_EDIT
    RenderDebugWindow();
#endif

    EndBitmap();
}

/**
 * @brief Main rendering function for the character selection scene.
 *
 * Orchestrates the complete rendering pipeline:
 * 1. Viewport setup and character positioning
 * 2. Character lighting application
 * 3. Character height adjustment
 * 4. 3D world rendering
 * 5. Selected character effects
 * 6. UI rendering
 *
 * @param hDC Device context (unused but required by interface)
 * @return true if rendering succeeded, false if scene not initialized
 */
bool NewRenderCharacterScene(HDC hDC)
{
    if (AccountCompanionClient::HasPendingSwitchTarget())
    {
        return true;
    }

    if (!InitCharacterScene)
    {
        return false;
    }
    if (CurrentProtocolState < RECEIVE_CHARACTERS_LIST)
    {
        return false;
    }

    FogEnable = true;

    int width, height;
    SetupCharacterSceneViewport(width, height);
    ApplySelectedCharacterLighting();
    RenderCharacterScene3D();
    RenderSelectedCharacterEffects();

#ifdef _EDITOR
    if (CameraManager::Instance().GetCurrentMode() == CameraMode::FreeFly)
    {
        ICamera* spectated = CameraManager::Instance().GetSpectatedCamera();
        if (spectated)
            RenderFrustumWireframe(spectated->GetFrustum());
    }
#endif

    RenderCharacterSceneUI();

    // Handle option window in login/character scenes (can't use full g_pNewUISystem update)
    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_OPTION))
    {
        g_pOption->UpdateMouseEvent();
        g_pOption->UpdateKeyEvent();
        BeginBitmap();
        g_pOption->Render();
        EndBitmap();
    }

    EndOpengl();

    return true;
}
