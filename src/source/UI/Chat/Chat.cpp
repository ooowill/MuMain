#include "stdafx.h"
#include "Core/Text/TextLineWrap.h"
#include "UI/Chat/Chat.h"
#include "Character/CharacterManager.h" // gCharacterManager
#include "Character/GuildProfileClient.h"
#include "Character/KillNotificationClient.h"
#include "Camera/CameraProjection.h" // CameraProjection

#include <algorithm>
#include <cwchar>
#include <iterator>

// Includes mirror ZzzInterface.cpp, the unit these were extracted from.
#include "Core/Platform/Imm.h"
#include "UI/Legacy/UIManager.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/PlayerActionState.h"
#include "Render/Textures/ZzzTexture.h"
#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/ZzzInterface.h"
#include "GameLogic/Combat/ClassAttack.h"
#include "GameLogic/Combat/SkillCast.h"
#include "GameLogic/Combat/SkillExecution.h"
#include "Core/Input/ImeInput.h"
#include "Engine/Object/ZzzInventory.h"
#include "Engine/Object/ZzzOpenData.h"
#include "Render/Effects/ZzzEffect.h"
#include "Scenes/SceneCore.h"
#include "Engine/Pathing/ZzzPath.h"
#include "Audio/DSPlaySound.h"
#include "I18N/All.h"
#include "GameLogic/Events/MatchEvent.h"
#include "GameLogic/Items/PersonalShopTitleImp.h"
#include "GameLogic/Quests/CSQuest.h"
#include "GameLogic/Items/CSItemOption.h"
#include "GameLogic/NPCs/npcBreeder.h"
#include "GameLogic/Pets/GIPetManager.h"
#include "Character/CSParts.h"
#include "UI/Legacy/UIMapName.h"	// rozy
#include "GameLogic/Events/Cinematic/CDirection.h"
#include "World/MapInfra/MapManager.h"
#include "GameLogic/Events/Event.h"
#include "UI/NewUI/NewUISystem.h"
#include "GameLogic/Events/w_CursedTemple.h"
#include "UI/Legacy/UIControls.h"
#include "GameLogic/Social/PartyManager.h"
#include "UI/NewUI/Dialogs/NewUICommonMessageBox.h"
#include "GameLogic/Skills/SummonSystem.h"
#include "GameLogic/Skills/SkillManager.h"
#include "World/MapInfra/w_MapHeaders.h"
#include "GameLogic/Combat/DuelMgr.h"

namespace UI::Chat
{
typedef struct
{
    wchar_t   ID[32];
    wchar_t      Union[64];
    wchar_t      Guild[64];
    wchar_t      szShopTitle[32];
    char      Color;
    char      GuildColor;
    float       IDLifeTime;
    wchar_t   Text[2][256];
    float       LifeTime[2];
    CHARACTER* Owner;
    int       x, y;
    int       Width;
    int       Height;
    vec3_t    Position;
} CHAT;

#define MAX_CHAT 120

CHAT Chat[MAX_CHAT];

namespace
{
    constexpr int kGuildMarkLineSize = 12;
    constexpr int kGuildMarkLineGap = 4;
    constexpr int kHoverCardWidth = 152;
    constexpr int kHoverCardNoGuildHeight = 44;
    constexpr int kHoverCardGuildHeight = 56;
    constexpr int kHoverAvatarSize = 24;
    constexpr int kHoverAvatarFrameSize = 28;
    constexpr int kHoverGuildMarkSize = 10;

    bool HasGuildMark(const CHARACTER* owner)
    {
        return owner != nullptr
            && owner->GuildMarkIndex >= 0
            && owner->GuildMarkIndex < MAX_MARKS
            && GuildMark[owner->GuildMarkIndex].GuildName[0] != L'\0';
    }

    int GetGuildKey(const CHARACTER* owner)
    {
        return HasGuildMark(owner) ? GuildMark[owner->GuildMarkIndex].Key : -1;
    }

    bool TryGetGuildLevel(const CHARACTER* owner, unsigned int& level)
    {
        return GuildProfileClient::GetGuildLevel(GetGuildKey(owner), level);
    }

    const wchar_t* GetGuildRoleLabel(BYTE guildStatus)
    {
        switch (guildStatus)
        {
        case G_MASTER:
            return I18N::Game::Master;
        case G_SUB_MASTER:
            return I18N::Game::AssistM;
        case G_BATTLE_MASTER:
            return I18N::Game::BattleM;
        case G_PERSON:
            return I18N::Game::Members;
        default:
            return L"Guild";
        }
    }

