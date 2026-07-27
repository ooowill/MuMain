//*****************************************************************************
// File: OptionWin.cpp
//*****************************************************************************

#include "stdafx.h"
#include "UI/Windows/OptionWin.h"
#include "Core/Input/Input.h"
#include "UI/Legacy/UIMng.h"
#include "UI/Legacy/UIControls.h"
#include "UI/NewUI/NewUISystem.h"
#include "Engine/Object/ZzzInterface.h"
#include "Audio/AudioPlayer.h"
#include "Audio/DSPlaySound.h"
#include "Data/GameConfig/GameConfig.h"
#include "I18N/All.h"

extern int m_MusicOnOff;
extern int m_SoundOnOff;

namespace
{
    constexpr int kPanelWidth = 322;
    constexpr int kPanelHeight = 386;
    constexpr int kTitleDividerY = 49;
    constexpr int kFooterDividerY = 327;

    constexpr int kVolumeBarX = 88;
    constexpr int kVolumeBarY = 145;
    constexpr int kVolumeSegmentWidth = 10;
    constexpr int kVolumeSegmentGap = 3;
    constexpr int kVolumeSegmentCount = 10;
    constexpr int kVolumeSegmentHeight = 15;

    constexpr int kRenderButtonCount = 5;
    constexpr int kRenderButtonWidth = 44;
    constexpr int kRenderButtonHeight = 27;
    constexpr int kRenderButtonGap = 7;
    constexpr int kRenderButtonX = 37;
    constexpr int kRenderButtonY = 249;

    constexpr int kRenderValues[kRenderButtonCount] = {5, 7, 9, 11, 13};

    int LogicalX(int value)
    {
        const float rate = g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f;
        return static_cast<int>(value / rate);
    }

    int LogicalY(int value)
    {
        const float rate = g_fScreenRate_y > 0.0f ? g_fScreenRate_y : 1.0f;
        return static_cast<int>(value / rate);
    }

    bool CursorInRect(int x, int y, int width, int height)
    {
        CInput& input = CInput::Instance();
        const int cursorX = static_cast<int>(input.GetCursorX());
        const int cursorY = static_cast<int>(input.GetCursorY());
        return cursorX >= x && cursorX < x + width
            && cursorY >= y && cursorY < y + height;
    }

    void FillPanelRect(int x, int y, int width, int height, BYTE red, BYTE green, BYTE blue, BYTE alpha)
    {
        const float rateX = g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f;
        const float rateY = g_fScreenRate_y > 0.0f ? g_fScreenRate_y : 1.0f;
        ::glColor4f(
            static_cast<float>(red) / 255.0f,
            static_cast<float>(green) / 255.0f,
            static_cast<float>(blue) / 255.0f,
            static_cast<float>(alpha) / 255.0f);
        ::RenderColor(
            static_cast<float>(x) / rateX,
            static_cast<float>(y) / rateY,
            static_cast<float>(width) / rateX,
            static_cast<float>(height) / rateY,
            0.0f,
            0);
    }

