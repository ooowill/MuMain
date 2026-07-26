#include "stdafx.h"
#include "Character/AccountCompanionClient.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Character/AccountCharacterList.h"
#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "GameLogic/Items/PersonalShopTitleImp.h"
#include "Network/Server/ServerListManager.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "World/MapInfra/MapManager.h"

namespace
{
    struct CompanionState
    {
        bool Active = false;
        bool ServerActive = false;
        bool OfflinePlay = false;
        bool CityStoreRequestPending = false;
        unsigned long long CityStoreRequestTick = 0;
        int ClientIndex = -1;
    };

    struct GuardRuntimeStatus
    {
        bool Known = false;
        bool Active = false;
        bool AutoSummon = false;
        int CooldownSeconds = 0;
        unsigned long long CooldownEndsAtTick = 0;
        AccountCompanionClient::CommandMode Mode = AccountCompanionClient::CommandMode::Follow;
        int FarmMapNumber = -1;
        int FarmX = -1;
        int FarmY = -1;
    };

    struct TrailPoint
    {
        float X = 0.f;
        float Y = 0.f;
        float ForwardX = 0.f;
        float ForwardY = 1.f;
    };

    struct TownFormationState
    {
        bool Initialized = false;
        int World = -1;
        float LastLeaderX = 0.f;
        float LastLeaderY = 0.f;
        float AnchorX = 0.f;
        float AnchorY = 0.f;
        float HoldAnchorX = 0.f;
        float HoldAnchorY = 0.f;
        float StableForwardX = 0.f;
        float StableForwardY = 1.f;
        float CandidateForwardX = 0.f;
        float CandidateForwardY = 1.f;
        float CandidateDistance = 0.f;
        bool WaitingForTurn = false;
        std::deque<TrailPoint> Trail;
    };

    struct FormationFrame
    {
        float AnchorX = 0.f;
        float AnchorY = 0.f;
        float ForwardX = 0.f;
        float ForwardY = 1.f;
        float RightX = 1.f;
        float RightY = 0.f;
        bool TownMode = false;
    };

    struct RemoteCompanionVisualState
    {
        int OwnerKey = -1;
        int FormationIndex = 0;
        bool OwnerHasWebStore = false;
        TownFormationState Formation;
    };

    struct PublicCompanionVisualState
    {
        int OwnerKey = -1;
        int FormationIndex = 0;
        int ClientIndex = -1;
        unsigned long long LastSeenTick = 0;
        CLASS_TYPE Class = CLASS_WIZARD;
        int Level = 0;
        BYTE Flags = 0;
        bool UsesExtendedEquipment = true;
        bool AppearanceDirty = true;
        BYTE Equipment[AccountCharacterList::EquipmentLength] = {};
        wchar_t Name[MAX_USERNAME_SIZE + 1] = {};
        TownFormationState Formation;
    };

    constexpr int kReservedClientIndexCount = AccountCompanionClient::MaxSummonedCompanions + 1;
    constexpr int kReservedClientIndexBase = MAX_CHARACTERS_CLIENT - kReservedClientIndexCount;
    constexpr int kPublicVisualClientIndexBase = kReservedClientIndexBase - 80;
    constexpr int kPublicVisualClientIndexCount = 60;
    constexpr int kPublicVisualKeyBase = 30000;
    constexpr float kSnapDistance = 700.f;
    constexpr float kStopDistance = 24.f;
    constexpr float kFollowSmoothing = 0.18f;
    constexpr float kTownFollowSmoothing = 0.145f;
    constexpr float kLeaderMoveEpsilon = 8.f;
    constexpr float kConfirmedTurnDistance = TERRAIN_SCALE * 4.f;
    constexpr float kSharpTurnDot = 0.35f;
    constexpr int kTownFormationColumns = 3;
    constexpr float kTownColumnSpacing = 58.f;
    constexpr float kTownRowSpacing = 82.f;
    constexpr float kTownFirstRowDistance = 112.f;
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTrailAppendEpsilon = 18.f;
    constexpr int kLeaderTrailMaxPoints = 220;
    constexpr unsigned long long kPublicVisualPublishIntervalMs = 1500;
    constexpr unsigned long long kPublicVisualExpireMs = 4500;
    constexpr unsigned long long kCityStoreCommandIntervalMs = 300;
    constexpr unsigned long long kCityStoreRetryIntervalMs = 2500;
    constexpr bool kSendServerGuardCommands = true;
    constexpr bool kUseClientVisualFormation = true;
    constexpr bool kHideServerDrivenGuardActors = true;
    constexpr int kDefaultPlanGuardLimit = 1;

    std::array<CompanionState, AccountCharacterList::MaxCharacters> g_companions;
    std::array<GuardRuntimeStatus, AccountCharacterList::MaxCharacters> g_guardStatuses;
    std::array<wchar_t, MAX_USERNAME_SIZE + 1> g_pendingSwitchTargetName = {};
    std::array<bool, AccountCharacterList::MaxCharacters> g_switchPreservedActiveSlots = {};
    std::array<bool, AccountCharacterList::MaxCharacters> g_switchPreservedServerSlots = {};
    std::array<bool, AccountCharacterList::MaxCharacters> g_switchPreservedOfflineSlots = {};
    int g_pendingSwitchSourceSlot = -1;
    int g_pendingSwitchTargetSlot = -1;
    TownFormationState g_townFormation;
    std::unordered_map<int, RemoteCompanionVisualState> g_remoteCompanions;
    std::unordered_map<long long, PublicCompanionVisualState> g_publicVisualCompanions;
    std::unordered_set<int> g_webStoreTitleOwnerKeys;
    AccountCompanionClient::CombatMode g_combatMode = AccountCompanionClient::CombatMode::AutoFarm;
    AccountCompanionClient::FormationMode g_formationMode = AccountCompanionClient::FormationMode::CityMarch;
    unsigned long long g_lastPublicVisualPublishTick = 0;
    int g_planGuardLimit = kDefaultPlanGuardLimit;
    int g_planTier = 0;
    bool g_requestedPlanInfo = false;
    bool g_wasHeroInSafeZone = false;
    unsigned long long g_lastCityStoreCommandTick = 0;

    int GetCompanionClientIndex(int accountSlot)
    {
        if (accountSlot < 0 || accountSlot >= AccountCharacterList::MaxCharacters)
        {
            return -1;
        }

        const int currentIndex = g_companions[accountSlot].ClientIndex;
        if (currentIndex >= kReservedClientIndexBase && currentIndex < MAX_CHARACTERS_CLIENT)
        {
            return currentIndex;
        }

        for (int offset = 0; offset < kReservedClientIndexCount; ++offset)
        {
            const int candidate = kReservedClientIndexBase + offset;
            const bool inUse = std::any_of(
                g_companions.begin(),
                g_companions.end(),
                [candidate](const CompanionState& state)
                {
                    return state.Active && state.ClientIndex == candidate;
                });
            if (!inUse)
            {
                return candidate;
            }
        }

        return -1;
    }

    bool IsReservedCompanionClientIndex(int clientIndex)
    {
        return clientIndex >= kReservedClientIndexBase && clientIndex < MAX_CHARACTERS_CLIENT;
    }

    int GetPublicVisualKey(int clientIndex)
    {
        return kPublicVisualKeyBase + clientIndex;
    }

    bool IsPublicVisualClientIndex(int clientIndex)
    {
        return clientIndex >= kPublicVisualClientIndexBase
            && clientIndex < kPublicVisualClientIndexBase + kPublicVisualClientIndexCount;
    }

    bool IsPublicVisualCharacter(const CHARACTER* character)
    {
        if (character == nullptr || !character->Object.Live)
        {
            return false;
        }

        return character->Key >= kPublicVisualKeyBase + kPublicVisualClientIndexBase
            && character->Key < kPublicVisualKeyBase + kPublicVisualClientIndexBase + kPublicVisualClientIndexCount;
    }

    bool IsValidSlot(int accountSlot)
    {
        return accountSlot >= 0 && accountSlot < AccountCharacterList::MaxCharacters;
    }

    AccountCompanionClient::CommandMode ParseCommandMode(const std::wstring& value)
    {
        return value == L"farm_position"
            ? AccountCompanionClient::CommandMode::FarmPosition
            : AccountCompanionClient::CommandMode::Follow;
    }

    bool IsFarmPositionCommand(int accountSlot)
    {
        return IsValidSlot(accountSlot)
            && g_guardStatuses[accountSlot].Mode == AccountCompanionClient::CommandMode::FarmPosition;
    }

    void SetGuardCooldown(GuardRuntimeStatus& status, int seconds)
    {
        status.CooldownSeconds = std::max(0, seconds);
        status.CooldownEndsAtTick = status.CooldownSeconds > 0
            ? GetTickCount64() + (static_cast<unsigned long long>(status.CooldownSeconds) * 1000ULL)
            : 0ULL;
    }

    int GetRemainingGuardCooldownSeconds(const GuardRuntimeStatus& status)
    {
        if (status.CooldownEndsAtTick == 0)
        {
            return 0;
        }

        const unsigned long long now = GetTickCount64();
        if (now >= status.CooldownEndsAtTick)
        {
            return 0;
        }

        return static_cast<int>((status.CooldownEndsAtTick - now + 999ULL) / 1000ULL);
    }

    long long MakePublicVisualStateKey(int ownerKey, int formationIndex)
    {
        return (static_cast<long long>(ownerKey) << 8)
            | static_cast<unsigned char>(std::clamp(formationIndex, 0, AccountCompanionClient::MaxSummonedCompanions - 1));
    }

    int HexValue(wchar_t ch)
    {
        if (ch >= L'0' && ch <= L'9')
        {
            return ch - L'0';
        }

        if (ch >= L'A' && ch <= L'F')
        {
            return 10 + (ch - L'A');
        }

        if (ch >= L'a' && ch <= L'f')
        {
            return 10 + (ch - L'a');
        }

        return -1;
    }

    bool DecodeHexBytes(const std::wstring& hex, BYTE* target, int targetLength)
    {
        if (target == nullptr || targetLength <= 0 || static_cast<int>(hex.length()) < targetLength * 2)
        {
            return false;
        }

        for (int index = 0; index < targetLength; ++index)
        {
            const int high = HexValue(hex[index * 2]);
            const int low = HexValue(hex[(index * 2) + 1]);
            if (high < 0 || low < 0)
            {
                return false;
            }

            target[index] = static_cast<BYTE>((high << 4) | low);
        }

        return true;
    }

