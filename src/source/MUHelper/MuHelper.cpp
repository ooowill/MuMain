#include "stdafx.h"
#include "GameLogic/Combat/SkillExecution.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cwctype>
#include <initializer_list>
#include <climits>

#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/PlayerActionState.h"
#include "UI/NewUI/NewUISystem.h"
#include "Core/Utilities/Log/muConsoleDebug.h"
#include "Character/CharacterManager.h"
#include "GameLogic/Items/CSItemOption.h"
#include "GameLogic/Skills/SkillManager.h"
#include "GameLogic/Social/PartyManager.h"
#include "World/MapInfra/MapManager.h"
#include "Network/Server/WSclient.h"

#include "MuHelper.h"

constexpr int MAX_ACTIONABLE_DISTANCE = 10;
constexpr int DEFAULT_DURABILITY_THRESHOLD = 50;
constexpr int DEFAULT_HUNTING_RANGE = 6;
constexpr int DEFAULT_OBTAINING_RANGE = 8;
constexpr int MAX_AUTO_STAT_VALUE = 32767;
constexpr int MAX_AUTO_STAT_SEND_PER_TICK = 5;

SpinLock _targetsLock;
SpinLock _itemsLock;

// Movement/target globals are defined in ZzzInterface.cpp.
extern MovementSkill g_MovementSkill;
extern int SelectedCharacter;
extern int TargetX;
extern int TargetY;

namespace MUHelper
{
	MovementSkill& g_MovementSkill = ::g_MovementSkill;
	int& SelectedCharacter = ::SelectedCharacter;
	int& TargetX = ::TargetX;
	int& TargetY = ::TargetY;

    CMuHelper g_MuHelper;

    int CountExcellentOptions(const ITEM* pItem)
    {
        if (pItem == nullptr)
        {
            return 0;
        }

        int count = 0;
        BYTE flags = pItem->ExcellentFlags & 0x3F;
        while (flags != 0)
        {
            count += flags & 1;
            flags >>= 1;
        }
        return count;
    }

    bool HasMinimumOptionLevel(const ITEM* pItem, int minimumOptionLevel)
    {
        if (minimumOptionLevel <= 0)
        {
            return true;
        }

        if (pItem == nullptr)
        {
            return false;
        }

        return std::max<int>(pItem->OptionLevel, CountExcellentOptions(pItem)) >= minimumOptionLevel;
    }

