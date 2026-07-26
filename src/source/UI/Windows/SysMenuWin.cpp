//*****************************************************************************
// File: SysMenuWin.cpp
//*****************************************************************************

#include "stdafx.h"
#include "UI/Windows/SysMenuWin.h"
#include "I18N/All.h"

#include "Core/Input/Input.h"
#include "UI/Legacy/UIMng.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Scenes/SceneCore.h"

#include "Audio/DSPlaySound.h"
#include "UI/NewUI/NewUISystem.h"

#include "Network/Server/WSclient.h"
#include "Core/Utilities/Log/ErrorReport.h"
#include "Core/Utilities/Log/muConsoleDebug.h"
#include "Render/Textures/ZzzOpenglUtil.h"

namespace
{
    constexpr int kPanelWidth = 306;
    constexpr int kPanelHeight = 286;
    constexpr int kButtonWidth = 108;
    constexpr int kButtonHeight = 30;
    constexpr int kButtonGap = 20;
    constexpr int kFirstButtonY = 67;
    constexpr int kCloseButtonY = 238;

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

    void FillRect(int x, int y, int width, int height, BYTE red, BYTE green, BYTE blue, BYTE alpha)
    {
        const float rateX = g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f;
        const float rateY = g_fScreenRate_y > 0.0f ? g_fScreenRate_y : 1.0f;
        ::glColor4ub(red, green, blue, alpha);
        ::RenderColor(
            static_cast<float>(x) / rateX,
            static_cast<float>(y) / rateY,
            static_cast<float>(width) / rateX,
            static_cast<float>(height) / rateY,
            0.0f,
            0);
    }
}

extern EGameScene  SceneFlag;
extern bool LogOut;

CSysMenuWin::CSysMenuWin()
{
}

CSysMenuWin::~CSysMenuWin()
{
}

void CSysMenuWin::Create()
{
    CInput rInput = CInput::Instance();
    CWin::Create(rInput.GetScreenWidth(), rInput.GetScreenHeight(), -2);
    m_winBack.Create(kPanelWidth, kPanelHeight, -2);

    const wchar_t* apszBtnText[SMW_BTN_MAX] =
    { I18N::Game::ExitGame, I18N::Game::SelectServer, I18N::Game::Option385, I18N::Game::Close388 };
    DWORD adwBtnClr[4] =
    { CLRDW_BR_GRAY, CLRDW_BR_GRAY, CLRDW_WHITE, 0 };
    for (int i = 0; i < SMW_BTN_MAX; ++i)
    {
        m_aBtn[i].Create(kButtonWidth, kButtonHeight, BITMAP_TEXT_BTN, 4, 2, 1);
        m_aBtn[i].SetText(apszBtnText[i], adwBtnClr);
        CWin::RegisterButton(&m_aBtn[i]);
    }

    switch (SceneFlag)
    {
    case LOG_IN_SCENE:
        m_aBtn[SMW_BTN_SERVER_SEL].SetEnable(false);
        break;
    case CHARACTER_SCENE:
        m_aBtn[SMW_BTN_SERVER_SEL].SetEnable(true);
        break;
    }

    SetPosition((rInput.GetScreenWidth() - m_winBack.GetWidth()) / 2,
        (rInput.GetScreenHeight() - m_winBack.GetHeight()) / 2);
}

void CSysMenuWin::PreRelease()
{
    m_winBack.Release();
}

void CSysMenuWin::SetPosition(int nXCoord, int nYCoord)
{
    m_winBack.SetPosition(nXCoord, nYCoord);

    const int buttonX = m_winBack.GetXPos() + (m_winBack.GetWidth() - kButtonWidth) / 2;
    const int buttonStride = kButtonHeight + kButtonGap;
    for (int button = SMW_BTN_GAME_END; button <= SMW_BTN_OPTION; ++button)
    {
        m_aBtn[button].SetPosition(
            buttonX,
            m_winBack.GetYPos() + kFirstButtonY + button * buttonStride);
    }
    m_aBtn[SMW_BTN_CLOSE].SetPosition(buttonX, m_winBack.GetYPos() + kCloseButtonY);
}
void CSysMenuWin::Show(bool bShow)
{
    CWin::Show(bShow);

    m_winBack.Show(bShow);
    for (int i = 0; i < SMW_BTN_MAX; ++i)
        m_aBtn[i].Show(bShow);
}

bool CSysMenuWin::CursorInWin(int nArea)
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

void CSysMenuWin::UpdateWhileActive(double dDeltaTick)
{
    if (m_aBtn[SMW_BTN_GAME_END].IsClick())
    {
        CUIMng::Instance().PopUpMsgWin(MESSAGE_GAME_END_COUNTDOWN);
    }
    else if (m_aBtn[SMW_BTN_SERVER_SEL].IsClick())
    {
        g_ErrorReport.Write(L"> Menu - Join another server.");
        g_ErrorReport.WriteCurrentTime();
        LogOut = true;
        SocketClient->ToGameServer()->SendLogOut(LogOutType::BackToServerSelection);
        g_ConsoleDebug->Write(MCD_SEND, L"0xF1 [SendRequestLogOut] 2");

        CUIMng& rUIMng = CUIMng::Instance();
        rUIMng.HideWin(this);
        rUIMng.HideWin(&rUIMng.m_CharSelMainWin);
    }
    else if (m_aBtn[SMW_BTN_OPTION].IsClick())
    {
        CUIMng& rUIMng = CUIMng::Instance();
        rUIMng.HideWin(this);
        g_pNewUISystem->Show(SEASON3B::INTERFACE_OPTION);
    }
    else if (m_aBtn[SMW_BTN_CLOSE].IsClick())
    {
        CUIMng::Instance().HideWin(this);
    }
    else if (CInput::Instance().IsKeyDown(VK_ESCAPE))
    {
        // ESC toggle is handled by CUIMng::Update()
        // No action needed here — CUIMng already hid this window
    }
}

void CSysMenuWin::RenderControls()
{
    const int panelX = m_winBack.GetXPos();
    const int panelY = m_winBack.GetYPos();

    ::EnableAlphaBlend();
    FillRect(panelX, panelY, kPanelWidth, kPanelHeight, 2, 3, 9, 218);
    FillRect(panelX, panelY, kPanelWidth, 2, 100, 73, 37, 245);
    FillRect(panelX, panelY + kPanelHeight - 2, kPanelWidth, 2, 100, 73, 37, 245);
    FillRect(panelX, panelY, 2, kPanelHeight, 100, 73, 37, 245);
    FillRect(panelX + kPanelWidth - 2, panelY, 2, kPanelHeight, 100, 73, 37, 245);
    FillRect(panelX + 6, panelY + 6, kPanelWidth - 12, 1, 48, 44, 43, 210);
    FillRect(panelX + 10, panelY + 47, kPanelWidth - 20, 1, 105, 76, 31, 170);
    FillRect(panelX + 10, panelY + 222, kPanelWidth - 20, 1, 105, 76, 31, 125);
    ::EndRenderColor();
    ::DisableAlphaBlend();

    ::EnableAlphaTest();
    g_pRenderText->SetFont(g_hFixFont);
    g_pRenderText->SetBgColor(0, 0, 0, 0);
    g_pRenderText->SetTextColor(255, 205, 28, 255);
    g_pRenderText->RenderText(
        LogicalX(panelX),
        LogicalY(panelY + 18),
        L"Menu de Sistema",
        LogicalX(kPanelWidth),
        0,
        RT3_SORT_CENTER);

    CWin::RenderButtons();
}