    void AppendHexByte(wchar_t*& cursor, BYTE value)
    {
        static constexpr wchar_t digits[] = L"0123456789ABCDEF";
        *cursor++ = digits[(value >> 4) & 0x0F];
        *cursor++ = digits[value & 0x0F];
    }

    int CountActiveCompanions()
    {
        return static_cast<int>(std::count_if(
            g_companions.begin(),
            g_companions.end(),
            [](const CompanionState& state)
            {
                return state.Active;
            }));
    }

    int GetCurrentSummonLimit()
    {
        return std::clamp(g_planGuardLimit, 1, AccountCompanionClient::MaxSummonedCompanions);
    }

    int FindVisibleCharacterIndexByName(const wchar_t* name)
    {
        if (name == nullptr || name[0] == L'\0')
        {
            return -1;
        }

        for (int index = 0; index < MAX_CHARACTERS_CLIENT; ++index)
        {
            CHARACTER* character = &CharactersClient[index];
            if (character == Hero)
            {
                continue;
            }

            if (character->Object.Live && std::wcscmp(character->ID, name) == 0)
            {
                return index;
            }
        }

        return -1;
    }

    int FindServerCharacterIndexByName(const wchar_t* name)
    {
        if (name == nullptr || name[0] == L'\0')
        {
            return -1;
        }

        for (int index = 0; index < MAX_CHARACTERS_CLIENT; ++index)
        {
            if (IsReservedCompanionClientIndex(index))
            {
                continue;
            }

            CHARACTER* character = &CharactersClient[index];
            if (character == Hero)
            {
                continue;
            }

            if (character->Object.Live && std::wcscmp(character->ID, name) == 0)
            {
                return index;
            }
        }

        return -1;
    }

    bool IsCurrentHero(const AccountCharacterList::Entry& entry)
    {
        return Hero != nullptr && std::wcscmp(entry.Name, Hero->ID) == 0;
    }

    bool Normalize(float& x, float& y)
    {
        const float length = std::sqrt((x * x) + (y * y));
        if (length <= 0.001f)
        {
            return false;
        }

        x /= length;
        y /= length;
        return true;
    }

    float Dot(float ax, float ay, float bx, float by)
    {
        return (ax * bx) + (ay * by);
    }

    void PushTrailPoint(TownFormationState& state, float x, float y, float forwardX, float forwardY)
    {
        Normalize(forwardX, forwardY);

        if (!state.Trail.empty())
        {
            const TrailPoint& last = state.Trail.back();
            const float dx = x - last.X;
            const float dy = y - last.Y;
            if (std::sqrt((dx * dx) + (dy * dy)) < kTrailAppendEpsilon)
            {
                return;
            }
        }

        state.Trail.push_back({ x, y, forwardX, forwardY });
        while (static_cast<int>(state.Trail.size()) > kLeaderTrailMaxPoints)
        {
            state.Trail.pop_front();
        }
    }

    void AppendLeaderTrail(TownFormationState& state, float leaderX, float leaderY, float forwardX, float forwardY)
    {
        PushTrailPoint(state, leaderX, leaderY, forwardX, forwardY);
    }

    bool SampleLeaderTrail(const TownFormationState& state, float distanceBack, TrailPoint& sample)
    {
        if (state.Trail.empty())
        {
            return false;
        }

        if (state.Trail.size() == 1)
        {
            sample = state.Trail.back();
            return true;
        }

        float remaining = std::max(0.f, distanceBack);
        for (int index = static_cast<int>(state.Trail.size()) - 1; index > 0; --index)
        {
            const TrailPoint& current = state.Trail[index];
            const TrailPoint& previous = state.Trail[index - 1];
            float segmentX = current.X - previous.X;
            float segmentY = current.Y - previous.Y;
            const float length = std::sqrt((segmentX * segmentX) + (segmentY * segmentY));

            if (length <= 0.001f)
            {
                continue;
            }

            segmentX /= length;
            segmentY /= length;

            if (remaining <= length)
            {
                sample.X = current.X - (segmentX * remaining);
                sample.Y = current.Y - (segmentY * remaining);
                sample.ForwardX = segmentX;
                sample.ForwardY = segmentY;
                return true;
            }

            remaining -= length;
        }

        sample = state.Trail.front();
        return true;
    }

    void DirectionFromAngle(float angle, float& x, float& y)
    {
        const float radians = angle * (kPi / 180.f);
        x = std::sin(radians);
        y = -std::cos(radians);
        Normalize(x, y);
    }

    bool IsDiagonalMovement(float x, float y)
    {
        const float absX = std::fabs(x);
        const float absY = std::fabs(y);
        if (absX < kLeaderMoveEpsilon || absY < kLeaderMoveEpsilon)
        {
            return false;
        }

        const float ratio = absX > absY ? absX / absY : absY / absX;
        return ratio <= 2.35f;
    }

    bool IsWorldPositionSafeZone(float x, float y)
    {
        const int tileX = std::clamp(static_cast<int>(x / TERRAIN_SCALE), 0, TERRAIN_SIZE - 1);
        const int tileY = std::clamp(static_cast<int>(y / TERRAIN_SCALE), 0, TERRAIN_SIZE - 1);
        return (TerrainWall[TERRAIN_INDEX(tileX, tileY)] & TW_SAFEZONE) == TW_SAFEZONE;
    }

    bool IsTownFormationMap()
    {
        return Hero != nullptr && IsWorldPositionSafeZone(Hero->Object.Position[0], Hero->Object.Position[1]);
    }

    bool ShouldUseCityVisualFormation(const CHARACTER* leader)
    {
        return leader != nullptr && leader->Object.Live;
    }

    bool IsPvpServerSelected()
    {
        return g_ServerListManager != nullptr && g_ServerListManager->IsSelectedPvpServer();
    }

    bool ShouldUseClientVisualFormation()
    {
        return kUseClientVisualFormation && !IsPvpServerSelected();
    }

    bool ShouldHideServerDrivenGuardActors()
    {
        return kHideServerDrivenGuardActors && !IsPvpServerSelected();
    }

    void ResetTownFormationState(TownFormationState& state, CHARACTER* leader)
    {
        if (leader == nullptr)
        {
            state = {};
            return;
        }

        state.Initialized = true;
        state.World = gMapManager.WorldActive;
        state.LastLeaderX = leader->Object.Position[0];
        state.LastLeaderY = leader->Object.Position[1];
        state.AnchorX = leader->Object.Position[0];
        state.AnchorY = leader->Object.Position[1];
        state.HoldAnchorX = state.AnchorX;
        state.HoldAnchorY = state.AnchorY;
        DirectionFromAngle(leader->Object.Angle[2], state.StableForwardX, state.StableForwardY);
        state.CandidateForwardX = state.StableForwardX;
        state.CandidateForwardY = state.StableForwardY;
        state.CandidateDistance = 0.f;
        state.WaitingForTurn = false;
        state.Trail.clear();
        PushTrailPoint(state, state.AnchorX, state.AnchorY, state.StableForwardX, state.StableForwardY);
    }

    void ResetTownFormationState()
    {
        ResetTownFormationState(g_townFormation, Hero);
    }

    FormationFrame UpdateTownFormationFrame(TownFormationState& state, CHARACTER* leader)
    {
        FormationFrame frame;
        if (leader == nullptr || !leader->Object.Live)
        {
            return frame;
        }

        if (!state.Initialized || state.World != gMapManager.WorldActive)
        {
            ResetTownFormationState(state, leader);
        }

        const float leaderX = leader->Object.Position[0];
        const float leaderY = leader->Object.Position[1];
        const float moveX = leaderX - state.LastLeaderX;
        const float moveY = leaderY - state.LastLeaderY;
        const float moveDistance = std::sqrt((moveX * moveX) + (moveY * moveY));

        if (moveDistance > kLeaderMoveEpsilon)
        {
            float moveForwardX = moveX;
            float moveForwardY = moveY;
            Normalize(moveForwardX, moveForwardY);
            AppendLeaderTrail(state, leaderX, leaderY, moveForwardX, moveForwardY);

            const bool diagonal = IsDiagonalMovement(moveX, moveY);
            const float stableDot = Dot(moveForwardX, moveForwardY, state.StableForwardX, state.StableForwardY);

            if (diagonal || stableDot >= kSharpTurnDot)
            {
                state.StableForwardX = moveForwardX;
                state.StableForwardY = moveForwardY;
                state.CandidateForwardX = moveForwardX;
                state.CandidateForwardY = moveForwardY;
                state.CandidateDistance = 0.f;
                state.WaitingForTurn = false;
                state.AnchorX = leaderX;
                state.AnchorY = leaderY;
            }
            else
            {
                const float candidateDot = Dot(moveForwardX, moveForwardY, state.CandidateForwardX, state.CandidateForwardY);
                if (!state.WaitingForTurn || candidateDot < kSharpTurnDot)
                {
                    state.CandidateForwardX = moveForwardX;
                    state.CandidateForwardY = moveForwardY;
                    state.CandidateDistance = 0.f;
                    state.HoldAnchorX = state.AnchorX;
                    state.HoldAnchorY = state.AnchorY;
                    state.WaitingForTurn = true;
                }

                state.CandidateDistance += moveDistance;
                if (state.CandidateDistance >= kConfirmedTurnDistance)
                {
                    state.StableForwardX = state.CandidateForwardX;
                    state.StableForwardY = state.CandidateForwardY;
                    state.CandidateDistance = 0.f;
                    state.WaitingForTurn = false;
                    state.AnchorX = leaderX;
                    state.AnchorY = leaderY;
                }
                else
                {
                    state.AnchorX = state.HoldAnchorX;
                    state.AnchorY = state.HoldAnchorY;
                }
            }

            state.LastLeaderX = leaderX;
            state.LastLeaderY = leaderY;
        }

        frame.AnchorX = state.AnchorX;
        frame.AnchorY = state.AnchorY;
        frame.ForwardX = state.StableForwardX;
        frame.ForwardY = state.StableForwardY;
        frame.RightX = state.StableForwardY;
        frame.RightY = -state.StableForwardX;
        frame.TownMode = true;
        return frame;
    }

