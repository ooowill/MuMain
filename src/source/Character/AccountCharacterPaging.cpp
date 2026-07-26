#include "stdafx.h"
#include "Character/AccountCharacterPaging.h"

#include <algorithm>

#include "Character/AccountCharacterList.h"
#include "Engine/AI/GOBoid.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Network/Server/WSclient.h"
#include "Scenes/SceneCommon.h"

namespace
{
    int g_currentPage = 0;

    struct CharacterSelectPose
    {
        float X;
        float Y;
        float Angle;
    };

    constexpr CharacterSelectPose kSelectedCharacterPose{
        13627.8f,
        12338.9f,
        100.0f,
    };

    CLASS_TYPE SanitizeClass(CLASS_TYPE classType)
    {
        if (classType == CLASS_UNDEFINED || classType > CLASS_END)
        {
            return CLASS_KNIGHT;
        }

        return classType;
    }

    void DeleteVisibleSelectionCharacter(int visibleIndex)
    {
        if (visibleIndex < 0 || visibleIndex >= AccountCharacterList::NativeVisibleSlots)
        {
            return;
        }

        CHARACTER* character = &CharactersClient[visibleIndex];
        if (character->Object.Live && character->Key == visibleIndex)
        {
            DeleteCharacter(visibleIndex);
        }
    }

    void CreateVisibleCharacter(const AccountCharacterList::Entry& entry, int visibleIndex)
    {
        const CharacterSelectPose& pose = kSelectedCharacterPose;

        DeleteVisibleSelectionCharacter(visibleIndex);
        CHARACTER* character = CreateHero(visibleIndex, SanitizeClass(entry.Class), 0, pose.X, pose.Y, pose.Angle);
        character->Level = entry.Level;
        character->CtlCode = entry.CtlCode;
        character->GuildStatus = entry.GuildStatus;

        std::wcsncpy(character->ID, entry.Name, MAX_USERNAME_SIZE);
        character->ID[MAX_USERNAME_SIZE] = L'\0';

        if (entry.HasEquipment && entry.UsesExtendedEquipment)
        {
            ReadEquipmentExtended(visibleIndex, entry.Flags, const_cast<BYTE*>(entry.Equipment));
        }
        else if (entry.HasEquipment)
        {
            ChangeCharacterExt(visibleIndex, const_cast<BYTE*>(entry.Equipment));
        }

        // The current server-side character list can arrive without a complete
        // appearance payload. Avoid rendering stale helper bits as mounts on
        // the character selection scene until the protocol is fully aligned.
        character->Helper.Type = -1;
        character->Helper.Level = 0;
        DeleteMount(&character->Object);
        character->Object.Scale *= 0.78f;
    }
}

int AccountCharacterPaging::GetCurrentPage()
{
    return g_currentPage;
}

int AccountCharacterPaging::GetPageCount()
{
    return (AccountCharacterList::MaxCharacters + SlotsPerPage - 1) / SlotsPerPage;
}

int AccountCharacterPaging::GetFirstSlotOnCurrentPage()
{
    return g_currentPage * SlotsPerPage;
}

int AccountCharacterPaging::GetVisibleSlot(int visibleIndex)
{
    if (visibleIndex < 0 || visibleIndex >= SlotsPerPage)
    {
        return -1;
    }

    const int slot = GetFirstSlotOnCurrentPage() + visibleIndex;
    return slot < AccountCharacterList::MaxCharacters ? slot : -1;
}

bool AccountCharacterPaging::IsSlotOnCurrentPage(int slot)
{
    return slot >= GetFirstSlotOnCurrentPage() && slot < GetFirstSlotOnCurrentPage() + SlotsPerPage;
}

bool AccountCharacterPaging::CanGoPrevious()
{
    return g_currentPage > 0;
}

bool AccountCharacterPaging::CanGoNext()
{
    return g_currentPage < GetPageCount() - 1;
}

void AccountCharacterPaging::ClampCurrentPage()
{
    g_currentPage = std::clamp(g_currentPage, 0, std::max(0, GetPageCount() - 1));
}

void AccountCharacterPaging::ResetToFirstPageWithoutRefresh()
{
    g_currentPage = 0;
    SelectedHero = -1;
}

bool AccountCharacterPaging::SetPage(int page)
{
    const int clampedPage = std::clamp(page, 0, std::max(0, GetPageCount() - 1));
    const bool pageChanged = clampedPage != g_currentPage;

    g_currentPage = clampedPage;
    SelectedHero = -1;
    ClearVisibleCharacters();
    if (!SendRequestCharacterPage(g_currentPage))
    {
        RefreshVisibleCharacters();
    }

    return pageChanged;
}

bool AccountCharacterPaging::SetPageForSlot(int slot)
{
    if (slot < 0 || slot >= AccountCharacterList::MaxCharacters)
    {
        return false;
    }

    return SetPage(slot / SlotsPerPage);
}

bool AccountCharacterPaging::PreviousPage()
{
    return SetPage(g_currentPage - 1);
}

bool AccountCharacterPaging::NextPage()
{
    return SetPage(g_currentPage + 1);
}

void AccountCharacterPaging::RefreshVisibleCharacters()
{
    ClampCurrentPage();

    ClearVisibleCharacters();

    for (int visibleIndex = 0; visibleIndex < SlotsPerPage; ++visibleIndex)
    {
        const int slot = GetVisibleSlot(visibleIndex);
        if (slot < 0)
        {
            continue;
        }

        const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot);
        if (entry == nullptr)
        {
            continue;
        }

        CreateVisibleCharacter(*entry, visibleIndex);
    }

    if (SelectedHero >= 0 && SelectedHero >= AccountCharacterList::NativeVisibleSlots)
    {
        SelectedHero = -1;
    }
}

void AccountCharacterPaging::ClearVisibleCharacters()
{
    for (int visibleIndex = 0; visibleIndex < SlotsPerPage; ++visibleIndex)
    {
        DeleteVisibleSelectionCharacter(visibleIndex);
    }
}