    void RenderOptionPanel(int x, int y)
    {
        ::EnableAlphaTest();

        FillPanelRect(x - 4, y + 4, kPanelWidth + 8, kPanelHeight + 8, 0, 0, 0, 135);
        FillPanelRect(x, y, kPanelWidth, kPanelHeight, 31, 23, 18, 245);
        FillPanelRect(x + 1, y + 1, kPanelWidth - 2, kPanelHeight - 2, 112, 78, 39, 232);
        FillPanelRect(x + 3, y + 3, kPanelWidth - 6, kPanelHeight - 6, 4, 10, 22, 242);
        FillPanelRect(x + 6, y + 6, kPanelWidth - 12, kPanelHeight - 12, 62, 70, 87, 205);
        FillPanelRect(x + 7, y + 7, kPanelWidth - 14, kPanelHeight - 14, 1, 7, 18, 216);

        FillPanelRect(x + 8, y + 8, kPanelWidth - 16, kTitleDividerY - 9, 3, 14, 31, 225);
        FillPanelRect(x + 9, y + kTitleDividerY, kPanelWidth - 18, 1, 126, 88, 40, 220);
        FillPanelRect(x + 9, y + kTitleDividerY + 1, kPanelWidth - 18, 1, 25, 39, 58, 190);
        FillPanelRect(x + 9, y + kFooterDividerY, kPanelWidth - 18, 1, 77, 56, 34, 180);

        FillPanelRect(x + 3, y + 3, kPanelWidth - 6, 1, 173, 122, 59, 190);
        FillPanelRect(x + 3, y + kPanelHeight - 4, kPanelWidth - 6, 1, 21, 29, 43, 220);

        FillPanelRect(x + 42, y + 132, 238, 42, 23, 15, 14, 205);
        FillPanelRect(x + 43, y + 133, 236, 40, 5, 9, 17, 225);
        FillPanelRect(x + 35, y + 242, 252, 41, 64, 43, 25, 180);
        FillPanelRect(x + 36, y + 243, 250, 39, 4, 9, 17, 225);

        FillPanelRect(x + 134, y + 185, 54, 23, 56, 37, 22, 190);
        FillPanelRect(x + 135, y + 186, 52, 21, 3, 7, 14, 230);
        FillPanelRect(x + 134, y + 293, 54, 23, 56, 37, 22, 190);
        FillPanelRect(x + 135, y + 294, 52, 21, 3, 7, 14, 230);

        ::EndRenderColor();
        ::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    void RenderSpeakerIcon(int x, int y, bool muted)
    {
        ::EnableAlphaTest();
        FillPanelRect(x, y + 5, 4, 8, 238, 133, 19, 255);
        FillPanelRect(x + 4, y + 3, 3, 12, 238, 133, 19, 255);
        FillPanelRect(x + 7, y + 1, 3, 16, 238, 133, 19, 255);
        FillPanelRect(x + 12, y + 4, 1, 10, 238, 133, 19, 230);
        FillPanelRect(x + 15, y + 2, 1, 14, 238, 133, 19, 210);
        FillPanelRect(x + 18, y, 1, 18, 238, 133, 19, 190);
        if (muted)
        {
            for (int i = 0; i < 16; ++i)
                FillPanelRect(x + 3 + i, y + 1 + i, 2, 2, 210, 67, 38, 255);
        }
        ::EndRenderColor();
        ::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    void RenderVolumeSegments(int panelX, int panelY, int volume)
    {
        const int filled = volume > 0 ? volume + 1 : 0;
        ::EnableAlphaTest();
        for (int i = 0; i < kVolumeSegmentCount; ++i)
        {
            const bool active = i < filled;
            FillPanelRect(
                panelX + kVolumeBarX + i * (kVolumeSegmentWidth + kVolumeSegmentGap),
                panelY + kVolumeBarY,
                kVolumeSegmentWidth,
                kVolumeSegmentHeight,
                active ? 255 : 58,
                active ? 132 : 42,
                active ? 10 : 32,
                active ? 255 : 210);
        }
        ::EndRenderColor();
        ::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    void RenderSelectionBorder(int x, int y, int width, int height)
    {
        ::EnableAlphaTest();
        FillPanelRect(x, y, width, 1, 244, 166, 42, 255);
        FillPanelRect(x, y + height - 1, width, 1, 244, 166, 42, 255);
        FillPanelRect(x, y, 1, height, 244, 166, 42, 255);
        FillPanelRect(x + width - 1, y, 1, height, 244, 166, 42, 255);
        ::EndRenderColor();
        ::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

COptionWin::COptionWin()
{
}

COptionWin::~COptionWin()
{
}

void COptionWin::Create()
{
    CInput& input = CInput::Instance();
    CWin::Create(input.GetScreenWidth(), input.GetScreenHeight(), -2);
    m_winBack.Create(kPanelWidth, kPanelHeight, -2);

    m_aBtn[OW_BTN_BGM].Create(16, 16, BITMAP_CHECK_BTN, 2, 0, 0, -1, 1, 1, 1);
    CWin::RegisterButton(&m_aBtn[OW_BTN_BGM]);

    DWORD buttonColors[4] =
    {
        ARGB(255, 210, 205, 198),
        ARGB(255, 255, 213, 116),
        ARGB(255, 255, 240, 190),
        ARGB(255, 105, 102, 98)
    };

    m_aBtn[OW_BTN_MUTE].CreateTextButton(58, 28);
    CWin::RegisterButton(&m_aBtn[OW_BTN_MUTE]);

    wchar_t renderLabels[kRenderButtonCount][3] = {L"5", L"7", L"9", L"11", L"13"};
    for (int i = 0; i < kRenderButtonCount; ++i)
    {
        CButton& button = m_aBtn[OW_BTN_RENDER_5 + i];
        button.CreateTextButton(kRenderButtonWidth, kRenderButtonHeight);
        button.SetText(renderLabels[i], buttonColors);
        CWin::RegisterButton(&button);
    }

    m_aBtn[OW_BTN_CLOSE].CreateTextButton(64, 30);
    m_aBtn[OW_BTN_CLOSE].SetText(I18N::Game::Close388, buttonColors);
    CWin::RegisterButton(&m_aBtn[OW_BTN_CLOSE]);

    SetPosition(
        (input.GetScreenWidth() - m_winBack.GetWidth()) / 2,
        (input.GetScreenHeight() - m_winBack.GetHeight()) / 2);

    UpdateDisplay();
}

void COptionWin::PreRelease()
{
    m_winBack.Release();
}

void COptionWin::SetPosition(int nXCoord, int nYCoord)
{
    m_winBack.SetPosition(nXCoord, nYCoord);

    m_aBtn[OW_BTN_BGM].SetPosition(nXCoord + 116, nYCoord + 69);
    m_aBtn[OW_BTN_MUTE].SetPosition(nXCoord + 216, nYCoord + 139);

    for (int i = 0; i < kRenderButtonCount; ++i)
    {
        m_aBtn[OW_BTN_RENDER_5 + i].SetPosition(
            nXCoord + kRenderButtonX + i * (kRenderButtonWidth + kRenderButtonGap),
            nYCoord + kRenderButtonY);
    }

    m_aBtn[OW_BTN_CLOSE].SetPosition(
        nXCoord + (kPanelWidth - m_aBtn[OW_BTN_CLOSE].GetWidth()) / 2,
        nYCoord + 342);
}

void COptionWin::Show(bool bShow)
{
    CWin::Show(bShow);
    ActiveBtns(bShow);

    m_winBack.Show(bShow);
    for (int i = 0; i < OW_BTN_MAX; ++i)
        m_aBtn[i].Show(bShow);

    if (bShow)
        UpdateDisplay();
}

bool COptionWin::CursorInWin(int nArea)
{
    if (!CWin::m_bShow)
        return false;

    if (nArea == WA_MOVE)
        return false;

    return CWin::CursorInWin(nArea);
}

void COptionWin::UpdateDisplay()
{
    const int soundVolume = g_pOption->GetVolumeLevel();
    if (soundVolume > 0)
        m_lastSoundVolume = soundVolume;

    const int musicVolume = GameConfig::GetInstance().GetMusicVolume();
    if (musicVolume > 0)
        m_lastMusicVolume = musicVolume;

    m_aBtn[OW_BTN_BGM].SetCheck(musicVolume > 0);
}

void COptionWin::ApplySoundVolume(int level)
{
    if (level < 0)
        level = 0;
    if (level > 9)
        level = 9;

    if (level > 0)
        m_lastSoundVolume = level;

    g_pOption->SetVolumeLevel(level);
    m_SoundOnOff = level > 0 ? 1 : 0;
    ::SetEffectVolumeLevel(level);

    GameConfig::GetInstance().SetSoundVolume(level);
    GameConfig::GetInstance().Save();
}

void COptionWin::ApplyMusicEnabled(bool enabled)
{
    GameConfig& config = GameConfig::GetInstance();
    const int currentVolume = config.GetMusicVolume();
    if (currentVolume > 0)
        m_lastMusicVolume = currentVolume;

    const int newVolume = enabled ? (m_lastMusicVolume > 0 ? m_lastMusicVolume : 5) : 0;
    m_MusicOnOff = newVolume > 0 ? 1 : 0;
    AudioPlayer::SetMusicVolume(newVolume);
    config.SetMusicVolume(newVolume);
    config.Save();
    m_aBtn[OW_BTN_BGM].SetCheck(enabled);
}

void COptionWin::CloseWindow()
{
    CUIMng::Instance().HideWin(this);
    CUIMng::Instance().SetSysMenuWinShow(false);
}

void COptionWin::UpdateWhileShow(double dDeltaTick)
{
    if (m_aBtn[OW_BTN_BGM].IsClick())
    {
        ApplyMusicEnabled(m_aBtn[OW_BTN_BGM].IsCheck());
        return;
    }

    if (m_aBtn[OW_BTN_MUTE].IsClick())
    {
        const int volume = g_pOption->GetVolumeLevel();
        ApplySoundVolume(volume > 0 ? 0 : (m_lastSoundVolume > 0 ? m_lastSoundVolume : 5));
        return;
    }

    for (int i = 0; i < kRenderButtonCount; ++i)
    {
        if (m_aBtn[OW_BTN_RENDER_5 + i].IsClick())
        {
            g_pOption->SetRenderLevel(i);
            return;
        }
    }

    if (m_aBtn[OW_BTN_CLOSE].IsClick())
    {
        CloseWindow();
        return;
    }

    CInput& input = CInput::Instance();
    if (input.IsLBtnUp())
    {
        const int panelX = m_winBack.GetXPos();
        const int panelY = m_winBack.GetYPos();
        const int barWidth = kVolumeSegmentCount * kVolumeSegmentWidth
            + (kVolumeSegmentCount - 1) * kVolumeSegmentGap;

        if (CursorInRect(panelX + kVolumeBarX, panelY + kVolumeBarY - 5, barWidth, kVolumeSegmentHeight + 10))
        {
            int segment = (static_cast<int>(input.GetCursorX()) - (panelX + kVolumeBarX))
                / (kVolumeSegmentWidth + kVolumeSegmentGap);
            if (segment < 0)
                segment = 0;
            if (segment >= kVolumeSegmentCount)
                segment = kVolumeSegmentCount - 1;
            ApplySoundVolume(segment == 0 ? 1 : segment);
            return;
        }
    }

    if (input.IsKeyDown(VK_ESCAPE))
    {
        ::PlayBuffer(SOUND_CLICK01);
        CloseWindow();
    }
}

void COptionWin::RenderControls()
{
    const int panelX = m_winBack.GetXPos();
    const int panelY = m_winBack.GetYPos();
    const int soundVolume = g_pOption->GetVolumeLevel();
    const int renderLevel = g_pOption->GetRenderLevel();
    const int renderValue = renderLevel * 2 + 5;

    RenderOptionPanel(panelX, panelY);

    ::EnableAlphaTest();
    g_pRenderText->SetFont(g_hFixFont);
    g_pRenderText->SetBgColor(0, 0, 0, 0);
    g_pRenderText->SetTextColor(255, 205, 28, 255);
    g_pRenderText->RenderText(
        LogicalX(panelX),
        LogicalY(panelY + 17),
        I18N::Game::Option385,
        LogicalX(kPanelWidth),
        0,
        RT3_SORT_CENTER);

    g_pRenderText->SetTextColor(220, 218, 212, 255);
    g_pRenderText->RenderText(LogicalX(panelX + 140), LogicalY(panelY + 72), L"BGM");
    g_pRenderText->RenderText(LogicalX(panelX + 42), LogicalY(panelY + 111), I18N::Game::Volume);
    g_pRenderText->RenderText(LogicalX(panelX + 42), LogicalY(panelY + 219), I18N::Game::EffectLimitation);

    wchar_t valueText[8] = {};
    ::_itow(soundVolume, valueText, 10);
    g_pRenderText->RenderText(
        LogicalX(panelX + 134),
        LogicalY(panelY + 190),
        valueText,
        LogicalX(54),
        0,
        RT3_SORT_CENTER);

    ::_itow(renderValue, valueText, 10);
    g_pRenderText->RenderText(
        LogicalX(panelX + 134),
        LogicalY(panelY + 298),
        valueText,
        LogicalX(54),
        0,
        RT3_SORT_CENTER);

    CWin::RenderButtons();

    RenderSpeakerIcon(panelX + 55, panelY + 143, soundVolume == 0);
    RenderVolumeSegments(panelX, panelY, soundVolume);
    RenderSpeakerIcon(panelX + 235, panelY + 144, true);

    if (renderLevel >= 0 && renderLevel < kRenderButtonCount)
    {
        CButton& selected = m_aBtn[OW_BTN_RENDER_5 + renderLevel];
        RenderSelectionBorder(
            selected.GetXPos(),
            selected.GetYPos(),
            selected.GetWidth(),
            selected.GetHeight());
    }
}