    FormationFrame UpdateTownFormationFrame()
    {
        return UpdateTownFormationFrame(g_townFormation, Hero);
    }

    void GetLooseFormationOffset(int formationIndex, float& offsetX, float& offsetY)
    {
        static constexpr int columns = 5;
        static constexpr float columnSpacing = 72.f;
        static constexpr float rowSpacing = 82.f;
        static constexpr float firstRowY = 95.f;

        const int index = std::clamp(formationIndex, 0, AccountCompanionClient::MaxSummonedCompanions - 1);
        const int row = index / columns;
        const int column = index % columns;
        const float centeredColumn = static_cast<float>(column) - ((columns - 1) / 2.f);

        offsetX = centeredColumn * columnSpacing;
        offsetY = firstRowY + (static_cast<float>(row) * rowSpacing);

        if ((row % 2) == 1)
        {
            offsetX += columnSpacing * 0.5f;
        }
    }

    void GetTownFormationTarget(const TownFormationState& state, int formationIndex, const FormationFrame& frame, float& targetX, float& targetY)
    {
        static constexpr int columnOrder[kTownFormationColumns] = { 0, -1, 1 };

        const int index = std::clamp(formationIndex, 0, AccountCompanionClient::MaxSummonedCompanions - 1);
        const int row = index / kTownFormationColumns;
        const int column = index % kTownFormationColumns;
        const float side = static_cast<float>(columnOrder[column]);
        const float backDistance = kTownFirstRowDistance + (static_cast<float>(row) * kTownRowSpacing);

        TrailPoint trailPoint;
        if (SampleLeaderTrail(state, backDistance, trailPoint))
        {
            const float rightX = trailPoint.ForwardY;
            const float rightY = -trailPoint.ForwardX;
            targetX = trailPoint.X + (rightX * side * kTownColumnSpacing);
            targetY = trailPoint.Y + (rightY * side * kTownColumnSpacing);
            return;
        }

        targetX = frame.AnchorX - (frame.ForwardX * backDistance) + (frame.RightX * side * kTownColumnSpacing);
        targetY = frame.AnchorY - (frame.ForwardY * backDistance) + (frame.RightY * side * kTownColumnSpacing);
    }

    void GetTownFormationTarget(int formationIndex, const FormationFrame& frame, float& targetX, float& targetY)
    {
        GetTownFormationTarget(g_townFormation, formationIndex, frame, targetX, targetY);
    }

    void SetTileFromWorldPosition(CHARACTER* character)
    {
        if (character == nullptr)
        {
            return;
        }

        const int tileX = std::clamp(static_cast<int>(character->Object.Position[0] / TERRAIN_SCALE), 0, TERRAIN_SIZE - 1);
        const int tileY = std::clamp(static_cast<int>(character->Object.Position[1] / TERRAIN_SCALE), 0, TERRAIN_SIZE - 1);
        character->PositionX = static_cast<BYTE>(tileX);
        character->PositionY = static_cast<BYTE>(tileY);
        character->TargetX = character->PositionX;
        character->TargetY = character->PositionY;
        character->SafeZone = IsWorldPositionSafeZone(character->Object.Position[0], character->Object.Position[1]);
    }

    void SetServerCharacterHidden(CHARACTER* character, bool hidden)
    {
        if (character == nullptr || character == Hero)
        {
            return;
        }

        character->Object.Visible = !hidden;
        character->Object.Alpha = hidden ? 0.f : 1.f;
        character->Object.AlphaTarget = hidden ? 0.f : 1.f;
        character->Object.EnableShadow = !hidden;
        character->Object.m_bRenderShadow = !hidden;

        if (hidden && Hero != nullptr && Hero->TargetCharacter == character->Key)
        {
            Hero->TargetCharacter = -1;
        }
    }

    void SetServerCharactersHiddenByName(const wchar_t* name, bool hidden)
    {
        if (name == nullptr || name[0] == L'\0')
        {
            return;
        }

        for (int index = 0; index < MAX_CHARACTERS_CLIENT; ++index)
        {
            if (IsReservedCompanionClientIndex(index))
            {
                continue;
            }

            CHARACTER* character = &CharactersClient[index];
            if (character == Hero || !character->Object.Live)
            {
                continue;
            }

            if (std::wcscmp(character->ID, name) == 0)
            {
                SetServerCharacterHidden(character, hidden);
            }
        }
    }

    void HideServerDrivenCompanion(const AccountCharacterList::Entry& entry)
    {
        if (!ShouldHideServerDrivenGuardActors())
        {
            return;
        }

        SetServerCharactersHiddenByName(entry.Name, true);
    }

    void RestoreServerDrivenCompanion(const AccountCharacterList::Entry& entry)
    {
        if (!kHideServerDrivenGuardActors)
        {
            return;
        }

        SetServerCharactersHiddenByName(entry.Name, false);
    }

    void SendGuardCommand(const wchar_t* command)
    {
        if (Hero == nullptr || command == nullptr || command[0] == L'\0')
        {
            return;
        }

        wchar_t text[128] = {};
        std::wcsncpy(text, command, std::size(text) - 1);
        SendMacroChat(text);
    }

    void SendGuardCommand(const wchar_t* action, const AccountCharacterList::Entry& entry)
    {
        if (entry.Name[0] == L'\0')
        {
            return;
        }

        wchar_t text[128] = {};
        swprintf_s(text, L"/guard %ls %ls", action, entry.Name);
        SendMacroChat(text);
    }

    void SendGuardCommand3(const wchar_t* action, const AccountCharacterList::Entry& entry, const wchar_t* argument)
    {
        if (entry.Name[0] == L'\0' || action == nullptr || argument == nullptr)
        {
            return;
        }

        wchar_t text[128] = {};
        swprintf_s(text, L"/guard %ls %ls %ls", action, entry.Name, argument);
        SendMacroChat(text);
    }

    void SendGuardVisualReleaseCommand(const AccountCharacterList::Entry& entry, const CHARACTER* visual)
    {
        if (entry.Name[0] == L'\0')
        {
            return;
        }

        if (visual == nullptr)
        {
            SendGuardCommand(L"call", entry);
            return;
        }

        const int tileX = std::clamp(static_cast<int>(visual->Object.Position[0] / TERRAIN_SCALE), 0, TERRAIN_SIZE - 1);
        const int tileY = std::clamp(static_cast<int>(visual->Object.Position[1] / TERRAIN_SCALE), 0, TERRAIN_SIZE - 1);

        wchar_t text[128] = {};
        swprintf_s(text, L"/guard callvisual %ls %d %d", entry.Name, tileX, tileY);
        SendMacroChat(text);
    }

    void SendGuardVisualPublicCommand(const AccountCharacterList::Entry& entry, int formationIndex)
    {
        if (entry.Name[0] == L'\0')
        {
            return;
        }

        wchar_t equipmentHex[(AccountCharacterList::EquipmentLength * 2) + 1] = {};
        wchar_t* cursor = equipmentHex;
        for (int index = 0; index < AccountCharacterList::EquipmentLength; ++index)
        {
            AppendHexByte(cursor, entry.Equipment[index]);
        }

        *cursor = L'\0';

        const int mode = (entry.Flags & 0xFF) | (entry.UsesExtendedEquipment ? 0x100 : 0);
        wchar_t text[128] = {};
        swprintf_s(
            text,
            L"/guard vp %d|%d|%d|%d|%ls|%ls",
            std::clamp(formationIndex, 0, AccountCompanionClient::MaxSummonedCompanions - 1),
            static_cast<int>(entry.Class),
            entry.Level,
            mode,
            equipmentHex,
            entry.Name);
        SendMacroChat(text);
    }

