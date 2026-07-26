//*****************************************************************************
// File: CharMakeWin.cpp
//*****************************************************************************

#include "stdafx.h"
#include "CharMakeWin.h"
#include "Core/Input/Input.h"
#include "UI/Legacy/UIMng.h"
#include "Render/Models/ZzzBMD.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Audio/DSPlaySound.h"
#include "Engine/AI/ZzzAI.h"
#include "Scenes/SceneCore.h"
#include "UI/Legacy/UIControls.h"
#include "I18N/All.h"

#include "App/Platform/Windows/Local.h"
#include "AccountCharacterList.h"
#include "CharacterManager.h"
#include "CharacterSceneTextures.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <string>

namespace
{
    constexpr std::array<DWORD, BTN_IMG_MAX> kJobButtonColors{
        CLRDW_BR_GRAY, CLRDW_WHITE, CLRDW_WHITE, CLRDW_GRAY,
        ARGB(255, 255, 222, 128), CLRDW_WHITE, ARGB(255, 255, 235, 178), CLRDW_GRAY
    };

    constexpr std::array<int, MAX_CLASS> kClassButtonTextIds{
        20, 21, 22, 23, 24, 1687, 3150
    };

    constexpr std::array<CLASS_TYPE, MAX_CLASS> kClassDisplayOrder{
        CLASS_WIZARD,
        CLASS_KNIGHT,
        CLASS_ELF,
        CLASS_SUMMONER,
        CLASS_RAGEFIGHTER,
        CLASS_DARK,
        CLASS_DARK_LORD,
    };

    constexpr std::array<const wchar_t*, CMW_FUTURE_CLASS_COUNT> kFutureClassNames{
        L"Grow Lancer",
        L"Mago da Runa",
        L"Slayer",
        L"Gun Crusher",
        L"Mago Branco: Kundun",
        L"Maga: Lemuria",
        L"Cavaleiro Ilusorio",
        L"Alchemist",
    };

    constexpr int kClassBeamFrameCount = 8;
    constexpr int kClassBeamWidth = 415;
    constexpr int kClassBeamHeight = 120;
    constexpr int kClassBeamOffsetX = -126;
    constexpr int kClassBeamOffsetY = -44;
    constexpr double kClassBeamFrameDelay = 72.0;
    constexpr double kClassBeamDuration = 620.0;

    constexpr std::size_t kMinCharacterNameLength = 4;
    constexpr std::size_t kMaxCharacterNameUtf8Bytes = MAX_USERNAME_SIZE;

    constexpr int kSummonerDescriptionTextId = 1690;
    constexpr int kRageFighterDescriptionTextId = 3152;
    constexpr int kDefaultDescriptionBase = 1705;

    constexpr int kStatLabelBaseId = 1701;
    constexpr int kStatLineSpacing = 43;
    constexpr int kStatYOffset = 25;
    constexpr int kStatValueOffset = 75;
    constexpr int kStatTextOffsetX = 18;
    constexpr int kDarkLordStatHeight = 220;
    constexpr int kDefaultStatHeight = 180;
    constexpr const wchar_t* kDarkLordLeadershipStatValue = L"25";
    constexpr int kDarkLordLeadershipTextId = 1738;

    constexpr int kDecorationOffsetX = -82;
    constexpr int kDecorationOffsetY = -159;
    constexpr int kDecorationWidth = 894;
    constexpr int kDecorationHeight = 742;
    constexpr int kDecorationTitleOffsetX = 381;
    constexpr int kDecorationTitleOffsetY = 55;
    constexpr int kJobButtonOffsetX = 440;
    constexpr int kJobButtonStartY = -114;
    constexpr int kJobButtonWidth = 145;
    constexpr int kJobButtonHeight = 31;
    constexpr int kActionButtonOffsetX = 360;
    constexpr int kActionButtonOffsetY = 297;
    constexpr int kActionButtonSpacing = 36;
    constexpr int kInputSpriteOffsetX = 30;
    constexpr int kInputSpriteOffsetY = 300;
    constexpr int kInputTextOffsetX = 10;
    constexpr int kInputTextOffsetY = 8;
    constexpr int kDescSpriteOffsetX = -98;
    constexpr int kDescSpriteOffsetY = 300;
    constexpr int kDescriptionTextOffsetX = 68;
    constexpr int kDescriptionTextOffsetY = 45;
    constexpr int kDescriptionLineSpacing = 18;
    constexpr int kStatSpriteOffsetX = 315;
    constexpr int kStatSpriteOffsetY = -95;
    constexpr int kCharacterViewportOffsetX = -57;

