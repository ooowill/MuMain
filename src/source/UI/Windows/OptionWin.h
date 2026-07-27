//*****************************************************************************
// File: OptionWin.h
//*****************************************************************************
#pragma once

#include "UI/Widgets/Win.h"
#include "UI/Widgets/Button.h"

#define OW_BTN_BGM             0
#define OW_BTN_MUTE            1
#define OW_BTN_RENDER_5        2
#define OW_BTN_RENDER_7        3
#define OW_BTN_RENDER_9        4
#define OW_BTN_RENDER_11       5
#define OW_BTN_RENDER_13       6
#define OW_BTN_CLOSE           7
#define OW_BTN_MAX             8

class COptionWin : public CWin
{
protected:
    CWin m_winBack;
    CButton m_aBtn[OW_BTN_MAX];
    int m_lastSoundVolume{5};
    int m_lastMusicVolume{5};

public:
    COptionWin();
    virtual ~COptionWin();

    void Create();
    void SetPosition(int nXCoord, int nYCoord);
    void Show(bool bShow);
    bool CursorInWin(int nArea);
    void UpdateDisplay();

protected:
    void PreRelease();
    void UpdateWhileShow(double dDeltaTick);
    void RenderControls();

private:
    void ApplySoundVolume(int level);
    void ApplyMusicEnabled(bool enabled);
    void CloseWindow();
};