    void PublishLocalCityVisuals()
    {
        if (Hero == nullptr || !Hero->Object.Live || !IsTownFormationMap())
        {
            return;
        }

        const unsigned long long now = GetTickCount64();
        if (now - g_lastPublicVisualPublishTick < kPublicVisualPublishIntervalMs)
        {
            return;
        }

        g_lastPublicVisualPublishTick = now;

        int formationIndex = 0;
        const int summonLimit = GetCurrentSummonLimit();
        for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
        {
            if (formationIndex >= summonLimit)
            {
                break;
            }

            if (!g_companions[slot].Active)
            {
                continue;
            }

            const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot);
            if (entry == nullptr || IsCurrentHero(*entry))
            {
                continue;
            }

            SendGuardVisualPublicCommand(*entry, formationIndex);

            ++formationIndex;
        }
    }

    bool ShouldSendServerCallImmediately()
    {
        return !ShouldUseClientVisualFormation() || !IsTownFormationMap();
    }

    void SetPendingSwitchTarget(const wchar_t* characterName)
    {
        if (characterName == nullptr || characterName[0] == L'\0')
        {
            g_pendingSwitchTargetName[0] = L'\0';
            return;
        }

        std::wcsncpy(g_pendingSwitchTargetName.data(), characterName, MAX_USERNAME_SIZE);
        g_pendingSwitchTargetName[MAX_USERNAME_SIZE] = L'\0';
    }

    void DeleteLocalCompanionObject(int accountSlot);
    void ClearPublicVisualCompanions();
    void ClearWebStoreLeaderTitles();

    bool HasPendingSwitchTargetInternal()
    {
        return g_pendingSwitchTargetName[0] != L'\0';
    }

    void PreserveActiveSlotsForSwitch(int targetSlot)
    {
        g_switchPreservedActiveSlots.fill(false);
        g_switchPreservedServerSlots.fill(false);
        g_switchPreservedOfflineSlots.fill(false);
        for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
        {
            g_switchPreservedActiveSlots[slot] = g_companions[slot].Active;
            g_switchPreservedServerSlots[slot] = g_companions[slot].ServerActive;
            g_switchPreservedOfflineSlots[slot] = g_companions[slot].OfflinePlay;
        }

        g_pendingSwitchSourceSlot = Hero != nullptr
            ? AccountCharacterList::FindSlotByName(Hero->ID)
            : -1;
        g_pendingSwitchTargetSlot = targetSlot;

        if (IsValidSlot(g_pendingSwitchSourceSlot))
        {
            g_switchPreservedActiveSlots[g_pendingSwitchSourceSlot] = true;
            g_switchPreservedServerSlots[g_pendingSwitchSourceSlot] = true;
            g_switchPreservedOfflineSlots[g_pendingSwitchSourceSlot] = true;
        }

        if (IsValidSlot(g_pendingSwitchTargetSlot))
        {
            g_switchPreservedActiveSlots[g_pendingSwitchTargetSlot] = false;
            g_switchPreservedServerSlots[g_pendingSwitchTargetSlot] = false;
            g_switchPreservedOfflineSlots[g_pendingSwitchTargetSlot] = false;
        }
    }

    void ResetLocalCompanionStateForPendingSwitch()
    {
        for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
        {
            if (const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot))
            {
                RestoreServerDrivenCompanion(*entry);
            }

            DeleteLocalCompanionObject(slot);
            g_companions[slot] = {};
            if (g_switchPreservedActiveSlots[slot])
            {
                g_companions[slot].Active = true;
                g_companions[slot].ServerActive = g_switchPreservedServerSlots[slot];
                g_companions[slot].OfflinePlay = g_switchPreservedOfflineSlots[slot];
                g_companions[slot].ClientIndex = GetCompanionClientIndex(slot);
            }
        }

        g_townFormation = {};
        g_wasHeroInSafeZone = false;
        g_lastCityStoreCommandTick = 0;
        ClearWebStoreLeaderTitles();
        g_remoteCompanions.clear();
        ClearPublicVisualCompanions();
    }

    void DeleteLocalCompanionObject(int accountSlot)
    {
        if (!IsValidSlot(accountSlot))
        {
            return;
        }

        const CompanionState state = g_companions[accountSlot];
        if (state.ClientIndex < 0 || state.ClientIndex >= MAX_CHARACTERS_CLIENT)
        {
            return;
        }

        CHARACTER* character = &CharactersClient[state.ClientIndex];
        if (character->Object.Live && character->Key == state.ClientIndex)
        {
            DeleteCharacter(state.ClientIndex);
        }

        g_companions[accountSlot].ClientIndex = -1;
    }

    void DeletePublicVisualObject(PublicCompanionVisualState& state)
    {
        if (!IsPublicVisualClientIndex(state.ClientIndex))
        {
            state.ClientIndex = -1;
            return;
        }

        CHARACTER* character = &CharactersClient[state.ClientIndex];
        if (character->Object.Live && character->Key == GetPublicVisualKey(state.ClientIndex))
        {
            DeleteCharacter(character, &character->Object);
        }

        state.ClientIndex = -1;
    }

    void ClearPublicVisualCompanions()
    {
        for (auto& entry : g_publicVisualCompanions)
        {
            DeletePublicVisualObject(entry.second);
        }

        g_publicVisualCompanions.clear();
    }

    bool IsPublicVisualIndexUsedByState(int clientIndex)
    {
        for (const auto& entry : g_publicVisualCompanions)
        {
            if (entry.second.ClientIndex == clientIndex)
            {
                return true;
            }
        }

        return false;
    }

    int AllocatePublicVisualClientIndex()
    {
        for (int clientIndex = kPublicVisualClientIndexBase;
            clientIndex < kPublicVisualClientIndexBase + kPublicVisualClientIndexCount;
            ++clientIndex)
        {
            if (IsPublicVisualIndexUsedByState(clientIndex))
            {
                continue;
            }

            CHARACTER* character = &CharactersClient[clientIndex];
            if (character->Object.Live && !IsPublicVisualCharacter(character))
            {
                continue;
            }

            if (IsPublicVisualCharacter(character))
            {
                DeleteCharacter(character, &character->Object);
            }

            return clientIndex;
        }

        return -1;
    }

    CHARACTER* GetCompanionCharacter(int accountSlot)
    {
        if (!IsValidSlot(accountSlot))
        {
            return nullptr;
        }

        const int clientIndex = g_companions[accountSlot].ClientIndex;
        if (clientIndex < 0 || clientIndex >= MAX_CHARACTERS_CLIENT)
        {
            return nullptr;
        }

        CHARACTER* character = &CharactersClient[clientIndex];
        return character->Object.Live ? character : nullptr;
    }

    CHARACTER* FindLiveCharacterByKey(int key)
    {
        const int index = FindCharacterIndex(key);
        if (index < 0 || index >= MAX_CHARACTERS_CLIENT)
        {
            return nullptr;
        }

        CHARACTER* character = &CharactersClient[index];
        return character->Object.Live ? character : nullptr;
    }

    bool HasRemoteAccountWideStore(int ownerKey)
    {
        if (ownerKey <= 0)
        {
            return false;
        }

        for (const auto& entry : g_remoteCompanions)
        {
            if (entry.second.OwnerKey == ownerKey && entry.second.OwnerHasWebStore)
            {
                return true;
            }
        }

        return false;
    }

    void RemoveWebStoreLeaderTitle(int ownerKey)
    {
        if (CHARACTER* owner = FindLiveCharacterByKey(ownerKey))
        {
            std::wstring title;
            GetShopTitle(owner, title);
            if (title == L"Loja da conta")
            {
                RemoveShopTitle(owner);
            }
        }
    }

    void ClearWebStoreLeaderTitles()
    {
        for (const int ownerKey : g_webStoreTitleOwnerKeys)
        {
            RemoveWebStoreLeaderTitle(ownerKey);
        }

        g_webStoreTitleOwnerKeys.clear();
    }

    void UpdateWebStoreLeaderTitles()
    {
        std::unordered_set<int> activeOwnerKeys;
        for (const auto& entry : g_remoteCompanions)
        {
            if (entry.second.OwnerHasWebStore && entry.second.OwnerKey > 0)
            {
                activeOwnerKeys.insert(entry.second.OwnerKey);
            }
        }

        for (auto it = g_webStoreTitleOwnerKeys.begin(); it != g_webStoreTitleOwnerKeys.end();)
        {
            if (activeOwnerKeys.find(*it) == activeOwnerKeys.end())
            {
                RemoveWebStoreLeaderTitle(*it);
                it = g_webStoreTitleOwnerKeys.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (const int ownerKey : activeOwnerKeys)
        {
            CHARACTER* owner = FindLiveCharacterByKey(ownerKey);
            if (owner == nullptr)
            {
                continue;
            }

            AddShopTitle(ownerKey, owner, L"Loja da conta");
            g_webStoreTitleOwnerKeys.insert(ownerKey);
        }
    }

    void GetFormationTarget(int formationIndex, float& targetX, float& targetY, float& smoothing, bool& axisLocked);

    void CopyVisualPoseToServerDrivenCompanion(const AccountCharacterList::Entry& entry, const CHARACTER* visual)
    {
        if (visual == nullptr)
        {
            return;
        }

        const int serverIndex = FindServerCharacterIndexByName(entry.Name);
        if (serverIndex < 0)
        {
            return;
        }

        CHARACTER* server = &CharactersClient[serverIndex];
        if (!server->Object.Live)
        {
            return;
        }

        server->Object.Position[0] = visual->Object.Position[0];
        server->Object.Position[1] = visual->Object.Position[1];
        server->Object.Position[2] = visual->Object.Position[2];
        server->Object.Angle[2] = visual->Object.Angle[2];
        server->PositionX = visual->PositionX;
        server->PositionY = visual->PositionY;
        server->TargetX = visual->TargetX;
        server->TargetY = visual->TargetY;
        server->SafeZone = visual->SafeZone;
    }

    void ReleaseLocalVisualCompanionToServer(int accountSlot, const AccountCharacterList::Entry& entry)
    {
        const CHARACTER* visual = GetCompanionCharacter(accountSlot);
        if (kSendServerGuardCommands && !g_companions[accountSlot].ServerActive)
        {
            SendGuardVisualReleaseCommand(entry, visual);
            g_companions[accountSlot].ServerActive = true;
            g_companions[accountSlot].OfflinePlay = true;
        }

        CopyVisualPoseToServerDrivenCompanion(entry, visual);
        RestoreServerDrivenCompanion(entry);
        DeleteLocalCompanionObject(accountSlot);
    }

    void StoreServerCompanionForCityVisual(int accountSlot, const AccountCharacterList::Entry& entry, const CHARACTER* visual)
    {
        if (!ShouldUseClientVisualFormation()
            || !IsValidSlot(accountSlot)
            || IsFarmPositionCommand(accountSlot)
            || !g_companions[accountSlot].ServerActive
            || visual == nullptr
            || !visual->Object.Live
            || (!IsTownFormationMap()
                && !IsWorldPositionSafeZone(visual->Object.Position[0], visual->Object.Position[1])))
        {
            return;
        }

        CompanionState& state = g_companions[accountSlot];
        const unsigned long long now = GetTickCount64();
        if ((state.CityStoreRequestPending && now - state.CityStoreRequestTick < kCityStoreRetryIntervalMs)
            || now - g_lastCityStoreCommandTick < kCityStoreCommandIntervalMs)
        {
            return;
        }

        const bool isRetry = state.CityStoreRequestPending;
        if (kSendServerGuardCommands)
        {
            SendGuardCommand(L"storevisual", entry);
        }

        state.CityStoreRequestPending = true;
        state.CityStoreRequestTick = now;
        g_lastCityStoreCommandTick = now;
        g_ErrorReport.Write(
            L"[SentinelCity] storevisual %ls slot=%d name=%ls\r\n",
            isRetry ? L"retry" : L"requested",
            accountSlot,
            entry.Name);
        HideServerDrivenCompanion(entry);
    }

    bool KeepFarmPositionCompanionServerDriven(int accountSlot, const AccountCharacterList::Entry& entry)
    {
        if (!IsFarmPositionCommand(accountSlot))
        {
            return false;
        }

        if (g_guardStatuses[accountSlot].Active)
        {
            g_companions[accountSlot].ServerActive = true;
            g_companions[accountSlot].OfflinePlay = true;
        }

        RestoreServerDrivenCompanion(entry);
        DeleteLocalCompanionObject(accountSlot);
        return true;
    }

    CHARACTER* CreateOrRefreshCompanion(const AccountCharacterList::Entry& entry, int formationIndex)
    {
        if (Hero == nullptr || !Hero->Object.Live || IsCurrentHero(entry))
        {
            return nullptr;
        }

        const int clientIndex = GetCompanionClientIndex(entry.Slot);
        if (clientIndex < 0 || clientIndex >= MAX_CHARACTERS_CLIENT)
        {
            return nullptr;
        }

        g_companions[entry.Slot].ClientIndex = clientIndex;

        CHARACTER* existing = GetCompanionCharacter(entry.Slot);
        if (existing != nullptr)
        {
            return existing;
        }

        float targetX = 0.f;
        float targetY = 0.f;
        float smoothing = kFollowSmoothing;
        bool axisLocked = false;
        GetFormationTarget(formationIndex, targetX, targetY, smoothing, axisLocked);

        float spawnX = targetX;
        float spawnY = targetY;
        float spawnAngle = Hero->Object.Angle[2];
        const int serverIndex = FindServerCharacterIndexByName(entry.Name);
        if (serverIndex >= 0 && CharactersClient[serverIndex].Object.Live)
        {
            const CHARACTER* server = &CharactersClient[serverIndex];
            spawnX = server->Object.Position[0];
            spawnY = server->Object.Position[1];
            spawnAngle = server->Object.Angle[2];
        }

        CHARACTER* companion = CreateHero(
            clientIndex,
            entry.Class,
            0,
            spawnX,
            spawnY,
            spawnAngle);

        companion->Level = entry.Level;
        companion->CtlCode = entry.CtlCode;
        companion->Key = clientIndex;
        companion->Object.Visible = true;
        companion->Object.Alpha = 0.92f;
        companion->Object.AlphaTarget = 0.92f;
        companion->Object.EnableShadow = true;
        companion->Object.m_bRenderShadow = true;
        companion->Object.m_fEdgeScale = 1.05f;
        companion->PK = PVP_NEUTRAL;

        std::wcsncpy(companion->ID, entry.Name, MAX_USERNAME_SIZE);
        companion->ID[MAX_USERNAME_SIZE] = L'\0';
        SetTileFromWorldPosition(companion);
        SetPlayerStop(companion);
        return companion;
    }

    void ApplyCompanionEquipment(CHARACTER* companion, const AccountCharacterList::Entry& entry)
    {
        if (companion == nullptr || !entry.HasEquipment)
        {
            return;
        }

        if (entry.UsesExtendedEquipment)
        {
            ReadEquipmentExtended(companion->Key, entry.Flags, const_cast<BYTE*>(entry.Equipment));
        }
        else
        {
            ChangeCharacterExt(companion->Key, const_cast<BYTE*>(entry.Equipment));
        }

        companion->Level = entry.Level;
        companion->CtlCode = entry.CtlCode;
        companion->Object.Visible = true;
        companion->Object.Alpha = 0.92f;
        companion->Object.AlphaTarget = 0.92f;
        companion->Object.EnableShadow = true;
        companion->Object.m_bRenderShadow = true;
    }

    CHARACTER* CreateOrRefreshPublicVisualCompanion(PublicCompanionVisualState& state, const CHARACTER* owner)
    {
        if (owner == nullptr || !owner->Object.Live || state.Name[0] == L'\0')
        {
            return nullptr;
        }

        if (IsPublicVisualClientIndex(state.ClientIndex))
        {
            CHARACTER* existing = &CharactersClient[state.ClientIndex];
            if (existing->Object.Live && existing->Key == GetPublicVisualKey(state.ClientIndex))
            {
                if (existing->Class == state.Class)
                {
                    if (state.AppearanceDirty)
                    {
                        if (state.UsesExtendedEquipment)
                        {
                            ReadEquipmentExtended(state.ClientIndex, state.Flags, state.Equipment);
                        }
                        else
                        {
                            ChangeCharacterExt(state.ClientIndex, state.Equipment);
                        }

                        existing->Key = GetPublicVisualKey(state.ClientIndex);
                        existing->Level = state.Level;
                        state.AppearanceDirty = false;
                    }

                    return existing;
                }

                DeletePublicVisualObject(state);
            }
            else
            {
                state.ClientIndex = -1;
            }
        }

        const int clientIndex = AllocatePublicVisualClientIndex();
        if (!IsPublicVisualClientIndex(clientIndex))
        {
            return nullptr;
        }

        state.ClientIndex = clientIndex;
        CHARACTER* visual = CreateHero(
            clientIndex,
            state.Class,
            0,
            owner->Object.Position[0],
            owner->Object.Position[1],
            owner->Object.Angle[2]);

        visual->Level = state.Level;
        visual->CtlCode = 0;
        visual->Object.Visible = true;
        visual->Object.Alpha = 0.92f;
        visual->Object.AlphaTarget = 0.92f;
        visual->Object.EnableShadow = true;
        visual->Object.m_bRenderShadow = true;
        visual->Object.m_fEdgeScale = 1.02f;
        visual->PK = PVP_NEUTRAL;

        std::wcsncpy(visual->ID, state.Name, MAX_USERNAME_SIZE);
        visual->ID[MAX_USERNAME_SIZE] = L'\0';

        if (state.UsesExtendedEquipment)
        {
            ReadEquipmentExtended(clientIndex, state.Flags, state.Equipment);
        }
        else
        {
            ChangeCharacterExt(clientIndex, state.Equipment);
        }

        visual->Key = GetPublicVisualKey(clientIndex);
        SetTileFromWorldPosition(visual);
        SetPlayerStop(visual);
        state.AppearanceDirty = false;
        return visual;
    }

    void GetFormationTarget(int formationIndex, float& targetX, float& targetY, float& smoothing, bool& axisLocked)
    {
        if (Hero == nullptr)
        {
            targetX = 0.f;
            targetY = 0.f;
            smoothing = kFollowSmoothing;
            axisLocked = false;
            return;
        }

        if (g_formationMode == AccountCompanionClient::FormationMode::CityMarch || IsTownFormationMap())
        {
            const FormationFrame frame = UpdateTownFormationFrame();
            GetTownFormationTarget(formationIndex, frame, targetX, targetY);
            smoothing = kTownFollowSmoothing;
            axisLocked = false;
            return;
        }

        float offsetX = 0.f;
        float offsetY = 0.f;
        GetLooseFormationOffset(formationIndex, offsetX, offsetY);
        targetX = Hero->Object.Position[0] + offsetX;
        targetY = Hero->Object.Position[1] + offsetY;
        smoothing = kFollowSmoothing;
        axisLocked = false;
    }

    void MoveCharacterToVisualTarget(CHARACTER* follower, const CHARACTER* leader, float targetX, float targetY, float smoothing, bool axisLocked)
    {
        if (follower == nullptr || leader == nullptr || !leader->Object.Live)
        {
            return;
        }

        const float targetZ = RequestTerrainHeight(targetX, targetY);
        const float deltaX = targetX - follower->Object.Position[0];
        const float deltaY = targetY - follower->Object.Position[1];
        const float distance = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));

        if (distance > kSnapDistance)
        {
            follower->Object.Position[0] = targetX;
            follower->Object.Position[1] = targetY;
            follower->Object.Position[2] = targetZ;
            follower->Object.Angle[2] = leader->Object.Angle[2];
            SetTileFromWorldPosition(follower);
            SetPlayerStop(follower);
            return;
        }

        if (distance > kStopDistance)
        {
            float moveX = deltaX;
            float moveY = deltaY;
            if (axisLocked)
            {
                const float absX = std::fabs(deltaX);
                const float absY = std::fabs(deltaY);
                if (absX >= absY)
                {
                    moveY = 0.f;
                }
                else
                {
                    moveX = 0.f;
                }
            }

            const float nextX = follower->Object.Position[0] + (moveX * smoothing);
            const float nextY = follower->Object.Position[1] + (moveY * smoothing);
            const float nextZ = RequestTerrainHeight(nextX, nextY);
            follower->Object.Angle[2] = CreateAngle(follower->Object.Position[0], follower->Object.Position[1], nextX, nextY);
            follower->Object.Position[0] = nextX;
            follower->Object.Position[1] = nextY;
            follower->Object.Position[2] += (nextZ - follower->Object.Position[2]) * smoothing;
            SetTileFromWorldPosition(follower);
            SetPlayerWalk(follower);
            return;
        }

        follower->Object.Angle[2] = leader->Object.Angle[2];
        SetTileFromWorldPosition(follower);
        SetPlayerStop(follower);
    }

    void MoveCompanionToTarget(int accountSlot, float targetX, float targetY, float smoothing, bool axisLocked)
    {
        MoveCharacterToVisualTarget(GetCompanionCharacter(accountSlot), Hero, targetX, targetY, smoothing, axisLocked);
    }

    void MoveCompanionToFormation(int accountSlot, int formationIndex)
    {
        float targetX = 0.f;
        float targetY = 0.f;
        float smoothing = kFollowSmoothing;
        bool axisLocked = false;
        GetFormationTarget(formationIndex, targetX, targetY, smoothing, axisLocked);
        MoveCompanionToTarget(accountSlot, targetX, targetY, smoothing, axisLocked);
    }

    void SnapCompanionToFormation(int accountSlot, int formationIndex)
    {
        CHARACTER* companion = GetCompanionCharacter(accountSlot);
        if (companion == nullptr || Hero == nullptr || !Hero->Object.Live)
        {
            return;
        }

        float targetX = 0.f;
        float targetY = 0.f;
        float smoothing = kFollowSmoothing;
        bool axisLocked = false;
        GetFormationTarget(formationIndex, targetX, targetY, smoothing, axisLocked);

        companion->Object.Position[0] = targetX;
        companion->Object.Position[1] = targetY;
        companion->Object.Position[2] = RequestTerrainHeight(targetX, targetY);
        companion->Object.Angle[2] = Hero->Object.Angle[2];
        SetTileFromWorldPosition(companion);
        SetPlayerStop(companion);
    }

    void StopActiveCompanions()
    {
        for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
        {
            if (!g_companions[slot].Active)
            {
                continue;
            }

            CHARACTER* companion = GetCompanionCharacter(slot);
            if (companion != nullptr)
            {
                SetTileFromWorldPosition(companion);
                SetPlayerStop(companion);
            }
        }
    }

    void ReleaseLocalVisualCompanionsToServer()
    {
        for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
        {
            if (!g_companions[slot].Active)
            {
                continue;
            }

            if (const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot))
            {
                ReleaseLocalVisualCompanionToServer(slot, *entry);
                continue;
            }

            DeleteLocalCompanionObject(slot);
        }

        g_townFormation = {};
    }

    void ClearLocalCompanionState()
    {
        for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
        {
            if (const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot))
            {
                RestoreServerDrivenCompanion(*entry);
            }

            DeleteLocalCompanionObject(slot);
            g_companions[slot] = {};
        }

        g_townFormation = {};
        g_wasHeroInSafeZone = false;
        g_lastCityStoreCommandTick = 0;
        g_remoteCompanions.clear();
        ClearPublicVisualCompanions();
    }

    bool ShouldUseVisualForTransition(const CHARACTER* visual, float targetX, float targetY)
    {
        if (IsTownFormationMap())
        {
            return true;
        }

        const bool targetSafeZone = IsWorldPositionSafeZone(targetX, targetY);
        const bool visualSafeZone = visual != nullptr
            && IsWorldPositionSafeZone(visual->Object.Position[0], visual->Object.Position[1]);

        return targetSafeZone || visualSafeZone;
    }

    void TickRemoteVisualCompanions()
    {
        for (auto it = g_remoteCompanions.begin(); it != g_remoteCompanions.end();)
        {
            CHARACTER* guard = FindLiveCharacterByKey(it->first);
            if (guard == nullptr)
            {
                it = g_remoteCompanions.erase(it);
                continue;
            }

            CHARACTER* owner = FindLiveCharacterByKey(it->second.OwnerKey);
            if (owner == nullptr || owner == Hero || guard == Hero)
            {
                ++it;
                continue;
            }

            if (!ShouldUseCityVisualFormation(owner))
            {
                it->second.Formation = {};
                ++it;
                continue;
            }

            FormationFrame frame = UpdateTownFormationFrame(it->second.Formation, owner);
            float targetX = 0.f;
            float targetY = 0.f;
            GetTownFormationTarget(it->second.Formation, it->second.FormationIndex, frame, targetX, targetY);
            if (!ShouldUseVisualForTransition(guard, targetX, targetY))
            {
                ++it;
                continue;
            }

            MoveCharacterToVisualTarget(guard, owner, targetX, targetY, kTownFollowSmoothing, false);

            guard->Object.Visible = true;
            guard->Object.Alpha = 1.f;
            guard->Object.AlphaTarget = 1.f;
            guard->Object.EnableShadow = true;
            guard->Object.m_bRenderShadow = true;
            ++it;
        }
    }

    void TickPublicVisualCompanions()
    {
        const unsigned long long now = GetTickCount64();
        for (auto it = g_publicVisualCompanions.begin(); it != g_publicVisualCompanions.end();)
        {
            PublicCompanionVisualState& state = it->second;
            CHARACTER* owner = FindLiveCharacterByKey(state.OwnerKey);
            const bool expired = now - state.LastSeenTick > kPublicVisualExpireMs;
            const bool invalidOwner = owner == nullptr
                || owner == Hero
                || !owner->Object.Live
                || !IsWorldPositionSafeZone(owner->Object.Position[0], owner->Object.Position[1]);

            if (expired || invalidOwner)
            {
                DeletePublicVisualObject(state);
                it = g_publicVisualCompanions.erase(it);
                continue;
            }

            CHARACTER* visual = CreateOrRefreshPublicVisualCompanion(state, owner);
            if (visual == nullptr)
            {
                ++it;
                continue;
            }

            FormationFrame frame = UpdateTownFormationFrame(state.Formation, owner);
            float targetX = 0.f;
            float targetY = 0.f;
            GetTownFormationTarget(state.Formation, state.FormationIndex, frame, targetX, targetY);
            MoveCharacterToVisualTarget(visual, owner, targetX, targetY, kTownFollowSmoothing, false);

            visual->Object.Visible = true;
            visual->Object.Alpha = 0.92f;
            visual->Object.AlphaTarget = 0.92f;
            visual->Object.EnableShadow = true;
            visual->Object.m_bRenderShadow = true;
            ++it;
        }
    }
}

