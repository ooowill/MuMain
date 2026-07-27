//*****************************************************************************
// File: LoginMainWin.cpp
//*****************************************************************************

#include "stdafx.h"
#include "UI/Windows/LoginMainWin.h"

#include "Core/Input/Input.h"
#include "I18N/All.h"
#include "UI/Legacy/UIMng.h"
#include "Network/Server/WSclient.h"

//=============================================================================
// Global Variables
//=============================================================================


//=============================================================================
// Constructor / Destructor
//=============================================================================

CLoginMainWin::CLoginMainWin()
{
}

CLoginMainWin::~CLoginMainWin()
{
}

//=============================================================================
// Public Methods
//=============================================================================

void CLoginMainWin::Create()
{
    constexpr int buttonWidth = 72;
    constexpr int buttonHeight = 26;
    constexpr int cornerInset = 68;
    CWin::Create(
        CInput::Instance().GetScreenWidth() - cornerInset * 2,
        buttonHeight,
        -2
    );

    const wchar_t* buttonText[LMW_BTN_MAX] =
    {
        I18N::Game::LoginMenu,
        I18N::Game::LoginCredit,
    };
    DWORD buttonTextColors[4] =
    {
        ARGB(255, 210, 205, 198),
        ARGB(255, 255, 213, 116),
        ARGB(255, 255, 244, 210),
        ARGB(255, 105, 102, 98),
    };
    for (int i = 0; i < LMW_BTN_MAX; ++i)
    {
        m_aBtn[i].CreateTextButton(buttonWidth, buttonHeight);
        m_aBtn[i].SetText(buttonText[i], buttonTextColors);
    }

    for (int i = 0; i < LMW_BTN_MAX; ++i)
        CWin::RegisterButton(&m_aBtn[i]);

    m_sprDeco.Create(189, 103, BITMAP_LOG_IN + 6, 0, nullptr, 105, 59);
}

void CLoginMainWin::PreRelease()
{
    m_sprDeco.Release();
}

void CLoginMainWin::SetPosition(int nXCoord, int nYCoord)
{
    (void)nXCoord;
    (void)nYCoord;

    constexpr int cornerInset = 68;
    constexpr int bottomInset = 30;
    const int buttonY = CInput::Instance().GetScreenHeight()
        - m_aBtn[LMW_BTN_MENU].GetHeight()
        - bottomInset;

    CWin::SetPosition(cornerInset, buttonY);

    m_aBtn[LMW_BTN_MENU].SetPosition(cornerInset, buttonY);

    m_aBtn[LMW_BTN_CREDIT].SetPosition(
        cornerInset + CWin::GetWidth() - m_aBtn[LMW_BTN_CREDIT].GetWidth(),
        buttonY
    );

    constexpr int decorationOffsetX = 55;
    constexpr int decorationOffsetY = 10;
    m_sprDeco.SetPosition(
        m_aBtn[LMW_BTN_CREDIT].GetXPos() + decorationOffsetX,
        m_aBtn[LMW_BTN_CREDIT].GetYPos() + decorationOffsetY
    );
}

void CLoginMainWin::Show(bool bShow)
{

    CWin::Show(bShow);

    for (int i = 0; i < LMW_BTN_MAX; ++i)
        m_aBtn[i].Show(bShow);

    m_sprDeco.Show(bShow);
}

bool CLoginMainWin::CursorInWin(int nArea)
{
    return CWin::CursorInWin(nArea);
}

void CLoginMainWin::UpdateWhileActive(double dDeltaTick)
{
    CUIMng& rUIMng = CUIMng::Instance();

    if (m_aBtn[LMW_BTN_MENU].IsClick())
    {
        rUIMng.ShowWin(&rUIMng.m_SysMenuWin);
        rUIMng.SetSysMenuWinShow(true);
    }
    else if (m_aBtn[LMW_BTN_CREDIT].IsClick())
    {
        SocketClient->ToConnectServer()->SendServerListRequest();

        rUIMng.ShowWin(&rUIMng.m_CreditWin);

        ::StopMp3(MUSIC_MAIN_THEME);
        ::PlayMp3(MUSIC_MUTHEME);
    }
}

void CLoginMainWin::RenderControls()
{
    m_sprDeco.Render();
    CWin::RenderButtons();
}
