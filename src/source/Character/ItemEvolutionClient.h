#pragma once

namespace ItemEvolutionClient
{
    bool HandleEvolutionMessage(const wchar_t* message);
    void Reset();
    void ApplySlotInfoToItem(int slot, ITEM* item);
    int GetWipeCount(const ITEM* item);
    int GetProgressPercent(const ITEM* item);
    int GetWeaponKillCount(const ITEM* item);
    int GetWeaponKillDamageBonusTenths(const ITEM* item);
}