bool AccountCompanionClient::IsSummoned(int accountSlot)
{
    return IsValidSlot(accountSlot) && g_companions[accountSlot].Active;
}

bool AccountCompanionClient::IsOfflinePlaying(int accountSlot)
{
    return IsValidSlot(accountSlot)
        && g_companions[accountSlot].Active
        && g_companions[accountSlot].OfflinePlay;
}

bool AccountCompanionClient::GetGuardStatus(int accountSlot, GuardStatus& status)
{
    if (!IsValidSlot(accountSlot))
    {
        status = {};
        return false;
    }

    const GuardRuntimeStatus& source = g_guardStatuses[accountSlot];
    status.Known = source.Known;
    status.Active = source.Known ? source.Active : IsSummoned(accountSlot);
    status.AutoSummon = source.AutoSummon;
    status.CooldownSeconds = GetRemainingGuardCooldownSeconds(source);
    status.Mode = source.Mode;
    status.FarmMapNumber = source.FarmMapNumber;
    status.FarmX = source.FarmX;
    status.FarmY = source.FarmY;
    return source.Known;
}

int AccountCompanionClient::GetCooldownSeconds(int accountSlot)
{
    return IsValidSlot(accountSlot) ? GetRemainingGuardCooldownSeconds(g_guardStatuses[accountSlot]) : 0;
}

