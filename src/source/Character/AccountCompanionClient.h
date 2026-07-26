#pragma once

#include <cstddef>

#include "Character/AccountCharacterList.h"

struct CHARACTER;

namespace AccountCompanionClient
{
    constexpr unsigned int WindowsMessage = 0x8000u + 0x0471u;

    enum Action
    {
        ActionCall = 1,
        ActionStore = 2,
        ActionCallAll = 3,
        ActionStoreAll = 4,
        ActionSwitch = 5
    };

    enum class CombatMode
    {
        Defensive,
        AssistLeader,
        AutoFarm,
        Passive
    };

    enum class FormationMode
    {
        CityMarch,
        CombatFollow,
        HoldPosition
    };

    enum class CommandMode
    {
        Follow,
        FarmPosition
    };

    struct MiniPartyEntry
    {
        int Slot = -1;
        int ClientIndex = -1;
        int StepHP = 10;
        bool Visible = false;
        bool OfflinePlay = false;
        wchar_t Name[MAX_USERNAME_SIZE + 1] = {};
    };

    struct GuardStatus
    {
        bool Known = false;
        bool Active = false;
        bool AutoSummon = false;
        int CooldownSeconds = 0;
        CommandMode Mode = CommandMode::Follow;
        int FarmMapNumber = -1;
        int FarmX = -1;
        int FarmY = -1;
    };

    constexpr int MaxSummonedCompanions = 19;

    bool IsSummoned(int accountSlot);
    bool IsOfflinePlaying(int accountSlot);
    bool GetGuardStatus(int accountSlot, GuardStatus& status);
    int GetCooldownSeconds(int accountSlot);
    bool IsAutoSummonEnabled(int accountSlot);
    int GetActiveCount();
    int GetSummonLimit();
    int GetPlanTier();
    bool CanPlayOffline();
    int CopyMiniPartyEntries(MiniPartyEntry* entries, int maxEntries);
    bool CanSummonMore();
    void RequestPlanInfo();
    bool Summon(int accountSlot);
    bool Store(int accountSlot);
    bool SwitchTo(int accountSlot);
    bool SetFarmHere(int accountSlot);
    bool SetFollowLeader(int accountSlot);
    bool SetAutoSummon(int accountSlot, bool enabled);
    void SummonAll();
    void StoreAll();
    bool HasPendingSwitchTarget();
    bool GetPendingSwitchTarget(wchar_t* targetName, size_t targetNameLength);
    void ClearPendingSwitchTarget();
    void ResetLocalState();
    bool RecallAllToLeader();
    CombatMode GetCombatMode();
    void SetCombatMode(CombatMode mode);
    FormationMode GetFormationMode();
    void SetFormationMode(FormationMode mode);
    void Tick();
    bool ShouldSuppressServerDrivenRender(int clientIndex);
    bool HasAccountWideStoreForLeader(const CHARACTER* character);
    bool HandlePlanMessage(const wchar_t* message);
    bool HandleVisualLinkMessage(int companionKey, const wchar_t* message);
    bool HandleVisualPublicMessage(const wchar_t* senderName, const wchar_t* message);
    bool HandleExternalAction(int action, int accountSlot);
}