    std::wstring ToLower(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch)
        {
            return static_cast<wchar_t>(std::towlower(ch));
        });
        return value;
    }

    bool ContainsAny(const std::wstring& text, std::initializer_list<const wchar_t*> terms)
    {
        for (const wchar_t* term : terms)
        {
            if (term != nullptr && text.find(term) != std::wstring::npos)
            {
                return true;
            }
        }
        return false;
    }

    bool IsEventOrConsumableItem(ITEM* pItem)
    {
        if (pItem == nullptr)
        {
            return false;
        }

        if (pItem->Type >= ITEM_POTION)
        {
            return true;
        }

        const std::wstring name = ToLower(GetItemDisplayName(pItem));
        return ContainsAny(
            name,
            {
                L"box", L"key", L"ticket", L"medal", L"heart", L"ribbon",
                L"invitation", L"fragment", L"scroll", L"potion", L"cherry",
                L"chocolate", L"pumpkin", L"sign", L"seal", L"mix", L"event"
            });
    }

    bool IsEquipmentOrClassItem(ITEM* pItem)
    {
        if (pItem == nullptr || pItem->Type < 0)
        {
            return false;
        }

        return IsRequireClassRenderItem(pItem->Type)
            || (pItem->Type >= ITEM_SWORD && pItem->Type < ITEM_POTION);
    }

    bool IsUsableByCurrentHero(ITEM* pItem)
    {
        if (pItem == nullptr || Hero == nullptr || CharacterAttribute == nullptr)
        {
            return true;
        }

        if (!IsEquipmentOrClassItem(pItem))
        {
            return true;
        }

        return IsRequireEquipItem(pItem);
    }

    void CALLBACK CMuHelper::TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
    {
        g_MuHelper.WorkLoop(hwnd, uMsg, idEvent, dwTime);
    }

    void CMuHelper::Save(const ConfigData& config)
    {
        m_config = config;
        m_config.bAutoDistributePoints = true;

        PRECEIVE_MUHELPER_DATA netData;
        ConfigDataSerDe::Serialize(m_config, netData);

        SocketClient->ToGameServer()->SendMuHelperSaveDataRequest(reinterpret_cast<BYTE*>(&netData), sizeof(netData));
    }

    void CMuHelper::Load(const ConfigData& config)
    {
        m_config = config;
        m_config.bAutoDistributePoints = true;
    }

    ConfigData CMuHelper::GetConfig() const {
        return m_config;
    }

    bool CMuHelper::CanRunInCurrentArea() const
    {
        return Hero != nullptr
            && (!Hero->SafeZone || IsSelectedPvpServerForAutoAttack());
    }

    bool CMuHelper::IsAttackableTarget(CHARACTER* pTarget) const
    {
        if (Hero == nullptr
            || pTarget == nullptr
            || pTarget == Hero
            || pTarget->Dead > 0
            || !pTarget->Object.Live)
        {
            return false;
        }

        if (IsMonster(pTarget))
        {
            return true;
        }

        const int iIndex = FindCharacterIndex(pTarget->Key);
        if (iIndex == MAX_CHARACTERS_CLIENT)
        {
            return false;
        }

        return IsPvpServerAutoAttackTarget(pTarget, iIndex);
    }

    void CMuHelper::Toggle()
    {
        if (m_bActive)
        {
            m_bStartRequested = false;
            TriggerStop();

            // Stop the client-driven bot immediately instead of waiting for the
            // server's status reply. After an auto-reconnect the server's new
            // session doesn't have the helper marked active, so it never replies
            // and the bot would otherwise keep running with no way to stop it.
            Stop();
        }
        else
        {
            if (!CanRunInCurrentArea())
            {
                ResetSessionState(true);
                return;
            }

            m_bStartRequested = true;
            TriggerStart();
            Start();
        }
    }

    void CMuHelper::TriggerStart()
    {
        auto* gameServer = SocketClient != nullptr ? SocketClient->ToGameServer() : nullptr;
        if (gameServer != nullptr)
        {
            gameServer->SendMuHelperStatusChangeRequest(0);
        }
    }

    void CMuHelper::TriggerStop()
    {
        auto* gameServer = SocketClient != nullptr ? SocketClient->ToGameServer() : nullptr;
        if (gameServer != nullptr)
        {
            gameServer->SendMuHelperStatusChangeRequest(1);
        }
    }

    void CMuHelper::ResetSessionState(bool notifyServer)
    {
        m_bStartRequested = false;
        Stop();

        if (notifyServer)
        {
            TriggerStop();
        }
    }

    void CMuHelper::ResumeAfterReconnect()
    {
        if (!CanRunInCurrentArea())
        {
            ResetSessionState(true);
            return;
        }

        m_bStartRequested = true;
        TriggerStart();
        Start();
    }

    bool CMuHelper::ShouldAcceptServerStart() const
    {
        return m_bStartRequested
            && CanRunInCurrentArea();
    }

    void CMuHelper::Start()
    {
        if (!CanRunInCurrentArea())
        {
            ResetSessionState(true);
            return;
        }

        if (m_bActive)
        {
            return;
        }

        m_iTotalCost = 0;
        m_iComboState = 0;
        m_iCurrentBuffIndex = 0;
        m_iCurrentBuffPartyIndex = 0;
        m_iCurrentHealPartyIndex = 0;
        m_iCurrentTarget = -1;
        m_iCurrentSkill = (ActionSkillType)m_config.aiSkill[0];
        m_iCurrentItem = MAX_ITEMS;
        m_posOriginal = { Hero->PositionX, Hero->PositionY };

        const int huntingRange = m_config.iHuntingRange > 0 ? m_config.iHuntingRange : DEFAULT_HUNTING_RANGE;
        const int obtainingRange = m_config.iObtainingRange > 0 ? m_config.iObtainingRange : DEFAULT_OBTAINING_RANGE;
        m_iHuntingDistance = ComputeDistanceByRange(huntingRange);
        m_iObtainingDistance = ComputeDistanceByRange(obtainingRange);

        m_iSecondsElapsed = 0;
        m_iSecondsAway = 0;

        m_bTimerActivatedBuffOngoing = false;
        m_bPetActivated = false;

        m_iLoopCounter = 0;
        m_iAutoPointLoopCounter = 0;

        DeleteAllTargets();
        m_bActive = true;
        SeedVisibleTargets();
        g_ConsoleDebug->Write(MCD_NORMAL, L"[MU Helper] Started");
    }

    void CMuHelper::Stop()
    {
        m_bActive = false;
        DeleteAllTargets();
        g_ConsoleDebug->Write(MCD_NORMAL, L"[MU Helper] Stopped");
    }

    void CMuHelper::WorkLoop(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
    {
        if (!m_bActive)
        {
            return;
        }

        if (!CanRunInCurrentArea())
        {
            ResetSessionState(true);
            return;
        }

        Work();
        AutoDistributeStatPoints();

        if (m_iLoopCounter++ == 4)
        {
            m_iSecondsElapsed++;

            if (ComputeDistanceBetween({ Hero->PositionX, Hero->PositionY }, m_posOriginal) > 1)
            {
                m_iSecondsAway++;
            }
            else
            {
                m_iSecondsAway = 0;
            }

            m_iLoopCounter = 0;
        }
    }

    void CMuHelper::Work()
    {
        try
        {
            if (!ActivatePet())
            {
                return;
            }

            if (!Buff())
            {
                return;
            }

            if (!RecoverHealth())
            {
                return;
            }

            if (!ObtainItem())
            {
                return;
            }

            if (!Regroup())
            {
                return;
            }

            Attack();

            RepairEquipments();
        }
        catch (...)
        {
            g_ConsoleDebug->Write(MCD_NORMAL, L"[MU Helper] Exception occurred. Ignoring...");
        }
    }

    void CMuHelper::AddTarget(int iTargetId, bool bIsAttacking)
    {
        if (!m_bActive)
        {
            return;
        }

        CHARACTER* pTarget = FindCharacterByKey(iTargetId);
        if (!pTarget || pTarget == Hero)
        {
            return;
        }

        if (!IsAttackableTarget(pTarget))
        {
            return;
        }

        int iDistance = ComputeDistanceFromTarget(pTarget);

        if ((iDistance <= m_iHuntingDistance)
            || (bIsAttacking && m_config.bLongRangeCounterAttack))
        {
            _targetsLock.lock();

            m_setTargets.insert(iTargetId);

            if (bIsAttacking)
            {
                m_setTargetsAttacking.insert(iTargetId);
            }

            _targetsLock.unlock();
        }

        if (m_config.bUseSelfDefense)
        {
            m_iCurrentTarget = iTargetId;
        }
    }

    void CMuHelper::DeleteTarget(int iTargetId)
    {
        _targetsLock.lock();

        m_setTargets.erase(iTargetId);
        m_setTargetsAttacking.erase(iTargetId);

        _targetsLock.unlock();

        if (iTargetId == m_iCurrentTarget)
        {
            m_iCurrentTarget = -1;
        }
    }

    void CMuHelper::DeleteAllTargets()
    {
        _targetsLock.lock();

        m_setTargets.clear();
        m_setTargetsAttacking.clear();

        _targetsLock.unlock();
    }

    int CMuHelper::ComputeDistanceByRange(int iRange)
    {
        return ComputeDistanceBetween({ 0, 0 }, { iRange, iRange });
    }

    int CMuHelper::ComputeDistanceFromTarget(CHARACTER* pTarget)
    {
        const POINT posHero = { Hero->PositionX, Hero->PositionY };

        const POINT posCurrent = { pTarget->PositionX, pTarget->PositionY };
        const POINT posNext    = { pTarget->TargetX,   pTarget->TargetY };

        return std::min(
            ComputeDistanceBetween(posHero, posCurrent),
            ComputeDistanceBetween(posHero, posNext)
        );
    }

    int CMuHelper::ComputeDistanceBetween(POINT posA, POINT posB)
    {
        int iDx = posA.x - posB.x;
        int iDy = posA.y - posB.y;

        return static_cast<int>(std::ceil(std::sqrt(iDx * iDx + iDy * iDy)));
    }

    int CMuHelper::GetNearestTarget()
    {
        int iClosestMonsterId = -1;
        int iMinDistance = m_iHuntingDistance;
        std::set<int> setTargets;
        {
            _targetsLock.lock();
            setTargets = m_setTargets;
            _targetsLock.unlock();
        }

        for (const int& iMonsterId : setTargets)
        {
            int iIndex = FindCharacterIndex(iMonsterId);
            if (iIndex == MAX_CHARACTERS_CLIENT)
            {
                DeleteTarget(iMonsterId);
                continue;
            }

            CHARACTER* pTarget = &CharactersClient[iIndex];

            if (!IsAttackableTarget(pTarget))
            {
                DeleteTarget(iMonsterId);
                continue;
            }

            int iDistance = ComputeDistanceFromTarget(pTarget);
            if (iDistance <= iMinDistance)
            {
                iMinDistance = iDistance;
                iClosestMonsterId = iMonsterId;
            }
        }

        return iClosestMonsterId;
    }

    int CMuHelper::GetFarthestAttackingTarget()
    {
        int iFarthestMonsterId = -1;
        int iMaxDistance = -1;

        std::set<int> setTargets;
        {
            _targetsLock.lock();
            setTargets = m_setTargetsAttacking;
            _targetsLock.unlock();
        }

        for (const int& iMonsterId : setTargets)
        {
            int iIndex = FindCharacterIndex(iMonsterId);
            if (iIndex == MAX_CHARACTERS_CLIENT)
            {
                DeleteTarget(iMonsterId);
                continue;
            }

            CHARACTER* pTarget = &CharactersClient[iIndex];

            if (!IsAttackableTarget(pTarget))
            {
                DeleteTarget(iMonsterId);
                continue;
            }

            int iDistance = ComputeDistanceFromTarget(pTarget);
            if (iDistance > iMaxDistance)
            {
                iMaxDistance = iDistance;
                iFarthestMonsterId = iMonsterId;
            }
        }

        return iFarthestMonsterId;
    }

    void CMuHelper::SeedVisibleTargets()
    {
        if (!m_bActive || Hero == nullptr)
        {
            return;
        }

        for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
        {
            CHARACTER* pTarget = &CharactersClient[i];
            if (!IsAttackableTarget(pTarget))
            {
                continue;
            }

            AddTarget(pTarget->Key, false);
        }
    }

    void CMuHelper::CleanupTargets()
    {
        std::set<int> setTargets;
        {
            _targetsLock.lock();
            setTargets = m_setTargets;
            _targetsLock.unlock();
        }

        for (const int& iMonsterId : setTargets)
        {
            int iIndex = FindCharacterIndex(iMonsterId);
            if (iIndex == MAX_CHARACTERS_CLIENT)
            {
                DeleteTarget(iMonsterId);
                continue;
            }

            CHARACTER* pTarget = &CharactersClient[iIndex];
            if (!IsAttackableTarget(pTarget))
            {
                DeleteTarget(iMonsterId);
            }
        }
    }

    int CMuHelper::ActivatePet()
    {
        if (!m_config.bUseDarkRaven)
        {
            return 1;
        }

        if (m_bPetActivated)
        {
            return 1;
        }

        if (m_config.iDarkRavenMode == PET_ATTACK_CEASE)
        {
            SocketClient->ToGameServer()->SendPetCommandRequest(PetType::DarkRaven, PetCommandMode::Normal, 0xFFFF);
        }
        else if (m_config.iDarkRavenMode == PET_ATTACK_AUTO)
        {
            SocketClient->ToGameServer()->SendPetCommandRequest(PetType::DarkRaven, PetCommandMode::AttackRandom, 0xFFFF);
        }
        else if (m_config.iDarkRavenMode == PET_ATTACK_TOGETHER)
        {
            SocketClient->ToGameServer()->SendPetCommandRequest(PetType::DarkRaven, PetCommandMode::AttackWithOwner, 0xFFFF);
        }

        m_bPetActivated = true;
        return 1;
    }

    int CMuHelper::Buff()
    {
        if (!HasAssignedBuffSkill())
        {
            return 1;
        }

        if (m_config.bSupportParty && g_pPartyManager->IsPartyActive())
        {
            PARTY_t* pMember = &Party[m_iCurrentBuffPartyIndex];
            CHARACTER* pChar = g_pPartyManager->GetPartyMemberChar(pMember);

            if (pChar != NULL
                && pMember->Map == gMapManager.WorldActive
                && ComputeDistanceFromTarget(pChar) <= MAX_ACTIONABLE_DISTANCE)
            {
                if (!m_config.bBuffDurationParty
                    && m_config.iBuffCastInterval != 0
                    && m_iSecondsElapsed % m_config.iBuffCastInterval == 0)
                {
                    m_bTimerActivatedBuffOngoing = true;
                }

                if (!BuffTarget(pChar, (ActionSkillType)m_config.aiBuff[m_iCurrentBuffIndex]))
                {
                    return 0;
                }
            }

            m_iCurrentBuffPartyIndex = (m_iCurrentBuffPartyIndex + 1) % (sizeof(Party) / sizeof(Party[0]));
        }
        else
        {
            if (!m_config.bBuffDuration
                && m_config.iBuffCastInterval != 0
                && m_iSecondsElapsed % m_config.iBuffCastInterval == 0)
            {
                m_bTimerActivatedBuffOngoing = true;
            }

            if (!BuffTarget(Hero, (ActionSkillType)m_config.aiBuff[m_iCurrentBuffIndex]))
            {
                return 0;
            }
        }

        if (m_iCurrentBuffPartyIndex == 0)
        {
            m_iCurrentBuffIndex = (m_iCurrentBuffIndex + 1) % m_config.aiBuff.size();

            // Reaching this branch means everyone's been buffed, 
            // so we're resetting the timer activated buff flag
            if (m_iCurrentBuffIndex == 0)
            {
                m_bTimerActivatedBuffOngoing = false;
            }
        }

        return 1;
    }

    int CMuHelper::BuffTarget(CHARACTER* pTargetChar, ActionSkillType iBuffSkill)
    {
        // TODO: List other buffs here
        OBJECT* obj = &pTargetChar->Object;

        auto CastIfMissing = [&](bool bBuffActive, bool bTimerRespected, bool bNeedsTarget) -> int
        {
            if (!bBuffActive || (bTimerRespected && m_bTimerActivatedBuffOngoing))
                return SimulateSkill(iBuffSkill, bNeedsTarget, pTargetChar->Key);
            return 1;
        };

        switch (iBuffSkill)
        {
        case AT_SKILL_ATTACK:
        case AT_SKILL_ATTACK_STR:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Attack), true, true);

        case AT_SKILL_DEFENSE:
        case AT_SKILL_DEFENSE_STR:
        case AT_SKILL_DEFENSE_MASTERY:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Defense), true, true);

        case AT_SKILL_INFINITY_ARROW:
        case AT_SKILL_INFINITY_ARROW_STR:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_InfinityArrow), false, false);

        case AT_SKILL_SOUL_BARRIER:
        case AT_SKILL_SOUL_BARRIER_STR:
        case AT_SKILL_SOUL_BARRIER_PROFICIENCY:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_WizDefense), true, true);

        case AT_SKILL_SWELL_LIFE:
        case AT_SKILL_SWELL_LIFE_STR:
        case AT_SKILL_SWELL_LIFE_PROFICIENCY:
            if (m_iComboState == 2)
            {
                return 1;
            }
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Life), true, false);

        case AT_SKILL_EXPANSION_OF_WIZARDRY:
        case AT_SKILL_EXPANSION_OF_WIZARDRY_STR:
        case AT_SKILL_EXPANSION_OF_WIZARDRY_MASTERY:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_SwellOfMagicPower), false, false);

        case AT_SKILL_ADD_CRITICAL:
        case AT_SKILL_ADD_CRITICAL_STR1:
        case AT_SKILL_ADD_CRITICAL_STR2:
        case AT_SKILL_ADD_CRITICAL_STR3:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_AddCriticalDamage), false, false);

        case AT_SKILL_ALICE_BERSERKER:
        case AT_SKILL_ALICE_BERSERKER_STR:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Berserker), false, false);

        case AT_SKILL_ALICE_THORNS:
            return CastIfMissing(g_isCharacterBuff(obj, eBuff_Thorns), false, false);

        default:
            return 1;
        }
    }

    int CMuHelper::ConsumePotion()
    {
        int64_t iLife = CharacterAttribute->Life;
        int64_t iLifeMax = CharacterAttribute->LifeMax;

        if (m_config.bUseHealPotion && iLifeMax > 0 && iLife > 0)
        {
            int64_t iRemaining = (iLife * 100 + iLifeMax - 1) / iLifeMax;
            if (iRemaining <= m_config.iPotionThreshold)
            {
                int iPotionIndex = g_pMyInventory->FindHealingItemIndex();
                if (iPotionIndex != -1)
                {
                    SendRequestUse(iPotionIndex, 0);
                }
            }
        }

        return 1;
    }

    int CMuHelper::RecoverHealth()
    {
        if (!Heal())
        {
            return 0;
        }
        
        if (!DrainLife())
        {
            return 0;
        }

        if (!ConsumePotion())
        {
            return 0;
        }

        return 1;
    }

    int CMuHelper::Heal()
    {
        if (!m_config.bAutoHeal)
        {
            return 1;
        }

        auto iHealingSkill = GetHealingSkill();
        if (iHealingSkill == AT_SKILL_UNDEFINED)
        {
            return 1;
        }

        if (m_config.bAutoHealParty && g_pPartyManager->IsPartyActive())
        {
            PARTY_t* pMember = &Party[m_iCurrentHealPartyIndex];
            CHARACTER* pChar = g_pPartyManager->GetPartyMemberChar(pMember);
            int iHealResult = 1;

            if (pChar != NULL)
            {
                if (pChar == Hero)
                {
                    iHealResult = HealSelf(iHealingSkill);
                }
                else if (pMember->Map == gMapManager.WorldActive
                    && pMember->stepHP * 10 <= m_config.iHealPartyThreshold
                    && ComputeDistanceFromTarget(pChar) <= MAX_ACTIONABLE_DISTANCE)
                {
                    iHealResult = SimulateSkill(iHealingSkill, true, pChar->Key);
                }
            }

            m_iCurrentHealPartyIndex = (m_iCurrentHealPartyIndex + 1) % (sizeof(Party) / sizeof(Party[0]));

            return iHealResult;
        }
        else
        {
            return HealSelf(iHealingSkill);
        }

        return 1;
    }

    int CMuHelper::HealSelf(ActionSkillType iHealingSkill)
    {
        int64_t iLife = CharacterAttribute->Life;
        int64_t iLifeMax = CharacterAttribute->LifeMax;
        int64_t iRemaining = (iLife * 100 + iLifeMax - 1) / iLifeMax;

        if (iRemaining <= m_config.iHealThreshold)
        {
            return SimulateSkill(iHealingSkill, true, HeroKey);
        }

        return 1;
    }

    int CMuHelper::DrainLife()
    {
        if (!m_config.bUseDrainLife)
        {
            return 1;
        }

        auto iDrainLife = GetDrainLifeSkill();
        if (iDrainLife == AT_SKILL_UNDEFINED)
        {
            return 1;
        }

        int64_t iLife = CharacterAttribute->Life;
        int64_t iLifeMax = CharacterAttribute->LifeMax;
        int64_t iRemaining = (iLife * 100 + iLifeMax - 1) / iLifeMax;

        if (iRemaining <= m_config.iHealThreshold)
        {
            m_iCurrentTarget = GetNearestTarget();
            if (m_iCurrentTarget != -1)
            {
                return SimulateSkill(iDrainLife, true, m_iCurrentTarget);
            }
        }

        return 1;
    }

    int CMuHelper::RepairEquipments()
    {
        if (m_config.bRepairItem)
        {
            for (int i = 0; i < MAX_EQUIPMENT; i++)
            {
                ITEM* pItem = &CharacterMachine->Equipment[i];
                if (!pItem || pItem->Type == -1)
                {
                    continue;
                }

                ITEM_ATTRIBUTE* pAttr = &ItemAttribute[pItem->Type];
                if (!pAttr)
                {
                    continue;
                }

                int iLevel = pItem->Level;
                int iDurability = pItem->Durability;
                int iMaxDurability = CalcMaxDurability(pItem, pAttr, iLevel);

                int64_t iHealth = (iDurability * 100 + iMaxDurability - 1) / iMaxDurability;

                if (iHealth <= DEFAULT_DURABILITY_THRESHOLD)
                {
                    int64_t iGoldCost = CalcSelfRepairCost(ItemValue(pItem, 2), iDurability, iMaxDurability, pItem->Type);
                    if (iGoldCost <= CharacterMachine->Gold)
                    {
                        SocketClient->ToGameServer()->SendRepairItemRequest(i, 1);
                    }
                }
            }
        }

        return 1;
    }

    int CMuHelper::Attack()
    {
        if (m_iCurrentTarget == -1)
        {
            CleanupTargets();

            bool hasTargets = false;
            {
                _targetsLock.lock();
                hasTargets = !m_setTargets.empty();
                _targetsLock.unlock();
            }

            if (!hasTargets)
            {
                SeedVisibleTargets();
            }

            if (!m_setTargets.empty())
            {
                if (m_config.bLongRangeCounterAttack)
                {
                    m_iCurrentTarget = GetFarthestAttackingTarget();
                }
                
                if (m_iCurrentTarget == -1)
                {
                    m_iCurrentTarget = GetNearestTarget();
                }
            }
            else
            {
                m_iComboState = 0;
                return 0;
            }
        }

        if (m_config.bUseCombo)
        {
            const int comboResult = SimulateComboAttack();
            if (comboResult != 0 || Hero->Movement)
            {
                return comboResult;
            }
        }

        m_iCurrentSkill = SelectAttackSkill();
        if (m_iCurrentSkill > AT_SKILL_UNDEFINED)
        {
            const int skillResult = SimulateAttack(m_iCurrentSkill);
            if (skillResult != 0 || Hero->Movement)
            {
                return skillResult;
            }
        }

        if (m_config.bFallbackBasicAttack)
        {
            if (!Hero->Movement)
            {
                return SimulateBasicAttack(m_iCurrentTarget);
            }
        }

        return 1;
    }

    bool CMuHelper::IsAttackSkillUsable(ActionSkillType iSkill) const
    {
        if (Hero == nullptr
            || CharacterAttribute == nullptr
            || iSkill <= AT_SKILL_UNDEFINED
            || iSkill >= MAX_SKILLS)
        {
            return false;
        }

        const int iSkillIndex = g_pSkillList->GetSkillIndex(iSkill);
        if (iSkillIndex < 0 || iSkillIndex >= MAX_MAGIC)
        {
            return false;
        }

        if (CharacterAttribute->Skill[iSkillIndex] == AT_SKILL_UNDEFINED)
        {
            return false;
        }

        if (!gSkillManager.AreSkillAttributeRequirementsMet(iSkill)
            || !IsCanBCSkill(iSkill)
            || !CheckSkillUseCondition(&Hero->Object, iSkill)
            || !g_csItemOption.IsNonWeaponSkillOrIsSkillEquipped(iSkill))
        {
            return false;
        }

        const bool isSittingOnPet = Hero->Helper.Type == MODEL_HORN_OF_UNIRIA
            || Hero->Helper.Type == MODEL_HORN_OF_DINORANT
            || Hero->Helper.Type == MODEL_HORN_OF_FENRIR;

        if ((iSkill == AT_SKILL_POWER_SLASH || iSkill == AT_SKILL_POWER_SLASH_STR) && isSittingOnPet)
        {
            return false;
        }

        if (iSkill == AT_SKILL_IMPALE)
        {
            if (!isSittingOnPet)
            {
                return false;
            }

            const int iTypeL = CharacterMachine->Equipment[EQUIPMENT_WEAPON_LEFT].Type;
            const int iTypeR = CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type;
            if ((iTypeL < ITEM_SPEAR || iTypeL >= ITEM_BOW) && (iTypeR < ITEM_SPEAR || iTypeR >= ITEM_BOW))
            {
                return false;
            }
        }

        const int iTypeL = CharacterMachine->Equipment[EQUIPMENT_WEAPON_LEFT].Type;
        const int iTypeR = CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type;
        const bool hasNonStaffWeapon = iTypeR != -1
            && (iTypeR < ITEM_STAFF || iTypeR >= ITEM_STAFF + MAX_ITEM_INDEX)
            && (iTypeL < ITEM_STAFF || iTypeL >= ITEM_STAFF + MAX_ITEM_INDEX);

        if (iSkill == AT_SKILL_FIRE_SLASH || iSkill == AT_SKILL_FIRE_SLASH_STR)
        {
            constexpr WORD requiredStrength = 596;
            const WORD strength = CharacterAttribute->Strength + CharacterAttribute->AddStrength;
            if (strength < requiredStrength || !hasNonStaffWeapon)
            {
                return false;
            }
        }

        if (iSkill == AT_SKILL_TWISTING_SLASH
            || iSkill == AT_SKILL_TWISTING_SLASH_STR
            || iSkill == AT_SKILL_TWISTING_SLASH_STR_MG
            || iSkill == AT_SKILL_TWISTING_SLASH_MASTERY
            || iSkill == AT_SKILL_RAGEFUL_BLOW
            || iSkill == AT_SKILL_RAGEFUL_BLOW_STR
            || iSkill == AT_SKILL_RAGEFUL_BLOW_MASTERY
            || iSkill == AT_SKILL_DEATHSTAB
            || iSkill == AT_SKILL_DEATHSTAB_STR)
        {
            if (!hasNonStaffWeapon)
            {
                return false;
            }
        }

        int iMana = 0;
        int iSkillMana = 0;
        gSkillManager.GetSkillInformation(iSkill, 1, nullptr, &iMana, nullptr, &iSkillMana);
        if (CharacterAttribute->Mana < iMana || CharacterAttribute->SkillMana < iSkillMana)
        {
            return false;
        }

        return true;
    }

    ActionSkillType CMuHelper::SelectAttackSkill()
    {
        auto isUsableSkill = [this](int iSkillId)
        {
            return IsAttackSkillUsable(static_cast<ActionSkillType>(iSkillId));
        };

        const size_t safeSize = std::min({m_config.aiSkill.size(), m_config.aiSkillCondition.size(), m_config.aiSkillInterval.size()});
        for (int i = 1; i < (int)safeSize; i++)
        {
            const int iSkillId = m_config.aiSkill[i];
            if (!isUsableSkill(iSkillId))
            {
                continue;
            }

            if ((m_config.aiSkillCondition[i] & ON_TIMER)
                && m_config.aiSkillInterval[i] != 0
                && m_iSecondsElapsed > 0
                && m_iSecondsElapsed % m_config.aiSkillInterval[i] == 0)
            {
                return (ActionSkillType)iSkillId;
            }

            if (m_config.aiSkillCondition[i] & ON_CONDITION)
            {
                int iCount = 0;
                if (m_config.aiSkillCondition[i] & ON_MOBS_NEARBY)
                {
                    iCount = (int)m_setTargets.size();
                }
                else if (m_config.aiSkillCondition[i] & ON_MOBS_ATTACKING)
                {
                    iCount = (int)m_setTargetsAttacking.size();
                }
                else
                {
                    continue;
                }

                if (((m_config.aiSkillCondition[i] & ON_MORE_THAN_TWO_MOBS)   && iCount >= 2)
                    || ((m_config.aiSkillCondition[i] & ON_MORE_THAN_THREE_MOBS) && iCount >= 3)
                    || ((m_config.aiSkillCondition[i] & ON_MORE_THAN_FOUR_MOBS)  && iCount >= 4)
                    || ((m_config.aiSkillCondition[i] & ON_MORE_THAN_FIVE_MOBS)  && iCount >= 5))
                {
                    return (ActionSkillType)iSkillId;
                }
            }
        }

        if (isUsableSkill(m_config.aiSkill[0]))
        {
            return (ActionSkillType)m_config.aiSkill[0];
        }

        return AT_SKILL_UNDEFINED;
    }

    int CMuHelper::SimulateComboAttack()
    {
        for (int i = 0; i < m_config.aiSkill.size(); i++)
        {
            if (!IsAttackSkillUsable(static_cast<ActionSkillType>(m_config.aiSkill[i])))
            {
                return 0;
            }
        }

        if (SimulateAttack((ActionSkillType)m_config.aiSkill[m_iComboState]))
        {
            m_iComboState = (m_iComboState + 1) % 3;
        }

        return 1;
    }

    // True while the hero is mid swing; gating helper actions on it makes the
    // bot's cadence follow AttackSpeed instead of the fixed helper timer, the
    // same way the manual click path gates in MoveHero (ZzzInterface.cpp).
    static bool IsHeroSwingInProgress()
    {
        return Engine::Object::IsAttackAction(Hero->Object.CurrentAction);
    }

    int CMuHelper::SimulateAttack(ActionSkillType iSkill)
    {
        if (!IsAttackSkillUsable(iSkill))
        {
            return 0;
        }

        return SimulateSkill(iSkill, true, m_iCurrentTarget);
    }

    int CMuHelper::SimulateSkill(ActionSkillType iSkill, bool bTargetRequired, int iTarget)
    {
        // Let the current swing finish before issuing another action, so the
        // cadence tracks AttackSpeed instead of the fixed helper timer.
        if (IsHeroSwingInProgress())
        {
            return 0;
        }

        const int iSkillIndex = g_pSkillList->GetSkillIndex(iSkill);
        if (iSkillIndex == -1)
        {
            return 0;
        }

        g_MovementSkill.m_iSkill = iSkillIndex;
        g_MovementSkill.m_bMagic = true;

        const float fSkillDistance = gSkillManager.GetSkillDistance(iSkill, Hero);
        const bool bSelfPositionSkill = IsSelfPositionSkill(iSkill);

        if (bTargetRequired)
        {
            if (bSelfPositionSkill)
            {
                TargetX = Hero->PositionX;
                TargetY = Hero->PositionY;

                g_MovementSkill.m_iTarget = -1;

                // Check if current target is still valid (exists and alive)
                if (iTarget != -1)
                {
                    const int iCharIndex = FindCharacterIndex(iTarget);
                    if (iCharIndex != MAX_CHARACTERS_CLIENT)
                    {
                        CHARACTER* pCurrentTarget = &CharactersClient[iCharIndex];
                        if (!IsAttackableTarget(pCurrentTarget))
                        {
                            DeleteTarget(iTarget);
                            return 0;
                        }
                    }
                    else
                    {
                        DeleteTarget(iTarget);
                        return 0;
                    }
                }
            }
            else
            {
                if (iTarget == -1)
                {
                    return 0;
                }

                const int iCharIndex = FindCharacterIndex(iTarget);
                if (iCharIndex == MAX_CHARACTERS_CLIENT)
                {
                    DeleteTarget(iTarget);
                    return 0;
                }

                CHARACTER* pTarget = &CharactersClient[iCharIndex];
                if (!IsAttackableTarget(pTarget))
                {
                    DeleteTarget(iTarget);
                    return 0;
                }

                SelectedCharacter = iCharIndex;
                g_MovementSkill.m_iTarget = iCharIndex;

                TargetX = (int)(pTarget->Object.Position[0] / TERRAIN_SCALE);
                TargetY = (int)(pTarget->Object.Position[1] / TERRAIN_SCALE);

                PATH_t tempPath;
                bool bHasPath = PathFinding2(Hero->PositionX, Hero->PositionY, TargetX, TargetY, &tempPath, m_iHuntingDistance + fSkillDistance);
                
                // Target not reachable, ignore it
                if (!bHasPath)
                {
                    DeleteTarget(iTarget);
                    return 0;
                }

                const bool bTargetNear = CheckTile(Hero, &Hero->Object, fSkillDistance);
                if (bTargetNear && !CheckWall(Hero->PositionX, Hero->PositionY, TargetX, TargetY))
                {
                    DeleteTarget(iTarget);
                    return 0;
                }

                // Target is not yet in range, move closer.
                if (!bTargetNear)
                {
                    Hero->Path.Lock.lock();

                    // Limit movement to 2 steps at a time
                    int pathNum = std::min<int>(tempPath.PathNum, 2);
                    for (int i = 0; i < pathNum; i++)
                    {
                        Hero->Path.PathX[i] = tempPath.PathX[i];
                        Hero->Path.PathY[i] = tempPath.PathY[i];
                    }
                    Hero->Path.PathNum = pathNum;
                    Hero->Path.CurrentPath = 0;
                    Hero->Path.CurrentPathFloat = 0;

                    Hero->Path.Lock.unlock();

                    Hero->MovementType = MOVEMENT_SKILL;
                    SendMove(Hero, &Hero->Object);
                    return 0;
                }
            }
        }
        else
        {
            TargetX = Hero->PositionX;
            TargetY = Hero->PositionY;
        }

        int iSkillResult = GameLogic::Combat::ExecuteSkill(Hero, iSkill, fSkillDistance);
        if (iSkillResult == -1 && iTarget != -1)
        {
            DeleteTarget(iTarget);
        }

        return (int)(iSkillResult == 1);
    }

    int CMuHelper::SimulateBasicAttack(int iTarget)
    {
        if (iTarget == -1)
        {
            return 0;
        }

        // Let the current swing finish before attacking again, so the cadence
        // tracks AttackSpeed instead of the fixed helper timer.
        if (IsHeroSwingInProgress())
        {
            return 0;
        }

        const int iCharIndex = FindCharacterIndex(iTarget);
        if (iCharIndex == MAX_CHARACTERS_CLIENT)
        {
            DeleteTarget(iTarget);
            return 0;
        }

        CHARACTER* pTarget = &CharactersClient[iCharIndex];
        if (!IsAttackableTarget(pTarget))
        {
            DeleteTarget(iTarget);
            return 0;
        }

        constexpr float BASIC_RANGE_DEFAULT = 1.8f;
        constexpr float BASIC_RANGE_SPEAR = 2.2f;
        constexpr float BASIC_RANGE_BOW = 6.0f;

        float fRange = BASIC_RANGE_DEFAULT;
        const int iWeaponRight = CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type;
        if (iWeaponRight >= ITEM_SPEAR && iWeaponRight < ITEM_SPEAR + MAX_ITEM_INDEX)
        {
            fRange = BASIC_RANGE_SPEAR;
        }
        if (gCharacterManager.GetEquipedBowType() != BOWTYPE_NONE)
        {
            fRange = BASIC_RANGE_BOW;
        }

        SelectedCharacter = iCharIndex;
        TargetX = (int)(pTarget->Object.Position[0] / TERRAIN_SCALE);
        TargetY = (int)(pTarget->Object.Position[1] / TERRAIN_SCALE);

        PATH_t tempPath;
        const bool bHasPath = PathFinding2(Hero->PositionX, Hero->PositionY, TargetX, TargetY, &tempPath, m_iHuntingDistance + fRange);
        if (!bHasPath)
        {
            DeleteTarget(iTarget);
            return 0;
        }

        const bool bTargetNear = CheckTile(Hero, &Hero->Object, fRange);
        if (bTargetNear && !CheckWall(Hero->PositionX, Hero->PositionY, TargetX, TargetY))
        {
            DeleteTarget(iTarget);
            return 0;
        }

        // Target is not yet in range, move closer.
        if (!bTargetNear)
        {
            Hero->Path.Lock.lock();
            const int pathNum = std::min<int>(tempPath.PathNum, 2);
            for (int i = 0; i < pathNum; i++)
            {
                Hero->Path.PathX[i] = tempPath.PathX[i];
                Hero->Path.PathY[i] = tempPath.PathY[i];
            }
            Hero->Path.PathNum = pathNum;
            Hero->Path.CurrentPath = 0;
            Hero->Path.CurrentPathFloat = 0;
            Hero->Path.Lock.unlock();

            Hero->MovementType = MOVEMENT_ATTACK;
            ActionTarget = iCharIndex;
            SendMove(Hero, &Hero->Object);
            return 0;
        }

        Hero->MovementType = MOVEMENT_ATTACK;
        ActionTarget = iCharIndex;
        Attacking = 1;
        Action(Hero, &Hero->Object, true);
        return 1;
    }

    int CMuHelper::Regroup()
    {
        if (m_config.bReturnToOriginalPosition && m_iSecondsAway > m_config.iMaxSecondsAway)
        {
            if (!SimulateMove(m_posOriginal))
            {
                return 0;
            }

            m_iSecondsAway = 0;
            m_iComboState = 0;
            m_iCurrentTarget = -1;
        }

        return 1;
    }

    int CMuHelper::SimulateMove(POINT posMove)
    {
        Hero->MovementType = MOVEMENT_MOVE;
        TargetX = (int)posMove.x;
        TargetY = (int)posMove.y;

        if (!CheckTile(Hero, &Hero->Object, 1.5f))
        {
            if (PathFinding2((Hero->PositionX), (Hero->PositionY), TargetX, TargetY, &Hero->Path))
            {
                SendMove(Hero, &Hero->Object);
            }
            return 0;
        }

        return 1;
    }

    bool CMuHelper::HasAssignedBuffSkill()
    {
        for (int i = 0; i < m_config.aiBuff.size(); i++)
        {
            if (m_config.aiBuff[i] != 0)
            {
                return true;
            }
        }

        return false;
    }

    ActionSkillType CMuHelper::GetHealingSkill()
    {
        std::vector<ActionSkillType> aiHealingSkills =
        {
            AT_SKILL_HEALING,
            AT_SKILL_HEALING_STR,
        };

        for (int i = 0; i < aiHealingSkills.size(); i++)
        {
            int iSkillIndex = g_pSkillList->GetSkillIndex(aiHealingSkills[i]);
            if (iSkillIndex != -1)
            {
                return aiHealingSkills[i];
            }
        }

        return AT_SKILL_UNDEFINED;
    }

    // Matches AttackWizard() behavior in ZzzInterface.cpp for these skill IDs.
    bool CMuHelper::IsSelfPositionSkill(ActionSkillType iSkill)
    {
        return (
            iSkill == AT_SKILL_NOVA_BEGIN ||
            iSkill == AT_SKILL_NOVA ||
            iSkill == AT_SKILL_HELL_FIRE ||
            iSkill == AT_SKILL_HELL_FIRE_STR ||
            iSkill == AT_SKILL_INFERNO ||
            iSkill == AT_SKILL_INFERNO_STR ||
            iSkill == AT_SKILL_INFERNO_STR_MG
        );
    }

    ActionSkillType CMuHelper::GetDrainLifeSkill()
    {
        std::vector<ActionSkillType> aiDrainLifeSkills =
        {
            AT_SKILL_ALICE_DRAINLIFE,
            AT_SKILL_ALICE_DRAINLIFE_STR
        };

        for (int i = 0; i < aiDrainLifeSkills.size(); i++)
        {
            int iSkillIndex = g_pSkillList->GetSkillIndex(aiDrainLifeSkills[i]);
            if (iSkillIndex != -1)
            {
                return aiDrainLifeSkills[i];
            }
        }

        return AT_SKILL_UNDEFINED;
    }

    int CMuHelper::ObtainItem()
    {
        const int iPriorityAzothItem = SelectAzothItemToObtain();
        if (iPriorityAzothItem != MAX_ITEMS)
        {
            m_iCurrentItem = iPriorityAzothItem;
        }

        if (m_iCurrentItem == MAX_ITEMS)
        {
            m_iCurrentItem = SelectItemToObtain();
            if (m_iCurrentItem == MAX_ITEMS)
            {
                return 1;
            }
        }

        ITEM_t* pDrop = &Items[m_iCurrentItem];

        if (!pDrop->Object.Live)
        {
            DeleteItem(m_iCurrentItem);
            return 1;
        }

        TargetX = (int)(Items[m_iCurrentItem].Object.Position[0] / TERRAIN_SCALE);
        TargetY = (int)(Items[m_iCurrentItem].Object.Position[1] / TERRAIN_SCALE);

        int iDistance = ComputeDistanceBetween({ Hero->PositionX, Hero->PositionY }, { TargetX, TargetY });
        if (iDistance <= m_iObtainingDistance)
        {
            if (!CheckTile(Hero, &Hero->Object, 2.0f))
            {
                if (PathFinding2((Hero->PositionX), (Hero->PositionY), TargetX, TargetY, &Hero->Path))
                {
                    SendMove(Hero, &Hero->Object);
                }

                return 0;
            }
            else
            {
                RequestPickupItem(m_iCurrentItem);
            }
        }

        return 1;
    }

    void CMuHelper::AutoDistributeStatPoints()
    {
        if (!m_config.bAutoDistributePoints
            || CharacterAttribute == nullptr
            || CharacterAttribute->LevelUpPoint <= 0
            || SocketClient == nullptr
            || SocketClient->ToGameServer() == nullptr)
        {
            return;
        }

        if (++m_iAutoPointLoopCounter < 5)
        {
            return;
        }
        m_iAutoPointLoopCounter = 0;

        std::array<int, 5> values =
        {
            static_cast<int>(CharacterAttribute->Strength),
            static_cast<int>(CharacterAttribute->Dexterity),
            static_cast<int>(CharacterAttribute->Vitality),
            static_cast<int>(CharacterAttribute->Energy),
            static_cast<int>(CharacterAttribute->Charisma)
        };

        std::array<int, 5> weights =
        {
            static_cast<int>(m_config.aAutoPointPercent[0]),
            static_cast<int>(m_config.aAutoPointPercent[1]),
            static_cast<int>(m_config.aAutoPointPercent[2]),
            static_cast<int>(m_config.aAutoPointPercent[3]),
            static_cast<int>(m_config.aAutoPointPercent[4])
        };

        if (gCharacterManager.GetBaseClass(Hero->Class) != CLASS_DARK_LORD)
        {
            weights[4] = 0;
        }

        const int maxSends = std::min<int>(CharacterAttribute->LevelUpPoint, MAX_AUTO_STAT_SEND_PER_TICK);
        for (int sendIndex = 0; sendIndex < maxSends; ++sendIndex)
        {
            int totalWeight = 0;
            int totalValue = 0;
            for (int index = 0; index < 5; ++index)
            {
                if (weights[index] > 0 && values[index] < MAX_AUTO_STAT_VALUE)
                {
                    totalWeight += weights[index];
                    totalValue += values[index];
                }
            }

            if (totalWeight <= 0)
            {
                return;
            }

            int bestIndex = -1;
            int bestDeficit = INT_MIN;
            int bestWeight = 0;
            for (int index = 0; index < 5; ++index)
            {
                if (weights[index] <= 0 || values[index] >= MAX_AUTO_STAT_VALUE)
                {
                    continue;
                }

                const int desired = ((totalValue + 1) * weights[index]) / totalWeight;
                const int deficit = desired - values[index];
                if (bestIndex < 0 || deficit > bestDeficit || (deficit == bestDeficit && weights[index] > bestWeight))
                {
                    bestIndex = index;
                    bestDeficit = deficit;
                    bestWeight = weights[index];
                }
            }

            if (bestIndex < 0)
            {
                return;
            }

            SocketClient->ToGameServer()->SendIncreaseCharacterStatPoint(static_cast<CharacterStatAttribute>(bestIndex));
            ++values[bestIndex];
        }
    }

    bool CMuHelper::ShouldObtainItem(int iItemId)
    {
        ITEM_t* pDrop = &Items[iItemId];
        ITEM* pItem = &pDrop->Item;

        if (IsAzothDrop(iItemId))
        {
            return true;
        }

        if (m_config.bPickZen && IsMoneyItem(pItem))
        {
            return true;
        }

        if (m_config.bPickJewel && IsJewelItem(pItem))
        {
            return true;
        }

        if (m_config.bPickEventItems && IsEventOrConsumableItem(pItem))
        {
            return true;
        }

        const bool isAncient = IsAncientItem(pItem);
        const bool isExcellent = IsExcellentItem(pItem);
        const bool isCommon = !isAncient
            && !isExcellent
            && !IsMoneyItem(pItem)
            && !IsJewelItem(pItem)
            && !IsEventOrConsumableItem(pItem);

        const bool matchesSelectedQuality =
            (m_config.bPickAncient && isAncient)
            || (m_config.bPickExcellent && isExcellent)
            || (m_config.bPickCommonItems && isCommon);

        if (matchesSelectedQuality
            && HasMinimumOptionLevel(pItem, m_config.iMinimumOptionLevel)
            && (!m_config.bPickOnlyUsableItems || IsUsableByCurrentHero(pItem)))
        {
            return true;
        }

        if (m_config.bPickExtraItems)
        {
            std::wstring strDisplayName = GetItemDisplayName(pItem);

            for (const auto& str : m_config.aExtraItems)
            {
                // Check if the search keyword is in the item's display name
                if (strDisplayName.find(str) != std::wstring::npos)
                {
                    return true;
                }
            }
        }

        return false;
    }

    void CMuHelper::AddItem(int iItemId, POINT posWhere)
    {
        _itemsLock.lock();
        m_setItems.insert(iItemId);
        _itemsLock.unlock();

        if (!m_bActive || Hero == nullptr || !IsAzothDrop(iItemId))
        {
            return;
        }

        const int iItemX = (int)(Items[iItemId].Object.Position[0] / TERRAIN_SCALE);
        const int iItemY = (int)(Items[iItemId].Object.Position[1] / TERRAIN_SCALE);
        const int iDistance = ComputeDistanceBetween({ Hero->PositionX, Hero->PositionY }, { iItemX, iItemY });
        if (iDistance <= 2)
        {
            RequestPickupItem(iItemId);
        }
    }

    void CMuHelper::DeleteItem(int iItemId)
    {
        _itemsLock.lock();
        m_setItems.erase(iItemId);
        _itemsLock.unlock();

        if (iItemId == m_iCurrentItem)
        {
            m_iCurrentItem = MAX_ITEMS;
        }
    }

    int CMuHelper::SelectItemToObtain()
    {
        int iClosestItemId = MAX_ITEMS;
        int iMinDistance = m_iObtainingDistance;

        std::set<int> setItems;
        {
            _itemsLock.lock();
            setItems = m_setItems;
            _itemsLock.unlock();
        }

        for (const int& iItemId : setItems)
        {
            if (!ShouldObtainItem(iItemId))
            {
                continue;
            }

            int iItemX = (int)(Items[iItemId].Object.Position[0] / TERRAIN_SCALE);
            int iItemY = (int)(Items[iItemId].Object.Position[1] / TERRAIN_SCALE);

            int iDistance = ComputeDistanceBetween({ Hero->PositionX, Hero->PositionY }, { iItemX, iItemY });
            if (iDistance <= iMinDistance)
            {
                iMinDistance = iDistance;
                iClosestItemId = iItemId;
            }
        }

        return iClosestItemId;
    }

    int CMuHelper::SelectAzothItemToObtain()
    {
        int iClosestItemId = MAX_ITEMS;
        int iMinDistance = m_iObtainingDistance;

        std::set<int> setItems;
        {
            _itemsLock.lock();
            setItems = m_setItems;
            _itemsLock.unlock();
        }

        for (const int& iItemId : setItems)
        {
            if (!IsAzothDrop(iItemId))
            {
                continue;
            }

            const int iItemX = (int)(Items[iItemId].Object.Position[0] / TERRAIN_SCALE);
            const int iItemY = (int)(Items[iItemId].Object.Position[1] / TERRAIN_SCALE);
            const int iDistance = ComputeDistanceBetween({ Hero->PositionX, Hero->PositionY }, { iItemX, iItemY });
            if (iDistance <= iMinDistance)
            {
                iMinDistance = iDistance;
                iClosestItemId = iItemId;
            }
        }

        return iClosestItemId;
    }

    bool CMuHelper::IsAzothDrop(int iItemId) const
    {
        if (iItemId < 0 || iItemId >= MAX_ITEMS)
        {
            return false;
        }

        const ITEM_t* pDrop = &Items[iItemId];
        return pDrop->Object.Live
            && pDrop->Item.Type == ITEM_ZEN
            && IsAzothMoneyDropAmount(pDrop->Item.Level);
    }

    bool CMuHelper::RequestPickupItem(int iItemId)
    {
        if (SendGetItem != -1
            || SocketClient == nullptr
            || SocketClient->ToGameServer() == nullptr)
        {
            return false;
        }

        SendGetItem = iItemId;
        SocketClient->ToGameServer()->SendPickupItemRequest(iItemId);
        DeleteItem(iItemId);
        return true;
    }
}