bool AccountCompanionClient::IsAutoSummonEnabled(int accountSlot)
{
    return IsValidSlot(accountSlot) && g_guardStatuses[accountSlot].AutoSummon;
}

int AccountCompanionClient::GetActiveCount()
{
    return CountActiveCompanions();
}

int AccountCompanionClient::GetSummonLimit()
{
    return GetCurrentSummonLimit();
}

int AccountCompanionClient::GetPlanTier()
{
    return std::clamp(g_planTier, 0, 4);
}

bool AccountCompanionClient::CanPlayOffline()
{
    return GetPlanTier() >= 2;
}

int AccountCompanionClient::CopyMiniPartyEntries(MiniPartyEntry* entries, int maxEntries)
{
    if (entries == nullptr || maxEntries <= 0)
    {
        return 0;
    }

    int copied = 0;
    const int summonLimit = GetCurrentSummonLimit();
    for (int slot = 0; slot < AccountCharacterList::MaxCharacters && copied < maxEntries; ++slot)
    {
        if (copied >= summonLimit)
        {
            break;
        }

        if (!g_companions[slot].Active)
        {
            continue;
        }

        const AccountCharacterList::Entry* characterEntry = AccountCharacterList::GetBySlot(slot);
        if (characterEntry == nullptr || IsCurrentHero(*characterEntry))
        {
            continue;
        }

        MiniPartyEntry& miniEntry = entries[copied];
        miniEntry = {};
        miniEntry.Slot = slot;
        miniEntry.ClientIndex = g_companions[slot].ClientIndex;
        miniEntry.OfflinePlay = g_companions[slot].OfflinePlay;
        if (miniEntry.ClientIndex < 0
            || miniEntry.ClientIndex >= MAX_CHARACTERS_CLIENT
            || !CharactersClient[miniEntry.ClientIndex].Object.Live)
        {
            miniEntry.ClientIndex = FindVisibleCharacterIndexByName(characterEntry->Name);
        }
        miniEntry.Visible = miniEntry.ClientIndex >= 0;
        miniEntry.StepHP = 10;

        if (miniEntry.Visible)
        {
            const CHARACTER* visibleCharacter = &CharactersClient[miniEntry.ClientIndex];
            miniEntry.StepHP = visibleCharacter->Object.Live && visibleCharacter->Dead == 0 ? 10 : 0;
        }

        std::wcsncpy(miniEntry.Name, characterEntry->Name, MAX_USERNAME_SIZE);
        miniEntry.Name[MAX_USERNAME_SIZE] = L'\0';
        ++copied;
    }

    return copied;
}

bool AccountCompanionClient::CanSummonMore()
{
    return CountActiveCompanions() < GetCurrentSummonLimit();
}

void AccountCompanionClient::RequestPlanInfo()
{
    SendGuardCommand(L"/guard limit");
}

bool AccountCompanionClient::Summon(int accountSlot)
{
    if (!IsValidSlot(accountSlot))
    {
        return false;
    }

    const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(accountSlot);
    if (entry == nullptr || IsCurrentHero(*entry))
    {
        return false;
    }

    if (GetCooldownSeconds(accountSlot) > 0)
    {
        return false;
    }

    if (!g_companions[accountSlot].Active && CountActiveCompanions() >= GetCurrentSummonLimit())
    {
        return false;
    }

    const bool wasActive = g_companions[accountSlot].Active;
    if (!wasActive && CountActiveCompanions() == 0)
    {
        ResetTownFormationState();
    }

    const bool sendServerNow = kSendServerGuardCommands && ShouldSendServerCallImmediately();
    if (sendServerNow)
    {
        SendGuardCommand(L"call", *entry);
    }

    g_companions[accountSlot].Active = true;
    g_companions[accountSlot].ServerActive = g_companions[accountSlot].ServerActive || sendServerNow;
    g_companions[accountSlot].OfflinePlay = g_companions[accountSlot].OfflinePlay || sendServerNow;
    g_companions[accountSlot].ClientIndex = GetCompanionClientIndex(accountSlot);
    g_guardStatuses[accountSlot].Known = true;
    g_guardStatuses[accountSlot].Active = true;
    SetGuardCooldown(g_guardStatuses[accountSlot], 0);

    if (!ShouldUseClientVisualFormation())
    {
        RestoreServerDrivenCompanion(*entry);
        DeleteLocalCompanionObject(accountSlot);
    }

    return true;
}