    void BuildGuildLine(wchar_t* destination, size_t destinationLength, const CHARACTER* owner)
    {
        if (destination == nullptr || destinationLength == 0)
        {
            return;
        }

        destination[0] = L'\0';
        if (!HasGuildMark(owner))
        {
            return;
        }

        unsigned int guildLevel = 0;
        const wchar_t* guildName = GuildMark[owner->GuildMarkIndex].GuildName;
        const wchar_t* role = GetGuildRoleLabel(owner->GuildStatus);
        if (TryGetGuildLevel(owner, guildLevel))
        {
            swprintf_s(destination, destinationLength, L"[%ls] Nv.%u %ls", guildName, guildLevel, role);
        }
        else
        {
            swprintf_s(destination, destinationLength, L"[%ls] %ls", guildName, role);
        }
    }

    void CopyWideText(wchar_t* destination, size_t destinationLength, const wchar_t* source)
    {
        if (destination == nullptr || destinationLength == 0)
        {
            return;
        }

        if (source == nullptr)
        {
            destination[0] = L'\0';
            return;
        }

        std::wcsncpy(destination, source, destinationLength - 1);
        destination[destinationLength - 1] = L'\0';
    }

    void DrawSolidRect(float x, float y, float width, float height, float red, float green, float blue, float alpha)
    {
        EnableAlphaTest();
        glColor4f(red, green, blue, alpha);
        RenderColor(x, y, width, height);
        EndRenderColor();
    }

    void DrawBorder(float x, float y, float width, float height, float red, float green, float blue, float alpha)
    {
        DrawSolidRect(x, y, width, 1.f, red, green, blue, alpha);
        DrawSolidRect(x, y + height - 1.f, width, 1.f, red, green, blue, alpha);
        DrawSolidRect(x, y, 1.f, height, red, green, blue, alpha);
        DrawSolidRect(x + width - 1.f, y, 1.f, height, red, green, blue, alpha);
    }

    void RenderGuildMarkIcon(const CHARACTER* owner, float x, float y, float size)
    {
        if (!HasGuildMark(owner))
        {
            return;
        }

        CreateGuildMark(owner->GuildMarkIndex);
        RenderBitmap(BITMAP_GUILD, x, y, size, size);
    }

    bool IsSelectedPlayer(const CHAT* chat)
    {
        if (chat == nullptr || chat->Owner == nullptr || chat->Owner == Hero)
        {
            return false;
        }

        if (SelectedCharacter < 0 || SelectedCharacter >= MAX_CHARACTERS_CLIENT)
        {
            return false;
        }

        return chat->Owner == &CharactersClient[SelectedCharacter]
            && chat->Owner->Object.Kind == KIND_PLAYER
            && chat->Owner->Object.Type == MODEL_PLAYER;
    }

