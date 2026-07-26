#include "stdafx.h"
#include "Character/AccountCharacterList.h"

#include <algorithm>
#include <array>
#include <cwchar>

namespace
{
    std::array<AccountCharacterList::Entry, AccountCharacterList::MaxCharacters> g_accountCharacters;
    int g_unlockedSlotCount = AccountCharacterList::DefaultUnlockedSlots;
    int g_pendingCreationSlot = -1;

    bool IsValidSlot(int slot)
    {
        return slot >= 0 && slot < AccountCharacterList::MaxCharacters;
    }

    void SetEmptyEquipment(BYTE* equipment)
    {
        std::fill_n(equipment, AccountCharacterList::EquipmentLength, 0xFF);
    }
}

void AccountCharacterList::Clear()
{
    for (auto& entry : g_accountCharacters)
    {
        entry = {};
        SetEmptyEquipment(entry.Equipment);
    }

    g_pendingCreationSlot = -1;
    g_unlockedSlotCount = DefaultUnlockedSlots;
}

void AccountCharacterList::ClearSlotRange(int firstSlot, int slotCount)
{
    if (slotCount <= 0)
    {
        return;
    }

    const int beginSlot = std::clamp(firstSlot, 0, MaxCharacters);
    const int endSlot = std::clamp(firstSlot + slotCount, 0, MaxCharacters);
    for (int slot = beginSlot; slot < endSlot; ++slot)
    {
        Remove(slot);
    }
}

void AccountCharacterList::Upsert(int slot, CLASS_TYPE classType, int level, int ctlCode, BYTE flags, const BYTE* equipment, int equipmentLength, bool usesExtendedEquipment, BYTE guildStatus, const wchar_t* name)
{
    if (!IsValidSlot(slot))
    {
        return;
    }

    Entry& entry = g_accountCharacters[slot];
    entry.IsUsed = true;
    entry.Slot = slot;
    entry.Class = classType;
    entry.Level = std::max(0, level);
    entry.CtlCode = ctlCode;
    entry.Flags = flags;
    entry.GuildStatus = guildStatus;
    entry.UsesExtendedEquipment = usesExtendedEquipment;
    entry.HasEquipment = equipment != nullptr && equipmentLength > 0;
    SetEmptyEquipment(entry.Equipment);
    if (equipment != nullptr)
    {
        std::copy_n(equipment, std::clamp(equipmentLength, 0, EquipmentLength), entry.Equipment);
    }

    std::wcsncpy(entry.Name, name != nullptr ? name : L"", MAX_USERNAME_SIZE);
    entry.Name[MAX_USERNAME_SIZE] = L'\0';
}

void AccountCharacterList::Remove(int slot)
{
    if (!IsValidSlot(slot))
    {
        return;
    }

    g_accountCharacters[slot] = {};
    SetEmptyEquipment(g_accountCharacters[slot].Equipment);
}

bool AccountCharacterList::SwapSlots(int firstSlot, int secondSlot)
{
    if (!IsValidSlot(firstSlot)
        || !IsValidSlot(secondSlot)
        || firstSlot == secondSlot
        || !g_accountCharacters[firstSlot].IsUsed
        || !g_accountCharacters[secondSlot].IsUsed)
    {
        return false;
    }

    std::swap(g_accountCharacters[firstSlot], g_accountCharacters[secondSlot]);
    g_accountCharacters[firstSlot].Slot = firstSlot;
    g_accountCharacters[secondSlot].Slot = secondSlot;
    return true;
}

void AccountCharacterList::SetPowerScore(int slot, unsigned long long powerScore)
{
    if (!IsValidSlot(slot) || !g_accountCharacters[slot].IsUsed)
    {
        return;
    }

    g_accountCharacters[slot].PowerScore = powerScore;
}

int AccountCharacterList::GetCount()
{
    return static_cast<int>(std::count_if(
        g_accountCharacters.begin(),
        g_accountCharacters.end(),
        [](const Entry& entry)
        {
            return entry.IsUsed;
        }));
}

int AccountCharacterList::GetUnlockedSlotCount()
{
    return g_unlockedSlotCount;
}

void AccountCharacterList::SetUnlockedSlotCount(int slotCount)
{
    g_unlockedSlotCount = std::clamp(slotCount, DefaultUnlockedSlots, MaxCharacters);
}

bool AccountCharacterList::CanPurchaseSlot()
{
    return g_unlockedSlotCount < MaxCharacters;
}

bool AccountCharacterList::HasEmptySlot()
{
    for (int slot = 0; slot < g_unlockedSlotCount; ++slot)
    {
        if (!g_accountCharacters[slot].IsUsed)
        {
            return true;
        }
    }

    return false;
}

int AccountCharacterList::FindFirstEmptySlot()
{
    for (int slot = 0; slot < g_unlockedSlotCount; ++slot)
    {
        if (!g_accountCharacters[slot].IsUsed)
        {
            return slot;
        }
    }

    return -1;
}

int AccountCharacterList::FindSlotByName(const wchar_t* name)
{
    if (name == nullptr || name[0] == L'\0')
    {
        return -1;
    }

    for (int slot = 0; slot < MaxCharacters; ++slot)
    {
        const Entry& entry = g_accountCharacters[slot];
        if (entry.IsUsed && std::wcscmp(entry.Name, name) == 0)
        {
            return slot;
        }
    }

    return -1;
}

void AccountCharacterList::SetPendingCreationSlot(int slot)
{
    g_pendingCreationSlot = IsValidSlot(slot) ? slot : -1;
}

int AccountCharacterList::ConsumePendingCreationSlot()
{
    const int slot = g_pendingCreationSlot;
    g_pendingCreationSlot = -1;
    return IsValidSlot(slot) ? slot : -1;
}

const AccountCharacterList::Entry* AccountCharacterList::GetByDisplayIndex(int displayIndex)
{
    if (displayIndex < 0)
    {
        return nullptr;
    }

    int currentDisplayIndex = 0;
    for (const Entry& entry : g_accountCharacters)
    {
        if (!entry.IsUsed)
        {
            continue;
        }

        if (currentDisplayIndex == displayIndex)
        {
            return &entry;
        }

        ++currentDisplayIndex;
    }

    return nullptr;
}

const AccountCharacterList::Entry* AccountCharacterList::GetBySlot(int slot)
{
    if (slot < 0 || slot >= MaxCharacters)
    {
        return nullptr;
    }

    const Entry& entry = g_accountCharacters[slot];
    return entry.IsUsed ? &entry : nullptr;
}