bool AccountCompanionClient::Store(int accountSlot)
{
    if (!IsValidSlot(accountSlot))
    {
        return false;
    }

    if (const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(accountSlot))
    {
        if (kSendServerGuardCommands)
        {
            SendGuardCommand(IsFarmPositionCommand(accountSlot) ? L"store" : L"storevisual", *entry);
        }

        RestoreServerDrivenCompanion(*entry);
    }

    DeleteLocalCompanionObject(accountSlot);
    g_companions[accountSlot] = {};
    if (IsValidSlot(accountSlot))
    {
        g_guardStatuses[accountSlot].Known = true;
        g_guardStatuses[accountSlot].Active = false;
        SetGuardCooldown(g_guardStatuses[accountSlot], 0);
    }
    return true;
}

bool AccountCompanionClient::SwitchTo(int accountSlot)
{
    if (!IsValidSlot(accountSlot))
    {
        return false;
    }

    const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(accountSlot);
    if (entry == nullptr || IsCurrentHero(*entry))
    {
        return false;
    }

    SetPendingSwitchTarget(entry->Name);
    PreserveActiveSlotsForSwitch(entry->Slot);
    if (kSendServerGuardCommands)
    {
        SendGuardCommand(L"switch", *entry);
    }

    return true;
}

bool AccountCompanionClient::SetFarmHere(int accountSlot)
{
    if (!IsValidSlot(accountSlot))
    {
        return false;
    }

    const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(accountSlot);
    if (entry == nullptr || IsCurrentHero(*entry))
    {
        return false;
    }

    SendGuardCommand(L"farmhere", *entry);
    GuardRuntimeStatus& status = g_guardStatuses[accountSlot];
    status.Known = true;
    status.Mode = CommandMode::FarmPosition;
    status.FarmMapNumber = gMapManager.WorldActive;
    status.FarmX = Hero != nullptr ? std::clamp<int>(Hero->PositionX, 0, 255) : -1;
    status.FarmY = Hero != nullptr ? std::clamp<int>(Hero->PositionY, 0, 255) : -1;
    return true;
}

bool AccountCompanionClient::SetFollowLeader(int accountSlot)
{
    if (!IsValidSlot(accountSlot))
    {
        return false;
    }

    const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(accountSlot);
    if (entry == nullptr || IsCurrentHero(*entry))
    {
        return false;
    }

    SendGuardCommand(L"follow", *entry);
    GuardRuntimeStatus& status = g_guardStatuses[accountSlot];
    status.Known = true;
    status.Mode = CommandMode::Follow;
    status.FarmMapNumber = -1;
    status.FarmX = -1;
    status.FarmY = -1;
    return true;
}

bool AccountCompanionClient::SetAutoSummon(int accountSlot, bool enabled)
{
    if (!IsValidSlot(accountSlot))
    {
        return false;
    }

    const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(accountSlot);
    if (entry == nullptr || IsCurrentHero(*entry))
    {
        return false;
    }

    SendGuardCommand3(L"auto", *entry, enabled ? L"on" : L"off");
    g_guardStatuses[accountSlot].Known = true;
    g_guardStatuses[accountSlot].AutoSummon = enabled;
    return true;
}

void AccountCompanionClient::SummonAll()
{
    if (ShouldUseClientVisualFormation())
    {
        ResetTownFormationState();
    }

    const bool sendServerNow = kSendServerGuardCommands && ShouldSendServerCallImmediately();
    const int summonLimit = GetCurrentSummonLimit();
    for (int slot = 0; slot < AccountCharacterList::MaxCharacters && CountActiveCompanions() < summonLimit; ++slot)
    {
        const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot);
        if (entry == nullptr || IsCurrentHero(*entry))
        {
            continue;
        }

        if (GetCooldownSeconds(slot) > 0)
        {
            continue;
        }

        if (sendServerNow && !g_companions[slot].ServerActive)
        {
            SendGuardCommand(L"call", *entry);
        }

        g_companions[slot].Active = true;
        g_companions[slot].ServerActive = g_companions[slot].ServerActive || sendServerNow;
        g_companions[slot].OfflinePlay = g_companions[slot].OfflinePlay || sendServerNow;
        g_companions[slot].ClientIndex = GetCompanionClientIndex(slot);
        g_guardStatuses[slot].Known = true;
        g_guardStatuses[slot].Active = true;
        SetGuardCooldown(g_guardStatuses[slot], 0);

        if (!ShouldUseClientVisualFormation())
        {
            RestoreServerDrivenCompanion(*entry);
            DeleteLocalCompanionObject(slot);
        }
    }
}

void AccountCompanionClient::StoreAll()
{
    if (kSendServerGuardCommands)
    {
        SendGuardCommand(L"/guard storeall");
    }

    ClearLocalCompanionState();
    for (auto& status : g_guardStatuses)
    {
        status.Active = false;
        SetGuardCooldown(status, 0);
    }
}

bool AccountCompanionClient::HasPendingSwitchTarget()
{
    return HasPendingSwitchTargetInternal();
}

bool AccountCompanionClient::GetPendingSwitchTarget(wchar_t* targetName, size_t targetNameLength)
{
    if (targetName == nullptr || targetNameLength == 0 || !HasPendingSwitchTargetInternal())
    {
        return false;
    }

    std::wcsncpy(targetName, g_pendingSwitchTargetName.data(), targetNameLength - 1);
    targetName[targetNameLength - 1] = L'\0';
    return targetName[0] != L'\0';
}

void AccountCompanionClient::ClearPendingSwitchTarget()
{
    g_pendingSwitchTargetName[0] = L'\0';
    g_pendingSwitchSourceSlot = -1;
    g_pendingSwitchTargetSlot = -1;
    g_switchPreservedActiveSlots.fill(false);
    g_switchPreservedServerSlots.fill(false);
    g_switchPreservedOfflineSlots.fill(false);
}

void AccountCompanionClient::ResetLocalState()
{
    if (HasPendingSwitchTargetInternal())
    {
        ResetLocalCompanionStateForPendingSwitch();
        return;
    }

    ClearLocalCompanionState();
}

bool AccountCompanionClient::RecallAllToLeader()
{
    if (Hero == nullptr || !Hero->Object.Live)
    {
        return false;
    }

    ResetTownFormationState();

    int formationIndex = 0;
    for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
    {
        if (!g_companions[slot].Active)
        {
            continue;
        }

        const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot);
        if (entry == nullptr || IsCurrentHero(*entry))
        {
            continue;
        }

        const FormationFrame frame = UpdateTownFormationFrame();
        float targetX = 0.f;
        float targetY = 0.f;
        GetTownFormationTarget(formationIndex, frame, targetX, targetY);
        if (!ShouldUseVisualForTransition(GetCompanionCharacter(slot), targetX, targetY))
        {
            ReleaseLocalVisualCompanionToServer(slot, *entry);
            ++formationIndex;
            continue;
        }

        CHARACTER* companion = CreateOrRefreshCompanion(*entry, formationIndex);
        ApplyCompanionEquipment(companion, *entry);
        HideServerDrivenCompanion(*entry);
        SnapCompanionToFormation(slot, formationIndex);
        StoreServerCompanionForCityVisual(slot, *entry, companion);
        ++formationIndex;
    }

    return formationIndex > 0;
}

AccountCompanionClient::CombatMode AccountCompanionClient::GetCombatMode()
{
    return g_combatMode;
}

void AccountCompanionClient::SetCombatMode(CombatMode mode)
{
    g_combatMode = mode;
}

AccountCompanionClient::FormationMode AccountCompanionClient::GetFormationMode()
{
    return g_formationMode;
}

void AccountCompanionClient::SetFormationMode(FormationMode mode)
{
    g_formationMode = mode;
}

bool AccountCompanionClient::ShouldSuppressServerDrivenRender(int clientIndex)
{
    if (!ShouldUseClientVisualFormation() || !ShouldHideServerDrivenGuardActors())
    {
        return false;
    }

    if (clientIndex < 0
        || clientIndex >= MAX_CHARACTERS_CLIENT
        || IsReservedCompanionClientIndex(clientIndex))
    {
        return false;
    }

    CHARACTER* character = &CharactersClient[clientIndex];
    if (character == Hero || !character->Object.Live || character->ID[0] == L'\0')
    {
        return false;
    }

    const bool heroSafeZone = Hero != nullptr
        && IsWorldPositionSafeZone(Hero->Object.Position[0], Hero->Object.Position[1]);
    const bool characterSafeZone = IsWorldPositionSafeZone(character->Object.Position[0], character->Object.Position[1]);
    if (!heroSafeZone && !characterSafeZone)
    {
        return false;
    }

    for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
    {
        if (!g_companions[slot].Active)
        {
            continue;
        }

        const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot);
        if (entry == nullptr || IsCurrentHero(*entry))
        {
            continue;
        }

        if (std::wcscmp(entry->Name, character->ID) == 0)
        {
            if (IsFarmPositionCommand(slot))
            {
                return false;
            }

            return true;
        }
    }

    return false;
}

bool AccountCompanionClient::HasAccountWideStoreForLeader(const CHARACTER* character)
{
    return character != nullptr && HasRemoteAccountWideStore(character->Key);
}