    struct ClassStats
    {
        std::array<const wchar_t*, 4> values;
    };

    constexpr std::array<ClassStats, MAX_CLASS> kClassStatTable{ {
        ClassStats{ { L"18", L"18", L"15", L"30" } }, // Knight
        ClassStats{ { L"28", L"20", L"25", L"10" } }, // Wizard
        ClassStats{ { L"22", L"25", L"20", L"15" } }, // Elf
        ClassStats{ { L"26", L"26", L"26", L"26" } }, // Magic Gladiator
        ClassStats{ { L"26", L"20", L"20", L"15" } }, // Dark Lord
        ClassStats{ { L"21", L"21", L"18", L"23" } }, // Summoner
        ClassStats{ { L"32", L"27", L"25", L"20" } }, // Rage Fighter
    } };

    struct ClassRenderParameters
    {
        bool overrideAngle;
        float angleX;
        float angleY;
        float angleZ;
        float scale;
        float positionOffsetX;
        float positionOffsetZ;
    };

    constexpr ClassRenderParameters GetRenderParameters(CLASS_TYPE classType)
    {
        switch (classType)
        {
        case CLASS_KNIGHT:
            return { true, 0.0f, 0.0f, -12.0f, 6.05f, 0.0f, 0.0f };
        case CLASS_WIZARD:
            return { true, 0.0f, 0.0f, -40.0f, 5.9f, 0.0f, 0.0f };
        case CLASS_ELF:
            return { true, 8.0f, 0.0f, 5.0f, 9.1f, 4.8f, 0.0f };
        case CLASS_DARK:
            return { true, 8.0f, 0.0f, -13.0f, 6.0f, 0.0f, 1.8f };
        case CLASS_DARK_LORD:
            return { true, 8.0f, 0.0f, -18.0f, 6.0f, 0.0f, 0.0f };
        case CLASS_SUMMONER:
            return { true, 2.0f, 0.0f, 2.0f, 9.1f, 4.8f, 4.0f };
        case CLASS_RAGEFIGHTER:
            return { false, 0.0f, 0.0f, 0.0f, 6.0f, 9.8f, -7.5f };
        default:
            return { false, 0.0f, 0.0f, 0.0f, 6.0f, 0.0f, 0.0f };
        }
    }

    constexpr int ResolveDescriptionTextId(CLASS_TYPE selectedClass)
    {
        if (selectedClass == CLASS_SUMMONER)
            return kSummonerDescriptionTextId;
        if (selectedClass == CLASS_RAGEFIGHTER)
            return kRageFighterDescriptionTextId;
        return kDefaultDescriptionBase + selectedClass;
    }

    bool IsAllowedCharacterNameChar(wchar_t value)
    {
        if ((value >= L'0' && value <= L'9')
            || (value >= L'A' && value <= L'Z')
            || (value >= L'a' && value <= L'z')
            || value == L'_'
            || value == L'-')
        {
            return true;
        }

        return value > 0x7F && !std::iswspace(value) && !std::iswcntrl(value);
    }

    bool HasInvalidCharacterNameChar(const std::wstring& name)
    {
        return std::any_of(name.begin(), name.end(), [](wchar_t value) {
            return !IsAllowedCharacterNameChar(value);
        });
    }

    std::size_t Utf8ByteCount(const std::wstring& value)
    {
        std::size_t bytes = 0;
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            std::uint32_t codePoint = static_cast<std::uint32_t>(value[index]);
            if (codePoint >= 0xD800 && codePoint <= 0xDBFF && index + 1 < value.size())
            {
                const auto low = static_cast<std::uint32_t>(value[index + 1]);
                if (low >= 0xDC00 && low <= 0xDFFF)
                {
                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                    ++index;
                }
            }

            if (codePoint <= 0x7F)
                bytes += 1;
            else if (codePoint <= 0x7FF)
                bytes += 2;
            else if (codePoint <= 0xFFFF)
                bytes += 3;
            else
                bytes += 4;
        }