    void RenderHoverCard(CHAT* chat)
    {
        if (!IsSelectedPlayer(chat))
        {
            return;
        }

        const bool hasGuild = HasGuildMark(chat->Owner);
        const int cardHeight = hasGuild ? kHoverCardGuildHeight : kHoverCardNoGuildHeight;
        int cardX = chat->x + chat->Width + 8;
        int cardY = chat->y - 6;

        if (cardX + kHoverCardWidth > static_cast<int>(WindowWidth) - 6)
        {
            cardX = chat->x - kHoverCardWidth - 8;
        }

        cardX = std::clamp(cardX, 6, static_cast<int>(WindowWidth) - kHoverCardWidth - 6);
        cardY = std::clamp(cardY, 6, static_cast<int>(WindowHeight) - cardHeight - 6);

        DrawSolidRect(static_cast<float>(cardX + 2), static_cast<float>(cardY + 3), static_cast<float>(kHoverCardWidth), static_cast<float>(cardHeight), 0.f, 0.f, 0.f, 0.48f);
        DrawSolidRect(static_cast<float>(cardX), static_cast<float>(cardY), static_cast<float>(kHoverCardWidth), static_cast<float>(cardHeight), 0.020f, 0.026f, 0.036f, 0.88f);
        DrawSolidRect(static_cast<float>(cardX + 1), static_cast<float>(cardY + 1), static_cast<float>(kHoverCardWidth - 2), 13.f, 0.23f, 0.14f, 0.035f, 0.82f);
        DrawSolidRect(static_cast<float>(cardX + kHoverCardWidth - 36), static_cast<float>(cardY), 34.f, 3.f, 0.76f, 0.55f, 0.18f, 0.95f);
        DrawBorder(static_cast<float>(cardX), static_cast<float>(cardY), static_cast<float>(kHoverCardWidth), static_cast<float>(cardHeight), 0.65f, 0.46f, 0.14f, 0.78f);

        const int avatarFrameX = cardX + kHoverCardWidth - kHoverAvatarFrameSize - 5;
        const int avatarFrameY = cardY + 5;
        DrawSolidRect(static_cast<float>(avatarFrameX), static_cast<float>(avatarFrameY), static_cast<float>(kHoverAvatarFrameSize), static_cast<float>(kHoverAvatarFrameSize), 0.07f, 0.055f, 0.035f, 0.92f);
        DrawBorder(static_cast<float>(avatarFrameX), static_cast<float>(avatarFrameY), static_cast<float>(kHoverAvatarFrameSize), static_cast<float>(kHoverAvatarFrameSize), 0.78f, 0.58f, 0.22f, 0.86f);
        KillNotificationClient::RenderProfileAvatar(chat->ID, static_cast<float>(avatarFrameX + 2), static_cast<float>(avatarFrameY + 2), static_cast<float>(kHoverAvatarSize));

        const int textX = cardX + 8;
        const int textWidth = kHoverCardWidth - kHoverAvatarFrameSize - 18;

        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->SetTextColor(255, 218, 94, 255);
        g_pRenderText->RenderText(textX, cardY + 3, chat->ID, textWidth, 0, RT3_SORT_LEFT);

        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetTextColor(210, 220, 235, 255);
        wchar_t classLine[64] = {};
        swprintf_s(classLine, L"%ls %d", gCharacterManager.GetCharacterClassText(chat->Owner->Class), chat->Owner->Level);
        g_pRenderText->RenderText(textX, cardY + 19, classLine, textWidth, 0, RT3_SORT_LEFT);

        if (!hasGuild)
        {
            g_pRenderText->SetTextColor(150, 160, 172, 255);
            g_pRenderText->RenderText(textX, cardY + 33, L"Sem guild", kHoverCardWidth - 16, 0, RT3_SORT_LEFT);
            return;
        }

        RenderGuildMarkIcon(chat->Owner, static_cast<float>(textX), static_cast<float>(cardY + 36), static_cast<float>(kHoverGuildMarkSize));

        wchar_t guildLine[64] = {};
        BuildGuildLine(guildLine, std::size(guildLine), chat->Owner);
        g_pRenderText->SetTextColor(255, 232, 165, 255);
        g_pRenderText->RenderText(textX + kHoverGuildMarkSize + 4, cardY + 34, guildLine, kHoverCardWidth - kHoverGuildMarkSize - 20, 0, RT3_SORT_LEFT);
    }
}

void SetBooleanPosition(CHAT* c)
{
    BOOL bResult[5];
    SIZE Size[5];
    memset(&Size[0], 0, sizeof(SIZE) * 5);

    if (g_isCharacterBuff((&c->Owner->Object), eBuff_GMEffect) || (c->Owner->CtlCode == CTLCODE_20OPERATOR) || (c->Owner->CtlCode == CTLCODE_08OPERATOR))
    {
        g_pRenderText->SetFont(g_hFontBold);
        bResult[0] = GetTextExtentPoint32(g_pRenderText->GetFontDC(), c->ID, lstrlen(c->ID), &Size[0]);
        g_pRenderText->SetFont(g_hFont);
    }
    else
    {
        bResult[0] = GetTextExtentPoint32(g_pRenderText->GetFontDC(), c->ID, lstrlen(c->ID), &Size[0]);
    }

    bResult[1] = GetTextExtentPoint32(g_pRenderText->GetFontDC(), c->Text[0], lstrlen(c->Text[0]), &Size[1]);
    bResult[2] = GetTextExtentPoint32(g_pRenderText->GetFontDC(), c->Text[1], lstrlen(c->Text[1]), &Size[2]);
    bResult[3] = GetTextExtentPoint32(g_pRenderText->GetFontDC(), c->Union, lstrlen(c->Union), &Size[3]);
    bResult[4] = GetTextExtentPoint32(g_pRenderText->GetFontDC(), c->Guild, lstrlen(c->Guild), &Size[4]);
    if (HasGuildMark(c->Owner) && c->Guild[0] != L'\0')
    {
        Size[4].cx += (kGuildMarkLineSize + kGuildMarkLineGap) * g_fScreenRate_x;
    }

    Size[0].cx += 3;

    if (c->LifeTime[1] > 0)
        c->Width = std::max<int>(std::max<int>(std::max<int>(Size[0].cx, Size[1].cx), std::max<int>(Size[2].cx, Size[3].cx)), Size[4].cx);

    else if (c->LifeTime[0] > 0)
        c->Width = std::max<int>(std::max<int>(Size[0].cx, Size[1].cx), std::max<int>(Size[3].cx, Size[4].cx));
    else
        c->Width = std::max<int>(std::max<int>(Size[0].cx, Size[3].cx), Size[4].cx);
    c->Height = FontHeight * (bResult[0] + bResult[1] + bResult[2] + bResult[3] + bResult[4]);

    if (lstrlen(c->szShopTitle) > 0)
    {
        SIZE sizeT[2];
        g_pRenderText->SetFont(g_hFontBold);

        if (GetTextExtentPoint32(g_pRenderText->GetFontDC(), c->szShopTitle, lstrlen(c->szShopTitle), &sizeT[0]) && GetTextExtentPoint32(g_pRenderText->GetFontDC(), I18N::Game::Store, wcslen(I18N::Game::Store), &sizeT[1]))
        {
            if (c->Width < sizeT[0].cx + sizeT[1].cx)
                c->Width = sizeT[0].cx + sizeT[1].cx;
            c->Height += std::max<int>(sizeT[0].cy, sizeT[1].cy);
        }
        g_pRenderText->SetFont(g_hFont);
    }
    c->Width /= g_fScreenRate_x;
    c->Height /= g_fScreenRate_y;
}

void SetPlayerColor(BYTE PK)
{
    switch (PK)
    {
    case 0:g_pRenderText->SetTextColor(150, 255, 240, 255); break;//npc
    case 1:g_pRenderText->SetTextColor(100, 120, 255, 255); break;
    case 2:g_pRenderText->SetTextColor(140, 180, 255, 255); break;
    case 3:g_pRenderText->SetTextColor(200, 220, 255, 255); break;//normal
    case 4:g_pRenderText->SetTextColor(255, 150, 60, 255); break; //pk1
    case 5:g_pRenderText->SetTextColor(255, 80, 30, 255); break;  //pk2
    default:g_pRenderText->SetTextColor(255, 0, 0, 255); break;    //pk3
    }
}

