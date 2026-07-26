//*****************************************************************************
// File: CharSelMainWin.cpp
//*****************************************************************************

#include "stdafx.h"
#include "CharSelMainWin.h"

#include "Character/AccountCharacterList.h"
#include "Character/AccountCharacterPaging.h"
#include "Character/CharacterManager.h"
#include "Character/CharacterSceneTextures.h"
#include "Audio/DSPlaySound.h"
#include "Core/Input/Input.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzOpenData.h"
#include "Guild/UIGuildInfo.h"
#include "I18N/All.h"
#include "Network/Server/WSclient.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Scenes/SceneCommon.h"
#include "UI/Legacy/UIMng.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>

namespace
{
    constexpr int kPanelWidth = 300;
    constexpr int kPanelHeight = 552;
    constexpr int kPanelInset = 5;
    constexpr int kListTopOffset = 5;
    constexpr int kVisibleRows = 10;
    constexpr int kRowHeight = 50;
    constexpr int kRowGap = 3;
    constexpr int kRowStride = kRowHeight + kRowGap;
    constexpr int kListHeight = (kVisibleRows * kRowStride) - kRowGap;
    constexpr int kScrollTrackTopOffset = kListTopOffset;
    constexpr int kScrollTrackHeight = kListHeight;
    constexpr int kScrollTrackWidth = 6;
    constexpr int kScrollThumbMinHeight = 38;
    constexpr int kArrowSize = 20;
    constexpr int kArrowColumnGap = 5;
    constexpr int kArrowUpTopOffset = 2;
    constexpr int kArrowDownTopOffset = kArrowUpTopOffset + kArrowSize + 5;
    constexpr DWORD kReorderRequestTimeout = 5000;
    constexpr int kBottomScreenInset = 14;
    constexpr int kSideScreenInset = 22;
    constexpr int kActionButtonGap = 1;
    constexpr int kBottomBarHeight = 28;
    constexpr int kBottomBarTopPadding = 3;
    constexpr float kPowerDigitAtlasCellWidth = 96.0f / 1024.0f;
    constexpr float kPowerDigitAtlasHeight = 140.0f / 256.0f;
    constexpr float kPowerDigitWidth = 5.0f;
    constexpr float kPowerDigitHeight = 9.0f;
    constexpr int kAccountBlockMsgX = 320;
    constexpr int kAccountBlockPrimaryY = 330;
    constexpr int kAccountBlockSecondaryY = 348;

    bool g_reorderPending = false;
    int g_reorderSourceSlot = -1;
    int g_reorderTargetSlot = -1;
    DWORD g_reorderRequestTime = 0;

    int LogicalX(int value)
    {
        const float rate = g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f;
        return static_cast<int>(std::lround(static_cast<float>(value) / rate));
    }

    int LogicalY(int value)
    {
        const float rate = g_fScreenRate_y > 0.0f ? g_fScreenRate_y : 1.0f;
        return static_cast<int>(std::lround(static_cast<float>(value) / rate));
    }

