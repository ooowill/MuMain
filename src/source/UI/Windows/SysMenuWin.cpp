//*****************************************************************************
// File: SysMenuWin.cpp
//*****************************************************************************

#include "stdafx.h"
#include "UI/Windows/SysMenuWin.h"
#include "UI/Legacy/UIControls.h"
#include "I18N/All.h"

#include "Core/Input/Input.h"
#include "UI/Legacy/UIMng.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Scenes/SceneCore.h"

#include "Audio/DSPlaySound.h"

#include "Network/Server/WSclient.h"
#include "Core/Utilities/Log/ErrorReport.h"
#include "Core/Utilities/Log/muConsoleDebug.h"

namespace
{
    constexpr int kPanelWidth = 304;
    constexpr int kPanelHeight = 286;
    constexpr int kTitleDividerY = 48;
    constexpr int kFooterDividerY = 226;
    constexpr int kButtonWidth = 108;
    constexpr int kButtonHeight = 30;
    constexpr int kButtonGap = 22;
    constexpr int kFirstButtonY = 72;
    constexpr int kCloseButtonBottomMargin = 15;

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

    void RenderSystemMenuPanel(int x, int y)
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

        ::EndRenderColor();

        ::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

extern EGameScene SceneFlag;
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
    {
        I18N::Game::ExitGame,
        I18N::Game::SelectServer,
        I18N::Game::Option385,
        I18N::Game::Close388
    };
    DWORD adwBtnClr[4] =
    {
        ARGB(255, 210, 205, 198),
        ARGB(255, 255, 213, 116),
        ARGB(255, 255, 240, 190),
        ARGB(255, 105, 102, 98)
    };
    for (int i = 0; i < SMW_BTN_MAX; ++i)
    {
        m_aBtn[i].CreateTextButton(kButtonWidth, kButtonHeight);
        m_aBtn[i].SetText(apszBtnText[i], adwBtnClr);
        CWin::RegisterButton(&m_aBtn[i]);
    }

    m_aBtn[SMW_BTN_SERVER_SEL].SetEnable(true);

    SetPosition(
        (rInput.GetScreenWidth() - m_winBack.GetWidth()) / 2,
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
    m_aBtn[SMW_BTN_CLOSE].SetPosition(
        buttonX,
        m_winBack.GetYPos() + m_winBack.GetHeight() - kButtonHeight - kCloseButtonBottomMargin);
}

void CSysMenuWin::Show(bool bShow)
{
    CWin::Show(bShow);
    ActiveBtns(bShow);
    CUIMng::Instance().SetSysMenuWinShow(bShow);

    m_winBack.Show(bShow);
    for (int i = 0; i < SMW_BTN_MAX; ++i)
    {
        m_aBtn[i].Show(bShow);
    }
}

bool CSysMenuWin::CursorInWin(int nArea)
{
    if (!CWin::m_bShow)
    {
        return false;
    }

    if (nArea == WA_MOVE)
    {
        return false;
    }

    return CWin::CursorInWin(nArea);
}

void CSysMenuWin::UpdateWhileShow(double dDeltaTick)
{
    if (m_aBtn[SMW_BTN_GAME_END].IsClick())
    {
        CUIMng::Instance().PopUpMsgWin(MESSAGE_GAME_END_COUNTDOWN);
    }
    else if (m_aBtn[SMW_BTN_SERVER_SEL].IsClick())
    {
        CUIMng& rUIMng = CUIMng::Instance();
        rUIMng.HideWin(this);

        if (SceneFlag == CHARACTER_SCENE)
        {
            g_ErrorReport.Write(L"> Menu - Join another server.");
            g_ErrorReport.WriteCurrentTime();
            LogOut = true;
            SocketClient->ToGameServer()->SendLogOut(LogOutType::BackToServerSelection);
            g_ConsoleDebug->Write(MCD_SEND, L"0xF1 [SendRequestLogOut] 2");
            rUIMng.HideWin(&rUIMng.m_CharSelMainWin);
        }
    }
    else if (m_aBtn[SMW_BTN_OPTION].IsClick())
    {
        CUIMng& rUIMng = CUIMng::Instance();
        rUIMng.HideWin(this);
        rUIMng.m_OptionWin.UpdateDisplay();
        rUIMng.ShowWin(&rUIMng.m_OptionWin);
    }
    else if (m_aBtn[SMW_BTN_CLOSE].IsClick())
    {
        CUIMng::Instance().HideWin(this);
    }
    else if (CInput::Instance().IsKeyDown(VK_ESCAPE))
    {
        // ESC toggle is handled by CUIMng::Update().
    }
}

void CSysMenuWin::RenderControls()
{
    RenderSystemMenuPanel(m_winBack.GetXPos(), m_winBack.GetYPos());

    ::EnableAlphaTest();
    g_pRenderText->SetFont(g_hFixFont);
    g_pRenderText->SetBgColor(0, 0, 0, 0);
    g_pRenderText->SetTextColor(255, 205, 28, 255);
    g_pRenderText->RenderText(
        LogicalX(m_winBack.GetXPos()),
        LogicalY(m_winBack.GetYPos() + 17),
        I18N::Game::SystemMenu,
        LogicalX(m_winBack.GetWidth()),
        0,
        RT3_SORT_CENTER);

    CWin::RenderButtons();
}