        return bytes;
    }

    void RenderBitmapAt(int texture, int x, int y, int width, int height)
    {
        const float rateX = g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f;
        const float rateY = g_fScreenRate_y > 0.0f ? g_fScreenRate_y : 1.0f;
        ::RenderBitmap(
            texture,
            static_cast<float>(x) / rateX,
            static_cast<float>(y) / rateY,
            static_cast<float>(width) / rateX,
            static_cast<float>(height) / rateY);
    }

    void RenderTextAt(int x, int y, int width, const wchar_t* text, DWORD color, int align = RT3_SORT_LEFT)
    {
        const float rateX = g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f;
        const float rateY = g_fScreenRate_y > 0.0f ? g_fScreenRate_y : 1.0f;
        ::EnableAlphaTest();
        g_pRenderText->SetBgColor(0, 0, 0, 0);
        g_pRenderText->SetTextColor(color);
        g_pRenderText->RenderText(
            static_cast<int>(x / rateX),
            static_cast<int>(y / rateY),
            text,
            static_cast<int>(width / rateX),
            0,
            align);
    }
}

#define	CMW_OK		0
#define	CMW_CANCEL	1



extern int g_iChatInputType;
extern CUITextInputBox* g_pSingleTextInputBox;

void MoveCharacterCamera(vec3_t Origin, vec3_t Position, vec3_t Angle);

CCharMakeWin::CCharMakeWin()
    : m_classSelectionBeamTime(0.0)
{
}

CCharMakeWin::~CCharMakeWin()
{
}

void CCharMakeWin::Create()
{
    CInput& rInput = CInput::Instance();
    CWin::Create(rInput.GetScreenWidth(), rInput.GetScreenHeight(), -2);

    m_winBack.Create(454, 406, -2);

    m_asprBack[CMW_SPR_INPUT].Create(300, 30, CharacterSceneTextures::CreateInput);

    m_asprBack[CMW_SPR_STAT].Create(132, kDefaultStatHeight);

    m_asprBack[CMW_SPR_DESC].Create(430, 125);

    for (int spriteIndex = CMW_SPR_STAT; spriteIndex < CMW_SPR_MAX; ++spriteIndex)
    {
        m_asprBack[spriteIndex].SetAlpha(143);
        m_asprBack[spriteIndex].SetColor(0, 0, 0);
    }

    std::array<DWORD, BTN_IMG_MAX> jobButtonColors = kJobButtonColors;
    for (int classIndex = 0; classIndex < MAX_CLASS; ++classIndex)
    {
        m_abtnJob[classIndex].Create(
            kJobButtonWidth,
            kJobButtonHeight,
            CharacterSceneTextures::CreateClassButton,
            8,
            1,
            2,
            3,
            4,
            5,
            6,
            7);
        const int textId = kClassButtonTextIds[classIndex];
        m_abtnJob[classIndex].SetText(I18N::Game::Lookup(textId), jobButtonColors.data());
        CWin::RegisterButton(&m_abtnJob[classIndex]);
    }

    std::array<SFrameCoord, 8> classFrames{};
    for (std::size_t frame = 0; frame < classFrames.size(); ++frame)
    {
        classFrames[frame].nX = 0;
        classFrames[frame].nY = static_cast<int>(frame) * kJobButtonHeight;
    }
    for (auto& futureClass : m_asprFutureJob)
    {
        futureClass.Create(
            kJobButtonWidth,
            kJobButtonHeight,
            CharacterSceneTextures::CreateClassButton,
            static_cast<int>(classFrames.size()),
            classFrames.data());
        futureClass.SetAction(0, static_cast<int>(classFrames.size()) - 1);
        futureClass.SetNowFrame(3);
    }

    std::array<SFrameCoord, kClassBeamFrameCount> beamFrames{};
    for (std::size_t frame = 0; frame < beamFrames.size(); ++frame)
    {
        beamFrames[frame].nX = 0;
        beamFrames[frame].nY = static_cast<int>(frame) * kClassBeamHeight;
    }
    m_sprClassSelectionBeam.Create(
        kClassBeamWidth,
        kClassBeamHeight,
        CharacterSceneTextures::CreateClassBeam,
        static_cast<int>(beamFrames.size()),
        beamFrames.data());
    m_sprClassSelectionBeam.SetAction(0, kClassBeamFrameCount - 1, kClassBeamFrameDelay, false);

    std::array<DWORD, BTN_IMG_MAX> actionButtonColors{
        CLRDW_BR_GRAY, CLRDW_WHITE, CLRDW_WHITE, CLRDW_GRAY,
        CLRDW_BR_GRAY, CLRDW_WHITE, CLRDW_WHITE, CLRDW_GRAY
    };
    for (int i = 0; i < 2; ++i)
    {
        m_aBtn[i].Create(75, 35, CharacterSceneTextures::CreateActionButton, 4, 1, 2, 3);
        CWin::RegisterButton(&m_aBtn[i]);
    }
    m_aBtn[CMW_OK].SetText(I18N::Game::OK, actionButtonColors.data());
    m_aBtn[CMW_CANCEL].SetText(I18N::Game::Cancel, actionButtonColors.data());

    std::fill(&m_aszJobDesc[0][0],
        &m_aszJobDesc[0][0] + (CMW_DESC_LINE_MAX * CMW_DESC_ROW_MAX), L'\0');
    m_nDescLine = 0;

    m_nSelJob = CLASS_KNIGHT;
    m_abtnJob[m_nSelJob].SetCheck(true);

    UpdateDisplay();
}

