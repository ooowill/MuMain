//*****************************************************************************
// File: CharInfoBalloonMng.cpp
//
// Desc: implementation of the CCharInfoBalloonMng class.
//
// producer: Ahn Sang-Kyu
//*****************************************************************************

#include "stdafx.h"
#include "CharInfoBalloonMng.h"

#include "AccountCharacterPaging.h"
#include "CharInfoBalloon.h"

CCharInfoBalloonMng::~CCharInfoBalloonMng()
{
    Release();
}

void CCharInfoBalloonMng::Release()
{
    if (!m_isInitialized)
        return;

    m_isInitialized = false;
}

//*****************************************************************************
// 함수 이름 : Create()
// 함수 설명 : 캐릭터 정보 풍선 매니저 생성.
//			   (캐릭터 선택씬에서 쓰임. 풍선 5개 생성.)
//*****************************************************************************
void CCharInfoBalloonMng::Create()
{
    for (std::size_t i = 0; i < kBalloonCount; ++i)
    {
        const int slot = AccountCharacterPaging::GetVisibleSlot(static_cast<int>(i));
        m_charInfoBalloons[i].Create(slot >= 0 ? &CharactersClient[i] : nullptr);
    }

    m_isInitialized = true;
}

//*****************************************************************************
// 함수 이름 : Render()
// 함수 설명 : 캐릭터 정보 풍선들 렌더.
//*****************************************************************************
void CCharInfoBalloonMng::Render()
{
    // Character details are rendered in the Season 21 list panel.
}

//*****************************************************************************
// 함수 이름 : UpdateDisplay()
// 함수 설명 : 캐릭터 정보를 업데이트.
//*****************************************************************************
void CCharInfoBalloonMng::UpdateDisplay()
{
    if (!m_isInitialized)
        return;

    for (std::size_t i = 0; i < kBalloonCount; ++i)
    {
        const int slot = AccountCharacterPaging::GetVisibleSlot(static_cast<int>(i));
        m_charInfoBalloons[i].SetCharacter(slot >= 0 ? &CharactersClient[i] : nullptr);
    }
}