bool AccountCompanionClient::HandlePlanMessage(const wchar_t* message)
{
    static constexpr const wchar_t* kPrefix = L"#PGL1|";
    static constexpr const wchar_t* kStatusPrefix = L"#PGS1|";
    static constexpr size_t kPrefixLength = 6;
    static constexpr size_t kStatusPrefixLength = 6;

    if (message == nullptr)
    {
        return false;
    }

    if (std::wcsncmp(message, kStatusPrefix, kStatusPrefixLength) == 0)
    {
        std::vector<std::wstring> fields;
        std::wstring current;
        for (const wchar_t* cursor = message + kStatusPrefixLength; *cursor != L'\0'; ++cursor)
        {
            if (*cursor == L'|')
            {
                fields.push_back(current);
                current.clear();
                continue;
            }

            current.push_back(*cursor);
        }

        fields.push_back(current);
        if (fields.size() < 8)
        {
            return true;
        }

        const int slot = AccountCharacterList::FindSlotByName(fields[0].c_str());
        if (!IsValidSlot(slot))
        {
            return true;
        }

        wchar_t* end = nullptr;
        const int active = static_cast<int>(std::wcstol(fields[1].c_str(), &end, 10));
        const int cooldown = static_cast<int>(std::wcstol(fields[2].c_str(), &end, 10));
        const int autoSummon = static_cast<int>(std::wcstol(fields[3].c_str(), &end, 10));
        const int farmMap = static_cast<int>(std::wcstol(fields[5].c_str(), &end, 10));
        const int farmX = static_cast<int>(std::wcstol(fields[6].c_str(), &end, 10));
        const int farmY = static_cast<int>(std::wcstol(fields[7].c_str(), &end, 10));

        GuardRuntimeStatus& status = g_guardStatuses[slot];
        status.Known = true;
        status.Active = active != 0;
        SetGuardCooldown(status, cooldown);
        status.AutoSummon = autoSummon != 0;
        status.Mode = ParseCommandMode(fields[4]);
        status.FarmMapNumber = farmMap;
        status.FarmX = farmX;
        status.FarmY = farmY;

        CompanionState& companionState = g_companions[slot];
        if (!status.Active && companionState.CityStoreRequestPending)
        {
            companionState.ServerActive = false;
            companionState.OfflinePlay = false;
            companionState.CityStoreRequestPending = false;
            companionState.CityStoreRequestTick = 0;
            g_ErrorReport.Write(
                L"[SentinelCity] storevisual confirmed slot=%d name=%ls\r\n",
                slot,
                fields[0].c_str());
        }
        else if (!status.Active && companionState.ServerActive)
        {
            DeleteLocalCompanionObject(slot);
            companionState = {};
        }
        else if (status.Active)
        {
            companionState.Active = true;
            companionState.ServerActive = true;
            companionState.OfflinePlay = true;
            companionState.ClientIndex = GetCompanionClientIndex(slot);
        }

        return true;
    }

    if (std::wcsncmp(message, kPrefix, kPrefixLength) != 0)
    {
        return false;
    }

    int limit = kDefaultPlanGuardLimit;
    int tier = 0;
    int active = 0;
    const int parsed = swscanf_s(message + kPrefixLength, L"%d|%d|%d", &limit, &tier, &active);
    if (parsed >= 1)
    {
        g_planGuardLimit = std::clamp(limit, 1, MaxSummonedCompanions);
    }

    if (parsed >= 2)
    {
        g_planTier = std::clamp(tier, 0, 4);
    }

    return true;
}

bool AccountCompanionClient::HandleVisualLinkMessage(int companionKey, const wchar_t* message)
{
    static constexpr const wchar_t* kPrefix = L"#PGV1|";
    static constexpr size_t kPrefixLength = 6;

    if (companionKey <= 0 || message == nullptr || std::wcsncmp(message, kPrefix, kPrefixLength) != 0)
    {
        return false;
    }

    int ownerKey = -1;
    int formationIndex = 0;
    int active = 1;
    int ownerHasWebStore = 0;
    const int parsed = swscanf_s(message + kPrefixLength, L"%d|%d|%d|%d", &ownerKey, &formationIndex, &active, &ownerHasWebStore);
    if (parsed < 2 || ownerKey <= 0)
    {
        g_remoteCompanions.erase(companionKey);
        return true;
    }

    if (active == 0)
    {
        g_remoteCompanions.erase(companionKey);
        return true;
    }

    RemoteCompanionVisualState& state = g_remoteCompanions[companionKey];
    if (state.OwnerKey != ownerKey)
    {
        state.Formation = {};
    }

    state.OwnerKey = ownerKey;
    state.FormationIndex = std::clamp(formationIndex, 0, MaxSummonedCompanions - 1);
    state.OwnerHasWebStore = parsed >= 4 && ownerHasWebStore != 0;
    return true;
}

bool AccountCompanionClient::HandleVisualPublicMessage(const wchar_t* senderName, const wchar_t* message)
{
    static constexpr const wchar_t* kPrefix = L"#PGC1|";
    static constexpr size_t kPrefixLength = 6;

    if (senderName == nullptr
        || senderName[0] == L'\0'
        || message == nullptr
        || std::wcsncmp(message, kPrefix, kPrefixLength) != 0)
    {
        return false;
    }

    const int ownerIndex = FindVisibleCharacterIndexByName(senderName);
    if (ownerIndex < 0 || ownerIndex >= MAX_CHARACTERS_CLIENT)
    {
        return true;
    }

    CHARACTER* owner = &CharactersClient[ownerIndex];
    if (owner == Hero || !owner->Object.Live || !IsWorldPositionSafeZone(owner->Object.Position[0], owner->Object.Position[1]))
    {
        return true;
    }

    std::vector<std::wstring> fields;
    std::wstring current;
    for (const wchar_t* cursor = message + kPrefixLength; *cursor != L'\0'; ++cursor)
    {
        if (*cursor == L'|')
        {
            fields.push_back(current);
            current.clear();
            continue;
        }

        current.push_back(*cursor);
    }

    fields.push_back(current);
    if (fields.size() < 6)
    {
        return true;
    }

    wchar_t* end = nullptr;
    const int formationIndex = static_cast<int>(std::wcstol(fields[0].c_str(), &end, 10));
    if (end == fields[0].c_str())
    {
        return true;
    }

    const int classValue = static_cast<int>(std::wcstol(fields[1].c_str(), &end, 10));
    if (end == fields[1].c_str())
    {
        return true;
    }

    const int level = static_cast<int>(std::wcstol(fields[2].c_str(), &end, 10));
    if (end == fields[2].c_str())
    {
        return true;
    }

    const int mode = static_cast<int>(std::wcstol(fields[3].c_str(), &end, 10));
    if (end == fields[3].c_str())
    {
        return true;
    }

    BYTE equipment[AccountCharacterList::EquipmentLength] = {};
    if (!DecodeHexBytes(fields[4], equipment, AccountCharacterList::EquipmentLength))
    {
        return true;
    }

    if (fields[5].empty())
    {
        return true;
    }

    const int clampedFormationIndex = std::clamp(formationIndex, 0, MaxSummonedCompanions - 1);
    PublicCompanionVisualState& state = g_publicVisualCompanions[MakePublicVisualStateKey(owner->Key, clampedFormationIndex)];
    if (state.OwnerKey != owner->Key || state.FormationIndex != clampedFormationIndex)
    {
        DeletePublicVisualObject(state);
        state.Formation = {};
    }

    state.OwnerKey = owner->Key;
    state.FormationIndex = clampedFormationIndex;
    state.Class = static_cast<CLASS_TYPE>(classValue);
    state.Level = std::max(0, level);
    state.Flags = static_cast<BYTE>(mode & 0xFF);
    state.UsesExtendedEquipment = (mode & 0x100) != 0;
    std::copy_n(equipment, AccountCharacterList::EquipmentLength, state.Equipment);
    std::wcsncpy(state.Name, fields[5].c_str(), MAX_USERNAME_SIZE);
    state.Name[MAX_USERNAME_SIZE] = L'\0';
    state.LastSeenTick = GetTickCount64();
    state.AppearanceDirty = true;
    return true;
}

void AccountCompanionClient::Tick()
{
    if (!ShouldUseClientVisualFormation())
    {
        g_wasHeroInSafeZone = false;
        for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
        {
            if (const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot))
            {
                RestoreServerDrivenCompanion(*entry);
            }

            DeleteLocalCompanionObject(slot);
        }

        ClearPublicVisualCompanions();
        return;
    }

    if (Hero == nullptr || !Hero->Object.Live)
    {
        g_planGuardLimit = kDefaultPlanGuardLimit;
        g_planTier = 0;
        g_requestedPlanInfo = false;
        g_wasHeroInSafeZone = false;
        return;
    }

    const bool heroInSafeZone = IsTownFormationMap();
    if (heroInSafeZone && !g_wasHeroInSafeZone)
    {
        ResetTownFormationState();
    }

    g_wasHeroInSafeZone = heroInSafeZone;

    if (!g_requestedPlanInfo)
    {
        g_requestedPlanInfo = true;
        RequestPlanInfo();
    }

    TickRemoteVisualCompanions();
    TickPublicVisualCompanions();
    UpdateWebStoreLeaderTitles();

    const FormationFrame frame = UpdateTownFormationFrame();
    int formationIndex = 0;
    const int summonLimit = GetCurrentSummonLimit();
    for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
    {
        if (!g_companions[slot].Active)
        {
            continue;
        }

        const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot);
        if (entry == nullptr || IsCurrentHero(*entry))
        {
            DeleteLocalCompanionObject(slot);
            g_companions[slot] = {};
            continue;
        }

        if (KeepFarmPositionCompanionServerDriven(slot, *entry))
        {
            continue;
        }

        if (formationIndex >= summonLimit)
        {
            if (g_companions[slot].ServerActive)
            {
                SendGuardCommand(L"store", *entry);
            }

            RestoreServerDrivenCompanion(*entry);
            DeleteLocalCompanionObject(slot);
            g_companions[slot] = {};
            continue;
        }

        float targetX = 0.f;
        float targetY = 0.f;
        GetTownFormationTarget(formationIndex, frame, targetX, targetY);

        CHARACTER* companion = GetCompanionCharacter(slot);
        if (!ShouldUseVisualForTransition(companion, targetX, targetY))
        {
            ReleaseLocalVisualCompanionToServer(slot, *entry);
            ++formationIndex;
            continue;
        }

        HideServerDrivenCompanion(*entry);

        if (companion == nullptr)
        {
            companion = CreateOrRefreshCompanion(*entry, formationIndex);
            ApplyCompanionEquipment(companion, *entry);
        }

        if (g_formationMode != FormationMode::HoldPosition)
        {
            MoveCompanionToTarget(slot, targetX, targetY, kTownFollowSmoothing, false);
        }
        else
        {
            if (companion != nullptr)
            {
                SetPlayerStop(companion);
            }
        }

        StoreServerCompanionForCityVisual(slot, *entry, companion);
        ++formationIndex;
    }

    PublishLocalCityVisuals();
}

bool AccountCompanionClient::HandleExternalAction(int action, int accountSlot)
{
    switch (action)
    {
    case ActionCall:
        return Summon(accountSlot);
    case ActionStore:
        return Store(accountSlot);
    case ActionCallAll:
        SummonAll();
        return true;
    case ActionStoreAll:
        StoreAll();
        return true;
    case ActionSwitch:
        return SwitchTo(accountSlot);
    default:
        return false;
    }
}