void CCharMakeWin::PreRelease()
{
    for (int i = 0; i < CMW_SPR_MAX; ++i)
        m_asprBack[i].Release();
    for (auto& futureClass : m_asprFutureJob)
        futureClass.Release();
    m_sprClassSelectionBeam.Release();
    m_winBack.Release();
}

void CCharMakeWin::SetPosition(int nXCoord, int nYCoord)
{
    m_winBack.SetPosition(nXCoord, nYCoord);

    m_asprBack[CMW_SPR_STAT].SetPosition(
        nXCoord + kStatSpriteOffsetX,
        nYCoord + kStatSpriteOffsetY);

    for (std::size_t row = 0; row < kClassDisplayOrder.size(); ++row)
    {
        const CLASS_TYPE classType = kClassDisplayOrder[row];
        m_abtnJob[classType].SetPosition(
            nXCoord + kJobButtonOffsetX,
            nYCoord + kJobButtonStartY + static_cast<int>(row) * kJobButtonHeight);
    }

    for (std::size_t row = 0; row < kFutureClassNames.size(); ++row)
    {
        m_asprFutureJob[row].SetPosition(
            nXCoord + kJobButtonOffsetX,
            nYCoord + kJobButtonStartY
                + static_cast<int>(kClassDisplayOrder.size() + row) * kJobButtonHeight);
    }

    m_aBtn[CMW_OK].SetPosition(
        nXCoord + kActionButtonOffsetX,
        nYCoord + kActionButtonOffsetY);
    m_aBtn[CMW_CANCEL].SetPosition(
        nXCoord + kActionButtonOffsetX,
        nYCoord + kActionButtonOffsetY + kActionButtonSpacing);

    m_asprBack[CMW_SPR_INPUT].SetPosition(
        nXCoord + kInputSpriteOffsetX,
        nYCoord + kInputSpriteOffsetY);

    if (g_iChatInputType == 1)
    {
        g_pSingleTextInputBox->SetPosition(
            int((m_asprBack[CMW_SPR_INPUT].GetXPos() + kInputTextOffsetX) / g_fScreenRate_x),
            int((m_asprBack[CMW_SPR_INPUT].GetYPos() + kInputTextOffsetY) / g_fScreenRate_y));
    }

    m_asprBack[CMW_SPR_DESC].SetPosition(
        nXCoord + kDescSpriteOffsetX,
        nYCoord + kDescSpriteOffsetY);
}

