//*****************************************************************************
// File: CharSelMainWin.h
//*****************************************************************************

#pragma once

#include "UI/Widgets/Win.h"
#include "UI/Widgets/Button.h"

#define CSMW_BTN_CREATE  0
#define CSMW_BTN_MENU    1
#define CSMW_BTN_CONNECT 2
#define CSMW_BTN_DELETE  3
#define CSMW_BTN_MAX     4

class CCharSelMainWin : public CWin
{
protected:
    CButton m_aBtn[CSMW_BTN_MAX];
    int m_scrollOffset;
    int m_pendingSelectionSlot;
    int m_scrollDragOffsetY;
    bool m_enterPendingSelection;
    bool m_scrollDragging;
    bool m_createButtonPressed;
    bool m_purchaseButtonPressed;
    bool m_purchaseClickHandled;
    int m_purchaseButtonVisualRow;
    bool m_bAccountBlockItem;

public:
    CCharSelMainWin();
    virtual ~CCharSelMainWin();

    void Create();
    void SetPosition(int nXCoord, int nYCoord);
    void Show(bool bShow);
    bool CursorInWin(int nArea);
    void UpdateDisplay();
    void OnCharacterReorderResponse(bool success, int sourceSlot, int targetSlot);

protected:
    void PreRelease();
    void UpdateWhileShow(double dDeltaTick);
    void UpdateWhileActive(double dDeltaTick);
    void RenderControls();
    void DeleteCharacter();
    void RenderCharacterList();
    void UpdateCharacterList();
    void ClampScrollOffset();
    void ScrollToSlot(int accountSlot);
    int GetHoveredAccountSlot();
    bool SelectAccountSlot(int accountSlot, bool enterGame);
    bool ApplyPendingSelection();
};
