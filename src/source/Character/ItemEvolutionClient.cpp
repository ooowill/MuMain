#include "stdafx.h"
#include "Character/ItemEvolutionClient.h"

#include <algorithm>
#include <array>
#include <cwchar>
#include <unordered_map>

#include "Engine/Object/ZzzInfomation.h"
#include "GameLogic/Items/InventoryUtils.h"

namespace
{
    constexpr wchar_t kEvolutionPrefix[] = L"#IEV1|";
    constexpr int kMaxTrackedItemSlots = MAX_MY_INVENTORY_EX_INDEX;

    struct ItemEvolutionSlotInfo
    {
        bool Known = false;
        WORD WipeCount = 0;
        BYTE ProgressPercent = 0;
        BYTE SentinelCompanionBonusLevel = 0;
        DWORD WeaponKillCount = 0;
        DWORD WeaponKillDamageBonusTenths = 0;
    };

    std::array<ItemEvolutionSlotInfo, kMaxTrackedItemSlots> g_itemEvolution = {};
    std::unordered_map<DWORD, ItemEvolutionSlotInfo> g_itemEvolutionByKey;

    bool TryParseUnsigned(const wchar_t*& cursor, unsigned long& value)
    {
        if (cursor == nullptr || *cursor == L'\0')
        {
            return false;
        }

        wchar_t* end = nullptr;
        value = std::wcstoul(cursor, &end, 10);
        if (end == cursor)
        {
            return false;
        }

        cursor = end;
        return true;
    }

    bool TryConsumeSeparator(const wchar_t*& cursor)
    {
        if (cursor == nullptr || *cursor != L'|')
        {
            return false;
        }

        ++cursor;
        return true;
    }

    bool IsTrackedSlot(int slot)
    {
        return slot >= 0 && slot < kMaxTrackedItemSlots;
    }

    bool TryParseEvolutionMessage(
        const wchar_t* message,
        int& slot,
        WORD& wipeCount,
        BYTE& progressPercent,
        BYTE& sentinelCompanionBonusLevel,
        DWORD& weaponKillCount,
        DWORD& weaponKillDamageBonusTenths)
    {
        if (message == nullptr)
        {
            return false;
        }

        const size_t prefixLength = std::wcslen(kEvolutionPrefix);
        if (std::wcsncmp(message, kEvolutionPrefix, prefixLength) != 0)
        {
            return false;
        }

        const wchar_t* cursor = message + prefixLength;
        unsigned long parsedSlot = 0;
        unsigned long parsedWipeCount = 0;
        unsigned long parsedProgress = 0;
        unsigned long parsedSentinelCompanionBonusLevel = 0;
        unsigned long parsedWeaponKillCount = 0;
        unsigned long parsedWeaponKillDamageBonusTenths = 0;
        if (!TryParseUnsigned(cursor, parsedSlot)
            || !TryConsumeSeparator(cursor)
            || !TryParseUnsigned(cursor, parsedWipeCount)
            || !TryConsumeSeparator(cursor)
            || !TryParseUnsigned(cursor, parsedProgress))
        {
            return false;
        }

        if (TryConsumeSeparator(cursor))
        {
            if (!TryParseUnsigned(cursor, parsedSentinelCompanionBonusLevel))
            {
                return false;
            }

            if (TryConsumeSeparator(cursor))
            {
                if (!TryParseUnsigned(cursor, parsedWeaponKillCount))
                {
                    return false;
                }

                if (TryConsumeSeparator(cursor) && !TryParseUnsigned(cursor, parsedWeaponKillDamageBonusTenths))
                {
                    return false;
                }
            }
        }

        if (!IsTrackedSlot(static_cast<int>(parsedSlot)))
        {
            return false;
        }

        slot = static_cast<int>(parsedSlot);
        wipeCount = static_cast<WORD>(std::min<unsigned long>(parsedWipeCount, 0xFFFFUL));
        progressPercent = static_cast<BYTE>(std::min<unsigned long>(parsedProgress, 100UL));
        sentinelCompanionBonusLevel = static_cast<BYTE>(std::min<unsigned long>(parsedSentinelCompanionBonusLevel, 3UL));
        weaponKillCount = static_cast<DWORD>(std::min<unsigned long>(parsedWeaponKillCount, 0xFFFFFFFFUL));
        weaponKillDamageBonusTenths = static_cast<DWORD>(std::min<unsigned long>(parsedWeaponKillDamageBonusTenths, 0xFFFFFFFFUL));
        return true;
    }