    void FillRect(int x, int y, int width, int height, BYTE red, BYTE green, BYTE blue, BYTE alpha)
    {
        const float rateX = g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f;
        const float rateY = g_fScreenRate_y > 0.0f ? g_fScreenRate_y : 1.0f;
        ::glColor4ub(red, green, blue, alpha);
        RenderColor(
            static_cast<float>(x) / rateX,
            static_cast<float>(y) / rateY,
            static_cast<float>(width) / rateX,
            static_cast<float>(height) / rateY,
            0.0f,
            0);
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

    int GetScrollTrackX(int panelX)
    {
        const int formerArrowColumnX = panelX + kPanelWidth - kArrowSize;
        return formerArrowColumnX + ((kArrowSize - kScrollTrackWidth) / 2);
    }

    int GetArrowX(int panelX)
    {
        return panelX - kArrowSize - kArrowColumnGap;
    }

    int GetArrowUpY(int panelY, int selectedSlot, int scrollOffset)
    {
        const int selectedRow = selectedSlot - scrollOffset;
        if (selectedRow < 0 || selectedRow >= kVisibleRows)
            return -1;

        return panelY
            + kListTopOffset
            + (selectedRow * kRowStride)
            + kArrowUpTopOffset;
    }

    int FindOccupiedSlotInDirection(int sourceSlot, int direction)
    {
        if (sourceSlot < 0
            || sourceSlot >= AccountCharacterList::GetUnlockedSlotCount()
            || direction == 0)
        {
            return -1;
        }

        for (int slot = sourceSlot + direction;
             slot >= 0 && slot < AccountCharacterList::GetUnlockedSlotCount();
             slot += direction)
        {
            if (AccountCharacterList::GetBySlot(slot) != nullptr)
                return slot;
        }

        return -1;
    }

    void RenderArrowButton(int texture, int x, int y, bool enabled, bool hovered, bool pressed)
    {
        if (!enabled)
            ::glColor4ub(105, 105, 105, 190);
        else if (pressed)
            ::glColor4ub(255, 176, 78, 255);
        else if (hovered)
            ::glColor4ub(255, 244, 194, 255);
        else
            ::glColor4ub(210, 210, 210, 255);

        RenderBitmapAt(texture, x, y, kArrowSize, kArrowSize);
        ::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    void RenderTextAt(int x, int y, int width, const wchar_t* text, DWORD color, int align = RT3_SORT_LEFT)
    {
        ::EnableAlphaTest();
        g_pRenderText->SetBgColor(0, 0, 0, 0);
        g_pRenderText->SetTextColor(color);
        g_pRenderText->RenderText(LogicalX(x), LogicalY(y), text, LogicalX(width), 0, align);
    }

    std::wstring FitTextToWidth(const wchar_t* text, int width)
    {
        if (text == nullptr || text[0] == L'\0')
            return {};

        const int logicalWidth = std::max(1, LogicalX(width));
        const auto fits = [logicalWidth](const std::wstring& candidate) {
            SIZE extent{};
            return ::GetTextExtentPoint32W(
                       g_pRenderText->GetFontDC(),
                       candidate.c_str(),
                       static_cast<int>(candidate.size()),
                       &extent)
                && extent.cx <= logicalWidth;
        };

        std::wstring fitted(text);
        if (fits(fitted))
            return fitted;

        constexpr const wchar_t* suffix = L"...";
        while (!fitted.empty())
        {
            fitted.pop_back();
            const std::wstring candidate = fitted + suffix;
            if (fits(candidate))
                return candidate;
        }

        return suffix;
    }

    void RenderFittedTextAt(int x, int y, int width, const wchar_t* text, DWORD color)
    {
        const std::wstring fitted = FitTextToWidth(text, width);
        RenderTextAt(x, y, width, fitted.c_str(), color);
    }

    int MeasureTextWidth(const std::wstring& text)
    {
        if (text.empty())
        {
            return 0;
        }

        SIZE extent{};
        if (!::GetTextExtentPoint32W(
                g_pRenderText->GetFontDC(),
                text.c_str(),
                static_cast<int>(text.size()),
                &extent))
        {
            return 0;
        }

        const float rate = g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f;
        return static_cast<int>(std::ceil(static_cast<float>(extent.cx) * rate));
    }

    int RenderPowerDigitsAt(int x, int y, unsigned long long powerScore)
    {
        wchar_t text[32] = {};
        mu_swprintf_s(text, L"%llu", powerScore);

        const float rateX = g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f;
        const float rateY = g_fScreenRate_y > 0.0f ? g_fScreenRate_y : 1.0f;
        float digitX = static_cast<float>(x);

        ::EnableAlphaTest();
        ::glEnable(GL_TEXTURE_2D);
        ::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        for (const wchar_t digitCharacter : std::wstring(text))
        {
            if (digitCharacter < L'0' || digitCharacter > L'9')
            {
                continue;
            }

            const int digit = digitCharacter - L'0';
            ::RenderBitmap(
                CharacterSceneTextures::SelectPowerDigits,
                digitX / rateX,
                static_cast<float>(y) / rateY,
                kPowerDigitWidth / rateX,
                kPowerDigitHeight / rateY,
                static_cast<float>(digit) * kPowerDigitAtlasCellWidth,
                0.0f,
                kPowerDigitAtlasCellWidth,
                kPowerDigitAtlasHeight);
            digitX += kPowerDigitWidth;
        }

        return static_cast<int>(std::ceil(digitX)) - x;
    }

    bool CursorInRect(int x, int y, int width, int height)
    {
        CInput& input = CInput::Instance();
        const long cursorX = input.GetCursorX();
        const long cursorY = input.GetCursorY();
        return cursorX >= x && cursorX < x + width && cursorY >= y && cursorY < y + height;
    }

    void OpenCharacterCreationWindow(int preferredSlot);

    int GetCharacterListItemCount()
    {
        return AccountCharacterList::GetUnlockedSlotCount()
            + (AccountCharacterList::CanPurchaseSlot() ? 1 : 0);
    }

    bool IsPurchaseSlotRow(int accountSlot)
    {
        return AccountCharacterList::CanPurchaseSlot()
            && accountSlot == AccountCharacterList::GetUnlockedSlotCount();
    }

    bool CursorInPurchaseSlotRow(int panelX, int panelY, int scrollOffset)
    {
        if (!AccountCharacterList::CanPurchaseSlot())
            return false;

        const int purchaseRow = AccountCharacterList::GetUnlockedSlotCount() - scrollOffset;
        if (purchaseRow < 0 || purchaseRow >= kVisibleRows)
            return false;

        const int listX = panelX + kPanelInset;
        const int listY = panelY + kListTopOffset;
        const int trackX = GetScrollTrackX(panelX);
        const int rowWidth = trackX - listX - 6;
        const int rowY = listY + (purchaseRow * kRowStride);
        return CursorInRect(listX, rowY, rowWidth, kRowHeight);
    }

    void OpenCharacterSlotStore()
    {
        using ShellExecuteWFunction = HINSTANCE(WINAPI*)(
            HWND,
            LPCWSTR,
            LPCWSTR,
            LPCWSTR,
            LPCWSTR,
            int);

        const HMODULE shell32 = ::LoadLibraryW(L"shell32.dll");
        const auto shellExecute = shell32 != nullptr
            ? reinterpret_cast<ShellExecuteWFunction>(::GetProcAddress(shell32, "ShellExecuteW"))
            : nullptr;
        if (shellExecute == nullptr)
        {
            g_ErrorReport.Write(
                L"[CharacterSlotStore] ShellExecuteW unavailable error=%lu\r\n",
                ::GetLastError());
            return;
        }

        const INT_PTR result = reinterpret_cast<INT_PTR>(shellExecute(
            g_hWnd,
            L"open",
            L"https://muonline.pt/store?tab=server&server_category=utilitye",
            nullptr,
            nullptr,
            SW_SHOWNORMAL));
        ::FreeLibrary(shell32);

        if (result <= 32)
        {
            g_ErrorReport.Write(
                L"[CharacterSlotStore] browser launch failed result=%d error=%lu\r\n",
                static_cast<int>(result),
                ::GetLastError());
        }
        else
        {
            g_ErrorReport.Write(
                L"[CharacterSlotStore] browser launch requested result=%d\r\n",
                static_cast<int>(result));
        }
    }

    void ActivateEmptyCharacterSlot(int accountSlot)
    {
        if (IsPurchaseSlotRow(accountSlot))
        {
            OpenCharacterSlotStore();
        }
        else
        {
            OpenCharacterCreationWindow(accountSlot);
        }
    }

    int GetMaxScrollOffset()
    {
        return std::max(0, GetCharacterListItemCount() - kVisibleRows);
    }

    int GetScrollThumbHeight()
    {
        const int proportional = (kScrollTrackHeight * kVisibleRows) / std::max(1, GetCharacterListItemCount());
        return std::max(kScrollThumbMinHeight, proportional);
    }

    int GetScrollThumbY(int trackY, int scrollOffset)
    {
        const int maxOffset = GetMaxScrollOffset();
        const int travel = kScrollTrackHeight - GetScrollThumbHeight();
        return maxOffset > 0 ? trackY + ((travel * scrollOffset) / maxOffset) : trackY;
    }

    bool HasAccountBlockedCharacter()
    {
        for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
        {
            const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot);
            if (entry != nullptr && (entry->CtlCode & CTLCODE_10ACCOUNT_BLOCKITEM))
                return true;
        }

        return false;
    }

    bool HasValidSelectedCharacter()
    {
        if (SelectedHero < 0 || SelectedHero >= AccountCharacterList::NativeVisibleSlots)
            return false;

        return CharactersClient[SelectedHero].Object.Live
            && CharactersClient[SelectedHero].ID[0] != L'\0';
    }

    int GetSelectedAccountSlot()
    {
        return HasValidSelectedCharacter()
            ? AccountCharacterPaging::GetVisibleSlot(SelectedHero)
            : -1;
    }

    void OpenCharacterCreationWindow(int preferredSlot = -1)
    {
        CUIMng& uiManager = CUIMng::Instance();
        const AccountCharacterList::Entry* preferredEntry = AccountCharacterList::GetBySlot(preferredSlot);
        const int emptySlot = preferredSlot >= 0
            && preferredSlot < AccountCharacterList::GetUnlockedSlotCount()
            && preferredEntry == nullptr
                ? preferredSlot
                : AccountCharacterList::FindFirstEmptySlot();

        if (emptySlot < 0)
        {
            uiManager.PopUpMsgWin(RECEIVE_CREATE_CHARACTER_FAIL2);
            return;
        }

        AccountCharacterList::SetPendingCreationSlot(emptySlot);
        uiManager.m_CharMakeWin.UpdateDisplay();
        uiManager.ShowWin(&uiManager.m_CharMakeWin);
    }

    CHARACTER* GetSelectedCharacter()
    {
        if (!HasValidSelectedCharacter())
            return nullptr;
        return &CharactersClient[SelectedHero];
    }

    void RenderAccountBlockMessage()
    {
        g_pRenderText->SetTextColor(0, 0, 0, 255);
        g_pRenderText->SetBgColor(255, 255, 0, 128);
        g_pRenderText->RenderText(kAccountBlockMsgX, kAccountBlockPrimaryY, I18N::Game::ThisAccountIsItemBlocked, 0, 0, RT3_WRITE_CENTER);
        g_pRenderText->RenderText(kAccountBlockMsgX, kAccountBlockSecondaryY, I18N::Game::PleaseCheckOnHttpMuonlineWebzenComSite, 0, 0, RT3_WRITE_CENTER);
    }
}

CCharSelMainWin::CCharSelMainWin()
    : m_scrollOffset(0),
      m_pendingSelectionSlot(-1),
      m_scrollDragOffsetY(0),
      m_enterPendingSelection(false),
      m_scrollDragging(false),
      m_createButtonPressed(false),
      m_purchaseButtonPressed(false),
      m_purchaseClickHandled(false),
      m_purchaseButtonVisualRow(-1),
      m_bAccountBlockItem(false)
{
}

CCharSelMainWin::~CCharSelMainWin()
{
}

void CCharSelMainWin::Create()
{
    m_aBtn[CSMW_BTN_CREATE].Create(54, 30, BITMAP_LOG_IN + 3, 4, 2, 1, 3);
    m_aBtn[CSMW_BTN_MENU].Create(54, 30, BITMAP_LOG_IN + 4, 3, 2, 1);
    m_aBtn[CSMW_BTN_CONNECT].Create(54, 30, BITMAP_LOG_IN + 5, 4, 2, 1, 3);
    m_aBtn[CSMW_BTN_DELETE].Create(54, 30, BITMAP_LOG_IN + 6, 4, 2, 1, 3);

    CWin::Create(kPanelWidth, kPanelHeight, -2);
    for (int i = 0; i < CSMW_BTN_MAX; ++i)
        CWin::RegisterButton(&m_aBtn[i]);

    m_scrollDragging = false;
    m_createButtonPressed = false;
    g_reorderPending = false;
    g_reorderSourceSlot = -1;
    g_reorderTargetSlot = -1;
    m_pendingSelectionSlot = -1;
    m_enterPendingSelection = false;
    m_bAccountBlockItem = HasAccountBlockedCharacter();
    ClampScrollOffset();
}

void CCharSelMainWin::PreRelease()
{
}

void CCharSelMainWin::SetPosition(int nXCoord, int nYCoord)
{
    CInput& input = CInput::Instance();
    const int maxX = std::max(0, static_cast<int>(input.GetScreenWidth()) - GetWidth() - 6);
    const int maxY = std::max(0, static_cast<int>(input.GetScreenHeight()) - GetHeight() - 6);
    const int panelX = std::clamp(nXCoord, 6, std::max(6, maxX));
    const int panelY = std::clamp(nYCoord, 6, std::max(6, maxY));

    CWin::SetPosition(panelX, panelY);
    m_ptTemp = m_ptPos;

    const int buttonWidth = m_aBtn[0].GetWidth();
    const int buttonY = static_cast<int>(input.GetScreenHeight()) - m_aBtn[0].GetHeight() - kBottomScreenInset;

    m_aBtn[CSMW_BTN_CREATE].SetPosition(kSideScreenInset, buttonY);
    m_aBtn[CSMW_BTN_MENU].SetPosition(kSideScreenInset + buttonWidth + kActionButtonGap, buttonY);

    const int rightX = static_cast<int>(input.GetScreenWidth()) - kSideScreenInset;
    m_aBtn[CSMW_BTN_DELETE].SetPosition(rightX - buttonWidth, buttonY);
    m_aBtn[CSMW_BTN_CONNECT].SetPosition(rightX - (buttonWidth * 2 + kActionButtonGap), buttonY);
}

void CCharSelMainWin::Show(bool bShow)
{
    CWin::Show(bShow);
    for (auto& button : m_aBtn)
        button.Show(bShow);

    if (!bShow)
    {
        m_createButtonPressed = false;
        m_purchaseButtonPressed = false;
        m_purchaseClickHandled = false;
        m_purchaseButtonVisualRow = -1;
    }
}

bool CCharSelMainWin::CursorInWin(int nArea)
{
    if (!CWin::m_bShow)
        return false;

    if (CWin::CursorInWin(nArea))
        return true;

    if (nArea == WA_ALL || nArea == WA_BUTTON)
    {
        if (CursorInPurchaseSlotRow(GetXPos(), GetYPos(), m_scrollOffset))
            return true;

        const int arrowX = GetArrowX(GetXPos());
        const int selectedSlot = m_pendingSelectionSlot >= 0
            ? m_pendingSelectionSlot
            : GetSelectedAccountSlot();
        const int arrowUpY = GetArrowUpY(GetYPos(), selectedSlot, m_scrollOffset);
        const int arrowDownY = arrowUpY >= 0
            ? arrowUpY + (kArrowDownTopOffset - kArrowUpTopOffset)
            : -1;
        if (arrowUpY >= 0
            && (CursorInRect(arrowX, arrowUpY, kArrowSize, kArrowSize)
                || CursorInRect(arrowX, arrowDownY, kArrowSize, kArrowSize)))
        {
            return true;
        }

        for (auto& button : m_aBtn)
        {
            if (button.CursorInObject())
                return true;
        }
    }

    return false;
}

void CCharSelMainWin::UpdateDisplay()
{
    AccountCharacterPaging::ClampCurrentPage();
    ClampScrollOffset();
    m_bAccountBlockItem = HasAccountBlockedCharacter();

    if (m_pendingSelectionSlot >= 0)
        ApplyPendingSelection();

    if (!HasValidSelectedCharacter() && m_pendingSelectionSlot < 0)
    {
        const int firstSlot = AccountCharacterPaging::GetFirstSlotOnCurrentPage();
        for (int visibleIndex = 0; visibleIndex < AccountCharacterPaging::SlotsPerPage; ++visibleIndex)
        {
            const int accountSlot = firstSlot + visibleIndex;
            const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(accountSlot);
            if (entry == nullptr || !CharactersClient[visibleIndex].Object.Live)
                continue;

            SelectedCharacter = visibleIndex;
            SelectedHero = visibleIndex;
            ScrollToSlot(accountSlot);
            break;
        }
    }

    m_aBtn[CSMW_BTN_CREATE].SetEnable(AccountCharacterList::HasEmptySlot());
    const bool hasSelection = HasValidSelectedCharacter();
    m_aBtn[CSMW_BTN_CONNECT].SetEnable(hasSelection);
    m_aBtn[CSMW_BTN_DELETE].SetEnable(hasSelection);
}

void CCharSelMainWin::UpdateWhileShow(double dDeltaTick)
{
    CUIMng& uiManager = CUIMng::Instance();
    CInput& input = CInput::Instance();
    m_purchaseClickHandled = false;

    const bool creationBlocked = uiManager.m_CharMakeWin.IsShow()
        || uiManager.m_MsgWin.IsShow()
        || uiManager.m_SysMenuWin.IsShow()
        || uiManager.m_OptionWin.IsShow();

    if (creationBlocked)
    {
        m_createButtonPressed = false;
        m_purchaseButtonPressed = false;
        m_purchaseButtonVisualRow = -1;
        return;
    }

    const bool createHovered = CursorInRect(
        m_aBtn[CSMW_BTN_CREATE].GetXPos(),
        m_aBtn[CSMW_BTN_CREATE].GetYPos(),
        m_aBtn[CSMW_BTN_CREATE].GetWidth(),
        m_aBtn[CSMW_BTN_CREATE].GetHeight());
    const bool purchaseHovered = CursorInPurchaseSlotRow(
        GetXPos(),
        GetYPos(),
        m_scrollOffset);

    if (input.IsLBtnDn() && createHovered)
        m_createButtonPressed = true;

    if (input.IsLBtnDn() && purchaseHovered)
    {
        m_purchaseButtonPressed = true;
        m_purchaseButtonVisualRow =
            AccountCharacterList::GetUnlockedSlotCount() - m_scrollOffset;
        g_ErrorReport.Write(
            L"[CharacterSlotStore] purchase row press cursor=(%d,%d) scroll=%d row=%d unlocked=%d\r\n",
            static_cast<int>(input.GetCursorX()),
            static_cast<int>(input.GetCursorY()),
            m_scrollOffset,
            m_purchaseButtonVisualRow,
            AccountCharacterList::GetUnlockedSlotCount());
    }

    if (!input.IsLBtnUp())
        return;

    if (m_purchaseButtonPressed)
    {
        const int listX = GetXPos() + kPanelInset;
        const int listY = GetYPos() + kListTopOffset;
        const int trackX = GetScrollTrackX(GetXPos());
        const int rowWidth = trackX - listX - 6;
        const int rowY = listY + (m_purchaseButtonVisualRow * kRowStride);
        const bool releasedInside = m_purchaseButtonVisualRow >= 0
            && m_purchaseButtonVisualRow < kVisibleRows
            && CursorInRect(listX, rowY, rowWidth, kRowHeight);

        m_purchaseButtonPressed = false;
        m_purchaseButtonVisualRow = -1;
        m_purchaseClickHandled = true;
        m_createButtonPressed = false;

        if (releasedInside)
        {
            g_ErrorReport.Write(
                L"[CharacterSlotStore] purchase row release cursor=(%d,%d)\r\n",
                static_cast<int>(input.GetCursorX()),
                static_cast<int>(input.GetCursorY()));
            ::PlayBuffer(SOUND_CLICK01);
            OpenCharacterSlotStore();
        }
        return;
    }

    const bool openCreation = m_createButtonPressed
        && createHovered
        && AccountCharacterList::HasEmptySlot();
    m_createButtonPressed = false;
    if (openCreation)
    {
        if (!m_aBtn[CSMW_BTN_CREATE].IsClick())
            ::PlayBuffer(SOUND_CLICK01);
        OpenCharacterCreationWindow();
    }
}

void CCharSelMainWin::UpdateWhileActive(double dDeltaTick)
{
    if (m_purchaseButtonPressed || m_purchaseClickHandled)
        return;

    if (ApplyPendingSelection())
        return;

    CUIMng& uiManager = CUIMng::Instance();
    if (m_aBtn[CSMW_BTN_CONNECT].IsClick())
    {
        ::StartGame();
    }
    else if (m_aBtn[CSMW_BTN_MENU].IsClick())
    {
        uiManager.ShowWin(&uiManager.m_SysMenuWin);
        uiManager.SetSysMenuWinShow(true);
    }
    else if (m_aBtn[CSMW_BTN_DELETE].IsClick())
    {
        DeleteCharacter();
    }

    UpdateCharacterList();
}

void CCharSelMainWin::RenderControls()
{
    ::EnableAlphaTest();
    ::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    if (!CUIMng::Instance().m_CharMakeWin.IsShow())
        RenderCharacterList();

    g_pRenderText->SetFont(g_hFixFont);
    g_pRenderText->SetTextColor(CLRDW_WHITE);
    g_pRenderText->SetBgColor(0, 0, 0, 0);

    if (m_bAccountBlockItem)
        RenderAccountBlockMessage();

    CInput& input = CInput::Instance();
    const int barY = static_cast<int>(input.GetScreenHeight()) - kBottomScreenInset - kBottomBarHeight - kBottomBarTopPadding;
    ::EnableAlphaBlend();
    FillRect(0, barY, static_cast<int>(input.GetScreenWidth()), kBottomBarHeight, 4, 6, 17, 104);
    FillRect(0, barY, static_cast<int>(input.GetScreenWidth()), 1, 138, 103, 54, 126);
    ::EndRenderColor();
    ::DisableAlphaBlend();

    CWin::RenderButtons();
}

void CCharSelMainWin::DeleteCharacter()
{
    CHARACTER* selected = GetSelectedCharacter();
    if (selected == nullptr)
        return;

    CUIMng& uiManager = CUIMng::Instance();
    if (selected->GuildStatus != G_NONE)
    {
        uiManager.PopUpMsgWin(MESSAGE_DELETE_CHARACTER_GUILDWARNING);
    }
    else if (selected->CtlCode & (CTLCODE_02BLOCKITEM | CTLCODE_10ACCOUNT_BLOCKITEM))
    {
        uiManager.PopUpMsgWin(MESSAGE_DELETE_CHARACTER_ID_BLOCK);
    }
    else
    {
        uiManager.PopUpMsgWin(MESSAGE_DELETE_CHARACTER_CONFIRM);
    }
}

void CCharSelMainWin::RenderCharacterList()
{
    const int panelX = GetXPos();
    const int panelY = GetYPos();
    const int listX = panelX + kPanelInset;
    const int listY = panelY + kListTopOffset;
    const int trackY = panelY + kScrollTrackTopOffset;
    const int trackX = GetScrollTrackX(panelX);
    const int rowWidth = trackX - listX - 6;
    const int selectedSlot = m_pendingSelectionSlot >= 0
        ? m_pendingSelectionSlot
        : GetSelectedAccountSlot();
    const int hoveredSlot = GetHoveredAccountSlot();

    ::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    for (int row = 0; row < kVisibleRows; ++row)
    {
        const int accountSlot = m_scrollOffset + row;
        if (accountSlot >= GetCharacterListItemCount())
            break;

        const int rowY = listY + (row * kRowStride);
        const bool selected = accountSlot == selectedSlot;
        const bool hovered = accountSlot == hoveredSlot;

        const int texture = selected
            ? CharacterSceneTextures::SelectRowSelected
            : hovered
                ? CharacterSceneTextures::SelectRowHover
                : CharacterSceneTextures::SelectRowNormal;
        RenderBitmapAt(texture, listX, rowY, rowWidth, kRowHeight);
    }

    ::EnableAlphaBlend();
    FillRect(trackX, trackY, kScrollTrackWidth, kScrollTrackHeight, 58, 64, 73, 145);
    const int thumbY = GetScrollThumbY(trackY, m_scrollOffset);
    const bool thumbHovered = CursorInRect(trackX - 4, thumbY, kScrollTrackWidth + 8, GetScrollThumbHeight());
    FillRect(
        trackX - 1,
        thumbY,
        kScrollTrackWidth + 2,
        GetScrollThumbHeight(),
        m_scrollDragging || thumbHovered ? 226 : 178,
        m_scrollDragging || thumbHovered ? 192 : 160,
        m_scrollDragging || thumbHovered ? 112 : 101,
        230);

    ::EndRenderColor();
    ::DisableAlphaBlend();

    const int arrowX = GetArrowX(panelX);
    const int selectedAccountSlot = m_pendingSelectionSlot >= 0
        ? m_pendingSelectionSlot
        : GetSelectedAccountSlot();
    const int arrowUpY = GetArrowUpY(panelY, selectedAccountSlot, m_scrollOffset);
    const int arrowDownY = arrowUpY >= 0
        ? arrowUpY + (kArrowDownTopOffset - kArrowUpTopOffset)
        : -1;
    const bool canMoveUp = !g_reorderPending
        && FindOccupiedSlotInDirection(selectedAccountSlot, -1) >= 0;
    const bool canMoveDown = !g_reorderPending
        && FindOccupiedSlotInDirection(selectedAccountSlot, 1) >= 0;
    const bool arrowUpHovered = canMoveUp && CursorInRect(arrowX, arrowUpY, kArrowSize, kArrowSize);
    const bool arrowDownHovered = canMoveDown && CursorInRect(arrowX, arrowDownY, kArrowSize, kArrowSize);
    if (arrowUpY >= 0)
    {
        CInput& input = CInput::Instance();
        RenderArrowButton(
            CharacterSceneTextures::SelectArrowUp,
            arrowX,
            arrowUpY,
            canMoveUp,
            arrowUpHovered,
            arrowUpHovered && input.IsLBtnHeldDn());
        RenderArrowButton(
            CharacterSceneTextures::SelectArrowDown,
            arrowX,
            arrowDownY,
            canMoveDown,
            arrowDownHovered,
            arrowDownHovered && input.IsLBtnHeldDn());
    }

    g_pRenderText->SetFont(g_hFixFont);
    ::EnableAlphaTest();
    g_pRenderText->SetBgColor(0, 0, 0, 0);
    wchar_t accountCount[32] = {};
    mu_swprintf_s(accountCount, L"%d/%d", AccountCharacterList::GetCount(), AccountCharacterList::GetUnlockedSlotCount());
    RenderTextAt(panelX + kPanelWidth - 82, panelY - 17, 70, accountCount, ARGB(255, 245, 245, 245), RT3_SORT_RIGHT);

    for (int row = 0; row < kVisibleRows; ++row)
    {
        const int accountSlot = m_scrollOffset + row;
        if (accountSlot >= GetCharacterListItemCount())
            break;

        const int rowY = listY + (row * kRowStride);
        const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(accountSlot);
        if (entry == nullptr)
        {
            if (IsPurchaseSlotRow(accountSlot))
            {
                RenderTextAt(listX - 6, rowY + 16, rowWidth, L"Comprar Slot", ARGB(255, 196, 188, 181), RT3_SORT_CENTER);
            }
            else
            {
                RenderTextAt(listX + 8, rowY + 17, rowWidth - 16, I18N::Game::CreateACharacter, ARGB(255, 196, 188, 181), RT3_SORT_CENTER);
            }
            continue;
        }

        const bool selected = accountSlot == selectedSlot;
        const DWORD nameColor = (entry->CtlCode & CTLCODE_01BLOCKCHAR)
            ? ARGB(255, 99, 213, 223)
            : selected ? ARGB(255, 255, 222, 108) : ARGB(255, 224, 213, 197);
        constexpr int textInsetLeft = 11;
        constexpr int textInsetRight = 16;
        constexpr int nameMaxWidth = 92;
        const int textWidth = rowWidth - textInsetLeft - textInsetRight;

        wchar_t classAndLevel[128] = {};
        mu_swprintf_s(
            classAndLevel,
            L"%ls - %d",
            gCharacterManager.GetCharacterClassText(entry->Class),
            entry->Level);
        RenderFittedTextAt(
            listX + textInsetLeft,
            rowY + 6,
            textWidth,
            classAndLevel,
            ARGB(255, 244, 236, 226));

        const std::wstring fittedName = FitTextToWidth(entry->Name, nameMaxWidth);
        const int nameX = listX + textInsetLeft;
        RenderTextAt(nameX, rowY + 25, nameMaxWidth, fittedName.c_str(), nameColor);

        const int separatorX = nameX + MeasureTextWidth(fittedName) + 2;
        constexpr const wchar_t* separator = L"-";
        RenderTextAt(separatorX, rowY + 25, 8, separator, ARGB(255, 214, 202, 183));

        const int powerIconX = separatorX + 10;
        RenderBitmapAt(CharacterSceneTextures::SelectPowerIcon, powerIconX, rowY + 26, 10, 10);
        RenderPowerDigitsAt(powerIconX + 12, rowY + 27, entry->PowerScore);
    }
}

void CCharSelMainWin::UpdateCharacterList()
{
    CInput& input = CInput::Instance();
    const int panelX = GetXPos();
    const int panelY = GetYPos();
    const int listY = panelY + kListTopOffset;
    const int trackY = panelY + kScrollTrackTopOffset;
    const int trackX = GetScrollTrackX(panelX);
    const int thumbY = GetScrollThumbY(trackY, m_scrollOffset);
    const int thumbHeight = GetScrollThumbHeight();
    const int arrowX = GetArrowX(panelX);
    const int selectedAccountSlot = m_pendingSelectionSlot >= 0
        ? m_pendingSelectionSlot
        : GetSelectedAccountSlot();
    const int arrowUpY = GetArrowUpY(panelY, selectedAccountSlot, m_scrollOffset);
    const int arrowDownY = arrowUpY >= 0
        ? arrowUpY + (kArrowDownTopOffset - kArrowUpTopOffset)
        : -1;
    const int moveUpTarget = FindOccupiedSlotInDirection(selectedAccountSlot, -1);
    const int moveDownTarget = FindOccupiedSlotInDirection(selectedAccountSlot, 1);

    if (g_reorderPending
        && timeGetTime() - g_reorderRequestTime >= kReorderRequestTimeout)
    {
        g_reorderPending = false;
        g_reorderSourceSlot = -1;
        g_reorderTargetSlot = -1;
        g_reorderRequestTime = 0;
    }

    if ((input.IsLBtnUp() || input.IsLBtnDbl())
        && CursorInPurchaseSlotRow(panelX, panelY, m_scrollOffset))
    {
        g_ErrorReport.Write(
            L"[CharacterSlotStore] purchase row click cursor=(%d,%d) scroll=%d unlocked=%d\r\n",
            static_cast<int>(input.GetCursorX()),
            static_cast<int>(input.GetCursorY()),
            m_scrollOffset,
            AccountCharacterList::GetUnlockedSlotCount());
        ::PlayBuffer(SOUND_CLICK01);
        OpenCharacterSlotStore();
        return;
    }

    if (CursorInRect(panelX, listY, kPanelWidth, kListHeight) && MouseWheel != 0)
    {
        m_scrollOffset += MouseWheel > 0 ? -1 : 1;
        ClampScrollOffset();
        MouseWheel = 0;
    }

    if (input.IsLBtnUp()
        && !g_reorderPending
        && moveUpTarget >= 0
        && CursorInRect(arrowX, arrowUpY, kArrowSize, kArrowSize))
    {
        g_ErrorReport.Write(
            L"[CharacterReorder] up click source=%d target=%d cursor=(%d,%d)\r\n",
            selectedAccountSlot,
            moveUpTarget,
            static_cast<int>(input.GetCursorX()),
            static_cast<int>(input.GetCursorY()));
        if (SendRequestCharacterReorder(selectedAccountSlot, moveUpTarget))
        {
            ::PlayBuffer(SOUND_CLICK01);
            g_reorderPending = true;
            g_reorderSourceSlot = selectedAccountSlot;
            g_reorderTargetSlot = moveUpTarget;
            g_reorderRequestTime = timeGetTime();
        }
        return;
    }

    if (input.IsLBtnUp()
        && !g_reorderPending
        && moveDownTarget >= 0
        && CursorInRect(arrowX, arrowDownY, kArrowSize, kArrowSize))
    {
        g_ErrorReport.Write(
            L"[CharacterReorder] down click source=%d target=%d cursor=(%d,%d)\r\n",
            selectedAccountSlot,
            moveDownTarget,
            static_cast<int>(input.GetCursorX()),
            static_cast<int>(input.GetCursorY()));
        if (SendRequestCharacterReorder(selectedAccountSlot, moveDownTarget))
        {
            ::PlayBuffer(SOUND_CLICK01);
            g_reorderPending = true;
            g_reorderSourceSlot = selectedAccountSlot;
            g_reorderTargetSlot = moveDownTarget;
            g_reorderRequestTime = timeGetTime();
        }
        return;
    }

    if (input.IsLBtnUp())
        m_scrollDragging = false;

    if (input.IsLBtnDn() && CursorInRect(trackX - 4, thumbY, kScrollTrackWidth + 8, thumbHeight))
    {
        m_scrollDragging = true;
        m_scrollDragOffsetY = static_cast<int>(input.GetCursorY()) - thumbY;
    }
    else if (input.IsLBtnDn() && CursorInRect(trackX - 4, trackY, kScrollTrackWidth + 8, kScrollTrackHeight))
    {
        const int travel = std::max(1, kScrollTrackHeight - thumbHeight);
        const int target = std::clamp(
            static_cast<int>(input.GetCursorY()) - trackY - (thumbHeight / 2),
            0,
            travel);
        m_scrollOffset = static_cast<int>(std::lround(
            (static_cast<double>(target) / static_cast<double>(travel)) * GetMaxScrollOffset()));
        ClampScrollOffset();
    }

    if (m_scrollDragging && input.IsLBtnHeldDn())
    {
        const int travel = std::max(1, kScrollTrackHeight - thumbHeight);
        const int target = std::clamp(
            static_cast<int>(input.GetCursorY()) - trackY - m_scrollDragOffsetY,
            0,
            travel);
        m_scrollOffset = static_cast<int>(std::lround(
            (static_cast<double>(target) / static_cast<double>(travel)) * GetMaxScrollOffset()));
        ClampScrollOffset();
        return;
    }

    const int hoveredSlot = GetHoveredAccountSlot();
    if (hoveredSlot < 0)
        return;

    if (input.IsLBtnDbl())
    {
        ::PlayBuffer(SOUND_CLICK01);
        const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(hoveredSlot);
        if (entry != nullptr)
            SelectAccountSlot(hoveredSlot, true);
        else
            ActivateEmptyCharacterSlot(hoveredSlot);
    }
    else if (input.IsLBtnUp())
    {
        ::PlayBuffer(SOUND_CLICK01);
        const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(hoveredSlot);
        if (entry != nullptr)
            SelectAccountSlot(hoveredSlot, false);
        else
            ActivateEmptyCharacterSlot(hoveredSlot);
    }
}

void CCharSelMainWin::OnCharacterReorderResponse(bool success, int sourceSlot, int targetSlot)
{
    g_ErrorReport.Write(
        L"[CharacterReorder] response success=%d source=%d target=%d pending=%d expected=(%d,%d)\r\n",
        success ? 1 : 0,
        sourceSlot,
        targetSlot,
        g_reorderPending ? 1 : 0,
        g_reorderSourceSlot,
        g_reorderTargetSlot);

    if (!g_reorderPending
        || sourceSlot != g_reorderSourceSlot
        || targetSlot != g_reorderTargetSlot)
    {
        return;
    }

    g_reorderPending = false;
    g_reorderSourceSlot = -1;
    g_reorderTargetSlot = -1;
    g_reorderRequestTime = 0;

    if (!success || !AccountCharacterList::SwapSlots(sourceSlot, targetSlot))
        return;

    ScrollToSlot(targetSlot);
    if (AccountCharacterPaging::IsSlotOnCurrentPage(targetSlot))
        AccountCharacterPaging::RefreshVisibleCharacters();

    SelectAccountSlot(targetSlot, false);
    UpdateDisplay();
}

void CCharSelMainWin::ClampScrollOffset()
{
    m_scrollOffset = std::clamp(m_scrollOffset, 0, GetMaxScrollOffset());
}

void CCharSelMainWin::ScrollToSlot(int accountSlot)
{
    if (accountSlot < 0 || accountSlot >= AccountCharacterList::MaxCharacters)
        return;

    if (accountSlot < m_scrollOffset)
        m_scrollOffset = accountSlot;
    else if (accountSlot >= m_scrollOffset + kVisibleRows)
        m_scrollOffset = accountSlot - kVisibleRows + 1;

    ClampScrollOffset();
}

int CCharSelMainWin::GetHoveredAccountSlot()
{
    const int listX = GetXPos() + kPanelInset;
    const int listY = GetYPos() + kListTopOffset;
    const int trackX = GetScrollTrackX(GetXPos());
    const int rowWidth = trackX - listX - 6;
    CInput& input = CInput::Instance();

    if (!CursorInRect(listX, listY, rowWidth, kListHeight))
        return -1;

    const int localY = static_cast<int>(input.GetCursorY()) - listY;
    const int row = localY / kRowStride;
    if (row < 0 || row >= kVisibleRows || (localY % kRowStride) >= kRowHeight)
        return -1;

    const int accountSlot = m_scrollOffset + row;
    return accountSlot < GetCharacterListItemCount() ? accountSlot : -1;
}

bool CCharSelMainWin::SelectAccountSlot(int accountSlot, bool enterGame)
{
    const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(accountSlot);
    if (entry == nullptr)
        return false;

    ScrollToSlot(accountSlot);
    m_pendingSelectionSlot = accountSlot;
    m_enterPendingSelection = enterGame;
    SelectedCharacter = -1;

    if (!AccountCharacterPaging::IsSlotOnCurrentPage(accountSlot))
    {
        SelectedHero = -1;
        AccountCharacterPaging::SetPageForSlot(accountSlot);
        CUIMng::Instance().m_CharInfoBalloonMng.UpdateDisplay();
        UpdateDisplay();
        return true;
    }

    ApplyPendingSelection();
    return true;
}

bool CCharSelMainWin::ApplyPendingSelection()
{
    if (m_pendingSelectionSlot < 0
        || !AccountCharacterPaging::IsSlotOnCurrentPage(m_pendingSelectionSlot))
    {
        return false;
    }

    const int visibleIndex = m_pendingSelectionSlot - AccountCharacterPaging::GetFirstSlotOnCurrentPage();
    const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(m_pendingSelectionSlot);
    if (entry == nullptr
        || visibleIndex < 0
        || visibleIndex >= AccountCharacterList::NativeVisibleSlots
        || !CharactersClient[visibleIndex].Object.Live
        || std::wcscmp(CharactersClient[visibleIndex].ID, entry->Name) != 0)
    {
        return false;
    }

    SelectedCharacter = visibleIndex;
    SelectedHero = visibleIndex;
    const bool enterGame = m_enterPendingSelection;
    m_pendingSelectionSlot = -1;
    m_enterPendingSelection = false;

    m_aBtn[CSMW_BTN_CONNECT].SetEnable(true);
    m_aBtn[CSMW_BTN_DELETE].SetEnable(true);
    CUIMng::Instance().m_CharInfoBalloonMng.UpdateDisplay();

    if (enterGame)
        ::StartGame();

    return enterGame;
}
