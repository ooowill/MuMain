#pragma once

#include "Core/Globals/_define.h"
#include "Core/Globals/_enum.h"

namespace AccountCharacterList
{
    constexpr int DefaultUnlockedSlots = 10;
    constexpr int MaxCharacters = 200;
    constexpr int ClassicPacketCharacterLimit = 7;
    constexpr int NativeVisibleSlots = MAX_CHARACTERS_PER_ACCOUNT;
    constexpr int EquipmentLength = 25;
    constexpr int LegacyAppearanceLength = 18;

    struct Entry
    {
        bool IsUsed = false;
        bool UsesExtendedEquipment = true;
        bool HasEquipment = false;
        int Slot = -1;
        CLASS_TYPE Class = CLASS_WIZARD;
        int Level = 0;
        unsigned long long PowerScore = 0;
        int CtlCode = 0;
        BYTE Flags = 0;
        BYTE GuildStatus = 0;
        BYTE Equipment[EquipmentLength] = {};
        wchar_t Name[MAX_USERNAME_SIZE + 1] = {};
    };

    void Clear();
    void ClearSlotRange(int firstSlot, int slotCount);
    void Upsert(int slot, CLASS_TYPE classType, int level, int ctlCode, BYTE flags, const BYTE* equipment, int equipmentLength, bool usesExtendedEquipment, BYTE guildStatus, const wchar_t* name);
    void Remove(int slot);
    bool SwapSlots(int firstSlot, int secondSlot);
    void SetPowerScore(int slot, unsigned long long powerScore);
    int GetCount();
    int GetUnlockedSlotCount();
    void SetUnlockedSlotCount(int slotCount);
    bool CanPurchaseSlot();
    bool HasEmptySlot();
    int FindFirstEmptySlot();
    int FindSlotByName(const wchar_t* name);
    void SetPendingCreationSlot(int slot);
    int ConsumePendingCreationSlot();
    const Entry* GetByDisplayIndex(int displayIndex);
    const Entry* GetBySlot(int slot);
}