void CCharMakeWin::Show(bool bShow)
{
    CWin::Show(bShow);

    int i;
    for (i = 0; i < CMW_SPR_MAX; ++i)
        m_asprBack[i].Show(bShow);

    for (i = 0; i < MAX_CLASS; ++i)
        m_abtnJob[i].Show(bShow);
    for (auto& futureClass : m_asprFutureJob)
        futureClass.Show(bShow);
    m_sprClassSelectionBeam.Show(false);
    m_classSelectionBeamTime = 0.0;
    for (i = 0; i < 2; ++i)
        m_aBtn[i].Show(bShow);

    if (bShow)
    {
        InputTextWidth = 73;
        ClearInput();
        InputEnable = true;
        InputNumber = 1;
        InputTextMax[0] = MAX_USERNAME_SIZE;
        if (g_iChatInputType == 1)
        {
            g_pSingleTextInputBox->SetState(UISTATE_NORMAL);
            g_pSingleTextInputBox->SetOption(UIOPTION_NULL);
            g_pSingleTextInputBox->SetBackColor(0, 0, 0, 0);
            g_pSingleTextInputBox->SetTextLimit(10);
            g_pSingleTextInputBox->GiveFocus();
        }
    }
    else
    {
        if (g_iChatInputType == 1)
        {
            g_pSingleTextInputBox->SetText(NULL);
            g_pSingleTextInputBox->SetState(UISTATE_HIDE);
        }
    }
}

bool CCharMakeWin::CursorInWin(int nArea)
{
    if (!CWin::m_bShow)
        return false;

    switch (nArea)
    {
    case WA_MOVE:
        return false;
    }

    return CWin::CursorInWin(nArea);
}

void CCharMakeWin::UpdateDisplay()
{
    for (auto& button : m_abtnJob)
        button.SetEnable(true);

#ifdef PBG_ADD_CHARACTERCARD
    for (int i = 0; i < CLASS_CHARACTERCARD_TOTALCNT; ++i)
    {
        if (!g_CharCardEnable.bCharacterEnable[i])
            m_abtnJob[i + CLASS_DARK].SetEnable(false);
    }
#else //PBG_ADD_CHARACTERCARD
    m_abtnJob[CLASS_SUMMONER].SetEnable(true);
#endif //PBG_ADD_CHARACTERCARD

    const bool isDarkLord = (m_nSelJob == CLASS_DARK_LORD);
    m_asprBack[CMW_SPR_STAT].SetSize(0, isDarkLord ? kDarkLordStatHeight : kDefaultStatHeight, Y);

    const int descriptionTextId = ResolveDescriptionTextId(m_nSelJob);
    m_nDescLine = ::SeparateTextIntoLines(
        I18N::Game::Lookup(descriptionTextId),
        m_aszJobDesc[0],
        CMW_DESC_LINE_MAX,
        CMW_DESC_ROW_MAX);

    SelectCreateCharacter();
}

void CCharMakeWin::UpdateWhileActive(double dDeltaTick)
{
    for (int classIndex = 0; classIndex < MAX_CLASS; ++classIndex)
    {
        if (!m_abtnJob[classIndex].IsClick())
            continue;

        for (auto& button : m_abtnJob)
            button.SetCheck(false);
        m_abtnJob[classIndex].SetCheck(true);

        if (m_nSelJob != classIndex)
        {
            m_nSelJob = static_cast<CLASS_TYPE>(classIndex);
            UpdateDisplay();

            m_sprClassSelectionBeam.SetPosition(
                m_abtnJob[classIndex].GetXPos() + kClassBeamOffsetX,
                m_abtnJob[classIndex].GetYPos() + kClassBeamOffsetY);
            m_sprClassSelectionBeam.SetAction(
                0,
                kClassBeamFrameCount - 1,
                kClassBeamFrameDelay,
                false);
            m_sprClassSelectionBeam.Show(true);
            m_classSelectionBeamTime = kClassBeamDuration;
        }
        break;
    }

    if (m_classSelectionBeamTime > 0.0)
    {
        m_sprClassSelectionBeam.Update(dDeltaTick);
        m_classSelectionBeamTime -= dDeltaTick;
        if (m_classSelectionBeamTime <= 0.0)
            m_sprClassSelectionBeam.Show(false);
    }

    {
        if (m_aBtn[CMW_OK].IsClick())
        {
            RequestCreateCharacter();
        }
        else if (m_aBtn[CMW_CANCEL].IsClick())
        {
            CUIMng::Instance().HideWin(this);
        }
        else if (CInput::Instance().IsKeyDown(VK_RETURN))
        {
            ::PlayBuffer(SOUND_CLICK01);
            RequestCreateCharacter();
        }
        else if (CInput::Instance().IsKeyDown(VK_ESCAPE))
        {
            ::PlayBuffer(SOUND_CLICK01);
            CUIMng::Instance().HideWin(this);
            CUIMng::Instance().SetSysMenuWinShow(false);
        }
    }
    UpdateCreateCharacter();
}