	// ※

const int ciSystemColor = 240;

void RenderBoolean(int x, int y, CHAT* c)
{
    if (g_isCharacterBuff((&c->Owner->Object), eBuff_CrywolfNPCHide))
    {
        return;
    }

    if (c->Owner != Hero && battleCastle::IsBattleCastleStart() == true && g_isCharacterBuff((&c->Owner->Object), eBuff_Cloaking))
    {
        if ((Hero->EtcPart == PARTS_ATTACK_KING_TEAM_MARK || Hero->EtcPart == PARTS_ATTACK_TEAM_MARK))
        {
            if (!(c->Owner->EtcPart == PARTS_ATTACK_KING_TEAM_MARK || c->Owner->EtcPart == PARTS_ATTACK_TEAM_MARK))
            {
                return;
            }
        }
        else if ((Hero->EtcPart == PARTS_ATTACK_KING_TEAM_MARK2 || Hero->EtcPart == PARTS_ATTACK_TEAM_MARK2))
        {
            if (!(c->Owner->EtcPart == PARTS_ATTACK_KING_TEAM_MARK2 || c->Owner->EtcPart == PARTS_ATTACK_TEAM_MARK2))
            {
                return;
            }
        }
        else if ((Hero->EtcPart == PARTS_ATTACK_KING_TEAM_MARK3 || Hero->EtcPart == PARTS_ATTACK_TEAM_MARK3))
        {
            if (!(c->Owner->EtcPart == PARTS_ATTACK_KING_TEAM_MARK3 || c->Owner->EtcPart == PARTS_ATTACK_TEAM_MARK3))
            {
                return;
            }
        }
        else if ((Hero->EtcPart == PARTS_DEFENSE_KING_TEAM_MARK || Hero->EtcPart == PARTS_DEFENSE_TEAM_MARK))
        {
            if (!(c->Owner->EtcPart == PARTS_DEFENSE_KING_TEAM_MARK || c->Owner->EtcPart == PARTS_DEFENSE_TEAM_MARK))
            {
                return;
            }
        }
    }

    EnableAlphaTest();
    glColor3f(1.f, 1.f, 1.f);

    if (FontHeight > 32) FontHeight = 32;

    POINT RenderPos = { x, y };
    SIZE RenderBoxSize = { c->Width, c->Height };
    int iLineHeight = FontHeight / g_fScreenRate_y;

    if (IsShopInViewport(c->Owner))
    {
        SIZE TextSize;
        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetBgColor(GetShopBGColor(c->Owner));

        g_pRenderText->SetTextColor(GetShopTextColor(c->Owner));
        g_pRenderText->RenderText(RenderPos.x, RenderPos.y, I18N::Game::Store, 0, iLineHeight, RT3_SORT_LEFT, &TextSize);
        RenderPos.x += TextSize.cx;

        g_pRenderText->SetTextColor(GetShopText2Color(c->Owner));
        g_pRenderText->RenderText(RenderPos.x, RenderPos.y, c->szShopTitle, RenderBoxSize.cx - TextSize.cx, iLineHeight, RT3_SORT_LEFT, &TextSize);
        g_pRenderText->SetFont(g_hFont);

        RenderPos.x = x;
        RenderPos.y += iLineHeight;
    }
    else if (::IsStrifeMap(gMapManager.WorldActive) && Hero->m_byGensInfluence != c->Owner->m_byGensInfluence && !::IsGMCharacter())
    {
        if (!c->Owner->GensContributionPoints)
            return;

        if (KIND_PLAYER == c->Owner->Object.Kind && MODEL_PLAYER == c->Owner->Object.Type)
        {
            int tempX = (int)(c->x + c->Width * 0.5f + 20.0f);
            switch (c->Owner->m_byGensInfluence)
            {
            case 1:
                g_pNewUIGensRanking->RanderMark(tempX, y, (SEASON3B::CNewUIGensRanking::GENS_TYPE)c->Owner->m_byGensInfluence, c->Owner->GensRanking, SEASON3B::CNewUIGensRanking::MARK_BOOLEAN, (float)RenderPos.y);
                return;
            case 2:
                g_pNewUIGensRanking->RanderMark(tempX, y, (SEASON3B::CNewUIGensRanking::GENS_TYPE)c->Owner->m_byGensInfluence, c->Owner->GensRanking, SEASON3B::CNewUIGensRanking::MARK_BOOLEAN, (float)RenderPos.y);
                return;
            default:
                break;
            }
        }
    }

    bool bGmMode = false;

    if (g_isCharacterBuff((&c->Owner->Object), eBuff_GMEffect) || (c->Owner->CtlCode == CTLCODE_20OPERATOR) || (c->Owner->CtlCode == CTLCODE_08OPERATOR))
    {
        bGmMode = true;
        g_pRenderText->SetBgColor(30, 30, 30, 200);
        g_pRenderText->SetTextColor(200, 255, 255, 255);
    }

    if (c->Owner == Hero)
    {
        g_pRenderText->SetBgColor(60, 100, 0, 150);
        g_pRenderText->SetTextColor(200, 255, 0, 255);
    }
    else if (c->Owner->GuildMarkIndex == Hero->GuildMarkIndex)
    {
        g_pRenderText->SetBgColor(GetGuildRelationShipBGColor(GR_UNION));
        g_pRenderText->SetTextColor(GetGuildRelationShipTextColor(GR_UNION));
    }
    else
    {
        g_pRenderText->SetBgColor(GetGuildRelationShipBGColor(c->Owner->GuildRelationShip));
        g_pRenderText->SetTextColor(GetGuildRelationShipTextColor(c->Owner->GuildRelationShip));
    }

    if (c->Union && c->Union[0])
    {
        g_pRenderText->RenderText(RenderPos.x, RenderPos.y, c->Union, RenderBoxSize.cx, iLineHeight, RT3_SORT_LEFT);
        RenderPos.y += iLineHeight;
    }
    if (c->Guild && c->Guild[0])
    {
        if (HasGuildMark(c->Owner))
        {
            RenderGuildMarkIcon(
                c->Owner,
                static_cast<float>(RenderPos.x),
                static_cast<float>(RenderPos.y + 1),
                static_cast<float>(kGuildMarkLineSize));
            g_pRenderText->RenderText(
                RenderPos.x + kGuildMarkLineSize + kGuildMarkLineGap,
                RenderPos.y,
                c->Guild,
                RenderBoxSize.cx - kGuildMarkLineSize - kGuildMarkLineGap,
                iLineHeight,
                RT3_SORT_LEFT);
        }
        else
        {
            g_pRenderText->RenderText(RenderPos.x, RenderPos.y, c->Guild, RenderBoxSize.cx, iLineHeight, RT3_SORT_LEFT);
        }

        RenderPos.y += iLineHeight;
    }

    if (bGmMode)
    {
        g_pRenderText->SetTextColor(100, 250, 250, 255);
    }
    else
    {
        SetPlayerColor(c->Color);
    }

    if (c->x <= MouseX && MouseX < (int)(c->x + c->Width * REFERENCE_WIDTH / WindowWidth) &&
        c->y <= MouseY && MouseY < (int)(c->y + c->Height * REFERENCE_HEIGHT / WindowHeight) &&
        InputEnable && Hero->SafeZone && wcscmp(c->ID, Hero->ID) != 0 &&
        (DWORD)WorldTime % 24 < 12)
    {
        unsigned int Temp = g_pRenderText->GetBgColor();
        g_pRenderText->SetBgColor(g_pRenderText->GetTextColor());
        g_pRenderText->SetTextColor(Temp);
    }

    if (bGmMode)
    {
        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->RenderText(RenderPos.x, RenderPos.y, c->ID, RenderBoxSize.cx, iLineHeight, RT3_SORT_LEFT);
        RenderPos.y += iLineHeight;
        g_pRenderText->SetFont(g_hFont);
    }
    else
    {
        g_pRenderText->RenderText(RenderPos.x, RenderPos.y, c->ID, RenderBoxSize.cx, iLineHeight, RT3_SORT_LEFT);
        RenderPos.y += iLineHeight;
    }

    if (c->GuildColor == 0)
        g_pRenderText->SetBgColor(10, 30, 50, 150);
    else if (c->GuildColor == 1)
        g_pRenderText->SetBgColor(30, 50, 0, 150);
    else if (bGmMode)
        g_pRenderText->SetBgColor(30, 30, 30, 200);
    else
        g_pRenderText->SetBgColor(50, 0, 0, 150);

    DWORD dwTextColor[2];
    BYTE byAlpha[2] = { 255, 255 };
    if ((c->LifeTime[0] > 0 && c->LifeTime[0] < 10))
        byAlpha[0] = 128;
    if ((c->LifeTime[1] > 0 && c->LifeTime[1] < 10))
        byAlpha[1] = 128;

    if (bGmMode)
    {
        dwTextColor[0] = RGBA(250, 200, 50, byAlpha[0]);
        dwTextColor[1] = RGBA(250, 200, 50, byAlpha[1]);
    }
    else
    {
        dwTextColor[0] = RGBA(230, 220, 200, byAlpha[0]);
        dwTextColor[1] = RGBA(230, 220, 200, byAlpha[1]);
    }

    if (c->LifeTime[1] > 0)
    {
        g_pRenderText->SetTextColor(dwTextColor[1]);
        g_pRenderText->RenderText(RenderPos.x, RenderPos.y, c->Text[1], RenderBoxSize.cx, iLineHeight, RT3_SORT_LEFT);
        RenderPos.y += iLineHeight;

        g_pRenderText->SetTextColor(dwTextColor[0]);
        g_pRenderText->RenderText(RenderPos.x, RenderPos.y, c->Text[0], RenderBoxSize.cx, iLineHeight);
    }
    else if (c->LifeTime[0] > 0)
    {
        g_pRenderText->SetTextColor(dwTextColor[0]);
        g_pRenderText->RenderText(RenderPos.x, RenderPos.y, c->Text[0], RenderBoxSize.cx, iLineHeight);
    }

    if (KIND_PLAYER == c->Owner->Object.Kind && MODEL_PLAYER == c->Owner->Object.Type)
    {
        const int nGensMarkHeight = 18;
        int nGensMarkPosY = (RenderPos.y - y - nGensMarkHeight) / 2 + y;

        if (c->LifeTime[1] > 0)
            RenderPos.y -= iLineHeight;

        if (1 == c->Owner->m_byGensInfluence)
            g_pNewUIGensRanking->RanderMark(x, y, (SEASON3B::CNewUIGensRanking::GENS_TYPE)c->Owner->m_byGensInfluence, c->Owner->GensRanking, SEASON3B::CNewUIGensRanking::MARK_BOOLEAN, (float)RenderPos.y);
        else if (2 == c->Owner->m_byGensInfluence)
            g_pNewUIGensRanking->RanderMark(x, y, (SEASON3B::CNewUIGensRanking::GENS_TYPE)c->Owner->m_byGensInfluence, c->Owner->GensRanking, SEASON3B::CNewUIGensRanking::MARK_BOOLEAN, (float)RenderPos.y);
    }

    RenderHoverCard(c);
}
void AddChat(CHAT* c, const wchar_t* chat_text, int flag)
{
    float Time = 0;
    int Length = (int)wcslen(chat_text);
    switch (flag)
    {
    case 0:
        Time = Length * 2 + 160;
        break;
    case 1:
        Time = 1000;
        g_pChatListBox->AddText(c->ID, chat_text, SEASON3B::TYPE_CHAT_MESSAGE);
        break;
    }

    if (Length >= 20)
    {
        CutText(chat_text, c->Text[1], c->Text[0], 256);
        c->LifeTime[0] = Time;
        c->LifeTime[1] = Time;
    }
    else
    {
        memset(c->Text[0], 0, 256);
        wcscpy(c->Text[0], chat_text);
        c->LifeTime[0] = Time;
    }
}

void AddGuildName(CHAT* c, CHARACTER* Owner)
{
    if (IsShopInViewport(Owner))
    {
        std::wstring summary;
        GetShopTitleSummary(Owner, summary);
        CopyWideText(c->szShopTitle, std::size(c->szShopTitle), summary.c_str());
    }
    else {
        c->szShopTitle[0] = '\0';
    }

    if (Owner->GuildMarkIndex >= 0 && GuildMark[Owner->GuildMarkIndex].UnionName[0])
    {
        if (Owner->GuildRelationShip == GR_UNION)
            mu_swprintf(c->Union, L"<%ls> %ls", GuildMark[Owner->GuildMarkIndex].UnionName, I18N::Game::Alliance);
        if (Owner->GuildRelationShip == GR_UNIONMASTER)
        {
            if (Owner->GuildStatus == G_MASTER)
                mu_swprintf(c->Union, L"<%ls> %ls", GuildMark[Owner->GuildMarkIndex].UnionName, I18N::Game::AllianceMaster);
            else
                mu_swprintf(c->Union, L"<%ls> %ls", GuildMark[Owner->GuildMarkIndex].UnionName, I18N::Game::Alliance);
        }
        else if (Owner->GuildRelationShip == GR_RIVAL)
        {
            if (Owner->GuildStatus == G_MASTER)
                mu_swprintf(c->Union, L"<%ls> %ls", GuildMark[Owner->GuildMarkIndex].UnionName, I18N::Game::OpposingMaster);
            else
                mu_swprintf(c->Union, L"<%ls> %ls", GuildMark[Owner->GuildMarkIndex].UnionName, I18N::Game::Oppose);
        }
        else if (Owner->GuildRelationShip == GR_RIVALUNION)
            mu_swprintf(c->Union, L"<%ls> %ls", GuildMark[Owner->GuildMarkIndex].UnionName, I18N::Game::OpposingAllianceMaster);
        else
            mu_swprintf(c->Union, L"<%ls>", GuildMark[Owner->GuildMarkIndex].UnionName);
    }
    else
        c->Union[0] = 0;

    if (Owner->GuildMarkIndex >= 0)
    {
        c->GuildColor = Owner->GuildTeam;
        BuildGuildLine(c->Guild, std::size(c->Guild), Owner);
    }
    else
    {
        c->GuildColor = 0;
        c->Guild[0] = 0;
    }
}

void CreateChat(wchar_t* character_name, const wchar_t* chat_text, CHARACTER* Owner, int Flag, int SetColor)
{
    OBJECT* o = &Owner->Object;
    if (!o->Live || !o->Visible) return;

    int Color;
    if (SetColor != -1)
    {
        Color = SetColor;
    }
    else
    {
        Color = Owner->PK;
        if (o->Kind == KIND_NPC)
            Color = 0;
    }

    for (int i = 0; i < MAX_CHAT; i++)
    {
        CHAT* c = &Chat[i];
        if (c->Owner == Owner)
        {
            wcscpy(c->ID, character_name);
            c->Color = Color;
            AddGuildName(c, Owner);
            if (wcslen(chat_text) == 0)
            {
                c->IDLifeTime = 10;
            }
            else
            {
                if (c->LifeTime[0] > 0)
                {
                    wcscpy(c->Text[1], c->Text[0]);
                    c->LifeTime[1] = c->LifeTime[0];
                }
                c->Owner = Owner;
                AddChat(c, chat_text, Flag);
            }
            return;
        }
    }

    for (int i = 0; i < MAX_CHAT; i++)
    {
        CHAT* c = &Chat[i];
        if (c->IDLifeTime <= 0 && c->LifeTime[0] <= 0)
        {
            c->Owner = Owner;
            wcscpy(c->ID, character_name);
            c->Color = Color;
            AddGuildName(c, Owner);
            if (wcslen(chat_text) == 0)
            {
                c->IDLifeTime = 100;
            }
            else
            {
                AddChat(c, chat_text, Flag);
            }
            return;
        }
    }
}

int CreateChat(wchar_t* character_name, const wchar_t* chat_text, OBJECT* Owner, int Flag, int SetColor)
{
    OBJECT* o = Owner;
    if (!o->Live || !o->Visible) return 0;

    int Color = 0;

    if (SetColor != -1)
    {
        Color = SetColor;
    }

    for (int i = 0; i < MAX_CHAT; i++)
    {
        CHAT* c = &Chat[i];
        if (c->IDLifeTime <= 0 && c->LifeTime[0] <= 0)
        {
            c->Owner = NULL;
            wcscpy(c->ID, character_name);
            c->Color = Color;
            c->GuildColor = 0;
            c->Guild[0] = 0;
            AddChat(c, chat_text, 0);
            c->LifeTime[0] = Flag;

            Vector(o->Position[0], o->Position[1], o->Position[2] + o->BoundingBoxMax[2] + 60.f, c->Position);
            return c->LifeTime[0];
        }
    }

    return 0;
}

void AssignChat(wchar_t* character_name, const wchar_t* chat_text, int flag)
{
    for (int i = 0; i < MAX_CHARACTERS_CLIENT; i++)
    {
        CHARACTER* c = &CharactersClient[i];
        OBJECT* o = &c->Object;
        if (o->Live && o->Kind == KIND_PLAYER)
        {
            if (wcscmp(c->ID, character_name) == 0)
            {
                CreateChat(character_name, chat_text, c, flag);
                return;
            }
        }
    }

    for (int i = 0; i < MAX_CHARACTERS_CLIENT; i++)
    {
        CHARACTER* c = &CharactersClient[i];
        OBJECT* o = &c->Object;
        if (o->Live && o->Kind == KIND_MONSTER)
        {
            if (wcscmp(c->ID, character_name) == 0)
            {
                CreateChat(character_name, chat_text, c, flag);
                return;
            }
        }
    }
}

void MoveChat()
{
    for (int i = 0; i < MAX_CHAT; i++)
    {
        CHAT* c = &Chat[i];
        if (c->IDLifeTime > 0)
            c->IDLifeTime -= FPS_ANIMATION_FACTOR;
        if (c->LifeTime[0] > 0)
            c->LifeTime[0]-=FPS_ANIMATION_FACTOR;
        if (c->LifeTime[1] > 0)
            c->LifeTime[1]-=FPS_ANIMATION_FACTOR;
        if (c->Owner != NULL && (!c->Owner->Object.Live || !c->Owner->Object.Visible))
        {
            c->IDLifeTime = 0;
            c->LifeTime[0] = 0;
            c->LifeTime[1] = 0;
        }
        if (c->LifeTime[0] > 0) {
            DisableShopTitleDraw(c->Owner);
        }
        if (c->Owner != NULL && c->LifeTime[0] <= 0) {
            EnableShopTitleDraw(c->Owner);
        }
    }
}


void RenderBooleans()
{
    g_pRenderText->SetFont(g_hFont);

    for (int i = 0; i < MAX_CHAT; i++)
    {
        CHAT* ci = &Chat[i];
        if (ci->IDLifeTime > 0 || ci->LifeTime[0] > 0)
        {
            vec3_t Position;
            int ScreenX, ScreenY;
            if (ci->Owner != NULL)
            {
                OBJECT* o = &ci->Owner->Object;
                Vector(o->Position[0], o->Position[1], o->Position[2] + o->BoundingBoxMax[2] + 60.f, Position);

                if (o->Type >= MODEL_LITTLESANTA && o->Type <= MODEL_LITTLESANTA_END)
                {
                    Vector(o->Position[0], o->Position[1], o->Position[2] + o->BoundingBoxMax[2] + 20.f, Position);
                }
                if (o->Type == MODEL_MERCHANT_MAN)
                {
                    Vector(o->Position[0], o->Position[1], o->Position[2] + o->BoundingBoxMax[2] + 20.f, Position);
                }
                CameraProjection::WorldToScreen(g_Camera, Position, &ScreenX, &ScreenY);
            }
            else
            {
                CameraProjection::WorldToScreen(g_Camera, ci->Position, &ScreenX, &ScreenY);
            }
            SetBooleanPosition(ci);
            ci->x = ScreenX - (ci->Width / 2);
            ci->y = ScreenY - ci->Height;
        }
    }

    for (int i = 0; i < MAX_CHAT; i++)		//. Bubble sorting
    {
        CHAT* ci = &Chat[i];
        if (ci->IDLifeTime > 0 || ci->LifeTime[0] > 0)
        {
            for (int j = 0; j < MAX_CHAT; j++)
            {
                CHAT* cj = &Chat[j];
                if (i != j && (cj->IDLifeTime > 0 || cj->LifeTime[0] > 0))
                {
                    if (ci->x + ci->Width > cj->x && ci->x<cj->x + cj->Width &&
                        ci->y + ci->Height>cj->y && ci->y < cj->y + cj->Height)
                    {
                        if (ci->y < cj->y + cj->Height / 2)
                            ci->y = cj->y - ci->Height;
                        else
                            ci->y = cj->y + cj->Height;
                    }
                }
            }
        }
    }

    for (int i = 0; i < MAX_CHAT; i++)
    {
        CHAT* ci = &Chat[i];
        if (ci->IDLifeTime > 0 || ci->LifeTime[0] > 0)
        {
            //. Fit to screen
            if (ci->x < 0) ci->x = 0;
            if (ci->x >= (int)WindowWidth - ci->Width) ci->x = WindowWidth - ci->Width;
            if (ci->y < 0) ci->y = 0;
            if (ci->y >= (int)WindowHeight - ci->Height) ci->y = WindowHeight - ci->Height;
            RenderBoolean(ci->x, ci->y, ci);
        }
    }
}
}