    ITEM* FindItemBySlot(int slot)
    {
        if (!IsTrackedSlot(slot))
        {
            return nullptr;
        }

        if (slot < MAX_EQUIPMENT)
        {
            return CharacterMachine != nullptr ? &CharacterMachine->Equipment[slot] : nullptr;
        }

        return const_cast<ITEM*>(FindInventoryItemBySlot(slot));
    }

    void RegisterItemInfo(const ItemEvolutionSlotInfo& info, ITEM& item)
    {
        if (!info.Known)
        {
            return;
        }

        if (item.Type == -1)
        {
            return;
        }

        g_itemEvolutionByKey[item.Key] = info;
    }

    void ApplySlotInfoToSlot(int slot)
    {
        if (!IsTrackedSlot(slot))
        {
            return;
        }

        if (ITEM* item = FindItemBySlot(slot))
        {
            RegisterItemInfo(g_itemEvolution[slot], *item);
        }
    }

    const ItemEvolutionSlotInfo* FindInfoByItem(const ITEM* item)
    {
        if (item == nullptr || item->Type == -1)
        {
            return nullptr;
        }

        const auto byKey = g_itemEvolutionByKey.find(item->Key);
        if (byKey != g_itemEvolutionByKey.end() && byKey->second.Known)
        {
            return &byKey->second;
        }

        return nullptr;
    }

}

bool ItemEvolutionClient::HandleEvolutionMessage(const wchar_t* message)
{
    int slot = -1;
    WORD wipeCount = 0;
    BYTE progressPercent = 0;
    BYTE sentinelCompanionBonusLevel = 0;
    DWORD weaponKillCount = 0;
    DWORD weaponKillDamageBonusTenths = 0;
    if (!TryParseEvolutionMessage(
        message,
        slot,
        wipeCount,
        progressPercent,
        sentinelCompanionBonusLevel,
        weaponKillCount,
        weaponKillDamageBonusTenths))
    {
        return false;
    }

    ItemEvolutionSlotInfo& info = g_itemEvolution[slot];
    info.Known = true;
    info.WipeCount = wipeCount;
    info.ProgressPercent = progressPercent;
    info.SentinelCompanionBonusLevel = sentinelCompanionBonusLevel;
    info.WeaponKillCount = weaponKillCount;
    info.WeaponKillDamageBonusTenths = weaponKillDamageBonusTenths;
    ApplySlotInfoToSlot(slot);
    return true;
}

void ItemEvolutionClient::Reset()
{
    for (ItemEvolutionSlotInfo& info : g_itemEvolution)
    {
        info = {};
    }

    g_itemEvolutionByKey.clear();
}

void ItemEvolutionClient::ApplySlotInfoToItem(int slot, ITEM* item)
{
    if (!IsTrackedSlot(slot) || item == nullptr)
    {
        return;
    }

    RegisterItemInfo(g_itemEvolution[slot], *item);
}

int ItemEvolutionClient::GetWipeCount(const ITEM* item)
{
    const ItemEvolutionSlotInfo* info = FindInfoByItem(item);
    return info != nullptr ? info->WipeCount : 0;
}

int ItemEvolutionClient::GetProgressPercent(const ITEM* item)
{
    const ItemEvolutionSlotInfo* info = FindInfoByItem(item);
    return info != nullptr ? info->ProgressPercent : 0;
}

int ItemEvolutionClient::GetWeaponKillCount(const ITEM* item)
{
    const ItemEvolutionSlotInfo* info = FindInfoByItem(item);
    return info != nullptr ? static_cast<int>(std::min<DWORD>(info->WeaponKillCount, 0x7FFFFFFF)) : 0;
}

int ItemEvolutionClient::GetWeaponKillDamageBonusTenths(const ITEM* item)
{
    const ItemEvolutionSlotInfo* info = FindInfoByItem(item);
    return info != nullptr ? static_cast<int>(std::min<DWORD>(info->WeaponKillDamageBonusTenths, 0x7FFFFFFF)) : 0;
}