void CCharMakeWin::RequestCreateCharacter()
{
    if (g_iChatInputType == 1)
        g_pSingleTextInputBox->GetText(InputText[0]);

    CUIMng& rUIMng = CUIMng::Instance();

    const std::wstring characterName = InputText[0];

    // todo: check with regex from server
    if (characterName.length() < kMinCharacterNameLength)
        rUIMng.PopUpMsgWin(MESSAGE_MIN_LENGTH);
    else if (Utf8ByteCount(characterName) > kMaxCharacterNameUtf8Bytes)
        rUIMng.PopUpMsgWin(MESSAGE_SPECIAL_NAME);
    else if (::CheckName())
        rUIMng.PopUpMsgWin(MESSAGE_ID_SPACE_ERROR);
    else if (HasInvalidCharacterNameChar(characterName))
        rUIMng.PopUpMsgWin(MESSAGE_SPECIAL_NAME);
    else
    {
        const int pendingSlot = AccountCharacterList::FindFirstEmptySlot();
        if (pendingSlot < 0)
        {
            rUIMng.PopUpMsgWin(RECEIVE_CREATE_CHARACTER_FAIL2);
            return;
        }

        AccountCharacterList::SetPendingCreationSlot(pendingSlot);
        const auto classByte = static_cast<CharacterClassNumber>((CharacterView.Class << 2) + CharacterView.Skin);
        CurrentProtocolState = REQUEST_CREATE_CHARACTER;
        SocketClient->ToGameServer()->SendCreateCharacter(InputText[0], classByte);
        //SendRequestCreateCharacter(InputText[0], CharacterView.Class, CharacterView.Skin);
        rUIMng.HideWin(this);
        rUIMng.PopUpMsgWin(MESSAGE_WAIT);
    }
}

void CCharMakeWin::RenderControls()
{
    RenderCreateCharacter();
    ::EnableAlphaTest();

    const int baseX = m_winBack.GetXPos();
    const int baseY = m_winBack.GetYPos();
    const int descriptionX = m_asprBack[CMW_SPR_DESC].GetXPos();
    const int descriptionY = m_asprBack[CMW_SPR_DESC].GetYPos();

    ::EnableAlphaTest();
    ::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    RenderBitmapAt(
        CharacterSceneTextures::CreateDecoration,
        baseX + kDecorationOffsetX,
        baseY + kDecorationOffsetY,
        kDecorationWidth,
        kDecorationHeight);
    m_asprBack[CMW_SPR_INPUT].Render();
    ::EnableAlphaTest();
    CWin::RenderButtons();
    for (auto& futureClass : m_asprFutureJob)
        futureClass.Render();
    m_sprClassSelectionBeam.Render();
    g_pRenderText->SetFont(g_hFixFont);
    g_pRenderText->SetTextColor(CLRDW_WHITE);
    g_pRenderText->SetBgColor(0, 0, 0, 0);

    RenderTextAt(
        baseX + kDecorationOffsetX + kDecorationTitleOffsetX,
        baseY + kDecorationOffsetY + kDecorationTitleOffsetY,
        150,
        gCharacterManager.GetCharacterClassText(m_nSelJob),
        ARGB(255, 232, 222, 211),
        RT3_SORT_CENTER);

    const auto& stats = kClassStatTable[static_cast<std::size_t>(m_nSelJob)];
    const int statBaseX = m_asprBack[CMW_SPR_STAT].GetXPos() + kStatTextOffsetX;
    for (std::size_t statIndex = 0; statIndex < stats.values.size(); ++statIndex)
    {
        const int statScreenY = int(
            (m_asprBack[CMW_SPR_STAT].GetYPos() + kStatYOffset + static_cast<int>(statIndex) * kStatLineSpacing)
            / g_fScreenRate_y);

        g_pRenderText->SetTextColor(CLRDW_ORANGE);
        g_pRenderText->RenderText(
            int((statBaseX + kStatValueOffset) / g_fScreenRate_x),
            statScreenY,
            stats.values[statIndex]);

        g_pRenderText->SetTextColor(CLRDW_WHITE);
        g_pRenderText->RenderText(
            int(statBaseX / g_fScreenRate_x),
            statScreenY,
            I18N::Game::Lookup(kStatLabelBaseId + static_cast<int>(statIndex)));
    }

    if (m_nSelJob == CLASS_DARK_LORD)
    {
        const int leadershipY = int(
            (m_asprBack[CMW_SPR_STAT].GetYPos() + kStatYOffset + 4 * kStatLineSpacing) / g_fScreenRate_y);

        g_pRenderText->SetTextColor(CLRDW_ORANGE);
        g_pRenderText->RenderText(
            int((statBaseX + kStatValueOffset) / g_fScreenRate_x),
            leadershipY,
            kDarkLordLeadershipStatValue);
        g_pRenderText->SetTextColor(CLRDW_WHITE);
        g_pRenderText->RenderText(
            int(statBaseX / g_fScreenRate_x),
            leadershipY,
            I18N::Game::Lookup(kDarkLordLeadershipTextId));
    }

    for (int lineIndex = 0; lineIndex < m_nDescLine; ++lineIndex)
    {
        g_pRenderText->RenderText(
            int((m_asprBack[CMW_SPR_DESC].GetXPos() + kDescriptionTextOffsetX) / g_fScreenRate_x),
            int((m_asprBack[CMW_SPR_DESC].GetYPos() + kDescriptionTextOffsetY + lineIndex * kDescriptionLineSpacing)
                / g_fScreenRate_y),
            m_aszJobDesc[lineIndex]);
    }

    RenderTextAt(
        descriptionX + 25,
        descriptionY + 10,
        90,
        L"Nome",
        ARGB(255, 224, 211, 193),
        RT3_SORT_CENTER);

    for (std::size_t row = 0; row < kFutureClassNames.size(); ++row)
    {
        RenderTextAt(
            m_asprFutureJob[row].GetXPos(),
            m_asprFutureJob[row].GetYPos() + 8,
            kJobButtonWidth,
            kFutureClassNames[row],
            ARGB(180, 132, 128, 139),
            RT3_SORT_CENTER);
    }

    g_pRenderText->SetFont(g_hFont);

    if (g_iChatInputType == 1)
        g_pSingleTextInputBox->Render();
    else if (g_iChatInputType == 0)
        ::RenderInputText(
            int((m_asprBack[CMW_SPR_INPUT].GetXPos() + 78) / g_fScreenRate_x),
            int((m_asprBack[CMW_SPR_INPUT].GetYPos() + 21) / g_fScreenRate_y),
            0);
}

void CCharMakeWin::SelectCreateCharacter()
{
    CharacterView.Class = m_nSelJob;
    CreateCharacterPointer(&CharacterView, static_cast<int>(MODEL_FACE) + CharacterView.Class, 0, 0);
    CharacterView.Object.Kind = 0;
    SetAction(&CharacterView.Object, 1);
}

void CCharMakeWin::UpdateCreateCharacter()
{
    if (!CharacterAnimation(&CharacterView, &CharacterView.Object))
        SetAction(&CharacterView.Object, 0);
}

void CCharMakeWin::RenderCreateCharacter()
{
    OBJECT* o = &CharacterView.Object;
    vec3_t Position, Angle;

    Vector(1.0f, 1.0f, 1.0f, o->Light);
    Vector(10, -500.f, 48.f, Position);
    Vector(-90.f, 0.f, 0.f, Angle);
    g_Camera.FOV = 10.f;
    MoveCharacterCamera(CharacterView.Object.Position, Position, Angle);

    BeginOpengl(
        (m_winBack.GetXPos() + kCharacterViewportOffsetX) / g_fScreenRate_x,
        m_winBack.GetYPos() / g_fScreenRate_y,
        410 / g_fScreenRate_x,
        335 / g_fScreenRate_y);

    const ClassRenderParameters params = GetRenderParameters(CharacterView.Class);
    vec3_t originalPosition;
    VectorCopy(o->Position, originalPosition);
    if (params.overrideAngle)
        Vector(params.angleX, params.angleY, params.angleZ, o->Angle);

    o->Scale = params.scale;

    if (params.positionOffsetX != 0.0f)
        CharacterView.Object.Position[0] += params.positionOffsetX;
    if (params.positionOffsetZ != 0.0f)
        CharacterView.Object.Position[2] += params.positionOffsetZ;

    RenderCharacter(&CharacterView, o);
    VectorCopy(originalPosition, o->Position);

    glViewport2(0, 0, WindowWidth, WindowHeight);

    EndOpengl();
}
