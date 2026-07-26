//////////////////////////////////////////////////////////////////////
// NewUIMainFrameWindow.cpp: implementation of the CNewUIMainFrameWindow class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <algorithm>
#include <array>
#include <cwchar>
#include <cwctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include "I18N/All.h"

#include "App/Platform/Windows/Winmain.h"
#include "UI/NewUI/HUD/NewUIMainFrameWindow.h"	// self
#include "UI/NewUI/Dialogs/NewUIMessageBox.h"
#include "UI/NewUI/Inventory/NewUIMyInventory.h"
#include "UI/NewUI/Inventory/NewUIInventoryCtrl.h"
#include "UI/NewUI/Options/NewUIOptionWindow.h"
#include "UI/NewUI/NewUISystem.h"
#include "UI/NewUI/Party/NewUIPartyListWindow.h"
#include "UI/Widgets/UIBaseDef.h"
#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Render/Models/ZzzBMD.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzInventory.h"
#include "Engine/AI/GOBoid.h"
#include "UI/Legacy/UIManager.h"

#include "GameLogic/Items/CSItemOption.h"
#include "GameLogic/Events/CSChaosCastle.h"
#include "World/MapInfra/MapManager.h"
#include "Character/CharacterManager.h"
#include "Character/AccountCharacterList.h"
#include "Character/AccountCompanionClient.h"
#include "Character/AzothClient.h"
#include "Character/JewelBankClient.h"
#include "GameLogic/Skills/SkillManager.h"
#include "MUHelper/MuHelper.h"
#include "UI/NewUI/HUD/Skills/SkillTooltip.h"
#include "Core/Time/CTimCheck.h"
#include "GameLogic/Social/MonkSystem.h"
#include "Data/GameConfig/GameConfig.h"

#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
#include "GameShop/InGameShopSystem.h"
#endif //PBG_ADD_INGAMESHOP_UI_MAINFRAME

namespace
{
    constexpr std::uint32_t kSkillHotKeyCacheMagic = 0x4D55484B; // MUHK
    constexpr std::uint32_t kSkillHotKeyCacheVersion = 1;

    struct SkillHotKeyCache
    {
        std::uint32_t Magic = kSkillHotKeyCacheMagic;
        std::uint32_t Version = kSkillHotKeyCacheVersion;
        std::int32_t SkillIds[10] = {};
    };

    constexpr float kAccountGuardButtonY = 34.f;
    constexpr float kAccountGuardButtonWidth = 24.f;
    constexpr float kAccountGuardButtonHeight = 24.f;
    constexpr float kAccountGuardButtonGap = 5.f;
    constexpr float kAccountGuardButtonRightMargin = 8.f;
    constexpr float kAccountGuardButtonsTotalWidth = (kAccountGuardButtonWidth * 5.f) + (kAccountGuardButtonGap * 4.f);
    constexpr float kAccountGuardMiniListWidth = 62.f;
    constexpr float kAccountGuardMiniListHeight = 17.f;
    constexpr float kAccountGuardMiniListRowGap = 1.f;
    constexpr float kAccountGuardMiniListHpBarWidth = 54.f;
    constexpr float kAccountGuardMiniListHpBarHeight = 2.f;
    constexpr int kAccountGuardMiniListMaxRows = 8;
    constexpr GLuint kAccountGuardHudBitmapBase = BITMAP_INTERFACE_TEXTURE_END - 80;
    constexpr GLuint kAccountGuardWindowImage = kAccountGuardHudBitmapBase;
    constexpr GLuint kAccountGuardSmallButtonImage = kAccountGuardHudBitmapBase + 1;
    constexpr GLuint kAccountGuardWideButtonImage = kAccountGuardHudBitmapBase + 2;
    constexpr GLuint kAccountGuardSummonButtonImage = kAccountGuardHudBitmapBase + 3;
    constexpr GLuint kAccountGuardSummonButtonHoverImage = kAccountGuardHudBitmapBase + 4;
    constexpr GLuint kPersonalStoreButtonImage = kAccountGuardHudBitmapBase + 5;
    constexpr GLuint kPersonalStoreButtonHoverImage = kAccountGuardHudBitmapBase + 6;
    constexpr GLuint kAccountGuardSwitchButtonImage = kAccountGuardHudBitmapBase + 9;
    constexpr GLuint kAccountGuardSwitchButtonHoverImage = kAccountGuardHudBitmapBase + 10;
    constexpr GLuint kGameSettingsButtonImage = kAccountGuardHudBitmapBase + 11;
    constexpr GLuint kGameSettingsButtonHoverImage = kAccountGuardHudBitmapBase + 12;
    constexpr GLuint kAzothHudImage = kAccountGuardHudBitmapBase + 13;
    constexpr GLuint kAzothHudHoverImage = kAccountGuardHudBitmapBase + 14;

    bool ResolveSkillHotKeyCharacterName(std::wstring& characterName)
    {
        characterName.clear();

        if (CharacterAttribute != nullptr && CharacterAttribute->Name[0] != L'\0')
        {
            characterName = CharacterAttribute->Name;
        }
        else if (Hero != nullptr && Hero->ID[0] != L'\0')
        {
            characterName = Hero->ID;
        }

        return !characterName.empty();
    }

    std::wstring MakeSkillHotKeySafeFileName(const std::wstring& value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (const wchar_t ch : value)
        {
            if (std::iswalnum(ch) || ch == L'_' || ch == L'-')
            {
                result.push_back(ch);
            }
            else
            {
                result.push_back(L'_');
            }
        }

        return result.empty() ? L"unknown" : result;
    }

    std::filesystem::path ResolveLocalSkillHotKeyPath()
    {
        std::wstring characterName;
        if (!ResolveSkillHotKeyCharacterName(characterName))
        {
            return {};
        }

        return std::filesystem::path(L"Data")
            / L"Local"
            / L"SkillHotKeys"
            / (MakeSkillHotKeySafeFileName(characterName) + L".bin");
    }

    int ResolveSkillIdFromHotKeySlot(int skillSlot)
    {
        if (skillSlot >= AT_PET_COMMAND_DEFAULT && skillSlot < AT_PET_COMMAND_END)
        {
            return skillSlot;
        }

        if (skillSlot < 0 || skillSlot >= MAX_MAGIC || CharacterAttribute == nullptr)
        {
            return -1;
        }

        const int skillId = CharacterAttribute->Skill[skillSlot];
        return skillId != 0 ? skillId : -1;
    }

    int ResolveHotKeySlotFromSkillId(int skillId)
    {
        if (skillId >= AT_PET_COMMAND_DEFAULT && skillId < AT_PET_COMMAND_END)
        {
            return skillId;
        }

        if (skillId <= 0 || CharacterAttribute == nullptr)
        {
            return -1;
        }

        for (int index = 0; index < MAX_MAGIC; ++index)
        {
            if (CharacterAttribute->Skill[index] == skillId)
            {
                return index;
            }
        }

        return -1;
    }
    constexpr GLuint kAzothDigitImage = kAccountGuardHudBitmapBase + 15;
    constexpr GLuint kCommandsButtonImage = kAccountGuardHudBitmapBase + 16;
    constexpr GLuint kCommandsButtonHoverImage = kAccountGuardHudBitmapBase + 17;
    constexpr GLuint kJewelBankButtonImage = kAccountGuardHudBitmapBase + 18;
    constexpr GLuint kJewelBankButtonHoverImage = kAccountGuardHudBitmapBase + 19;
    constexpr GLuint kJewelBankFooterStoreImage = kAccountGuardHudBitmapBase + 20;
    constexpr GLuint kJewelBankFooterStoreHoverImage = kJewelBankFooterStoreImage + 1;
    constexpr GLuint kJewelBankFooterWithdrawImage = kJewelBankFooterStoreImage + 2;
    constexpr GLuint kJewelBankFooterWithdrawHoverImage = kJewelBankFooterStoreImage + 3;
    constexpr GLuint kAccountGuardFarmButtonImage = kAccountGuardHudBitmapBase + 24;
    constexpr GLuint kAccountGuardFarmButtonHoverImage = kAccountGuardHudBitmapBase + 25;
    constexpr GLuint kAccountGuardFollowButtonImage = kAccountGuardHudBitmapBase + 26;
    constexpr GLuint kAccountGuardFollowButtonHoverImage = kAccountGuardHudBitmapBase + 27;
    constexpr GLuint kAccountGuardCallButtonImage = kAccountGuardHudBitmapBase + 28;
    constexpr GLuint kAccountGuardCallButtonHoverImage = kAccountGuardHudBitmapBase + 29;
    constexpr GLuint kAccountGuardStoreButtonImage = kAccountGuardHudBitmapBase + 30;
    constexpr GLuint kAccountGuardStoreButtonHoverImage = kAccountGuardHudBitmapBase + 31;
    constexpr DWORD kInvalidResourceValue = static_cast<DWORD>(-1);
    constexpr DWORD kMaxRenderableResourceValue = 0x7fffffff;

    constexpr float kAzothHudWidth = 105.f;
    constexpr float kAzothHudHeight = 29.5f;
    constexpr float kAzothHudX = REFERENCE_WIDTH - kAzothHudWidth - 6.f;
    constexpr float kAzothHudY = REFERENCE_HEIGHT - kAzothHudHeight - 54.f;
    constexpr float kAzothHudTextureU = 1.f;
    constexpr float kAzothHudTextureV = 144.f / 256.f;
    constexpr float kAzothDigitAtlasCellWidth = 96.f / 1024.f;
    constexpr float kAzothDigitAtlasHeight = 140.f / 256.f;
    constexpr float kAzothHudDigitWidth = 4.7f;
    constexpr float kAzothHudDigitHeight = 9.f;
    constexpr float kAzothHudDigitGap = -0.1f;
    constexpr float kAzothHudDigitRight = kAzothHudX + kAzothHudWidth - 11.f;
    constexpr float kAzothHudDigitY = kAzothHudY + 11.5f;

    constexpr float kAzothPanelX = 20.f;
    constexpr float kAzothPanelY = 51.f;
    constexpr float kAzothPanelWidth = 200.f;
    constexpr float kAzothPanelHeight = 224.f;
    constexpr float kAzothButtonWidth = 56.f;
    constexpr float kAzothButtonHeight = 23.f;
    constexpr float kAzothWideButtonWidth = 92.f;
    constexpr float kAzothAutoButtonWidth = 34.f;
    constexpr float kAzothAutoButtonHeight = 20.f;
    constexpr float kAzothAutoButtonGap = 4.f;

    constexpr float kAccountGuardWindowWidth = 240.f;
    constexpr float kAccountGuardWindowHeight = 348.f;
    constexpr float kAccountGuardWindowTextureU = 386.f / 512.f;
    constexpr float kAccountGuardWindowTextureV = 511.f / 512.f;
    constexpr float kAccountGuardWindowX = (REFERENCE_WIDTH - kAccountGuardWindowWidth) / 2.f;
    constexpr float kAccountGuardWindowY = 66.f;
    constexpr float kAccountGuardTableX = 17.f;
    constexpr float kAccountGuardTableY = 46.f;
    constexpr float kAccountGuardTableWidth = kAccountGuardWindowWidth - 34.f;
    constexpr float kAccountGuardTableHeight = 165.f;
    constexpr float kAccountGuardHeaderY = kAccountGuardTableY + 5.f;
    constexpr float kAccountGuardRowStartY = kAccountGuardWindowY + kAccountGuardTableY + 25.f;

    DWORD NormalizeResourceMax(DWORD preferredMax, DWORD fallbackMax = 0)
    {
        const auto sanitize = [](DWORD value) -> DWORD
        {
            if (value == 0 || value == kInvalidResourceValue)
            {
                return 0;
            }

            return std::min(value, kMaxRenderableResourceValue);
        };

        const DWORD preferred = sanitize(preferredMax);
        return preferred > 0 ? preferred : sanitize(fallbackMax);
    }

    DWORD NormalizeResourceCurrent(DWORD currentValue, DWORD maxValue)
    {
        if (maxValue == 0)
        {
            return 0;
        }

        if (currentValue == kInvalidResourceValue || currentValue > kMaxRenderableResourceValue)
        {
            return maxValue;
        }

        return std::min(currentValue, maxValue);
    }

    float GetMissingResourceRatio(DWORD currentValue, DWORD maxValue)
    {
        if (maxValue == 0)
        {
            return 0.0f;
        }

        const float currentRatio = std::clamp(currentValue / static_cast<float>(maxValue), 0.0f, 1.0f);
        return 1.0f - currentRatio;
    }

    int ToRenderableResourceNumber(DWORD value)
    {
        return static_cast<int>(std::min(value, kMaxRenderableResourceValue));
    }
    constexpr float kAccountGuardRowHeight = 28.f;
    constexpr int kAccountGuardRowsPerPage = 5;
    constexpr float kAccountGuardCloseButtonX = kAccountGuardWindowWidth - 30.f;
    constexpr float kAccountGuardCloseButtonY = 12.f;
    constexpr float kAccountGuardCloseButtonSize = 20.f;
    constexpr float kAccountGuardNameColumnX = 25.f;
    constexpr float kAccountGuardNameColumnWidth = 45.f;
    constexpr float kAccountGuardCallButtonX = 75.f;
    constexpr float kAccountGuardStoreButtonX = 98.f;
    constexpr float kAccountGuardSwitchButtonX = 121.f;
    constexpr float kAccountGuardFarmButtonX = 144.f;
    constexpr float kAccountGuardFollowButtonX = 167.f;
    constexpr float kAccountGuardAutoRespawnCheckboxX = 198.f;
    constexpr float kAccountGuardRowButtonWidth = 20.f;
    constexpr float kAccountGuardRowButtonHeight = 20.f;
    constexpr float kAccountGuardSwitchButtonSize = 20.f;
    constexpr float kAccountGuardModeButtonSize = 20.f;
    constexpr float kAccountGuardAutoRespawnCheckboxSize = 11.f;
    constexpr float kAccountGuardFooterButtonWidth = 84.f;
    constexpr float kAccountGuardFooterButtonHeight = 23.f;
    constexpr float kAccountGuardFooterLeftButtonX = 25.f;
    constexpr float kAccountGuardFooterRightButtonX = kAccountGuardWindowWidth - kAccountGuardFooterLeftButtonX - kAccountGuardFooterButtonWidth;
    constexpr float kAccountGuardFooterCloseButtonWidth = 64.f;
    constexpr float kAccountGuardFooterCloseButtonX = (kAccountGuardWindowWidth - kAccountGuardFooterCloseButtonWidth) / 2.f;
    constexpr float kAccountGuardPagePrevButtonX = 72.f;
    constexpr float kAccountGuardPageNextButtonX = kAccountGuardWindowWidth - kAccountGuardPagePrevButtonX - 32.f;
    constexpr float kAccountGuardPageTextX = (kAccountGuardWindowWidth - 44.f) / 2.f;
    constexpr float kAccountGuardPageButtonY = kAccountGuardWindowHeight - 96.f;
    constexpr float kAccountGuardPageTextY = kAccountGuardPageButtonY + 6.f;
    constexpr float kAccountGuardFooterButtonY = kAccountGuardWindowHeight - 70.f;
    constexpr float kAccountGuardFooterCloseButtonY = kAccountGuardWindowHeight - 43.f;
    constexpr float kAzothFooterButtonWidth = 74.f;
    constexpr float kAzothFooterButtonGap = 10.f;
    constexpr float kAzothFooterApplyButtonX = (kAccountGuardWindowWidth - ((kAzothFooterButtonWidth * 2.f) + kAzothFooterButtonGap)) / 2.f;
    constexpr float kAzothFooterCloseButtonX = kAzothFooterApplyButtonX + kAzothFooterButtonWidth + kAzothFooterButtonGap;

    constexpr float kJewelBankPanelX = 17.f;
    constexpr float kJewelBankPanelY = 50.f;
    constexpr float kJewelBankPanelWidth = kAccountGuardWindowWidth - 34.f;
    constexpr float kJewelBankPanelHeight = 210.f;
    constexpr int kJewelBankRowsPerPage = 5;
    constexpr int kJewelBankPageCount = (JewelBankClient::JewelCount + kJewelBankRowsPerPage - 1) / kJewelBankRowsPerPage;
    constexpr float kJewelBankRowHeight = 31.f;
    constexpr float kJewelBankIconSize = 20.f;
    constexpr float kJewelBankRowButtonSize = 20.f;
    constexpr float kJewelBankTableHeaderY = kJewelBankPanelY + 37.f;
    constexpr float kJewelBankFirstRowY = kJewelBankPanelY + 54.f;
    constexpr float kJewelBankModelColumnX = kJewelBankPanelX + 16.f;
    constexpr float kJewelBankNameColumnX = kJewelBankPanelX + 48.f;
    constexpr float kJewelBankCountColumnX = kJewelBankPanelX + 113.f;
    constexpr float kJewelBankStoreColumnX = kJewelBankPanelX + 151.f;
    constexpr float kJewelBankWithdrawColumnX = kJewelBankPanelX + 176.f;
    constexpr float kJewelBankPageButtonWidth = 48.f;
    constexpr float kJewelBankPageButtonY = kAccountGuardWindowHeight - 61.f;
    constexpr float kJewelBankPagePrevButtonX = 25.f;
    constexpr float kJewelBankPageTextX = (kAccountGuardWindowWidth - 56.f) / 2.f;
    constexpr float kJewelBankPageNextButtonX = kAccountGuardWindowWidth - kJewelBankPagePrevButtonX - kJewelBankPageButtonWidth;
    constexpr float kJewelBankFooterTextButtonWidth = 70.f;
    constexpr float kJewelBankFooterButtonY = kAccountGuardWindowHeight - 35.f;
    constexpr float kJewelBankFooterRefreshButtonX = 43.f;
    constexpr float kJewelBankFooterCloseButtonX = kAccountGuardWindowWidth - kJewelBankFooterRefreshButtonX - kJewelBankFooterTextButtonWidth;

    constexpr float kAccountGuardFormationPanelX = 17.f;
    constexpr float kAccountGuardFormationPanelY = 48.f;
    constexpr float kAccountGuardFormationPanelWidth = kAccountGuardWindowWidth - 34.f;
    constexpr float kAccountGuardFormationPanelHeight = 190.f;
    constexpr float kAccountGuardFormationModeButtonWidth = 45.f;
    constexpr float kAccountGuardFormationModeButtonHeight = 22.f;
    constexpr float kAccountGuardFormationModeButtonGap = 5.f;
    constexpr float kAccountGuardFormationWideButtonWidth = 88.f;
    constexpr float kAccountGuardFormationWideButtonHeight = 23.f;
    constexpr float kCharacterConfigWindowWidth = 560.f;
    constexpr float kCharacterConfigWindowHeight = 350.f;
    constexpr float kCharacterConfigWindowX = (REFERENCE_WIDTH - kCharacterConfigWindowWidth) / 2.f;
    constexpr float kCharacterConfigWindowY = 58.f;
    constexpr float kCharacterConfigCloseButtonX = kCharacterConfigWindowWidth - 30.f;
    constexpr float kCharacterConfigCloseButtonY = 12.f;
    constexpr float kCharacterConfigPanelY = 48.f;
    constexpr float kCharacterConfigPanelHeight = 240.f;
    constexpr float kCharacterConfigColumnWidth = 165.f;
    constexpr float kCharacterConfigColumnGap = 13.f;
    constexpr float kCharacterConfigLeftX = 17.f;
    constexpr float kCharacterConfigCenterX = kCharacterConfigLeftX + kCharacterConfigColumnWidth + kCharacterConfigColumnGap;
    constexpr float kCharacterConfigRightX = kCharacterConfigCenterX + kCharacterConfigColumnWidth + kCharacterConfigColumnGap;
    constexpr float kCharacterConfigRowHeight = 20.f;
    constexpr float kCharacterConfigStepButtonSize = 18.f;
    constexpr float kCharacterConfigFooterButtonY = kCharacterConfigWindowHeight - 44.f;
    constexpr float kCharacterConfigSaveButtonWidth = 148.f;
    constexpr float kCharacterConfigSaveButtonX = kCharacterConfigWindowWidth - kCharacterConfigSaveButtonWidth - 24.f;
    constexpr float kCharacterConfigFooterCloseButtonX = 24.f;
    constexpr float kCharacterConfigFooterCloseButtonWidth = 70.f;

    constexpr float kGameSettingsPanelX = 17.f;
    constexpr float kGameSettingsPanelY = 48.f;
    constexpr float kGameSettingsPanelWidth = kAccountGuardWindowWidth - 34.f;
    constexpr float kGameSettingsPanelHeight = 197.f;
    constexpr float kGameSettingsOptionRowHeight = 20.f;
    constexpr float kGameSettingsOptionRowGap = 1.f;
    constexpr float kGameSettingsCheckboxSize = 11.f;
    constexpr float kGameSettingsPresetButtonWidth = 80.f;
    constexpr float kGameSettingsPresetButtonHeight = 23.f;
    constexpr float kCommandsPanelX = 17.f;
    constexpr float kCommandsPanelY = 50.f;
    constexpr float kCommandsPanelWidth = kAccountGuardWindowWidth - 34.f;
    constexpr float kCommandsPanelHeight = 176.f;
    constexpr float kCommandsButtonWidth = 172.f;
    constexpr float kCommandsButtonHeight = 26.f;
    constexpr float kCommandsButtonGap = 10.f;

    enum class AccountGuardHudAction
    {
        OpenPersonalStore,
        OpenSummonWindow,
        OpenCommandsWindow,
        OpenGameSettingsWindow,
        OpenJewelBankWindow,
    };

    float AccountGuardHudButtonX(int index)
    {
        return REFERENCE_WIDTH - kAccountGuardButtonRightMargin - kAccountGuardButtonsTotalWidth
            + ((kAccountGuardButtonWidth + kAccountGuardButtonGap) * static_cast<float>(index));
    }

    struct AccountGuardHudButton
    {
        const wchar_t* Label;
        const wchar_t* Tooltip;
        const wchar_t* PendingMessage;
        AccountGuardHudAction Action;
        float X;
        float Y;
    };

    bool g_isAccountGuardWindowOpen = false;
    bool g_isAccountGuardFormationWindowOpen = false;
    bool g_isCommandsWindowOpen = false;
    bool g_isGameSettingsWindowOpen = false;
    bool g_isAzothWindowOpen = false;
    bool g_isJewelBankWindowOpen = false;
    int g_accountGuardWindowPage = 0;
    int g_jewelBankWindowPage = 0;
    int g_selectedAccountGuardSlot = -1;
    unsigned long long g_lastAccountGuardPlanRequestTick = 0;
    bool g_azothPendingAutoEnabled = false;
    bool g_azothPendingAutoDirty = false;
    unsigned long long g_azothPendingAutoReserveZen = 0;
    unsigned int g_azothObservedAutoSettingsVersion = 0;

    enum CharacterConfigFlag : unsigned int
    {
        CharacterConfigFlagEnabled = 1u << 0,
        CharacterConfigFlagOwnDrops = 1u << 1,
        CharacterConfigFlagZen = 1u << 2,
        CharacterConfigFlagCommon = 1u << 3,
        CharacterConfigFlagMatching = 1u << 4,
        CharacterConfigFlagJewels = 1u << 5,
        CharacterConfigFlagAncient = 1u << 6,
        CharacterConfigFlagExcellent = 1u << 7,
        CharacterConfigFlagNamed = 1u << 8,
        CharacterConfigFlagAutoAzoth = 1u << 9,
        CharacterConfigFlagAutoReset = 1u << 10,
    };

    enum class CharacterConfigCommandMode
    {
        Follow,
        StayCity,
        FarmPosition,
    };

    struct CharacterConfigState
    {
        bool Initialized = false;
        wchar_t HeroName[MAX_USERNAME_SIZE + 1] = {};
        bool Enabled = true;
        bool PickOnlyOwnedItems = true;
        bool PickZen = false;
        bool PickCommonItems = false;
        bool PickMatchingCharacterItems = false;
        bool PickJewels = true;
        bool PickAncient = true;
        bool PickExcellent = true;
        bool PickNamedItems = false;
        bool AutoConvertZenToAzoth = false;
        bool AutoReset = false;
        int MinimumOptionLevel = 0;
        int Strength = 25;
        int Agility = 25;
        int Vitality = 20;
        int Energy = 30;
        int Command = 0;
        CharacterConfigCommandMode CommandMode = CharacterConfigCommandMode::Follow;
    };

    CharacterConfigState g_characterConfigState;

    struct JewelBankUiEntry
    {
        const wchar_t* Label;
        short BankKey;
        int ItemType;
    };

    const JewelBankUiEntry kJewelBankUiEntries[] =
    {
        { L"Chaos", -12015, ITEM_WING + 15 },
        { L"Bless", 13, ITEM_POTION + 13 },
        { L"Soul", 14, ITEM_POTION + 14 },
        { L"Life", 16, ITEM_POTION + 16 },
        { L"Creation", 22, ITEM_POTION + 22 },
        { L"Guardian", 31, ITEM_POTION + 31 },
        { L"Gemstone", 41, ITEM_POTION + 41 },
        { L"Harmony", 42, ITEM_POTION + 42 },
        { L"Low Refine", 43, ITEM_POTION + 43 },
        { L"High Refine", 44, ITEM_POTION + 44 },
        { L"Grade VI", 200, ITEM_POTION + 200 },
        { L"Grade IX", 201, ITEM_POTION + 201 },
        { L"+28 Option", 202, ITEM_POTION + 202 },
        { L"Exc. Change", 203, ITEM_POTION + 203 },
        { L"Luck", 204, ITEM_POTION + 204 },
        { L"Skill", 205, ITEM_POTION + 205 },
        { L"Grade XV", 206, ITEM_POTION + 206 },
        { L"Full", 207, ITEM_POTION + 207 },
        { L"Socket", 208, ITEM_POTION + 208 },
        { L"Armored", 209, ITEM_POTION + 209 },
        { L"Ancient", 210, ITEM_POTION + 210 },
        { L"Excellent", 211, ITEM_POTION + 211 },
    };
    static_assert(std::size(kJewelBankUiEntries) == JewelBankClient::JewelCount);

    const AccountGuardHudButton kAccountGuardHudButtons[] =
    {
        {
            L"L",
            L"Minha Loja",
            L"",
            AccountGuardHudAction::OpenPersonalStore,
            AccountGuardHudButtonX(0),
            kAccountGuardButtonY
        },
        {
            L"G",
            L"Sentinelas",
            L"",
            AccountGuardHudAction::OpenSummonWindow,
            AccountGuardHudButtonX(1),
            kAccountGuardButtonY
        },
        {
            L"J",
            L"Banco de Joias",
            L"",
            AccountGuardHudAction::OpenJewelBankWindow,
            AccountGuardHudButtonX(2),
            kAccountGuardButtonY
        },
        {
            L"M",
            L"Comandos",
            L"",
            AccountGuardHudAction::OpenCommandsWindow,
            AccountGuardHudButtonX(3),
            kAccountGuardButtonY
        },
        {
            L"C",
            L"Configuracoes do Jogo",
            L"",
            AccountGuardHudAction::OpenGameSettingsWindow,
            AccountGuardHudButtonX(4),
            kAccountGuardButtonY
        }
    };

    void ShowAccountGuardSystemMessage(const wchar_t* message)
    {
        if (message == nullptr || message[0] == L'\0')
        {
            return;
        }

        if (g_pSystemLogBox->CheckChatRedundancy(message) == FALSE)
        {
            g_pSystemLogBox->AddText(message, SEASON3B::TYPE_SYSTEM_MESSAGE);
        }
    }

    void RefreshAzothPendingAutoSettings(bool force)
    {
        const unsigned int version = AzothClient::GetAutoConversionSettingsVersion();
        if (!force && (g_azothPendingAutoDirty || g_azothObservedAutoSettingsVersion == version))
        {
            return;
        }

        g_azothPendingAutoEnabled = AzothClient::IsAutoConversionEnabled();
        g_azothPendingAutoReserveZen = AzothClient::GetAutoConversionReserveZen();
        g_azothPendingAutoDirty = false;
        g_azothObservedAutoSettingsVersion = version;
    }

    void SetAzothPendingAutoSettings(bool enabled, unsigned long long reserveZen)
    {
        g_azothPendingAutoEnabled = enabled;
        g_azothPendingAutoReserveZen = enabled ? std::min(reserveZen, AzothClient::MaxAutoConversionReserveZen) : 0ULL;
        g_azothPendingAutoDirty = true;
    }

    unsigned long long GetAzothPendingAutoReserveOrDefault()
    {
        if (g_azothPendingAutoReserveZen > 0)
        {
            return g_azothPendingAutoReserveZen;
        }

        return 100000000ULL;
    }

    void ResetCharacterConfigStateForCurrentHero()
    {
        g_characterConfigState = CharacterConfigState{};
        g_characterConfigState.Initialized = true;
        if (Hero != nullptr && Hero->ID[0] != L'\0')
        {
            std::wcsncpy(g_characterConfigState.HeroName, Hero->ID, MAX_USERNAME_SIZE);
            g_characterConfigState.HeroName[MAX_USERNAME_SIZE] = L'\0';
        }
    }

    void EnsureCharacterConfigStateForCurrentHero()
    {
        if (Hero == nullptr || Hero->ID[0] == L'\0')
        {
            g_characterConfigState.Initialized = false;
            g_characterConfigState.HeroName[0] = L'\0';
            return;
        }

        if (!g_characterConfigState.Initialized || std::wcscmp(g_characterConfigState.HeroName, Hero->ID) != 0)
        {
            ResetCharacterConfigStateForCurrentHero();
        }
    }

    const wchar_t* GetCharacterConfigCommandToken()
    {
        switch (g_characterConfigState.CommandMode)
        {
        case CharacterConfigCommandMode::StayCity:
            return L"stay_city";
        case CharacterConfigCommandMode::FarmPosition:
            return L"farm_position";
        case CharacterConfigCommandMode::Follow:
        default:
            return L"follow";
        }
    }

    int GetCurrentMapNumberForCharacterConfig()
    {
        return gMapManager.WorldActive;
    }

    int GetCurrentHeroXForCharacterConfig()
    {
        return Hero != nullptr ? std::clamp<int>(Hero->PositionX, 0, 255) : 0;
    }

    int GetCurrentHeroYForCharacterConfig()
    {
        return Hero != nullptr ? std::clamp<int>(Hero->PositionY, 0, 255) : 0;
    }

    unsigned int GetCharacterConfigFlags()
    {
        unsigned int flags = 0;
        flags |= g_characterConfigState.Enabled ? CharacterConfigFlagEnabled : 0;
        flags |= g_characterConfigState.PickOnlyOwnedItems ? CharacterConfigFlagOwnDrops : 0;
        flags |= g_characterConfigState.PickZen ? CharacterConfigFlagZen : 0;
        flags |= g_characterConfigState.PickCommonItems ? CharacterConfigFlagCommon : 0;
        flags |= g_characterConfigState.PickMatchingCharacterItems ? CharacterConfigFlagMatching : 0;
        flags |= g_characterConfigState.PickJewels ? CharacterConfigFlagJewels : 0;
        flags |= g_characterConfigState.PickAncient ? CharacterConfigFlagAncient : 0;
        flags |= g_characterConfigState.PickExcellent ? CharacterConfigFlagExcellent : 0;
        flags |= g_characterConfigState.PickNamedItems ? CharacterConfigFlagNamed : 0;
        flags |= g_characterConfigState.AutoConvertZenToAzoth ? CharacterConfigFlagAutoAzoth : 0;
        flags |= g_characterConfigState.AutoReset ? CharacterConfigFlagAutoReset : 0;
        return flags;
    }

    int& GetCharacterConfigPointValue(int index)
    {
        switch (index)
        {
        case 0:
            return g_characterConfigState.Strength;
        case 1:
            return g_characterConfigState.Agility;
        case 2:
            return g_characterConfigState.Vitality;
        case 3:
            return g_characterConfigState.Energy;
        case 4:
        default:
            return g_characterConfigState.Command;
        }
    }

    void AdjustCharacterConfigPointValue(int index, int delta)
    {
        int& value = GetCharacterConfigPointValue(index);
        value = std::clamp(value + delta, 0, 100);
    }

    void SendCharacterConfigToServer()
    {
        EnsureCharacterConfigStateForCurrentHero();
        if (Hero == nullptr || Hero->ID[0] == L'\0')
        {
            ShowAccountGuardSystemMessage(L"Entre com um personagem antes de salvar a configuracao.");
            return;
        }

        wchar_t command[256] = {};
        swprintf_s(
            command,
            L"/guard config %u %d %d %d %d %d %d %ls %d %d %d",
            GetCharacterConfigFlags(),
            std::clamp(g_characterConfigState.MinimumOptionLevel, 0, 16),
            std::clamp(g_characterConfigState.Strength, 0, 100),
            std::clamp(g_characterConfigState.Agility, 0, 100),
            std::clamp(g_characterConfigState.Vitality, 0, 100),
            std::clamp(g_characterConfigState.Energy, 0, 100),
            std::clamp(g_characterConfigState.Command, 0, 100),
            GetCharacterConfigCommandToken(),
            GetCurrentMapNumberForCharacterConfig(),
            GetCurrentHeroXForCharacterConfig(),
            GetCurrentHeroYForCharacterConfig());
        SendMacroChat(command);
        ShowAccountGuardSystemMessage(L"Configuracao nativa enviada para o personagem atual.");
    }

    int GetAccountGuardPageCount()
    {
        return std::max(1, (AccountCharacterList::MaxCharacters + kAccountGuardRowsPerPage - 1) / kAccountGuardRowsPerPage);
    }

    int GetAccountGuardSlotOnCurrentPage(int row)
    {
        if (row < 0 || row >= kAccountGuardRowsPerPage)
        {
            return -1;
        }

        const int slot = (g_accountGuardWindowPage * kAccountGuardRowsPerPage) + row;
        return slot < AccountCharacterList::MaxCharacters ? slot : -1;
    }

    void NormalizeAccountGuardPage()
    {
        const int pageCount = GetAccountGuardPageCount();
        g_accountGuardWindowPage = std::clamp(g_accountGuardWindowPage, 0, pageCount - 1);
    }

    bool IsCurrentAccountGuardHero(const AccountCharacterList::Entry& entry)
    {
        return Hero != nullptr && std::wcscmp(entry.Name, Hero->ID) == 0;
    }

    bool AreAllAccountGuardAutoRespawnsEnabled()
    {
        bool hasGuard = false;
        for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
        {
            const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot);
            if (entry == nullptr || IsCurrentAccountGuardHero(*entry))
            {
                continue;
            }

            hasGuard = true;
            AccountCompanionClient::GuardStatus status{};
            AccountCompanionClient::GetGuardStatus(entry->Slot, status);
            if (!status.AutoSummon)
            {
                return false;
            }
        }

        return hasGuard;
    }

    int SetAllAccountGuardAutoRespawns(bool enabled)
    {
        int updatedCount = 0;
        for (int slot = 0; slot < AccountCharacterList::MaxCharacters; ++slot)
        {
            const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot);
            if (entry == nullptr || IsCurrentAccountGuardHero(*entry))
            {
                continue;
            }

            if (AccountCompanionClient::SetAutoSummon(entry->Slot, enabled))
            {
                ++updatedCount;
            }
        }

        return updatedCount;
    }

    void OpenAccountGuardWindow()
    {
        NormalizeAccountGuardPage();
        AccountCompanionClient::RequestPlanInfo();
        g_lastAccountGuardPlanRequestTick = GetTickCount64();
        g_isAccountGuardWindowOpen = true;
        g_isAccountGuardFormationWindowOpen = false;
        g_isCommandsWindowOpen = false;
        g_isGameSettingsWindowOpen = false;
        g_isAzothWindowOpen = false;
        g_isJewelBankWindowOpen = false;
    }

    void CloseAccountGuardWindow()
    {
        g_isAccountGuardWindowOpen = false;
        g_selectedAccountGuardSlot = -1;
        g_lastAccountGuardPlanRequestTick = 0;
    }

    void OpenAccountGuardFormationWindow()
    {
        EnsureCharacterConfigStateForCurrentHero();
        g_isAccountGuardFormationWindowOpen = true;
        g_isAccountGuardWindowOpen = false;
        g_isCommandsWindowOpen = false;
        g_isGameSettingsWindowOpen = false;
        g_isAzothWindowOpen = false;
        g_isJewelBankWindowOpen = false;
    }

    void CloseAccountGuardFormationWindow()
    {
        g_isAccountGuardFormationWindowOpen = false;
    }

    void OpenCommandsWindow()
    {
        AccountCompanionClient::RequestPlanInfo();
        g_isCommandsWindowOpen = true;
        g_isAccountGuardWindowOpen = false;
        g_isAccountGuardFormationWindowOpen = false;
        g_isGameSettingsWindowOpen = false;
        g_isAzothWindowOpen = false;
        g_isJewelBankWindowOpen = false;
    }

    void CloseCommandsWindow()
    {
        g_isCommandsWindowOpen = false;
    }

    void OpenGameSettingsWindow()
    {
        g_isGameSettingsWindowOpen = true;
        g_isAccountGuardWindowOpen = false;
        g_isAccountGuardFormationWindowOpen = false;
        g_isCommandsWindowOpen = false;
        g_isAzothWindowOpen = false;
        g_isJewelBankWindowOpen = false;
    }

    void CloseGameSettingsWindow()
    {
        g_isGameSettingsWindowOpen = false;
    }

    void OpenAzothWindow()
    {
        g_isAzothWindowOpen = true;
        g_isAccountGuardWindowOpen = false;
        g_isAccountGuardFormationWindowOpen = false;
        g_isCommandsWindowOpen = false;
        g_isGameSettingsWindowOpen = false;
        g_isJewelBankWindowOpen = false;
        RefreshAzothPendingAutoSettings(true);
        AzothClient::RequestBalance();
    }

    void CloseAzothWindow()
    {
        g_isAzothWindowOpen = false;
    }

    void OpenJewelBankWindow()
    {
        g_isJewelBankWindowOpen = true;
        g_jewelBankWindowPage = 0;
        g_isAccountGuardWindowOpen = false;
        g_isAccountGuardFormationWindowOpen = false;
        g_isCommandsWindowOpen = false;
        g_isGameSettingsWindowOpen = false;
        g_isAzothWindowOpen = false;
        JewelBankClient::RequestSync();
    }

    void CloseJewelBankWindow()
    {
        g_isJewelBankWindowOpen = false;
    }

    bool IsAccountGuardSlotSummoned(int slot)
    {
        return AccountCompanionClient::IsSummoned(slot);
    }

    bool SetAccountGuardSlotSummoned(int slot, bool isSummoned)
    {
        if (isSummoned)
        {
            return AccountCompanionClient::Summon(slot);
        }
        else
        {
            return AccountCompanionClient::Store(slot);
        }
    }

    bool IsAccountGuardHudBlockedByForegroundWindow()
    {
        if (g_isAccountGuardWindowOpen || g_isAccountGuardFormationWindowOpen || g_isCommandsWindowOpen || g_isGameSettingsWindowOpen || g_isAzothWindowOpen || g_isJewelBankWindowOpen)
        {
            return true;
        }

        if (g_pNewUISystem == nullptr)
        {
            return false;
        }

        static constexpr DWORD kBlockingInterfaces[] =
        {
            SEASON3B::INTERFACE_INVENTORY,
            SEASON3B::INTERFACE_INVENTORY_EXT,
            SEASON3B::INTERFACE_STORAGE,
            SEASON3B::INTERFACE_STORAGE_EXT,
            SEASON3B::INTERFACE_CHARACTER,
            SEASON3B::INTERFACE_NPCSHOP,
            SEASON3B::INTERFACE_MIXINVENTORY,
            SEASON3B::INTERFACE_TRADE,
            SEASON3B::INTERFACE_MYSHOP_INVENTORY,
            SEASON3B::INTERFACE_PURCHASESHOP_INVENTORY,
            SEASON3B::INTERFACE_WINDOW_MENU,
            SEASON3B::INTERFACE_OPTION,
            SEASON3B::INTERFACE_COMMAND,
            SEASON3B::INTERFACE_QUICK_COMMAND,
            SEASON3B::INTERFACE_MOVEMAP,
            SEASON3B::INTERFACE_INGAMESHOP,
            SEASON3B::INTERFACE_MUHELPER,
            SEASON3B::INTERFACE_MUHELPER_EXT,
            SEASON3B::INTERFACE_MUHELPER_SKILL_LIST,
        };

        for (const DWORD interfaceKey : kBlockingInterfaces)
        {
            if (g_pNewUISystem->IsVisible(interfaceKey))
            {
                return true;
            }
        }

        return false;
    }

    std::wstring GetAccountGuardExecutableDirectory()
    {
        wchar_t exePath[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0)
        {
            return L".";
        }

        std::wstring directory(exePath);
        const std::wstring::size_type separator = directory.find_last_of(L"\\/");
        if (separator == std::wstring::npos)
        {
            return L".";
        }

        return directory.substr(0, separator);
    }

    std::string ToAccountGuardUtf8(const wchar_t* text)
    {
        if (text == nullptr || text[0] == L'\0')
        {
            return {};
        }

        const int byteCount = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        if (byteCount <= 1)
        {
            return {};
        }

        std::string output(static_cast<size_t>(byteCount - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, output.data(), byteCount, nullptr, nullptr);
        return output;
    }

    std::string EscapeAccountGuardJson(const std::string& value)
    {
        std::string output;
        output.reserve(value.size() + 8);

        for (const unsigned char character : value)
        {
            switch (character)
            {
            case '\\':
                output += "\\\\";
                break;
            case '"':
                output += "\\\"";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                if (character < 0x20)
                {
                    output += "\\u00";
                    constexpr char hex[] = "0123456789abcdef";
                    output += hex[(character >> 4) & 0x0F];
                    output += hex[character & 0x0F];
                }
                else
                {
                    output += static_cast<char>(character);
                }
                break;
            }
        }

        return output;
    }

    std::string UrlEncodeAccountGuardValue(const std::string& value)
    {
        std::string output;
        output.reserve(value.size());

        constexpr char hex[] = "0123456789ABCDEF";
        for (const unsigned char character : value)
        {
            const bool isUnreserved =
                (character >= 'A' && character <= 'Z')
                || (character >= 'a' && character <= 'z')
                || (character >= '0' && character <= '9')
                || character == '-'
                || character == '_'
                || character == '.'
                || character == '~';

            if (isUnreserved)
            {
                output += static_cast<char>(character);
                continue;
            }

            output += '%';
            output += hex[(character >> 4) & 0x0F];
            output += hex[character & 0x0F];
        }

        return output;
    }

    bool LaunchAccountGuardFormationBrowser()
    {
        std::string url = "https://muonline.pt/guard-formation";
        bool hasQuery = false;
        const auto appendQuery = [&url, &hasQuery](const char* key, const std::string& value)
        {
            if (value.empty())
            {
                return;
            }

            url += hasQuery ? "&" : "?";
            hasQuery = true;
            url += key;
            url += "=";
            url += UrlEncodeAccountGuardValue(value);
        };

        const std::string account = ToAccountGuardUtf8(m_Username);
        appendQuery("account", account);
        appendQuery("store_ticket", ToAccountGuardUtf8(g_WebStoreTicket));

        const std::wstring wideUrl(url.begin(), url.end());
        const INT_PTR result = reinterpret_cast<INT_PTR>(ShellExecuteW(
            g_hWnd,
            L"open",
            wideUrl.c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL));

        return result > 32;
    }

    bool WriteAccountGuardOverlayState(const std::wstring& dataPath)
    {
        const std::wstring executableDirectory = GetAccountGuardExecutableDirectory();
        CreateDirectoryW((executableDirectory + L"\\ui").c_str(), nullptr);
        CreateDirectoryW((executableDirectory + L"\\ui\\account-guard").c_str(), nullptr);

        std::ofstream file(std::filesystem::path(dataPath), std::ios::binary | std::ios::trunc);
        if (!file)
        {
            return false;
        }

        file << "{\n";
        file << "  \"currentHero\": \"" << EscapeAccountGuardJson(ToAccountGuardUtf8(Hero != nullptr ? Hero->ID : L"")) << "\",\n";
        file << "  \"characters\": [\n";

        const int characterCount = AccountCharacterList::GetCount();
        bool isFirst = true;
        for (int index = 0; index < characterCount; ++index)
        {
            const AccountCharacterList::Entry* entry = AccountCharacterList::GetByDisplayIndex(index);
            if (entry == nullptr)
            {
                continue;
            }

            if (!isFirst)
            {
                file << ",\n";
            }

            const bool isCurrentHero = Hero != nullptr && std::wcscmp(entry->Name, Hero->ID) == 0;
            file << "    { ";
            file << "\"slot\": " << entry->Slot << ", ";
            file << "\"name\": \"" << EscapeAccountGuardJson(ToAccountGuardUtf8(entry->Name)) << "\", ";
            file << "\"isCurrent\": " << (isCurrentHero ? "true" : "false") << ", ";
            file << "\"isSummoned\": " << (IsAccountGuardSlotSummoned(entry->Slot) ? "true" : "false");
            file << " }";

            isFirst = false;
        }

        file << "\n";
        file << "  ]\n";
        file << "}\n";
        return true;
    }

    bool LaunchAccountGuardBrowserOverlay()
    {
        const std::wstring executableDirectory = GetAccountGuardExecutableDirectory();
        const std::wstring overlayPath = executableDirectory + L"\\MuGuardOverlay.exe";
        const std::wstring dataPath = executableDirectory + L"\\ui\\account-guard\\account-guard-data.json";

        if (GetFileAttributesW(overlayPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }

        WriteAccountGuardOverlayState(dataPath);

        const auto hwndValue = static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(g_hWnd));
        const std::wstring arguments =
            L"--owner-hwnd=" + std::to_wstring(hwndValue)
            + L" --data=\"" + dataPath + L"\"";

        const INT_PTR result = reinterpret_cast<INT_PTR>(ShellExecuteW(
            g_hWnd,
            L"open",
            overlayPath.c_str(),
            arguments.c_str(),
            executableDirectory.c_str(),
            SW_SHOWNORMAL));

        return result > 32;
    }

    bool CheckAccountGuardMouseIn(float x, float y, float width, float height)
    {
        return SEASON3B::CheckMouseIn(x, y, width, height);
    }

    void DrawAccountGuardRect(float x, float y, float width, float height, float r, float g, float b, float a)
    {
        EnableAlphaTest();
        glColor4f(r, g, b, a);
        RenderColor(x, y, width, height);
        EndRenderColor();
    }

    void DrawAccountGuardOutline(float x, float y, float width, float height, float r, float g, float b, float a)
    {
        EnableAlphaTest();
        glColor4f(r, g, b, a);
        RenderColor(x, y, width, 1.f);
        RenderColor(x, y + height - 1.f, width, 1.f);
        RenderColor(x, y, 1.f, height);
        RenderColor(x + width - 1.f, y, 1.f, height);
        EndRenderColor();
    }

    void DrawAccountGuardShade(float x, float y, float width, float height, float alpha)
    {
        EnableAlphaTest();
        RenderColor(x, y, width, height, alpha, 1);
        EndRenderColor();
    }

    void DrawAccountGuardNativeFrame(float x, float y, float width, float height)
    {
        EnableAlphaTest();
        glEnable(GL_TEXTURE_2D);
        glColor4f(1.f, 1.f, 1.f, 1.f);
        RenderBitmap(kAccountGuardWindowImage, x, y, width, height, 0.f, 0.f, kAccountGuardWindowTextureU, kAccountGuardWindowTextureV);
    }

    void DrawAccountGuardTableFrame(float x, float y, float width, float height, float alpha = 0.72f)
    {
        DrawAccountGuardShade(x + 3.f, y + 3.f, width - 6.f, height - 6.f, alpha);

        EnableAlphaTest();
        glColor4f(1.f, 1.f, 1.f, 1.f);
        RenderImage(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_LEFT, x, y, 14.f, 14.f);
        RenderImage(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_RIGHT, x + width - 14.f, y, 14.f, 14.f);
        RenderImage(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_LEFT, x, y + height - 14.f, 14.f, 14.f);
        RenderImage(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_RIGHT, x + width - 14.f, y + height - 14.f, 14.f, 14.f);

        RenderImage(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_PIXEL, x + 6.f, y, width - 12.f, 14.f);
        RenderImage(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_PIXEL, x + 6.f, y + height - 14.f, width - 12.f, 14.f);
        RenderImage(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_LEFT_PIXEL, x, y + 6.f, 14.f, height - 12.f);
        RenderImage(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_RIGHT_PIXEL, x + width - 14.f, y + 6.f, 14.f, height - 12.f);
    }

    void DrawAccountGuardBorder(float x, float y, float width, float height, float r, float g, float b, float a)
    {
        DrawAccountGuardRect(x, y, width, 1.f, r, g, b, a);
        DrawAccountGuardRect(x, y + height - 1.f, width, 1.f, r, g, b, a);
        DrawAccountGuardRect(x, y, 1.f, height, r, g, b, a);
        DrawAccountGuardRect(x + width - 1.f, y, 1.f, height, r, g, b, a);
    }

    bool ProcessAccountGuardTextButton(
        float x,
        float y,
        float width,
        float height,
        const wchar_t* text,
        bool enabled = true)
    {
        const bool isHovered = enabled && CheckAccountGuardMouseIn(x, y, width, height);

        EnableAlphaTest();
        glEnable(GL_TEXTURE_2D);
        if (!enabled)
        {
            glColor4f(0.42f, 0.42f, 0.42f, 0.68f);
        }
        else if (isHovered)
        {
            glColor4f(1.14f, 1.08f, 0.96f, 1.0f);
        }
        else
        {
            glColor4f(0.92f, 0.90f, 0.84f, 0.96f);
        }

        const GLuint buttonImage = width <= 64.f ? kAccountGuardSmallButtonImage : kAccountGuardWideButtonImage;
        RenderBitmap(buttonImage, x, y, width, height, 0.f, 0.f, 1.f, 1.f);
        glColor4f(1.f, 1.f, 1.f, 1.f);

        if (isHovered)
        {
            DrawAccountGuardRect(x + 5.f, y + 4.f, width - 10.f, height - 8.f, 0.76f, 0.55f, 0.22f, 0.24f);
        }

        glEnable(GL_TEXTURE_2D);
        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetTextColor(enabled ? 245 : 130, enabled ? 224 : 130, enabled ? 145 : 130, 255);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->RenderText(
            static_cast<int>(x),
            static_cast<int>(y + 5.f),
            text,
            static_cast<int>(width),
            0,
            RT3_SORT_CENTER);

        return isHovered && MouseLButtonPush;
    }

    bool ProcessAccountGuardImageButton(
        float x,
        float y,
        float width,
        float height,
        GLuint normalImage,
        GLuint hoverImage,
        bool enabled,
        const wchar_t* tooltip)
    {
        const bool isHovered = enabled && CheckAccountGuardMouseIn(x, y, width, height);
        const GLuint image = isHovered ? hoverImage : normalImage;

        EnableAlphaTest();
        glEnable(GL_TEXTURE_2D);
        if (!enabled)
        {
            glColor4f(0.36f, 0.36f, 0.36f, 0.62f);
        }
        else
        {
            glColor4f(1.f, 1.f, 1.f, 1.f);
        }

        RenderBitmap(image, x, y, width, height, 0.f, 0.f, 1.f, 1.f);
        glColor4f(1.f, 1.f, 1.f, 1.f);

        if (isHovered && tooltip != nullptr && tooltip[0] != L'\0')
        {
            RenderTipText(static_cast<int>(x - 34.f), static_cast<int>(y + height + 3.f), tooltip);
        }

        return isHovered && MouseLButtonPush;
    }

    void RenderAccountGuardCheckbox(float x, float y, float size, bool checked, bool enabled)
    {
        DrawAccountGuardRect(x, y, size, size, 0.01f, 0.01f, 0.01f, enabled ? 0.86f : 0.48f);
        DrawAccountGuardBorder(x, y, size, size, enabled ? 0.62f : 0.34f, enabled ? 0.52f : 0.34f, enabled ? 0.30f : 0.34f, enabled ? 0.82f : 0.58f);
        if (checked)
        {
            const float inset = std::max(2.f, size * 0.25f);
            DrawAccountGuardRect(x + inset, y + inset, size - (inset * 2.f), size - (inset * 2.f), enabled ? 0.92f : 0.48f, enabled ? 0.62f : 0.48f, enabled ? 0.14f : 0.48f, 0.96f);
        }

        if (enabled && CheckAccountGuardMouseIn(x - 2.f, y - 2.f, size + 4.f, size + 4.f))
        {
            RenderTipText(static_cast<int>(x - 38.f), static_cast<int>(y + size + 4.f), L"Auto Respawn");
        }
    }

    void RenderAccountGuardModeButton(float x, float y, float width, float height, const wchar_t* text, bool selected)
    {
        if (selected)
        {
            DrawAccountGuardRect(x - 2.f, y - 2.f, width + 4.f, height + 4.f, 0.78f, 0.58f, 0.20f, 0.26f);
            DrawAccountGuardBorder(x - 2.f, y - 2.f, width + 4.f, height + 4.f, 0.80f, 0.62f, 0.28f, 0.68f);
        }

        ProcessAccountGuardTextButton(x, y, width, height, text, true);
    }

    bool IsMouseOverAccountGuardButton(const AccountGuardHudButton& button)
    {
        return SEASON3B::CheckMouseIn(
            button.X,
            button.Y,
            kAccountGuardButtonWidth,
            kAccountGuardButtonHeight);
    }

    void DrawAccountGuardButtonBox(const AccountGuardHudButton& button, bool isHovered)
    {
        const float x = button.X;
        const float y = button.Y;
        const float width = kAccountGuardButtonWidth;
        const float height = kAccountGuardButtonHeight;

        glColor4f(0.02f, 0.02f, 0.02f, 0.88f);
        RenderColor(x, y, width, height);
        EndRenderColor();

        if (isHovered)
        {
            glColor4f(0.48f, 0.34f, 0.10f, 0.88f);
        }
        else
        {
            glColor4f(0.12f, 0.12f, 0.12f, 0.82f);
        }
        RenderColor(x + 1.f, y + 1.f, width - 2.f, height - 2.f);
        EndRenderColor();

        glColor4f(0.78f, 0.64f, 0.28f, isHovered ? 1.0f : 0.72f);
        RenderColor(x + 3.f, y + 3.f, width - 6.f, 1.f);
        RenderColor(x + 3.f, y + height - 4.f, width - 6.f, 1.f);
        RenderColor(x + 3.f, y + 3.f, 1.f, height - 6.f);
        RenderColor(x + width - 4.f, y + 3.f, 1.f, height - 6.f);
        EndRenderColor();
    }

    void RenderAccountGuardHudButton(const AccountGuardHudButton& button)
    {
        const bool isHovered = IsMouseOverAccountGuardButton(button);

        GLuint image = 0;
        switch (button.Action)
        {
        case AccountGuardHudAction::OpenPersonalStore:
            image = isHovered ? kPersonalStoreButtonHoverImage : kPersonalStoreButtonImage;
            break;
        case AccountGuardHudAction::OpenSummonWindow:
            image = isHovered ? kAccountGuardSummonButtonHoverImage : kAccountGuardSummonButtonImage;
            break;
        case AccountGuardHudAction::OpenCommandsWindow:
            image = isHovered ? kCommandsButtonHoverImage : kCommandsButtonImage;
            break;
        case AccountGuardHudAction::OpenGameSettingsWindow:
            image = isHovered ? kGameSettingsButtonHoverImage : kGameSettingsButtonImage;
            break;
        case AccountGuardHudAction::OpenJewelBankWindow:
            image = isHovered ? kJewelBankButtonHoverImage : kJewelBankButtonImage;
            break;
        }

        if (image != 0)
        {
            glColor4f(1.f, 1.f, 1.f, 1.f);
            RenderBitmap(image, button.X, button.Y, kAccountGuardButtonWidth, kAccountGuardButtonHeight, 0.f, 0.f, 1.f, 1.f);

            if (isHovered)
            {
                const int tooltipX = static_cast<int>(std::min(button.X, REFERENCE_WIDTH - 128.f));
                RenderTipText(
                    tooltipX,
                    static_cast<int>(button.Y + kAccountGuardButtonHeight + 4.f),
                    button.Tooltip);
            }

            return;
        }

        DrawAccountGuardButtonBox(button, isHovered);

        glEnable(GL_TEXTURE_2D);
        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetTextColor(isHovered ? 255 : 230, isHovered ? 220 : 210, 120, 255);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->RenderText(
            static_cast<int>(button.X),
            static_cast<int>(button.Y + (kAccountGuardButtonHeight / 2.f) - 5.f),
            button.Label,
            static_cast<int>(kAccountGuardButtonWidth),
            0,
            RT3_SORT_CENTER);
        g_pRenderText->SetFont(g_hFont);

        if (isHovered)
        {
            const int tooltipX = static_cast<int>(std::min(button.X, REFERENCE_WIDTH - 128.f));
            RenderTipText(
                tooltipX,
                static_cast<int>(button.Y + kAccountGuardButtonHeight + 4.f),
                button.Tooltip);
        }
    }

    void RenderAccountGuardHudButtons()
    {
        if (IsAccountGuardHudBlockedByForegroundWindow())
        {
            return;
        }

        for (const AccountGuardHudButton& button : kAccountGuardHudButtons)
        {
            RenderAccountGuardHudButton(button);
        }
    }

    void RenderAzothDigits(float rightX, float y, unsigned long long value, float digitWidth, float digitHeight)
    {
        wchar_t text[32] = {};
        swprintf_s(text, L"%llu", std::min(value, AzothClient::MaxDisplayBalance));

        const size_t length = std::wcslen(text);
        if (length == 0)
        {
            return;
        }

        const float totalWidth = (static_cast<float>(length) * digitWidth) + (static_cast<float>(length - 1) * kAzothHudDigitGap);
        float x = rightX - totalWidth;

        EnableAlphaTest();
        glEnable(GL_TEXTURE_2D);
        glColor4f(1.f, 1.f, 1.f, 1.f);

        for (size_t index = 0; index < length; ++index)
        {
            if (text[index] < L'0' || text[index] > L'9')
            {
                continue;
            }

            const int digit = static_cast<int>(text[index] - L'0');
            const float u = static_cast<float>(digit) * kAzothDigitAtlasCellWidth;
            RenderBitmap(
                kAzothDigitImage,
                x,
                y,
                digitWidth,
                digitHeight,
                u,
                0.f,
                kAzothDigitAtlasCellWidth,
                kAzothDigitAtlasHeight);
            x += digitWidth + kAzothHudDigitGap;
        }

        glColor4f(1.f, 1.f, 1.f, 1.f);
    }

    std::wstring FormatAzothZenAmount(unsigned long long value)
    {
        wchar_t raw[32] = {};
        swprintf_s(raw, L"%llu", value);

        std::wstring formatted;
        const size_t length = std::wcslen(raw);
        formatted.reserve(length + (length / 3));

        int groupSize = 0;
        for (size_t reverseIndex = 0; reverseIndex < length; ++reverseIndex)
        {
            const wchar_t digit = raw[length - reverseIndex - 1];
            if (groupSize == 3)
            {
                formatted.insert(formatted.begin(), L'.');
                groupSize = 0;
            }

            formatted.insert(formatted.begin(), digit);
            ++groupSize;
        }

        return formatted;
    }

    std::wstring FormatAzothReserveLabel(unsigned long long value)
    {
        if (value >= 1000000000ULL && value % 1000000000ULL == 0)
        {
            wchar_t text[32] = {};
            swprintf_s(text, L"%llu bi", value / 1000000000ULL);
            return text;
        }

        if (value >= 1000000ULL && value % 1000000ULL == 0)
        {
            wchar_t text[32] = {};
            swprintf_s(text, L"%llu mi", value / 1000000ULL);
            return text;
        }

        return FormatAzothZenAmount(value);
    }

    void RenderAzothAutoButton(float x, float y, float width, float height, const wchar_t* text, bool selected)
    {
        ProcessAccountGuardTextButton(x, y, width, height, text, true);
        if (selected)
        {
            DrawAccountGuardBorder(x + 2.f, y + 2.f, width - 4.f, height - 4.f, 0.92f, 0.70f, 0.24f, 0.95f);
        }
    }

    bool IsMouseOverAzothHud()
    {
        return CheckAccountGuardMouseIn(kAzothHudX, kAzothHudY, kAzothHudWidth, kAzothHudHeight);
    }

    void RenderAzothHud()
    {
        if (IsAccountGuardHudBlockedByForegroundWindow())
        {
            return;
        }

        const bool isHovered = IsMouseOverAzothHud();
        const GLuint image = isHovered ? kAzothHudHoverImage : kAzothHudImage;

        EnableAlphaTest();
        glEnable(GL_TEXTURE_2D);
        glColor4f(1.f, 1.f, 1.f, 1.f);
        RenderBitmap(
            image,
            kAzothHudX,
            kAzothHudY,
            kAzothHudWidth,
            kAzothHudHeight,
            0.f,
            0.f,
            kAzothHudTextureU,
            kAzothHudTextureV);

        RenderAzothDigits(
            kAzothHudDigitRight,
            kAzothHudDigitY,
            AzothClient::GetBalance(),
            kAzothHudDigitWidth,
            kAzothHudDigitHeight);

        if (isHovered)
        {
            RenderTipText(static_cast<int>(kAzothHudX + 68.f), static_cast<int>(kAzothHudY - 14.f), L"Azoth");
        }
    }

    void RenderAzothWindow()
    {
        if (!g_isAzothWindowOpen)
        {
            return;
        }

        EnableAlphaTest();
        DrawAccountGuardShade(0.f, 0.f, REFERENCE_WIDTH, REFERENCE_HEIGHT, 0.38f);

        const float x = kAccountGuardWindowX;
        const float y = kAccountGuardWindowY;
        const float width = kAccountGuardWindowWidth;
        const float height = kAccountGuardWindowHeight;
        const float panelX = x + kAzothPanelX;
        const float panelY = y + kAzothPanelY;
        RefreshAzothPendingAutoSettings(false);

        DrawAccountGuardNativeFrame(x, y, width, height);

        glEnable(GL_TEXTURE_2D);
        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetTextColor(245, 225, 150, 255);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->RenderText(static_cast<int>(x), static_cast<int>(y + 17.f), L"Azoth", static_cast<int>(width), 0, RT3_SORT_CENTER);

        DrawAccountGuardTableFrame(panelX, panelY, kAzothPanelWidth, kAzothPanelHeight, 0.76f);
        DrawAccountGuardRect(panelX + 8.f, panelY + 10.f, kAzothPanelWidth - 16.f, 38.f, 0.02f, 0.02f, 0.02f, 0.70f);
        DrawAccountGuardBorder(panelX + 8.f, panelY + 10.f, kAzothPanelWidth - 16.f, 38.f, 0.58f, 0.46f, 0.20f, 0.62f);

        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetTextColor(170, 205, 205, 255);
        g_pRenderText->RenderText(static_cast<int>(panelX + 14.f), static_cast<int>(panelY + 15.f), L"Saldo global", 74, 0, RT3_SORT_LEFT);
        RenderAzothDigits(panelX + kAzothPanelWidth - 16.f, panelY + 18.f, AzothClient::GetBalance(), 8.7f, 17.f);

        g_pRenderText->SetTextColor(220, 210, 170, 255);
        g_pRenderText->RenderText(
            static_cast<int>(panelX + 10.f),
            static_cast<int>(panelY + 55.f),
            L"20.000.000 Zen = 1 Azoth",
            static_cast<int>(kAzothPanelWidth - 20.f),
            0,
            RT3_SORT_CENTER);

        g_pRenderText->SetTextColor(180, 180, 180, 255);
        g_pRenderText->RenderText(
            static_cast<int>(panelX + 10.f),
            static_cast<int>(panelY + 73.f),
            L"Manual: converter Zen agora",
            static_cast<int>(kAzothPanelWidth - 20.f),
            0,
            RT3_SORT_CENTER);

        const float firstRowY = panelY + 91.f;
        const float secondRowY = firstRowY + 25.f;
        const float thirdRowY = secondRowY + 25.f;
        ProcessAccountGuardTextButton(panelX + 10.f, firstRowY, kAzothButtonWidth, kAzothButtonHeight, L"100 mi", true);
        ProcessAccountGuardTextButton(panelX + 72.f, firstRowY, kAzothButtonWidth, kAzothButtonHeight, L"500 mi", true);
        ProcessAccountGuardTextButton(panelX + 134.f, firstRowY, kAzothButtonWidth, kAzothButtonHeight, L"1 bi", true);
        ProcessAccountGuardTextButton(panelX + 10.f, secondRowY, kAzothButtonWidth, kAzothButtonHeight, L"2 bi", true);
        ProcessAccountGuardTextButton(panelX + 72.f, secondRowY, kAzothWideButtonWidth, kAzothButtonHeight, L"Max atual", true);
        ProcessAccountGuardTextButton(panelX + 10.f, thirdRowY, kAzothWideButtonWidth, kAzothButtonHeight, L"Atualizar", true);
        ProcessAccountGuardTextButton(panelX + 108.f, thirdRowY, 82.f, kAzothButtonHeight, L"Todos", true);

        g_pRenderText->SetTextColor(180, 180, 180, 255);
        g_pRenderText->RenderText(
            static_cast<int>(panelX + 10.f),
            static_cast<int>(panelY + 169.f),
            L"Auto: acima da reserva vira Azoth",
            static_cast<int>(kAzothPanelWidth - 20.f),
            0,
            RT3_SORT_CENTER);

        const bool isAutoEnabled = g_azothPendingAutoEnabled;
        const std::wstring autoStatus = isAutoEnabled
            ? L"Reserva: " + FormatAzothReserveLabel(g_azothPendingAutoReserveZen) + L" Zen"
            : L"OFF";
        g_pRenderText->SetTextColor(isAutoEnabled ? 220 : 150, isAutoEnabled ? 210 : 150, isAutoEnabled ? 170 : 150, 255);
        g_pRenderText->RenderText(
            static_cast<int>(panelX + 10.f),
            static_cast<int>(panelY + 184.f),
            autoStatus.c_str(),
            static_cast<int>(kAzothPanelWidth - 20.f),
            0,
            RT3_SORT_CENTER);

        const float autoRowY = panelY + 199.f;
        const float autoStartX = panelX + 10.f;
        RenderAzothAutoButton(autoStartX, autoRowY, kAzothAutoButtonWidth, kAzothAutoButtonHeight, isAutoEnabled ? L"On" : L"Off", isAutoEnabled);
        RenderAzothAutoButton(autoStartX + ((kAzothAutoButtonWidth + kAzothAutoButtonGap) * 1.f), autoRowY, kAzothAutoButtonWidth, kAzothAutoButtonHeight, L"100", isAutoEnabled && g_azothPendingAutoReserveZen == 100000000ULL);
        RenderAzothAutoButton(autoStartX + ((kAzothAutoButtonWidth + kAzothAutoButtonGap) * 2.f), autoRowY, kAzothAutoButtonWidth, kAzothAutoButtonHeight, L"500", isAutoEnabled && g_azothPendingAutoReserveZen == 500000000ULL);
        RenderAzothAutoButton(autoStartX + ((kAzothAutoButtonWidth + kAzothAutoButtonGap) * 3.f), autoRowY, kAzothAutoButtonWidth, kAzothAutoButtonHeight, L"1b", isAutoEnabled && g_azothPendingAutoReserveZen == 1000000000ULL);
        RenderAzothAutoButton(autoStartX + ((kAzothAutoButtonWidth + kAzothAutoButtonGap) * 4.f), autoRowY, kAzothAutoButtonWidth, kAzothAutoButtonHeight, L"2b", isAutoEnabled && g_azothPendingAutoReserveZen == 2000000000ULL);
        ProcessAccountGuardTextButton(x + kAzothFooterApplyButtonX, y + kAccountGuardFooterCloseButtonY, kAzothFooterButtonWidth, kAccountGuardFooterButtonHeight, L"Aplicar", true);
        ProcessAccountGuardTextButton(x + kAzothFooterCloseButtonX, y + kAccountGuardFooterCloseButtonY, kAzothFooterButtonWidth, kAccountGuardFooterButtonHeight, L"Close", true);

        DisableAlphaBlend();
    }

    void RenderAccountGuardMiniPartyList()
    {
        if (Hero == nullptr || IsAccountGuardHudBlockedByForegroundWindow())
        {
            return;
        }

        AccountCompanionClient::MiniPartyEntry entries[AccountCharacterList::MaxCharacters]{};
        const int entryCount = AccountCompanionClient::CopyMiniPartyEntries(entries, AccountCharacterList::MaxCharacters);
        if (entryCount <= 0)
        {
            return;
        }

        const float rowStep = kAccountGuardMiniListHeight + kAccountGuardMiniListRowGap;
        const float x = kAzothHudX + kAzothHudWidth - kAccountGuardMiniListWidth;
        const float bottomY = kAzothHudY - 6.f;
        const int maxRowsBySpace = std::max(1, static_cast<int>((bottomY - 14.f) / rowStep));
        int renderCount = std::min(entryCount, std::min(kAccountGuardMiniListMaxRows, maxRowsBySpace));
        if (entryCount > renderCount && renderCount >= maxRowsBySpace && renderCount > 1)
        {
            --renderCount;
        }

        const bool hasMoreRow = entryCount > renderCount;
        const float totalRows = static_cast<float>(renderCount + (hasMoreRow ? 1 : 0));
        const float y = std::max(14.f, bottomY - (totalRows * rowStep));

        EnableAlphaTest();
        glColor4f(1.f, 1.f, 1.f, 1.f);

        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetBgColor(0, 0, 0, 0);

        for (int index = 0; index < renderCount; ++index)
        {
            const AccountCompanionClient::MiniPartyEntry& entry = entries[index];
            const float rowY = y + (static_cast<float>(index) * rowStep);

            glColor4f(0.f, 0.f, 0.f, 0.88f);
            RenderColor(x + 1.f, rowY + 1.f, kAccountGuardMiniListWidth - 2.f, kAccountGuardMiniListHeight - 4.f);
            EndRenderColor();

            if (!entry.Visible || entry.StepHP <= 0)
            {
                glColor4f(0.35f, 0.f, 0.f, 0.42f);
                RenderColor(x + 1.f, rowY + 1.f, kAccountGuardMiniListWidth - 2.f, kAccountGuardMiniListHeight - 4.f);
                EndRenderColor();
            }

            glColor4f(1.f, 1.f, 1.f, 1.f);
            RenderImage(SEASON3B::CNewUIPartyListWindow::IMAGE_PARTY_LIST_BACK, x, rowY, kAccountGuardMiniListWidth, kAccountGuardMiniListHeight);

            if (index == 0)
            {
                RenderImage(SEASON3B::CNewUIPartyListWindow::IMAGE_PARTY_LIST_FLAG, x + 43.f, rowY + 2.f, 7.f, 8.f);
                g_pRenderText->SetTextColor(entry.OfflinePlay ? RGBA(80, 255, 120, 255) : (entry.Visible ? RGBA(255, 190, 80, 255) : RGBA(128, 90, 70, 255)));
            }
            else
            {
                g_pRenderText->SetTextColor(entry.OfflinePlay ? RGBA(80, 255, 120, 255) : (entry.Visible ? RGBA(235, 235, 235, 255) : RGBA(140, 140, 140, 255)));
            }

            g_pRenderText->RenderText(
                static_cast<int>(x + 3.f),
                static_cast<int>(rowY + 2.f),
                entry.Name,
                index == 0 ? 39 : 52,
                0,
                RT3_SORT_LEFT);

            const int hpStep = std::clamp(entry.StepHP, 0, 10);
            const float hpWidth = (static_cast<float>(hpStep) / 10.f) * kAccountGuardMiniListHpBarWidth;
            if (hpWidth > 0.f)
            {
                RenderImage(
                    SEASON3B::CNewUIPartyListWindow::IMAGE_PARTY_LIST_HPBAR,
                    x + 4.f,
                    rowY + 12.f,
                    hpWidth,
                    kAccountGuardMiniListHpBarHeight);
            }
        }

        if (hasMoreRow)
        {
            wchar_t moreText[32]{};
            mu_swprintf(moreText, L"+%d Sent.", entryCount - renderCount);
            const float rowY = y + (static_cast<float>(renderCount) * rowStep);
            g_pRenderText->SetTextColor(230, 210, 120, 255);
            g_pRenderText->RenderText(
                static_cast<int>(x),
                static_cast<int>(rowY + 4.f),
                moreText,
                static_cast<int>(kAccountGuardMiniListWidth),
                0,
                RT3_SORT_CENTER);
        }

        glColor4f(1.f, 1.f, 1.f, 1.f);
        DisableAlphaBlend();
    }

    void RenderAccountGuardWindow()
    {
        if (!g_isAccountGuardWindowOpen)
        {
            return;
        }

        NormalizeAccountGuardPage();
        const unsigned long long nowTick = GetTickCount64();
        if (g_lastAccountGuardPlanRequestTick == 0 || nowTick - g_lastAccountGuardPlanRequestTick >= 2000ULL)
        {
            AccountCompanionClient::RequestPlanInfo();
            g_lastAccountGuardPlanRequestTick = nowTick;
        }

        EnableAlphaTest();
        DrawAccountGuardShade(0.f, 0.f, REFERENCE_WIDTH, REFERENCE_HEIGHT, 0.38f);

        const float x = kAccountGuardWindowX;
        const float y = kAccountGuardWindowY;
        const float width = kAccountGuardWindowWidth;
        const float height = kAccountGuardWindowHeight;

        DrawAccountGuardNativeFrame(x, y, width, height);

        glEnable(GL_TEXTURE_2D);
        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetTextColor(245, 225, 150, 255);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->RenderText(static_cast<int>(x), static_cast<int>(y + 17.f), L"Sentinelas", static_cast<int>(width), 0, RT3_SORT_CENTER);

        const int loadedCharacterCount = AccountCharacterList::GetCount();
        const bool allAutoRespawnsEnabled = AreAllAccountGuardAutoRespawnsEnabled();

        DrawAccountGuardRect(x + kAccountGuardTableX, y + kAccountGuardTableY, kAccountGuardTableWidth, kAccountGuardTableHeight, 0.0f, 0.0f, 0.0f, 0.72f);
        DrawAccountGuardBorder(x + kAccountGuardTableX, y + kAccountGuardTableY, kAccountGuardTableWidth, kAccountGuardTableHeight, 0.58f, 0.52f, 0.42f, 0.62f);
        DrawAccountGuardRect(x + kAccountGuardTableX + 4.f, y + kAccountGuardHeaderY, kAccountGuardTableWidth - 8.f, 18.f, 0.0f, 0.0f, 0.0f, 0.76f);

        glEnable(GL_TEXTURE_2D);
        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetTextColor(180, 180, 180, 255);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->RenderText(static_cast<int>(x + kAccountGuardNameColumnX), static_cast<int>(y + kAccountGuardHeaderY + 2.f), L"Name", static_cast<int>(kAccountGuardNameColumnWidth), 0, RT3_SORT_LEFT);
        g_pRenderText->RenderText(static_cast<int>(x + kAccountGuardCallButtonX - 2.f), static_cast<int>(y + kAccountGuardHeaderY + 2.f), L"C", static_cast<int>(kAccountGuardRowButtonWidth + 4.f), 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(static_cast<int>(x + kAccountGuardStoreButtonX - 2.f), static_cast<int>(y + kAccountGuardHeaderY + 2.f), L"S", static_cast<int>(kAccountGuardRowButtonWidth + 4.f), 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(static_cast<int>(x + kAccountGuardSwitchButtonX - 4.f), static_cast<int>(y + kAccountGuardHeaderY + 2.f), L"Swap", static_cast<int>(kAccountGuardSwitchButtonSize + 8.f), 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(static_cast<int>(x + kAccountGuardFarmButtonX - 2.f), static_cast<int>(y + kAccountGuardHeaderY + 2.f), L"F", static_cast<int>(kAccountGuardModeButtonSize + 4.f), 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(static_cast<int>(x + kAccountGuardFollowButtonX - 2.f), static_cast<int>(y + kAccountGuardHeaderY + 2.f), L"L", static_cast<int>(kAccountGuardModeButtonSize + 4.f), 0, RT3_SORT_CENTER);
        RenderAccountGuardCheckbox(
            x + kAccountGuardAutoRespawnCheckboxX,
            y + kAccountGuardHeaderY + 4.f,
            kAccountGuardAutoRespawnCheckboxSize,
            allAutoRespawnsEnabled,
            loadedCharacterCount > 0);

        if (loadedCharacterCount == 0)
        {
            g_pRenderText->SetTextColor(210, 210, 210, 255);
            g_pRenderText->RenderText(
                static_cast<int>(x + 24.f),
                static_cast<int>(kAccountGuardRowStartY + 28.f),
                L"No account characters were received yet.",
                static_cast<int>(width - 48.f),
                0,
                RT3_SORT_CENTER);
            g_pRenderText->SetTextColor(160, 160, 160, 255);
            g_pRenderText->RenderText(
                static_cast<int>(x + 24.f),
                static_cast<int>(kAccountGuardRowStartY + 46.f),
                L"Reconnect to refresh the local list.",
                static_cast<int>(width - 48.f),
                0,
                RT3_SORT_CENTER);
        }
        else
        {
            for (int row = 0; row < kAccountGuardRowsPerPage; ++row)
            {
                const int slot = GetAccountGuardSlotOnCurrentPage(row);
                const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot);
                const float rowY = kAccountGuardRowStartY + (row * kAccountGuardRowHeight);
                if (entry == nullptr)
                {
                    DrawAccountGuardShade(x + 21.f, rowY + 1.f, width - 42.f, kAccountGuardRowHeight - 4.f, 0.28f);
                    DrawAccountGuardRect(
                        x + 21.f,
                        rowY + kAccountGuardRowHeight - 3.f,
                        width - 42.f,
                        1.f,
                        0.20f,
                        0.18f,
                        0.12f,
                        0.54f);
                    g_pRenderText->SetTextColor(110, 110, 110, 255);
                    g_pRenderText->RenderText(static_cast<int>(x + kAccountGuardNameColumnX), static_cast<int>(rowY + 8.f), L"Empty", static_cast<int>(kAccountGuardNameColumnWidth), 0, RT3_SORT_LEFT);
                    ProcessAccountGuardImageButton(x + kAccountGuardCallButtonX, rowY + 3.f, kAccountGuardRowButtonWidth, kAccountGuardRowButtonHeight, kAccountGuardCallButtonImage, kAccountGuardCallButtonHoverImage, false, L"Invocar");
                    ProcessAccountGuardImageButton(x + kAccountGuardStoreButtonX, rowY + 3.f, kAccountGuardRowButtonWidth, kAccountGuardRowButtonHeight, kAccountGuardStoreButtonImage, kAccountGuardStoreButtonHoverImage, false, L"Guardar");
                    ProcessAccountGuardImageButton(x + kAccountGuardSwitchButtonX, rowY + 3.f, kAccountGuardSwitchButtonSize, kAccountGuardSwitchButtonSize, kAccountGuardSwitchButtonImage, kAccountGuardSwitchButtonHoverImage, false, L"Switch Character");
                    ProcessAccountGuardImageButton(x + kAccountGuardFarmButtonX, rowY + 3.f, kAccountGuardModeButtonSize, kAccountGuardModeButtonSize, kAccountGuardFarmButtonImage, kAccountGuardFarmButtonHoverImage, false, L"Farm Here");
                    ProcessAccountGuardImageButton(x + kAccountGuardFollowButtonX, rowY + 3.f, kAccountGuardModeButtonSize, kAccountGuardModeButtonSize, kAccountGuardFollowButtonImage, kAccountGuardFollowButtonHoverImage, false, L"Follow Leader");
                    RenderAccountGuardCheckbox(x + kAccountGuardAutoRespawnCheckboxX, rowY + 7.f, kAccountGuardAutoRespawnCheckboxSize, false, false);
                    continue;
                }

                AccountCompanionClient::GuardStatus guardStatus{};
                AccountCompanionClient::GetGuardStatus(entry->Slot, guardStatus);
                const int cooldownSeconds = guardStatus.CooldownSeconds;
                const bool isSummoned = IsAccountGuardSlotSummoned(entry->Slot) || guardStatus.Active;
                const bool isOfflinePlaying = AccountCompanionClient::IsOfflinePlaying(entry->Slot);
                const bool isSelected = entry->Slot == g_selectedAccountGuardSlot || isSummoned;
                const bool isCurrentHero = IsCurrentAccountGuardHero(*entry);

                DrawAccountGuardShade(x + 21.f, rowY + 1.f, width - 42.f, kAccountGuardRowHeight - 4.f, isSelected ? 0.78f : 0.48f);
                DrawAccountGuardRect(
                    x + 21.f,
                    rowY + kAccountGuardRowHeight - 3.f,
                    width - 42.f,
                    1.f,
                    isSelected ? 0.74f : 0.28f,
                    isSelected ? 0.56f : 0.23f,
                    isSelected ? 0.24f : 0.14f,
                    0.72f);

                if (isOfflinePlaying)
                {
                    g_pRenderText->SetTextColor(80, 255, 120, 255);
                }
                else
                {
                    g_pRenderText->SetTextColor(isCurrentHero ? 120 : 235, isCurrentHero ? 190 : 235, isCurrentHero ? 255 : 235, 255);
                }
                g_pRenderText->RenderText(static_cast<int>(x + kAccountGuardNameColumnX), static_cast<int>(rowY + 8.f), entry->Name, static_cast<int>(kAccountGuardNameColumnWidth), 0, RT3_SORT_LEFT);

                const bool canCall = !isCurrentHero && !isSummoned && cooldownSeconds <= 0 && AccountCompanionClient::CanSummonMore();
                const bool canSwitch = !isCurrentHero;
                ProcessAccountGuardImageButton(x + kAccountGuardCallButtonX, rowY + 3.f, kAccountGuardRowButtonWidth, kAccountGuardRowButtonHeight, kAccountGuardCallButtonImage, kAccountGuardCallButtonHoverImage, canCall, L"Invocar");
                if (cooldownSeconds > 0)
                {
                    wchar_t cooldownLabel[8]{};
                    mu_swprintf(cooldownLabel, L"%d", cooldownSeconds);
                    DrawAccountGuardRect(x + kAccountGuardCallButtonX + 2.f, rowY + 9.f, kAccountGuardRowButtonWidth - 4.f, 8.f, 0.0f, 0.0f, 0.0f, 0.66f);
                    g_pRenderText->SetFont(g_hFont);
                    g_pRenderText->SetTextColor(245, 224, 145, 255);
                    g_pRenderText->SetBgColor(0);
                    g_pRenderText->RenderText(static_cast<int>(x + kAccountGuardCallButtonX), static_cast<int>(rowY + 8.f), cooldownLabel, static_cast<int>(kAccountGuardRowButtonWidth), 0, RT3_SORT_CENTER);
                }

                ProcessAccountGuardImageButton(x + kAccountGuardStoreButtonX, rowY + 3.f, kAccountGuardRowButtonWidth, kAccountGuardRowButtonHeight, kAccountGuardStoreButtonImage, kAccountGuardStoreButtonHoverImage, isSummoned, L"Guardar");
                ProcessAccountGuardImageButton(x + kAccountGuardSwitchButtonX, rowY + 3.f, kAccountGuardSwitchButtonSize, kAccountGuardSwitchButtonSize, kAccountGuardSwitchButtonImage, kAccountGuardSwitchButtonHoverImage, canSwitch, L"Switch Character");
                const bool canEditGuardMode = !isCurrentHero;
                const bool farmMode = guardStatus.Mode == AccountCompanionClient::CommandMode::FarmPosition;
                ProcessAccountGuardImageButton(x + kAccountGuardFarmButtonX, rowY + 3.f, kAccountGuardModeButtonSize, kAccountGuardModeButtonSize, kAccountGuardFarmButtonImage, kAccountGuardFarmButtonHoverImage, canEditGuardMode, L"Farm Here");
                ProcessAccountGuardImageButton(x + kAccountGuardFollowButtonX, rowY + 3.f, kAccountGuardModeButtonSize, kAccountGuardModeButtonSize, kAccountGuardFollowButtonImage, kAccountGuardFollowButtonHoverImage, canEditGuardMode, L"Follow Leader");
                if (canEditGuardMode)
                {
                    if (farmMode)
                    {
                        DrawAccountGuardBorder(x + kAccountGuardFarmButtonX - 1.f, rowY + 2.5f, kAccountGuardModeButtonSize + 2.f, kAccountGuardModeButtonSize + 2.f, 0.86f, 0.64f, 0.20f, 0.86f);
                    }
                    else
                    {
                        DrawAccountGuardBorder(x + kAccountGuardFollowButtonX - 1.f, rowY + 2.5f, kAccountGuardModeButtonSize + 2.f, kAccountGuardModeButtonSize + 2.f, 0.86f, 0.64f, 0.20f, 0.86f);
                    }
                }

                RenderAccountGuardCheckbox(x + kAccountGuardAutoRespawnCheckboxX, rowY + 7.f, kAccountGuardAutoRespawnCheckboxSize, guardStatus.AutoSummon, canEditGuardMode);
            }
        }

        const int pageCount = GetAccountGuardPageCount();
        wchar_t pageText[32]{};
        mu_swprintf(pageText, L"%d / %d", g_accountGuardWindowPage + 1, pageCount);
        g_pRenderText->SetTextColor(220, 220, 220, 255);
        g_pRenderText->RenderText(static_cast<int>(x + kAccountGuardPageTextX), static_cast<int>(y + kAccountGuardPageTextY), pageText, 44, 0, RT3_SORT_CENTER);

        ProcessAccountGuardTextButton(x + kAccountGuardPagePrevButtonX, y + kAccountGuardPageButtonY, 32.f, 22.f, L"<", g_accountGuardWindowPage > 0);
        ProcessAccountGuardTextButton(x + kAccountGuardPageNextButtonX, y + kAccountGuardPageButtonY, 32.f, 22.f, L">", g_accountGuardWindowPage < pageCount - 1);
        ProcessAccountGuardTextButton(x + kAccountGuardFooterLeftButtonX, y + kAccountGuardFooterButtonY, kAccountGuardFooterButtonWidth, kAccountGuardFooterButtonHeight, L"Invocar Todas", loadedCharacterCount > 0 && AccountCompanionClient::CanSummonMore());
        ProcessAccountGuardTextButton(x + kAccountGuardFooterRightButtonX, y + kAccountGuardFooterButtonY, kAccountGuardFooterButtonWidth, kAccountGuardFooterButtonHeight, L"Guardar Todas", loadedCharacterCount > 0);
        ProcessAccountGuardTextButton(x + kAccountGuardFooterCloseButtonX, y + kAccountGuardFooterCloseButtonY, kAccountGuardFooterCloseButtonWidth, kAccountGuardFooterButtonHeight, L"Close", true);

        DisableAlphaBlend();
    }

    void RenderCharacterConfigCheckbox(float x, float y, bool checked)
    {
        DrawAccountGuardRect(x, y, 11.f, 11.f, 0.01f, 0.01f, 0.01f, 0.86f);
        DrawAccountGuardBorder(x, y, 11.f, 11.f, 0.62f, 0.52f, 0.30f, 0.82f);
        if (checked)
        {
            DrawAccountGuardRect(x + 3.f, y + 3.f, 5.f, 5.f, 0.92f, 0.62f, 0.14f, 0.96f);
        }
    }

    void RenderCharacterConfigToggleRow(float x, float y, float width, const wchar_t* label, bool checked)
    {
        DrawAccountGuardShade(x, y, width, kCharacterConfigRowHeight - 1.f, checked ? 0.64f : 0.42f);
        DrawAccountGuardRect(x, y + kCharacterConfigRowHeight - 1.f, width, 1.f, 0.44f, 0.36f, 0.18f, 0.46f);

        glEnable(GL_TEXTURE_2D);
        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->SetTextColor(checked ? RGBA(245, 225, 150, 255) : RGBA(198, 198, 198, 255));
        g_pRenderText->RenderText(static_cast<int>(x + 18.f), static_cast<int>(y + 5.f), label, static_cast<int>(width - 22.f), 0, RT3_SORT_LEFT);
        RenderCharacterConfigCheckbox(x + 5.f, y + 4.f, checked);
    }

    void RenderCharacterConfigColumn(float x, float y, const wchar_t* title)
    {
        DrawAccountGuardTableFrame(x, y, kCharacterConfigColumnWidth, kCharacterConfigPanelHeight, 0.70f);
        DrawAccountGuardRect(x + 7.f, y + 24.f, kCharacterConfigColumnWidth - 14.f, 1.f, 0.52f, 0.42f, 0.18f, 0.54f);
        glEnable(GL_TEXTURE_2D);
        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetTextColor(245, 225, 150, 255);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->RenderText(static_cast<int>(x + 8.f), static_cast<int>(y + 10.f), title, static_cast<int>(kCharacterConfigColumnWidth - 16.f), 0, RT3_SORT_LEFT);
    }

    void RenderCharacterConfigPointRow(float x, float y, const wchar_t* label, int value)
    {
        DrawAccountGuardShade(x, y, kCharacterConfigColumnWidth - 18.f, 21.f, 0.45f);
        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetTextColor(220, 220, 220, 255);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->RenderText(static_cast<int>(x + 8.f), static_cast<int>(y + 6.f), label, 52, 0, RT3_SORT_LEFT);

        wchar_t valueText[16] = {};
        swprintf_s(valueText, L"%d", value);
        g_pRenderText->SetTextColor(245, 225, 150, 255);
        g_pRenderText->RenderText(static_cast<int>(x + 70.f), static_cast<int>(y + 6.f), valueText, 28, 0, RT3_SORT_CENTER);

        ProcessAccountGuardTextButton(x + 104.f, y + 1.f, kCharacterConfigStepButtonSize, kCharacterConfigStepButtonSize, L"-", true);
        ProcessAccountGuardTextButton(x + 127.f, y + 1.f, kCharacterConfigStepButtonSize, kCharacterConfigStepButtonSize, L"+", true);
    }

    void RenderCharacterConfigCommandButton(float x, float y, float width, const wchar_t* title, const wchar_t* detail, bool selected, bool enabled)
    {
        DrawAccountGuardShade(x, y, width, 37.f, selected ? 0.76f : (enabled ? 0.48f : 0.28f));
        DrawAccountGuardBorder(x, y, width, 37.f, selected ? 0.86f : 0.42f, selected ? 0.64f : 0.34f, 0.20f, selected ? 0.78f : 0.42f);

        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->SetTextColor(enabled ? RGBA(245, 225, 150, 255) : RGBA(120, 120, 120, 255));
        g_pRenderText->RenderText(static_cast<int>(x + 8.f), static_cast<int>(y + 7.f), title, static_cast<int>(width - 16.f), 0, RT3_SORT_LEFT);

        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetTextColor(enabled ? RGBA(190, 190, 190, 255) : RGBA(95, 95, 95, 255));
        g_pRenderText->RenderText(static_cast<int>(x + 8.f), static_cast<int>(y + 21.f), detail, static_cast<int>(width - 16.f), 0, RT3_SORT_LEFT);
    }

    void RenderAccountGuardFormationWindow()
    {
        if (!g_isAccountGuardFormationWindowOpen)
        {
            return;
        }

        EnsureCharacterConfigStateForCurrentHero();

        EnableAlphaTest();
        DrawAccountGuardShade(0.f, 0.f, REFERENCE_WIDTH, REFERENCE_HEIGHT, 0.34f);

        const float x = kCharacterConfigWindowX;
        const float y = kCharacterConfigWindowY;
        const float width = kCharacterConfigWindowWidth;
        const float height = kCharacterConfigWindowHeight;
        const float panelY = y + kCharacterConfigPanelY;
        const float leftX = x + kCharacterConfigLeftX;
        const float centerX = x + kCharacterConfigCenterX;
        const float rightX = x + kCharacterConfigRightX;

        DrawAccountGuardNativeFrame(x, y, width, height);

        glEnable(GL_TEXTURE_2D);
        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetTextColor(245, 225, 150, 255);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->RenderText(static_cast<int>(x), static_cast<int>(y + 17.f), L"Configuracao do Personagem", static_cast<int>(width), 0, RT3_SORT_CENTER);

        if (Hero != nullptr && Hero->ID[0] != L'\0')
        {
            g_pRenderText->SetFont(g_hFont);
            g_pRenderText->SetTextColor(180, 210, 230, 255);
            g_pRenderText->RenderText(static_cast<int>(x), static_cast<int>(y + 34.f), Hero->ID, static_cast<int>(width), 0, RT3_SORT_CENTER);
        }

        RenderCharacterConfigColumn(leftX, panelY, L"Itens");
        float rowY = panelY + 34.f;
        RenderCharacterConfigToggleRow(leftX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Ativado", g_characterConfigState.Enabled);
        rowY += kCharacterConfigRowHeight + 2.f;
        RenderCharacterConfigToggleRow(leftX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Apenas drops proprios", g_characterConfigState.PickOnlyOwnedItems);
        rowY += kCharacterConfigRowHeight + 2.f;
        RenderCharacterConfigToggleRow(leftX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Zen", g_characterConfigState.PickZen);
        rowY += kCharacterConfigRowHeight + 2.f;
        RenderCharacterConfigToggleRow(leftX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Itens comuns", g_characterConfigState.PickCommonItems);
        rowY += kCharacterConfigRowHeight + 2.f;
        RenderCharacterConfigToggleRow(leftX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Apenas itens do char", g_characterConfigState.PickMatchingCharacterItems);
        rowY += kCharacterConfigRowHeight + 2.f;
        RenderCharacterConfigToggleRow(leftX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Joias", g_characterConfigState.PickJewels);
        rowY += kCharacterConfigRowHeight + 2.f;
        RenderCharacterConfigToggleRow(leftX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Ancient", g_characterConfigState.PickAncient);
        rowY += kCharacterConfigRowHeight + 2.f;
        RenderCharacterConfigToggleRow(leftX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Excellent", g_characterConfigState.PickExcellent);
        rowY += kCharacterConfigRowHeight + 2.f;
        RenderCharacterConfigToggleRow(leftX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Itens pelo nome", g_characterConfigState.PickNamedItems);
        rowY += kCharacterConfigRowHeight + 2.f;
        RenderCharacterConfigToggleRow(leftX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Zen para Azoth", g_characterConfigState.AutoConvertZenToAzoth);

        RenderCharacterConfigColumn(centerX, panelY, L"Reset e Pontos");
        rowY = panelY + 34.f;
        RenderCharacterConfigToggleRow(centerX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Auto Reset", g_characterConfigState.AutoReset);
        rowY += 25.f;
        ProcessAccountGuardTextButton(centerX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, 22.f, L"Reset Now", true);
        rowY += 33.f;

        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetTextColor(180, 180, 180, 255);
        g_pRenderText->RenderText(static_cast<int>(centerX + 10.f), static_cast<int>(rowY), L"Distribuicao automatica", static_cast<int>(kCharacterConfigColumnWidth - 20.f), 0, RT3_SORT_CENTER);
        rowY += 15.f;
        RenderCharacterConfigPointRow(centerX + 9.f, rowY, L"STR", g_characterConfigState.Strength);
        rowY += 23.f;
        RenderCharacterConfigPointRow(centerX + 9.f, rowY, L"AGI", g_characterConfigState.Agility);
        rowY += 23.f;
        RenderCharacterConfigPointRow(centerX + 9.f, rowY, L"VIT", g_characterConfigState.Vitality);
        rowY += 23.f;
        RenderCharacterConfigPointRow(centerX + 9.f, rowY, L"ENE", g_characterConfigState.Energy);
        rowY += 23.f;
        RenderCharacterConfigPointRow(centerX + 9.f, rowY, L"CMD", g_characterConfigState.Command);

        RenderCharacterConfigColumn(rightX, panelY, L"Comando");
        rowY = panelY + 34.f;
        RenderCharacterConfigCommandButton(rightX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Seguir lider", L"Padrao das Sentinelas.", g_characterConfigState.CommandMode == CharacterConfigCommandMode::Follow, true);
        rowY += 43.f;
        RenderCharacterConfigCommandButton(rightX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Ficar na cidade", L"Permanece em area segura.", g_characterConfigState.CommandMode == CharacterConfigCommandMode::StayCity, true);
        rowY += 43.f;
        wchar_t farmPositionText[48] = {};
        swprintf_s(
            farmPositionText,
            L"Farmar aqui (%d,%d)",
            GetCurrentHeroXForCharacterConfig(),
            GetCurrentHeroYForCharacterConfig());
        RenderCharacterConfigCommandButton(rightX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, farmPositionText, L"Salva o ponto atual de farm.", g_characterConfigState.CommandMode == CharacterConfigCommandMode::FarmPosition, true);
        rowY += 43.f;
        RenderCharacterConfigCommandButton(rightX + 9.f, rowY, kCharacterConfigColumnWidth - 18.f, L"Negociar com IAs", L"Em desenvolvimento.", false, false);

        ProcessAccountGuardTextButton(x + kCharacterConfigFooterCloseButtonX, y + kCharacterConfigFooterButtonY, kCharacterConfigFooterCloseButtonWidth, kAccountGuardFooterButtonHeight, L"Close", true);
        ProcessAccountGuardTextButton(x + kCharacterConfigSaveButtonX, y + kCharacterConfigFooterButtonY, kCharacterConfigSaveButtonWidth, kAccountGuardFooterButtonHeight, L"Salvar no Personagem", true);

        DisableAlphaBlend();
    }

    enum class GamePerformanceOption
    {
        HideWorldObjects,
        DisableHeavyEffects,
        ReduceCharacterGlow,
        HideWings,
        HideMountsPets,
        SimplifyOtherPlayers
    };

    struct GamePerformanceOptionRow
    {
        const wchar_t* Label;
        GamePerformanceOption Option;
    };

    const GamePerformanceOptionRow kGamePerformanceOptions[] =
    {
        { L"Objetos do cenario", GamePerformanceOption::HideWorldObjects },
        { L"Efeitos pesados", GamePerformanceOption::DisableHeavyEffects },
        { L"Brilho dos chars", GamePerformanceOption::ReduceCharacterGlow },
        { L"Asas", GamePerformanceOption::HideWings },
        { L"Montarias e pets", GamePerformanceOption::HideMountsPets },
        { L"Outros jogadores simples", GamePerformanceOption::SimplifyOtherPlayers },
    };

    bool GetGamePerformanceOption(GamePerformanceOption option)
    {
        GameConfig& config = GameConfig::GetInstance();

        switch (option)
        {
        case GamePerformanceOption::HideWorldObjects:
            return config.GetHideWorldObjects();
        case GamePerformanceOption::DisableHeavyEffects:
            return config.GetDisableHeavyEffects();
        case GamePerformanceOption::ReduceCharacterGlow:
            return config.GetReduceCharacterGlow();
        case GamePerformanceOption::HideWings:
            return config.GetHideWings();
        case GamePerformanceOption::HideMountsPets:
            return config.GetHideMountsPets();
        case GamePerformanceOption::SimplifyOtherPlayers:
            return config.GetSimplifyOtherPlayers();
        }

        return false;
    }

    void SetGamePerformanceOption(GamePerformanceOption option, bool enabled)
    {
        GameConfig& config = GameConfig::GetInstance();

        switch (option)
        {
        case GamePerformanceOption::HideWorldObjects:
            config.SetHideWorldObjects(enabled);
            break;
        case GamePerformanceOption::DisableHeavyEffects:
            config.SetDisableHeavyEffects(enabled);
            break;
        case GamePerformanceOption::ReduceCharacterGlow:
            config.SetReduceCharacterGlow(enabled);
            break;
        case GamePerformanceOption::HideWings:
            config.SetHideWings(enabled);
            break;
        case GamePerformanceOption::HideMountsPets:
            config.SetHideMountsPets(enabled);
            break;
        case GamePerformanceOption::SimplifyOtherPlayers:
            config.SetSimplifyOtherPlayers(enabled);
            break;
        }
    }

    bool IsGameFastPresetEnabled()
    {
        for (const GamePerformanceOptionRow& row : kGamePerformanceOptions)
        {
            if (!GetGamePerformanceOption(row.Option))
            {
                return false;
            }
        }

        return true;
    }

    bool IsGameNormalPresetEnabled()
    {
        for (const GamePerformanceOptionRow& row : kGamePerformanceOptions)
        {
            if (GetGamePerformanceOption(row.Option))
            {
                return false;
            }
        }

        return true;
    }

    void ApplyGamePerformanceSettingsToClient()
    {
        GameConfig& config = GameConfig::GetInstance();

        if (g_pOption != nullptr)
        {
            g_pOption->SetRenderAllEffects(!config.GetDisableHeavyEffects());
            g_pOption->SetRenderLevel(config.GetReduceCharacterGlow() ? 0 : 4);
        }

        if (config.GetHideMountsPets())
        {
            for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
            {
                DeleteMount(&CharactersClient[i].Object);
            }
        }
    }

    void SyncGamePerformanceSettingsToServer()
    {
        if (Hero == nullptr)
        {
            return;
        }

        wchar_t command[96] = {};
        swprintf_s(command, L"/gamesettings save %u", GameConfig::GetInstance().GetGamePerformanceFlags());
        SendMacroChat(command);
    }

    void SaveGamePerformanceSettings()
    {
        GameConfig::GetInstance().Save();
        ApplyGamePerformanceSettingsToClient();
        SyncGamePerformanceSettingsToServer();
    }

    void SetGamePerformancePreset(bool fast)
    {
        for (const GamePerformanceOptionRow& row : kGamePerformanceOptions)
        {
            SetGamePerformanceOption(row.Option, fast);
        }

        SaveGamePerformanceSettings();
    }

    void RenderGameSettingsCheckbox(float x, float y, bool checked)
    {
        DrawAccountGuardRect(x, y, kGameSettingsCheckboxSize, kGameSettingsCheckboxSize, 0.01f, 0.01f, 0.01f, 0.86f);
        DrawAccountGuardBorder(x, y, kGameSettingsCheckboxSize, kGameSettingsCheckboxSize, 0.62f, 0.52f, 0.30f, 0.82f);

        if (checked)
        {
            DrawAccountGuardRect(x + 3.f, y + 3.f, kGameSettingsCheckboxSize - 6.f, kGameSettingsCheckboxSize - 6.f, 0.90f, 0.62f, 0.16f, 0.96f);
        }
    }

    void RenderGameSettingsOptionRow(float x, float y, const GamePerformanceOptionRow& row)
    {
        const bool enabled = GetGamePerformanceOption(row.Option);

        DrawAccountGuardShade(x + 9.f, y, kGameSettingsPanelWidth - 18.f, kGameSettingsOptionRowHeight, enabled ? 0.70f : 0.42f);
        DrawAccountGuardRect(x + 9.f, y + kGameSettingsOptionRowHeight - 1.f, kGameSettingsPanelWidth - 18.f, 1.f, 0.44f, 0.36f, 0.18f, 0.52f);

        glEnable(GL_TEXTURE_2D);
        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->SetTextColor(enabled ? RGBA(245, 225, 150, 255) : RGBA(210, 210, 210, 255));
        g_pRenderText->RenderText(
            static_cast<int>(x + 15.f),
            static_cast<int>(y + 6.f),
            row.Label,
            static_cast<int>(kGameSettingsPanelWidth - 45.f),
            0,
            RT3_SORT_LEFT);

        RenderGameSettingsCheckbox(
            x + kGameSettingsPanelWidth - 27.f,
            y + 5.f,
            enabled);
    }

    void SendCommandsMenuCommand(const wchar_t* action)
    {
        if (Hero == nullptr || action == nullptr || action[0] == L'\0')
        {
            ShowAccountGuardSystemMessage(L"Entre no jogo antes de usar comandos.");
            return;
        }

        wchar_t command[64] = {};
        swprintf_s(command, L"/comandos %ls", action);
        SendMacroChat(command);
    }

    void RenderCommandsWindow()
    {
        if (!g_isCommandsWindowOpen)
        {
            return;
        }

        EnableAlphaTest();
        DrawAccountGuardShade(0.f, 0.f, REFERENCE_WIDTH, REFERENCE_HEIGHT, 0.34f);

        const float x = kAccountGuardWindowX;
        const float y = kAccountGuardWindowY;
        const float width = kAccountGuardWindowWidth;
        const float height = kAccountGuardWindowHeight;
        const float panelX = x + kCommandsPanelX;
        const float panelY = y + kCommandsPanelY;

        DrawAccountGuardNativeFrame(x, y, width, height);

        glEnable(GL_TEXTURE_2D);
        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetTextColor(245, 225, 150, 255);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->RenderText(static_cast<int>(x), static_cast<int>(y + 17.f), L"Comandos", static_cast<int>(width), 0, RT3_SORT_CENTER);

        DrawAccountGuardRect(panelX, panelY, kCommandsPanelWidth, kCommandsPanelHeight, 0.f, 0.f, 0.f, 0.72f);
        DrawAccountGuardBorder(panelX, panelY, kCommandsPanelWidth, kCommandsPanelHeight, 0.58f, 0.52f, 0.42f, 0.62f);

        const float buttonX = panelX + ((kCommandsPanelWidth - kCommandsButtonWidth) / 2.f);
        float buttonY = panelY + 18.f;
        ProcessAccountGuardTextButton(buttonX, buttonY, kCommandsButtonWidth, kCommandsButtonHeight, L"Reset", true);
        buttonY += kCommandsButtonHeight + kCommandsButtonGap;
        ProcessAccountGuardTextButton(buttonX, buttonY, kCommandsButtonWidth, kCommandsButtonHeight, L"Limpar Inventario", true);
        buttonY += kCommandsButtonHeight + kCommandsButtonGap;
        ProcessAccountGuardTextButton(buttonX, buttonY, kCommandsButtonWidth, kCommandsButtonHeight, L"Evoluir Classe", true);
        if (AccountCompanionClient::CanPlayOffline())
        {
            buttonY += kCommandsButtonHeight + kCommandsButtonGap;
            ProcessAccountGuardTextButton(buttonX, buttonY, kCommandsButtonWidth, kCommandsButtonHeight, L"Play Offline", true);
        }

        ProcessAccountGuardTextButton(x + kAccountGuardFooterCloseButtonX, y + kAccountGuardFooterCloseButtonY, kAccountGuardFooterCloseButtonWidth, kAccountGuardFooterButtonHeight, L"Close", true);

        DisableAlphaBlend();
    }

    void RenderGameSettingsWindow()
    {
        if (!g_isGameSettingsWindowOpen)
        {
            return;
        }

        EnableAlphaTest();
        DrawAccountGuardShade(0.f, 0.f, REFERENCE_WIDTH, REFERENCE_HEIGHT, 0.34f);

        const float x = kAccountGuardWindowX;
        const float y = kAccountGuardWindowY;
        const float width = kAccountGuardWindowWidth;
        const float height = kAccountGuardWindowHeight;
        const float panelX = x + kGameSettingsPanelX;
        const float panelY = y + kGameSettingsPanelY;

        DrawAccountGuardNativeFrame(x, y, width, height);

        glEnable(GL_TEXTURE_2D);
        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetTextColor(245, 225, 150, 255);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->RenderText(static_cast<int>(x), static_cast<int>(y + 17.f), L"Configuracoes do Jogo", static_cast<int>(width), 0, RT3_SORT_CENTER);

        DrawAccountGuardRect(panelX, panelY, kGameSettingsPanelWidth, kGameSettingsPanelHeight, 0.f, 0.f, 0.f, 0.72f);
        DrawAccountGuardBorder(panelX, panelY, kGameSettingsPanelWidth, kGameSettingsPanelHeight, 0.58f, 0.52f, 0.42f, 0.62f);

        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetTextColor(220, 220, 220, 255);
        g_pRenderText->RenderText(static_cast<int>(panelX + 12.f), static_cast<int>(panelY + 12.f), L"Modo grafico", 180, 0, RT3_SORT_LEFT);

        const float presetY = panelY + 31.f;
        RenderAccountGuardModeButton(panelX + 13.f, presetY, kGameSettingsPresetButtonWidth, kGameSettingsPresetButtonHeight, L"Normal", IsGameNormalPresetEnabled());
        RenderAccountGuardModeButton(panelX + 113.f, presetY, kGameSettingsPresetButtonWidth, kGameSettingsPresetButtonHeight, L"Rapido", IsGameFastPresetEnabled());

        DrawAccountGuardRect(panelX + 10.f, panelY + 63.f, kGameSettingsPanelWidth - 20.f, 1.f, 0.52f, 0.42f, 0.18f, 0.54f);

        float rowY = panelY + 68.f;
        for (const GamePerformanceOptionRow& row : kGamePerformanceOptions)
        {
            RenderGameSettingsOptionRow(panelX, rowY, row);
            rowY += kGameSettingsOptionRowHeight + kGameSettingsOptionRowGap;
        }

        ProcessAccountGuardTextButton(x + kAccountGuardFooterCloseButtonX, y + kAccountGuardFooterCloseButtonY, kAccountGuardFooterCloseButtonWidth, kAccountGuardFooterButtonHeight, L"Close", true);

        DisableAlphaBlend();
    }

    void RenderJewelBankWindow()
    {
        if (!g_isJewelBankWindowOpen)
        {
            return;
        }

        EnableAlphaTest();
        DrawAccountGuardShade(0.f, 0.f, REFERENCE_WIDTH, REFERENCE_HEIGHT, 0.38f);

        const float x = kAccountGuardWindowX;
        const float y = kAccountGuardWindowY;
        const float width = kAccountGuardWindowWidth;
        const float height = kAccountGuardWindowHeight;
        const float panelX = x + kJewelBankPanelX;
        const float panelY = y + kJewelBankPanelY;

        DrawAccountGuardNativeFrame(x, y, width, height);

        glEnable(GL_TEXTURE_2D);
        g_pRenderText->SetFont(g_hFontBold);
        g_pRenderText->SetTextColor(245, 225, 150, 255);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->RenderText(static_cast<int>(x), static_cast<int>(y + 17.f), L"Banco de Joias", static_cast<int>(width), 0, RT3_SORT_CENTER);

        DrawAccountGuardTableFrame(panelX, panelY, kJewelBankPanelWidth, kJewelBankPanelHeight, 0.76f);

        const float autoRowY = panelY + 9.f;
        DrawAccountGuardShade(panelX + 8.f, autoRowY, kJewelBankPanelWidth - 16.f, 22.f, JewelBankClient::IsAutoStoreEnabled() ? 0.70f : 0.42f);
        DrawAccountGuardRect(panelX + 8.f, autoRowY + 21.f, kJewelBankPanelWidth - 16.f, 1.f, 0.44f, 0.36f, 0.18f, 0.52f);
        g_pRenderText->SetFont(g_hFont);
        g_pRenderText->SetTextColor(JewelBankClient::IsAutoStoreEnabled() ? RGBA(245, 225, 150, 255) : RGBA(210, 210, 210, 255));
        g_pRenderText->RenderText(static_cast<int>(panelX + 15.f), static_cast<int>(autoRowY + 7.f), L"Guardar joias automaticamente", static_cast<int>(kJewelBankPanelWidth - 47.f), 0, RT3_SORT_LEFT);
        RenderGameSettingsCheckbox(panelX + kJewelBankPanelWidth - 28.f, autoRowY + 6.f, JewelBankClient::IsAutoStoreEnabled());

        const float headerY = y + kJewelBankTableHeaderY;
        g_pRenderText->SetTextColor(190, 170, 105, 255);
        g_pRenderText->RenderText(static_cast<int>(x + kJewelBankModelColumnX - 2.f), static_cast<int>(headerY), L"3D", 26, 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(static_cast<int>(x + kJewelBankNameColumnX), static_cast<int>(headerY), L"Joia", 62, 0, RT3_SORT_LEFT);
        g_pRenderText->RenderText(static_cast<int>(x + kJewelBankCountColumnX), static_cast<int>(headerY), L"Qtd.", 40, 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(static_cast<int>(x + kJewelBankStoreColumnX - 4.f), static_cast<int>(headerY), L"Guardar", 38, 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(static_cast<int>(x + kJewelBankWithdrawColumnX - 4.f), static_cast<int>(headerY), L"Sacar", 34, 0, RT3_SORT_CENTER);
        DrawAccountGuardRect(panelX + 8.f, headerY + 14.f, kJewelBankPanelWidth - 16.f, 1.f, 0.52f, 0.42f, 0.18f, 0.54f);

        g_jewelBankWindowPage = std::clamp(g_jewelBankWindowPage, 0, kJewelBankPageCount - 1);
        const int firstIndex = g_jewelBankWindowPage * kJewelBankRowsPerPage;
        for (int visibleRow = 0; visibleRow < kJewelBankRowsPerPage; ++visibleRow)
        {
            const int index = firstIndex + visibleRow;
            if (index >= JewelBankClient::JewelCount)
            {
                break;
            }

            const float rowX = panelX + 8.f;
            const float rowY = y + kJewelBankFirstRowY + (static_cast<float>(visibleRow) * kJewelBankRowHeight);
            const float modelX = x + kJewelBankModelColumnX;
            const float modelY = rowY + ((kJewelBankRowHeight - kJewelBankIconSize) / 2.f) - 1.f;
            const JewelBankUiEntry& entry = kJewelBankUiEntries[index];

            DrawAccountGuardShade(rowX, rowY, kJewelBankPanelWidth - 16.f, kJewelBankRowHeight - 2.f, 0.46f);
            DrawAccountGuardRect(rowX, rowY + kJewelBankRowHeight - 3.f, kJewelBankPanelWidth - 16.f, 1.f, 0.44f, 0.36f, 0.18f, 0.48f);
            DrawAccountGuardShade(modelX, modelY, kJewelBankIconSize, kJewelBankIconSize, 0.18f);
            DrawAccountGuardOutline(modelX, modelY, kJewelBankIconSize, kJewelBankIconSize, 0.56f, 0.47f, 0.22f, 0.55f);

            g_pRenderText->SetFont(g_hFont);
            g_pRenderText->SetTextColor(220, 220, 220, 255);
            g_pRenderText->SetBgColor(0);
            g_pRenderText->RenderText(
                static_cast<int>(x + kJewelBankNameColumnX),
                static_cast<int>(rowY + 8.f),
                entry.Label,
                62,
                0,
                RT3_SORT_LEFT);

            RenderAzothDigits(
                x + kJewelBankCountColumnX + 34.f,
                rowY + 17.f,
                JewelBankClient::GetCount(index),
                4.3f,
                8.2f);

            ProcessAccountGuardImageButton(
                x + kJewelBankStoreColumnX,
                rowY + 5.f,
                kJewelBankRowButtonSize,
                kJewelBankRowButtonSize,
                kJewelBankFooterStoreImage,
                kJewelBankFooterStoreHoverImage,
                true,
                L"Guardar esta joia");

            ProcessAccountGuardImageButton(
                x + kJewelBankWithdrawColumnX,
                rowY + 5.f,
                kJewelBankRowButtonSize,
                kJewelBankRowButtonSize,
                kJewelBankFooterWithdrawImage,
                kJewelBankFooterWithdrawHoverImage,
                true,
                L"Sacar esta joia");
        }

        wchar_t pageText[32] = {};
        mu_swprintf(pageText, L"%d/%d", g_jewelBankWindowPage + 1, kJewelBankPageCount);
        ProcessAccountGuardTextButton(x + kJewelBankPagePrevButtonX, y + kJewelBankPageButtonY, kJewelBankPageButtonWidth, kAccountGuardFooterButtonHeight, L"Prev", g_jewelBankWindowPage > 0);
        g_pRenderText->SetTextColor(230, 210, 140, 255);
        g_pRenderText->RenderText(static_cast<int>(x + kJewelBankPageTextX), static_cast<int>(y + kJewelBankPageButtonY + 6.f), pageText, 56, 0, RT3_SORT_CENTER);
        ProcessAccountGuardTextButton(x + kJewelBankPageNextButtonX, y + kJewelBankPageButtonY, kJewelBankPageButtonWidth, kAccountGuardFooterButtonHeight, L"Next", g_jewelBankWindowPage + 1 < kJewelBankPageCount);
        ProcessAccountGuardTextButton(x + kJewelBankFooterRefreshButtonX, y + kJewelBankFooterButtonY, kJewelBankFooterTextButtonWidth, kAccountGuardFooterButtonHeight, L"Atualizar", true);
        ProcessAccountGuardTextButton(x + kJewelBankFooterCloseButtonX, y + kJewelBankFooterButtonY, kJewelBankFooterTextButtonWidth, kAccountGuardFooterButtonHeight, L"Close", true);

        DisableAlphaBlend();
    }

    void RenderJewelBankWindow3D()
    {
        if (!g_isJewelBankWindowOpen)
        {
            return;
        }

        const float x = kAccountGuardWindowX;
        const float y = kAccountGuardWindowY;
        const int page = std::clamp(g_jewelBankWindowPage, 0, kJewelBankPageCount - 1);
        const int firstIndex = page * kJewelBankRowsPerPage;

        for (int visibleRow = 0; visibleRow < kJewelBankRowsPerPage; ++visibleRow)
        {
            const int index = firstIndex + visibleRow;
            if (index >= JewelBankClient::JewelCount)
            {
                break;
            }

            const JewelBankUiEntry& entry = kJewelBankUiEntries[index];
            const float rowY = y + kJewelBankFirstRowY + (static_cast<float>(visibleRow) * kJewelBankRowHeight);
            const float modelX = x + kJewelBankModelColumnX;
            const float modelY = rowY + ((kJewelBankRowHeight - kJewelBankIconSize) / 2.f) - 1.f;

            glColor4f(1.f, 1.f, 1.f, 1.f);
            RenderItem3D(modelX, modelY, kJewelBankIconSize, kJewelBankIconSize, entry.ItemType, 0, 0, 0, false);
        }
    }

    void HandleAccountGuardCharacterAction(const AccountCharacterList::Entry& entry)
    {
        if (!SetAccountGuardSlotSummoned(entry.Slot, true))
        {
            wchar_t limitMessage[128]{};
            mu_swprintf(limitMessage, L"Limite de Sentinelas atingido: %d ativas.", AccountCompanionClient::GetSummonLimit());
            ShowAccountGuardSystemMessage(limitMessage);
            return;
        }

        g_selectedAccountGuardSlot = entry.Slot;

        wchar_t message[128]{};
        mu_swprintf(message, L"Sentinela invocada: %ls.", entry.Name);
        ShowAccountGuardSystemMessage(message);
    }

    void HandleAccountGuardStoreAction(const AccountCharacterList::Entry& entry)
    {
        SetAccountGuardSlotSummoned(entry.Slot, false);
        if (g_selectedAccountGuardSlot == entry.Slot)
        {
            g_selectedAccountGuardSlot = -1;
        }

        wchar_t message[128]{};
        mu_swprintf(message, L"Sentinela guardada: %ls.", entry.Name);
        ShowAccountGuardSystemMessage(message);
    }

    void HandleAccountGuardSwitchAction(const AccountCharacterList::Entry& entry)
    {
        if (!AccountCompanionClient::SwitchTo(entry.Slot))
        {
            ShowAccountGuardSystemMessage(L"Nao foi possivel trocar o lider das Sentinelas.");
            return;
        }

        wchar_t message[128]{};
        mu_swprintf(message, L"Switching character to %ls.", entry.Name);
        ShowAccountGuardSystemMessage(message);
        CloseAccountGuardWindow();
    }

    bool ProcessAccountGuardWindowMouseEvents()
    {
        if (!g_isAccountGuardWindowOpen)
        {
            return false;
        }

        MouseOnWindow = true;

        const float x = kAccountGuardWindowX;
        const float y = kAccountGuardWindowY;
        const float width = kAccountGuardWindowWidth;
        const float height = kAccountGuardWindowHeight;

        if (!MouseLButtonPush)
        {
            return true;
        }

        const int loadedCharacterCount = AccountCharacterList::GetCount();
        const int pageCount = GetAccountGuardPageCount();

        if (CheckAccountGuardMouseIn(x + kAccountGuardCloseButtonX, y + kAccountGuardCloseButtonY, kAccountGuardCloseButtonSize, kAccountGuardCloseButtonSize)
            || CheckAccountGuardMouseIn(x + kAccountGuardFooterCloseButtonX, y + kAccountGuardFooterCloseButtonY, kAccountGuardFooterCloseButtonWidth, kAccountGuardFooterButtonHeight))
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            CloseAccountGuardWindow();
            PlayBuffer(SOUND_CLICK01);
            return true;
        }

        if (CheckAccountGuardMouseIn(x + kAccountGuardPagePrevButtonX, y + kAccountGuardPageButtonY, 32.f, 22.f) && g_accountGuardWindowPage > 0)
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            --g_accountGuardWindowPage;
            PlayBuffer(SOUND_CLICK01);
            return true;
        }

        if (CheckAccountGuardMouseIn(x + kAccountGuardPageNextButtonX, y + kAccountGuardPageButtonY, 32.f, 22.f) && g_accountGuardWindowPage < pageCount - 1)
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            ++g_accountGuardWindowPage;
            PlayBuffer(SOUND_CLICK01);
            return true;
        }

        if (CheckAccountGuardMouseIn(
                x + kAccountGuardAutoRespawnCheckboxX - 2.f,
                y + kAccountGuardHeaderY + 2.f,
                kAccountGuardAutoRespawnCheckboxSize + 4.f,
                kAccountGuardAutoRespawnCheckboxSize + 4.f)
            && loadedCharacterCount > 0)
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            const bool enabled = !AreAllAccountGuardAutoRespawnsEnabled();
            const int updatedCount = SetAllAccountGuardAutoRespawns(enabled);
            if (updatedCount > 0)
            {
                wchar_t message[128]{};
                mu_swprintf(message, L"Auto Respawn %ls para %d Sentinelas.", enabled ? L"ativado" : L"desativado", updatedCount);
                ShowAccountGuardSystemMessage(message);
                AccountCompanionClient::RequestPlanInfo();
                g_lastAccountGuardPlanRequestTick = GetTickCount64();
            }
            else
            {
                ShowAccountGuardSystemMessage(L"Nenhuma Sentinela disponivel para Auto Respawn.");
            }

            PlayBuffer(SOUND_CLICK01);
            return true;
        }

        if (CheckAccountGuardMouseIn(x + kAccountGuardFooterLeftButtonX, y + kAccountGuardFooterButtonY, kAccountGuardFooterButtonWidth, kAccountGuardFooterButtonHeight) && loadedCharacterCount > 0 && AccountCompanionClient::CanSummonMore())
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            int calledCount = 0;
            for (int slot = 0; slot < AccountCharacterList::MaxCharacters && AccountCompanionClient::CanSummonMore(); ++slot)
            {
                const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot);
                if (entry == nullptr)
                {
                    continue;
                }

                const bool isCurrentHero = Hero != nullptr && std::wcscmp(entry->Name, Hero->ID) == 0;
                if (!isCurrentHero)
                {
                    if (SetAccountGuardSlotSummoned(entry->Slot, true))
                    {
                        ++calledCount;
                    }
                }
            }

            wchar_t message[128]{};
            mu_swprintf(message, L"%d Sentinelas invocadas. Limite: %d ativas.", calledCount, AccountCompanionClient::GetSummonLimit());
            ShowAccountGuardSystemMessage(message);
            PlayBuffer(SOUND_CLICK01);
            return true;
        }

        if (CheckAccountGuardMouseIn(x + kAccountGuardFooterRightButtonX, y + kAccountGuardFooterButtonY, kAccountGuardFooterButtonWidth, kAccountGuardFooterButtonHeight) && loadedCharacterCount > 0)
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            AccountCompanionClient::StoreAll();
            g_selectedAccountGuardSlot = -1;
            ShowAccountGuardSystemMessage(L"Todas as Sentinelas foram guardadas.");
            PlayBuffer(SOUND_CLICK01);
            return true;
        }

        for (int row = 0; row < kAccountGuardRowsPerPage; ++row)
        {
            const int slot = GetAccountGuardSlotOnCurrentPage(row);
            const AccountCharacterList::Entry* entry = AccountCharacterList::GetBySlot(slot);
            if (entry == nullptr)
            {
                continue;
            }

            const float rowY = kAccountGuardRowStartY + (row * kAccountGuardRowHeight);
            const bool isCurrentHero = IsCurrentAccountGuardHero(*entry);
            const bool isCallClicked = CheckAccountGuardMouseIn(x + kAccountGuardCallButtonX, rowY + 3.f, kAccountGuardRowButtonWidth, kAccountGuardRowButtonHeight);
            const bool isStoreClicked = CheckAccountGuardMouseIn(x + kAccountGuardStoreButtonX, rowY + 3.f, kAccountGuardRowButtonWidth, kAccountGuardRowButtonHeight);
            const bool isSwitchClicked = CheckAccountGuardMouseIn(x + kAccountGuardSwitchButtonX, rowY + 3.f, kAccountGuardSwitchButtonSize, kAccountGuardSwitchButtonSize);
            const bool isFarmClicked = CheckAccountGuardMouseIn(x + kAccountGuardFarmButtonX, rowY + 3.f, kAccountGuardModeButtonSize, kAccountGuardModeButtonSize);
            const bool isFollowClicked = CheckAccountGuardMouseIn(x + kAccountGuardFollowButtonX, rowY + 3.f, kAccountGuardModeButtonSize, kAccountGuardModeButtonSize);
            const bool isAutoRespawnClicked = CheckAccountGuardMouseIn(
                x + kAccountGuardAutoRespawnCheckboxX - 2.f,
                rowY + 5.f,
                kAccountGuardAutoRespawnCheckboxSize + 4.f,
                kAccountGuardAutoRespawnCheckboxSize + 4.f);
            const bool isRowClicked = CheckAccountGuardMouseIn(x + 21.f, rowY + 1.f, width - 42.f, kAccountGuardRowHeight - 4.f);
            AccountCompanionClient::GuardStatus guardStatus{};
            AccountCompanionClient::GetGuardStatus(entry->Slot, guardStatus);
            const int cooldownSeconds = guardStatus.CooldownSeconds;
            const bool isSummoned = IsAccountGuardSlotSummoned(entry->Slot) || guardStatus.Active;
            if (!isCallClicked && !isStoreClicked && !isSwitchClicked && !isFarmClicked && !isFollowClicked && !isAutoRespawnClicked && !isRowClicked)
            {
                continue;
            }

            MouseLButtonPush = false;
            MouseLButton = false;

            if (isCallClicked && isCurrentHero)
            {
                g_selectedAccountGuardSlot = entry->Slot;
                ShowAccountGuardSystemMessage(L"O personagem ativo nao precisa ser invocado como Sentinela.");
            }
            else if (isCallClicked && cooldownSeconds > 0)
            {
                g_selectedAccountGuardSlot = entry->Slot;
                wchar_t message[128]{};
                mu_swprintf(message, L"Cooldown da Sentinela: %d segundos.", cooldownSeconds);
                ShowAccountGuardSystemMessage(message);
            }
            else if (isSwitchClicked && isCurrentHero)
            {
                g_selectedAccountGuardSlot = entry->Slot;
                ShowAccountGuardSystemMessage(L"This character is already the active leader.");
            }
            else if (isSwitchClicked)
            {
                HandleAccountGuardSwitchAction(*entry);
            }
            else if ((isFarmClicked || isFollowClicked || isAutoRespawnClicked) && isCurrentHero)
            {
                g_selectedAccountGuardSlot = entry->Slot;
                ShowAccountGuardSystemMessage(L"O lider ativo nao pode ser configurado como Sentinela.");
            }
            else if (isFarmClicked)
            {
                g_selectedAccountGuardSlot = entry->Slot;
                if (AccountCompanionClient::SetFarmHere(entry->Slot))
                {
                    wchar_t message[128]{};
                    mu_swprintf(message, L"Sentinela %ls: farmar aqui.", entry->Name);
                    ShowAccountGuardSystemMessage(message);
                    AccountCompanionClient::RequestPlanInfo();
                    g_lastAccountGuardPlanRequestTick = GetTickCount64();
                }
            }
            else if (isFollowClicked)
            {
                g_selectedAccountGuardSlot = entry->Slot;
                if (AccountCompanionClient::SetFollowLeader(entry->Slot))
                {
                    wchar_t message[128]{};
                    mu_swprintf(message, L"Sentinela %ls: seguir lider.", entry->Name);
                    ShowAccountGuardSystemMessage(message);
                    AccountCompanionClient::RequestPlanInfo();
                    g_lastAccountGuardPlanRequestTick = GetTickCount64();
                }
            }
            else if (isAutoRespawnClicked)
            {
                g_selectedAccountGuardSlot = entry->Slot;
                const bool enabled = !guardStatus.AutoSummon;
                if (AccountCompanionClient::SetAutoSummon(entry->Slot, enabled))
                {
                    wchar_t message[128]{};
                    mu_swprintf(message, L"Sentinela %ls: Auto Respawn %ls.", entry->Name, enabled ? L"on" : L"off");
                    ShowAccountGuardSystemMessage(message);
                    AccountCompanionClient::RequestPlanInfo();
                    g_lastAccountGuardPlanRequestTick = GetTickCount64();
                }
            }
            else if (isCallClicked && !isSummoned)
            {
                HandleAccountGuardCharacterAction(*entry);
            }
            else if (isStoreClicked && isSummoned)
            {
                HandleAccountGuardStoreAction(*entry);
            }
            else if (isRowClicked)
            {
                g_selectedAccountGuardSlot = entry->Slot;
            }

            PlayBuffer(SOUND_CLICK01);
            return true;
        }

        if (CheckAccountGuardMouseIn(x, y, width, height))
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            return true;
        }

        MouseLButtonPush = false;
        MouseLButton = false;
        return true;
    }

    bool ProcessAccountGuardFormationWindowMouseEvents()
    {
        if (!g_isAccountGuardFormationWindowOpen)
        {
            return false;
        }

        MouseOnWindow = true;

        EnsureCharacterConfigStateForCurrentHero();

        const float x = kCharacterConfigWindowX;
        const float y = kCharacterConfigWindowY;
        const float width = kCharacterConfigWindowWidth;
        const float height = kCharacterConfigWindowHeight;
        const float panelY = y + kCharacterConfigPanelY;
        const float leftX = x + kCharacterConfigLeftX + 9.f;
        const float centerX = x + kCharacterConfigCenterX + 9.f;
        const float rightX = x + kCharacterConfigRightX + 9.f;
        const float rowWidth = kCharacterConfigColumnWidth - 18.f;

        if (!MouseLButtonPush)
        {
            return true;
        }

        auto acceptClick = []()
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            PlayBuffer(SOUND_CLICK01);
        };

        if (CheckAccountGuardMouseIn(x + kCharacterConfigCloseButtonX, y + kCharacterConfigCloseButtonY, kAccountGuardCloseButtonSize, kAccountGuardCloseButtonSize)
            || CheckAccountGuardMouseIn(x + kCharacterConfigFooterCloseButtonX, y + kCharacterConfigFooterButtonY, kCharacterConfigFooterCloseButtonWidth, kAccountGuardFooterButtonHeight))
        {
            acceptClick();
            CloseAccountGuardFormationWindow();
            return true;
        }

        if (CheckAccountGuardMouseIn(x + kCharacterConfigSaveButtonX, y + kCharacterConfigFooterButtonY, kCharacterConfigSaveButtonWidth, kAccountGuardFooterButtonHeight))
        {
            acceptClick();
            SendCharacterConfigToServer();
            return true;
        }

        float rowY = panelY + 34.f;
        bool* toggleRows[] =
        {
            &g_characterConfigState.Enabled,
            &g_characterConfigState.PickOnlyOwnedItems,
            &g_characterConfigState.PickZen,
            &g_characterConfigState.PickCommonItems,
            &g_characterConfigState.PickMatchingCharacterItems,
            &g_characterConfigState.PickJewels,
            &g_characterConfigState.PickAncient,
            &g_characterConfigState.PickExcellent,
            &g_characterConfigState.PickNamedItems,
            &g_characterConfigState.AutoConvertZenToAzoth,
        };

        for (bool* toggle : toggleRows)
        {
            if (CheckAccountGuardMouseIn(leftX, rowY, rowWidth, kCharacterConfigRowHeight))
            {
                acceptClick();
                *toggle = !*toggle;
                return true;
            }

            rowY += kCharacterConfigRowHeight + 2.f;
        }

        rowY = panelY + 34.f;
        if (CheckAccountGuardMouseIn(centerX, rowY, rowWidth, kCharacterConfigRowHeight))
        {
            acceptClick();
            g_characterConfigState.AutoReset = !g_characterConfigState.AutoReset;
            return true;
        }

        rowY += 25.f;
        if (CheckAccountGuardMouseIn(centerX, rowY, rowWidth, 22.f))
        {
            acceptClick();
            wchar_t resetCommand[] = L"/autoreset now";
            SendMacroChat(resetCommand);
            ShowAccountGuardSystemMessage(L"Reset manual solicitado.");
            return true;
        }

        rowY += 48.f;
        for (int index = 0; index < 5; ++index)
        {
            if (CheckAccountGuardMouseIn(centerX + 104.f, rowY + 1.f, kCharacterConfigStepButtonSize, kCharacterConfigStepButtonSize))
            {
                acceptClick();
                AdjustCharacterConfigPointValue(index, -5);
                return true;
            }

            if (CheckAccountGuardMouseIn(centerX + 127.f, rowY + 1.f, kCharacterConfigStepButtonSize, kCharacterConfigStepButtonSize))
            {
                acceptClick();
                AdjustCharacterConfigPointValue(index, 5);
                return true;
            }

            rowY += 23.f;
        }

        rowY = panelY + 34.f;
        if (CheckAccountGuardMouseIn(rightX, rowY, rowWidth, 37.f))
        {
            acceptClick();
            g_characterConfigState.CommandMode = CharacterConfigCommandMode::Follow;
            return true;
        }

        rowY += 43.f;
        if (CheckAccountGuardMouseIn(rightX, rowY, rowWidth, 37.f))
        {
            acceptClick();
            g_characterConfigState.CommandMode = CharacterConfigCommandMode::StayCity;
            return true;
        }

        rowY += 43.f;
        if (CheckAccountGuardMouseIn(rightX, rowY, rowWidth, 37.f))
        {
            acceptClick();
            g_characterConfigState.CommandMode = CharacterConfigCommandMode::FarmPosition;
            return true;
        }

        if (CheckAccountGuardMouseIn(x, y, width, height))
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            return true;
        }

        MouseLButtonPush = false;
        MouseLButton = false;
        return true;
    }

    bool ProcessGameSettingsWindowMouseEvents()
    {
        if (!g_isGameSettingsWindowOpen)
        {
            return false;
        }

        MouseOnWindow = true;

        const float x = kAccountGuardWindowX;
        const float y = kAccountGuardWindowY;
        const float width = kAccountGuardWindowWidth;
        const float height = kAccountGuardWindowHeight;
        const float panelX = x + kGameSettingsPanelX;
        const float panelY = y + kGameSettingsPanelY;
        const float presetY = panelY + 31.f;

        if (!MouseLButtonPush)
        {
            return true;
        }

        auto acceptClick = []()
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            PlayBuffer(SOUND_CLICK01);
        };

        if (CheckAccountGuardMouseIn(x + kAccountGuardCloseButtonX, y + kAccountGuardCloseButtonY, kAccountGuardCloseButtonSize, kAccountGuardCloseButtonSize)
            || CheckAccountGuardMouseIn(x + kAccountGuardFooterCloseButtonX, y + kAccountGuardFooterCloseButtonY, kAccountGuardFooterCloseButtonWidth, kAccountGuardFooterButtonHeight))
        {
            acceptClick();
            CloseGameSettingsWindow();
            return true;
        }

        if (CheckAccountGuardMouseIn(panelX + 13.f, presetY, kGameSettingsPresetButtonWidth, kGameSettingsPresetButtonHeight))
        {
            acceptClick();
            SetGamePerformancePreset(false);
            ShowAccountGuardSystemMessage(L"Modo normal aplicado.");
            return true;
        }

        if (CheckAccountGuardMouseIn(panelX + 113.f, presetY, kGameSettingsPresetButtonWidth, kGameSettingsPresetButtonHeight))
        {
            acceptClick();
            SetGamePerformancePreset(true);
            ShowAccountGuardSystemMessage(L"Modo rapido aplicado.");
            return true;
        }

        float rowY = panelY + 68.f;
        for (const GamePerformanceOptionRow& row : kGamePerformanceOptions)
        {
            if (CheckAccountGuardMouseIn(panelX + 9.f, rowY, kGameSettingsPanelWidth - 18.f, kGameSettingsOptionRowHeight))
            {
                acceptClick();
                SetGamePerformanceOption(row.Option, !GetGamePerformanceOption(row.Option));
                SaveGamePerformanceSettings();
                ShowAccountGuardSystemMessage(L"Configuracoes do Jogo salvas.");
                return true;
            }

            rowY += kGameSettingsOptionRowHeight + kGameSettingsOptionRowGap;
        }

        if (CheckAccountGuardMouseIn(x, y, width, height))
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            return true;
        }

        MouseLButtonPush = false;
        MouseLButton = false;
        return true;
    }

    bool ProcessJewelBankWindowMouseEvents()
    {
        if (!g_isJewelBankWindowOpen)
        {
            return false;
        }

        MouseOnWindow = true;

        const float x = kAccountGuardWindowX;
        const float y = kAccountGuardWindowY;
        const float width = kAccountGuardWindowWidth;
        const float height = kAccountGuardWindowHeight;
        const float panelX = x + kJewelBankPanelX;
        const float panelY = y + kJewelBankPanelY;
        const float autoRowY = panelY + 9.f;

        if (!MouseLButtonPush)
        {
            return true;
        }

        auto acceptClick = []()
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            PlayBuffer(SOUND_CLICK01);
        };

        if (CheckAccountGuardMouseIn(x + kAccountGuardCloseButtonX, y + kAccountGuardCloseButtonY, kAccountGuardCloseButtonSize, kAccountGuardCloseButtonSize)
            || CheckAccountGuardMouseIn(x + kJewelBankFooterCloseButtonX, y + kJewelBankFooterButtonY, kJewelBankFooterTextButtonWidth, kAccountGuardFooterButtonHeight))
        {
            acceptClick();
            CloseJewelBankWindow();
            return true;
        }

        if (CheckAccountGuardMouseIn(panelX + 8.f, autoRowY, kJewelBankPanelWidth - 16.f, 22.f))
        {
            acceptClick();
            const bool nextValue = !JewelBankClient::IsAutoStoreEnabled();
            JewelBankClient::SetAutoStore(nextValue);
            ShowAccountGuardSystemMessage(nextValue ? L"Banco de Joias automatico ligado." : L"Banco de Joias automatico desligado.");
            return true;
        }

        g_jewelBankWindowPage = std::clamp(g_jewelBankWindowPage, 0, kJewelBankPageCount - 1);
        const int firstIndex = g_jewelBankWindowPage * kJewelBankRowsPerPage;
        for (int visibleRow = 0; visibleRow < kJewelBankRowsPerPage; ++visibleRow)
        {
            const int index = firstIndex + visibleRow;
            if (index >= JewelBankClient::JewelCount)
            {
                break;
            }

            const float rowY = y + kJewelBankFirstRowY + (static_cast<float>(visibleRow) * kJewelBankRowHeight);
            const JewelBankUiEntry& entry = kJewelBankUiEntries[index];
            if (CheckAccountGuardMouseIn(x + kJewelBankStoreColumnX, rowY + 5.f, kJewelBankRowButtonSize, kJewelBankRowButtonSize))
            {
                acceptClick();
                JewelBankClient::Store(entry.BankKey);
                ShowAccountGuardSystemMessage(L"Guardando joia selecionada.");
                return true;
            }

            if (CheckAccountGuardMouseIn(x + kJewelBankWithdrawColumnX, rowY + 5.f, kJewelBankRowButtonSize, kJewelBankRowButtonSize))
            {
                acceptClick();
                JewelBankClient::Withdraw(entry.BankKey);
                ShowAccountGuardSystemMessage(L"Sacando joia selecionada.");
                return true;
            }
        }

        if (g_jewelBankWindowPage > 0
            && CheckAccountGuardMouseIn(x + kJewelBankPagePrevButtonX, y + kJewelBankPageButtonY, kJewelBankPageButtonWidth, kAccountGuardFooterButtonHeight))
        {
            acceptClick();
            --g_jewelBankWindowPage;
            ShowAccountGuardSystemMessage(L"Pagina anterior do Banco de Joias.");
            return true;
        }

        if (g_jewelBankWindowPage + 1 < kJewelBankPageCount
            && CheckAccountGuardMouseIn(x + kJewelBankPageNextButtonX, y + kJewelBankPageButtonY, kJewelBankPageButtonWidth, kAccountGuardFooterButtonHeight))
        {
            acceptClick();
            ++g_jewelBankWindowPage;
            ShowAccountGuardSystemMessage(L"Proxima pagina do Banco de Joias.");
            return true;
        }

        if (CheckAccountGuardMouseIn(x + kJewelBankFooterRefreshButtonX, y + kJewelBankFooterButtonY, kJewelBankFooterTextButtonWidth, kAccountGuardFooterButtonHeight))
        {
            acceptClick();
            JewelBankClient::RequestSync();
            ShowAccountGuardSystemMessage(L"Atualizando Banco de Joias.");
            return true;
        }

        if (CheckAccountGuardMouseIn(x, y, width, height))
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            return true;
        }

        MouseLButtonPush = false;
        MouseLButton = false;
        return true;
    }

    bool ProcessCommandsWindowMouseEvents()
    {
        if (!g_isCommandsWindowOpen)
        {
            return false;
        }

        MouseOnWindow = true;

        const float x = kAccountGuardWindowX;
        const float y = kAccountGuardWindowY;
        const float width = kAccountGuardWindowWidth;
        const float height = kAccountGuardWindowHeight;
        const float panelX = x + kCommandsPanelX;
        const float panelY = y + kCommandsPanelY;
        const float buttonX = panelX + ((kCommandsPanelWidth - kCommandsButtonWidth) / 2.f);

        if (!MouseLButtonPush)
        {
            return true;
        }

        auto acceptClick = []()
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            PlayBuffer(SOUND_CLICK01);
        };

        if (CheckAccountGuardMouseIn(x + kAccountGuardCloseButtonX, y + kAccountGuardCloseButtonY, kAccountGuardCloseButtonSize, kAccountGuardCloseButtonSize)
            || CheckAccountGuardMouseIn(x + kAccountGuardFooterCloseButtonX, y + kAccountGuardFooterCloseButtonY, kAccountGuardFooterCloseButtonWidth, kAccountGuardFooterButtonHeight))
        {
            acceptClick();
            CloseCommandsWindow();
            return true;
        }

        float buttonY = panelY + 18.f;
        if (CheckAccountGuardMouseIn(buttonX, buttonY, kCommandsButtonWidth, kCommandsButtonHeight))
        {
            acceptClick();
            SendCommandsMenuCommand(L"reset");
            return true;
        }

        buttonY += kCommandsButtonHeight + kCommandsButtonGap;
        if (CheckAccountGuardMouseIn(buttonX, buttonY, kCommandsButtonWidth, kCommandsButtonHeight))
        {
            acceptClick();
            SendCommandsMenuCommand(L"limpar");
            return true;
        }

        buttonY += kCommandsButtonHeight + kCommandsButtonGap;
        if (CheckAccountGuardMouseIn(buttonX, buttonY, kCommandsButtonWidth, kCommandsButtonHeight))
        {
            acceptClick();
            SendCommandsMenuCommand(L"evoluir");
            return true;
        }

        if (AccountCompanionClient::CanPlayOffline())
        {
            buttonY += kCommandsButtonHeight + kCommandsButtonGap;
            if (CheckAccountGuardMouseIn(buttonX, buttonY, kCommandsButtonWidth, kCommandsButtonHeight))
            {
                acceptClick();
                SendCommandsMenuCommand(L"offline");
                return true;
            }
        }

        if (CheckAccountGuardMouseIn(x, y, width, height))
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            return true;
        }

        MouseLButtonPush = false;
        MouseLButton = false;
        return true;
    }

    bool ProcessAzothWindowMouseEvents()
    {
        if (!g_isAzothWindowOpen)
        {
            return false;
        }

        MouseOnWindow = true;

        const float x = kAccountGuardWindowX;
        const float y = kAccountGuardWindowY;
        const float width = kAccountGuardWindowWidth;
        const float height = kAccountGuardWindowHeight;
        const float panelX = x + kAzothPanelX;
        const float panelY = y + kAzothPanelY;
        const float firstRowY = panelY + 91.f;
        const float secondRowY = firstRowY + 25.f;
        const float thirdRowY = secondRowY + 25.f;
        const float autoRowY = panelY + 199.f;
        const float autoStartX = panelX + 10.f;

        if (!MouseLButtonPush)
        {
            return true;
        }

        auto acceptClick = []()
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            PlayBuffer(SOUND_CLICK01);
        };

        if (CheckAccountGuardMouseIn(x + kAccountGuardCloseButtonX, y + kAccountGuardCloseButtonY, kAccountGuardCloseButtonSize, kAccountGuardCloseButtonSize))
        {
            acceptClick();
            CloseAzothWindow();
            return true;
        }

        if (CheckAccountGuardMouseIn(x + kAzothFooterApplyButtonX, y + kAccountGuardFooterCloseButtonY, kAzothFooterButtonWidth, kAccountGuardFooterButtonHeight))
        {
            acceptClick();
            AzothClient::ApplyAutoConversionSettings(g_azothPendingAutoEnabled, g_azothPendingAutoReserveZen);
            g_azothPendingAutoDirty = false;
            g_azothObservedAutoSettingsVersion = AzothClient::GetAutoConversionSettingsVersion();
            ShowAccountGuardSystemMessage(g_azothPendingAutoEnabled
                ? L"Aplicando Auto Azoth aos personagens marcados."
                : L"Auto Azoth desligado.");
            return true;
        }

        if (CheckAccountGuardMouseIn(x + kAzothFooterCloseButtonX, y + kAccountGuardFooterCloseButtonY, kAzothFooterButtonWidth, kAccountGuardFooterButtonHeight))
        {
            acceptClick();
            CloseAzothWindow();
            return true;
        }

        if (CheckAccountGuardMouseIn(panelX + 10.f, firstRowY, kAzothButtonWidth, kAzothButtonHeight))
        {
            acceptClick();
            AzothClient::ConvertCurrent(100000000LL);
            ShowAccountGuardSystemMessage(L"Solicitando conversao de 100 mi Zen.");
            return true;
        }

        if (CheckAccountGuardMouseIn(panelX + 72.f, firstRowY, kAzothButtonWidth, kAzothButtonHeight))
        {
            acceptClick();
            AzothClient::ConvertCurrent(500000000LL);
            ShowAccountGuardSystemMessage(L"Solicitando conversao de 500 mi Zen.");
            return true;
        }

        if (CheckAccountGuardMouseIn(panelX + 134.f, firstRowY, kAzothButtonWidth, kAzothButtonHeight))
        {
            acceptClick();
            AzothClient::ConvertCurrent(1000000000LL);
            ShowAccountGuardSystemMessage(L"Solicitando conversao de 1 bi Zen.");
            return true;
        }

        if (CheckAccountGuardMouseIn(panelX + 10.f, secondRowY, kAzothButtonWidth, kAzothButtonHeight))
        {
            acceptClick();
            AzothClient::ConvertCurrent(2000000000LL);
            ShowAccountGuardSystemMessage(L"Solicitando conversao de 2 bi Zen.");
            return true;
        }

        if (CheckAccountGuardMouseIn(panelX + 72.f, secondRowY, kAzothWideButtonWidth, kAzothButtonHeight))
        {
            acceptClick();
            AzothClient::ConvertCurrentMax();
            ShowAccountGuardSystemMessage(L"Solicitando conversao maxima do personagem atual.");
            return true;
        }

        if (CheckAccountGuardMouseIn(panelX + 10.f, thirdRowY, kAzothWideButtonWidth, kAzothButtonHeight))
        {
            acceptClick();
            AzothClient::RequestBalance();
            ShowAccountGuardSystemMessage(L"Atualizando saldo Azoth.");
            return true;
        }

        if (CheckAccountGuardMouseIn(panelX + 108.f, thirdRowY, 82.f, kAzothButtonHeight))
        {
            acceptClick();
            AzothClient::ConvertAccountCharacters();
            ShowAccountGuardSystemMessage(L"Solicitando conversao dos personagens da conta.");
            return true;
        }

        if (CheckAccountGuardMouseIn(autoStartX, autoRowY, kAzothAutoButtonWidth, kAzothAutoButtonHeight))
        {
            acceptClick();
            if (g_azothPendingAutoEnabled)
            {
                SetAzothPendingAutoSettings(false, 0);
                ShowAccountGuardSystemMessage(L"Auto Azoth selecionado: Off.");
            }
            else
            {
                SetAzothPendingAutoSettings(true, GetAzothPendingAutoReserveOrDefault());
                ShowAccountGuardSystemMessage(L"Auto Azoth selecionado: On.");
            }

            return true;
        }

        if (CheckAccountGuardMouseIn(autoStartX + ((kAzothAutoButtonWidth + kAzothAutoButtonGap) * 1.f), autoRowY, kAzothAutoButtonWidth, kAzothAutoButtonHeight))
        {
            acceptClick();
            SetAzothPendingAutoSettings(true, 100000000ULL);
            ShowAccountGuardSystemMessage(L"Selecionado: Auto Azoth acima de 100 mi Zen.");
            return true;
        }

        if (CheckAccountGuardMouseIn(autoStartX + ((kAzothAutoButtonWidth + kAzothAutoButtonGap) * 2.f), autoRowY, kAzothAutoButtonWidth, kAzothAutoButtonHeight))
        {
            acceptClick();
            SetAzothPendingAutoSettings(true, 500000000ULL);
            ShowAccountGuardSystemMessage(L"Selecionado: Auto Azoth acima de 500 mi Zen.");
            return true;
        }

        if (CheckAccountGuardMouseIn(autoStartX + ((kAzothAutoButtonWidth + kAzothAutoButtonGap) * 3.f), autoRowY, kAzothAutoButtonWidth, kAzothAutoButtonHeight))
        {
            acceptClick();
            SetAzothPendingAutoSettings(true, 1000000000ULL);
            ShowAccountGuardSystemMessage(L"Selecionado: Auto Azoth acima de 1 bi Zen.");
            return true;
        }

        if (CheckAccountGuardMouseIn(autoStartX + ((kAzothAutoButtonWidth + kAzothAutoButtonGap) * 4.f), autoRowY, kAzothAutoButtonWidth, kAzothAutoButtonHeight))
        {
            acceptClick();
            SetAzothPendingAutoSettings(true, 2000000000ULL);
            ShowAccountGuardSystemMessage(L"Selecionado: Auto Azoth acima de 2 bi Zen.");
            return true;
        }

        if (CheckAccountGuardMouseIn(x, y, width, height))
        {
            MouseLButtonPush = false;
            MouseLButton = false;
            return true;
        }

        MouseLButtonPush = false;
        MouseLButton = false;
        return true;
    }

    bool ProcessAzothHudMouseEvents()
    {
        if (IsAccountGuardHudBlockedByForegroundWindow() || !IsMouseOverAzothHud())
        {
            return false;
        }

        MouseOnWindow = true;
        if (!MouseLButtonPush)
        {
            return false;
        }

        MouseLButtonPush = false;
        MouseLButton = false;
        PlayBuffer(SOUND_CLICK01);
        OpenAzothWindow();
        ShowAccountGuardSystemMessage(L"Azoth aberto.");
        return true;
    }

    bool ProcessAccountGuardHudButtons()
    {
        if (IsAccountGuardHudBlockedByForegroundWindow())
        {
            return false;
        }

        for (const AccountGuardHudButton& button : kAccountGuardHudButtons)
        {
            if (!IsMouseOverAccountGuardButton(button))
            {
                continue;
            }

            MouseOnWindow = true;

            if (MouseLButtonPush)
            {
                MouseLButtonPush = false;
                MouseLButton = false;
                PlayBuffer(SOUND_CLICK01);

                if (button.Action == AccountGuardHudAction::OpenPersonalStore)
                {
                    if (LaunchPersonalStoreBrowserOverlayForCurrentAccount())
                    {
                        ShowAccountGuardSystemMessage(L"Minha Loja aberta.");
                    }
                    else
                    {
                        ShowAccountGuardSystemMessage(L"Nao foi possivel abrir Minha Loja.");
                    }
                }
                else if (button.Action == AccountGuardHudAction::OpenSummonWindow)
                {
                    OpenAccountGuardWindow();
                    ShowAccountGuardSystemMessage(L"Sentinelas abertas.");
                }
                else if (button.Action == AccountGuardHudAction::OpenCommandsWindow)
                {
                    OpenCommandsWindow();
                    ShowAccountGuardSystemMessage(L"Comandos abertos.");
                }
                else if (button.Action == AccountGuardHudAction::OpenGameSettingsWindow)
                {
                    OpenGameSettingsWindow();
                    ShowAccountGuardSystemMessage(L"Configuracoes do Jogo abertas.");
                }
                else if (button.Action == AccountGuardHudAction::OpenJewelBankWindow)
                {
                    OpenJewelBankWindow();
                    ShowAccountGuardSystemMessage(L"Banco de Joias aberto.");
                }

                return true;
            }
        }

        return false;
    }
}

SEASON3B::CNewUIMainFrameWindow::CNewUIMainFrameWindow()
{
    m_bExpEffect = false;
    m_dwExpEffectTime = 0;
    m_dwPreExp = 0;
    m_dwGetExp = 0;
    m_bButtonBlink = false;
}

SEASON3B::CNewUIMainFrameWindow::~CNewUIMainFrameWindow()
{
    Release();
}

void SEASON3B::CNewUIMainFrameWindow::LoadImages()
{
    LoadBitmap(L"Interface\\newui_menu01.jpg", IMAGE_MENU_1, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_menu02.jpg", IMAGE_MENU_2, GL_LINEAR);
    LoadBitmap(L"Interface\\partCharge1\\newui_menu03.jpg", IMAGE_MENU_3, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_menu02-03.jpg", IMAGE_MENU_2_1, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_menu_blue.jpg", IMAGE_GAUGE_BLUE, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_menu_green.jpg", IMAGE_GAUGE_GREEN, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_menu_red.jpg", IMAGE_GAUGE_RED, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_menu_ag.jpg", IMAGE_GAUGE_AG, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_menu_sd.jpg", IMAGE_GAUGE_SD, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_exbar.jpg", IMAGE_GAUGE_EXBAR, GL_LINEAR);
    LoadBitmap(L"Interface\\Exbar_Master.jpg", IMAGE_MASTER_GAUGE_BAR, GL_LINEAR);
    LoadBitmap(L"Interface\\partCharge1\\newui_menu_Bt05.jpg", IMAGE_MENU_BTN_CSHOP, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\partCharge1\\newui_menu_Bt01.jpg", IMAGE_MENU_BTN_CHAINFO, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\partCharge1\\newui_menu_Bt02.jpg", IMAGE_MENU_BTN_MYINVEN, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\partCharge1\\newui_menu_Bt03.jpg", IMAGE_MENU_BTN_FRIEND, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\partCharge1\\newui_menu_Bt04.jpg", IMAGE_MENU_BTN_WINDOW, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\account_guard_button.tga", kAccountGuardSummonButtonImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\account_guard_button_hover.tga", kAccountGuardSummonButtonHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\store_button.tga", kPersonalStoreButtonImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\store_button_hover.tga", kPersonalStoreButtonHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\game_settings_button.tga", kGameSettingsButtonImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\game_settings_button_hover.tga", kGameSettingsButtonHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\commands_button.tga", kCommandsButtonImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\commands_button_hover.tga", kCommandsButtonHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\JewelBank\\jewel_bank_button.tga", kJewelBankButtonImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\JewelBank\\jewel_bank_button_hover.tga", kJewelBankButtonHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\JewelBank\\guardar.tga", kJewelBankFooterStoreImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\JewelBank\\guardar_hover.tga", kJewelBankFooterStoreHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\JewelBank\\sacar.tga", kJewelBankFooterWithdrawImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\JewelBank\\sacar_hover.tga", kJewelBankFooterWithdrawHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Azoth\\box_azoth.tga", kAzothHudImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Azoth\\box_azoth_hover.tga", kAzothHudHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Azoth\\azoth_digits.tga", kAzothDigitImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\switch_character_button.tga", kAccountGuardSwitchButtonImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\switch_character_button_hover.tga", kAccountGuardSwitchButtonHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\personal_guard_call.tga", kAccountGuardCallButtonImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\personal_guard_call_hover.tga", kAccountGuardCallButtonHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\personal_guard_store.tga", kAccountGuardStoreButtonImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\personal_guard_store_hover.tga", kAccountGuardStoreButtonHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\personal_guard_farm.tga", kAccountGuardFarmButtonImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\personal_guard_farm_hover.tga", kAccountGuardFarmButtonHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\personal_guard_follow.tga", kAccountGuardFollowButtonImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\personal_guard_follow_hover.tga", kAccountGuardFollowButtonHoverImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\personal_guard_window.tga", kAccountGuardWindowImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\personal_guard_button_small.tga", kAccountGuardSmallButtonImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
    LoadBitmap(L"Interface\\Custom\\personal_guard_button_wide.tga", kAccountGuardWideButtonImage, GL_LINEAR, GL_CLAMP_TO_EDGE);
}

void SEASON3B::CNewUIMainFrameWindow::UnloadImages()
{
    DeleteBitmap(IMAGE_MENU_1);
    DeleteBitmap(IMAGE_MENU_2);
    DeleteBitmap(IMAGE_MENU_3);
    DeleteBitmap(IMAGE_MENU_2_1);
    DeleteBitmap(IMAGE_GAUGE_BLUE);
    DeleteBitmap(IMAGE_GAUGE_GREEN);
    DeleteBitmap(IMAGE_GAUGE_RED);
    DeleteBitmap(IMAGE_GAUGE_AG);
    DeleteBitmap(IMAGE_GAUGE_SD);
    DeleteBitmap(IMAGE_GAUGE_EXBAR);
    DeleteBitmap(IMAGE_MENU_BTN_CHAINFO);
    DeleteBitmap(IMAGE_MENU_BTN_MYINVEN);
    DeleteBitmap(IMAGE_MENU_BTN_FRIEND);
    DeleteBitmap(IMAGE_MENU_BTN_WINDOW);
    DeleteBitmap(kAccountGuardSummonButtonImage);
    DeleteBitmap(kAccountGuardSummonButtonHoverImage);
    DeleteBitmap(kPersonalStoreButtonImage);
    DeleteBitmap(kPersonalStoreButtonHoverImage);
    DeleteBitmap(kGameSettingsButtonImage);
    DeleteBitmap(kGameSettingsButtonHoverImage);
    DeleteBitmap(kCommandsButtonImage);
    DeleteBitmap(kCommandsButtonHoverImage);
    DeleteBitmap(kJewelBankButtonImage);
    DeleteBitmap(kJewelBankButtonHoverImage);
    DeleteBitmap(kJewelBankFooterStoreImage);
    DeleteBitmap(kJewelBankFooterStoreHoverImage);
    DeleteBitmap(kJewelBankFooterWithdrawImage);
    DeleteBitmap(kJewelBankFooterWithdrawHoverImage);
    DeleteBitmap(kAzothHudImage);
    DeleteBitmap(kAzothHudHoverImage);
    DeleteBitmap(kAzothDigitImage);
    DeleteBitmap(kAccountGuardSwitchButtonImage);
    DeleteBitmap(kAccountGuardSwitchButtonHoverImage);
    DeleteBitmap(kAccountGuardCallButtonImage);
    DeleteBitmap(kAccountGuardCallButtonHoverImage);
    DeleteBitmap(kAccountGuardStoreButtonImage);
    DeleteBitmap(kAccountGuardStoreButtonHoverImage);
    DeleteBitmap(kAccountGuardFarmButtonImage);
    DeleteBitmap(kAccountGuardFarmButtonHoverImage);
    DeleteBitmap(kAccountGuardFollowButtonImage);
    DeleteBitmap(kAccountGuardFollowButtonHoverImage);
    DeleteBitmap(kAccountGuardWindowImage);
    DeleteBitmap(kAccountGuardSmallButtonImage);
    DeleteBitmap(kAccountGuardWideButtonImage);
}

bool SEASON3B::CNewUIMainFrameWindow::Create(CNewUIManager* pNewUIMng, CNewUI3DRenderMng* pNewUI3DRenderMng)
{
    if (NULL == pNewUIMng || NULL == pNewUI3DRenderMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_MAINFRAME, this);

    m_pNewUI3DRenderMng = pNewUI3DRenderMng;
    m_pNewUI3DRenderMng->Add3DRenderObj(this, ITEMHOTKEYNUMBER_CAMERA_Z_ORDER);

    LoadImages();

    SetButtonInfo();

    Show(true);

    return true;
}

void SEASON3B::CNewUIMainFrameWindow::SetButtonInfo()
{
    int x_Next = 489;
    int y_Next = REFERENCE_HEIGHT - 51;
    int x_Add = 30;
    int y_Add = 41;
    m_BtnCShop.ChangeTextBackColor(RGBA(255, 255, 255, 0));
    m_BtnCShop.ChangeButtonImgState(true, IMAGE_MENU_BTN_CSHOP, true);
    m_BtnCShop.ChangeButtonInfo(x_Next, y_Next, x_Add, y_Add);
    x_Next += x_Add;
    m_BtnCShop.ChangeImgColor(BUTTON_STATE_UP, RGBA(255, 255, 255, 255));
    m_BtnCShop.ChangeImgColor(BUTTON_STATE_DOWN, RGBA(255, 255, 255, 255));
    m_BtnCShop.ChangeToolTipText(&I18N::Game::MUItemShopX, true);

    m_BtnChaInfo.ChangeTextBackColor(RGBA(255, 255, 255, 0));
    m_BtnChaInfo.ChangeButtonImgState(true, IMAGE_MENU_BTN_CHAINFO, true);
    m_BtnChaInfo.ChangeButtonInfo(x_Next, y_Next, x_Add, y_Add);
    x_Next += x_Add;
    m_BtnChaInfo.ChangeImgColor(BUTTON_STATE_UP, RGBA(255, 255, 255, 255));
    m_BtnChaInfo.ChangeImgColor(BUTTON_STATE_DOWN, RGBA(255, 255, 255, 255));
    m_BtnChaInfo.ChangeToolTipText(&I18N::Game::CharacterC, true);

    m_BtnMyInven.ChangeTextBackColor(RGBA(255, 255, 255, 0));
    m_BtnMyInven.ChangeButtonImgState(true, IMAGE_MENU_BTN_MYINVEN, true);
    m_BtnMyInven.ChangeButtonInfo(x_Next, y_Next, x_Add, y_Add);
    x_Next += x_Add;
    m_BtnMyInven.ChangeImgColor(BUTTON_STATE_UP, RGBA(255, 255, 255, 255));
    m_BtnMyInven.ChangeImgColor(BUTTON_STATE_DOWN, RGBA(255, 255, 255, 255));
    m_BtnMyInven.ChangeToolTipText(&I18N::Game::InventoryIV, true);

    m_BtnFriend.ChangeTextBackColor(RGBA(255, 255, 255, 0));
    m_BtnFriend.ChangeButtonImgState(true, IMAGE_MENU_BTN_FRIEND, true);
    m_BtnFriend.ChangeButtonInfo(x_Next, y_Next, x_Add, y_Add);
    x_Next += x_Add;
    m_BtnFriend.ChangeImgColor(BUTTON_STATE_UP, RGBA(255, 255, 255, 255));
    m_BtnFriend.ChangeImgColor(BUTTON_STATE_DOWN, RGBA(255, 255, 255, 255));
    m_BtnFriend.ChangeToolTipText(&I18N::Game::FriendF, true);

    m_BtnWindow.ChangeTextBackColor(RGBA(255, 255, 255, 0));
    m_BtnWindow.ChangeButtonImgState(true, IMAGE_MENU_BTN_WINDOW, true);
    m_BtnWindow.ChangeButtonInfo(x_Next, y_Next, x_Add, y_Add);
    m_BtnWindow.ChangeImgColor(BUTTON_STATE_UP, RGBA(255, 255, 255, 255));
    m_BtnWindow.ChangeImgColor(BUTTON_STATE_DOWN, RGBA(255, 255, 255, 255));
    m_BtnWindow.ChangeToolTipText(&I18N::Game::MenuU, true);
}

void SEASON3B::CNewUIMainFrameWindow::Release()
{
    UnloadImages();

    if (m_pNewUI3DRenderMng)
    {
        m_pNewUI3DRenderMng->Remove3DRenderObj(this);
        m_pNewUI3DRenderMng = NULL;
    }

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }
}

bool SEASON3B::CNewUIMainFrameWindow::Render()
{
    AccountCompanionClient::Tick();
    AzothClient::Tick();
    JewelBankClient::Tick();

    EnableAlphaTest();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    RenderFrame();

    m_pNewUI3DRenderMng->RenderUI2DEffect(ITEMHOTKEYNUMBER_CAMERA_Z_ORDER, UI2DEffectCallback, this, 0, 0);

    g_pSkillList->RenderCurrentSkillAndHotSkillList();

    EnableAlphaTest();
    RenderLifeMana();
    RenderGuageSD();
    RenderGuageAG();
    RenderButtons();
    RenderAccountGuardHudButtons();
    RenderAzothHud();
    RenderAccountGuardMiniPartyList();
    RenderExperience();
    RenderAccountGuardWindow();
    RenderAccountGuardFormationWindow();
    RenderCommandsWindow();
    RenderGameSettingsWindow();
    RenderJewelBankWindow();
    RenderAzothWindow();
    DisableAlphaBlend();

    return true;
}

void SEASON3B::CNewUIMainFrameWindow::Render3D()
{
    m_ItemHotKey.RenderItems();
    RenderJewelBankWindow3D();
}

void SEASON3B::CNewUIMainFrameWindow::UI2DEffectCallback(LPVOID pClass, DWORD dwParamA, DWORD dwParamB)
{
    g_pMainFrame->RenderHotKeyItemCount();
}

bool SEASON3B::CNewUIMainFrameWindow::IsVisible() const
{
    return CNewUIObj::IsVisible();
}

void SEASON3B::CNewUIMainFrameWindow::RenderFrame()
{
    float width, height;
    float x, y;

    width = 256.f; height = 51.f;
    x = 0.f; y = (float)REFERENCE_HEIGHT - height;
    SEASON3B::RenderImage(IMAGE_MENU_1, x, y, width, height);
    width = 128.f;
    x = 256.f;
    SEASON3B::RenderImage(IMAGE_MENU_2, x, y, width, height);
    width = 256.f;
    x = 256.f + 128.f;
    SEASON3B::RenderImage(IMAGE_MENU_3, x, y, width, height);

    if (g_pSkillList->IsSkillListUp() == true)
    {
        width = 160.f; height = 40.f;
        x = 222.f;
        SEASON3B::RenderImage(IMAGE_MENU_2_1, x, y, width, height);
    }
}

void SEASON3B::CNewUIMainFrameWindow::RenderLifeMana()
{
    DWORD wLifeMax, wLife, wManaMax, wMana;
    const bool isMasterLevel = gCharacterManager.IsMasterLevel(Hero->Class) == true;

    if (isMasterLevel)
    {
        wLifeMax = NormalizeResourceMax(Master_Level_Data.wMaxLife, CharacterAttribute->LifeMax);
        wManaMax = NormalizeResourceMax(Master_Level_Data.wMaxMana, CharacterAttribute->ManaMax);
    }
    else
    {
        wLifeMax = NormalizeResourceMax(CharacterAttribute->LifeMax, Master_Level_Data.wMaxLife);
        wManaMax = NormalizeResourceMax(CharacterAttribute->ManaMax, Master_Level_Data.wMaxMana);
    }
    wLife = NormalizeResourceCurrent(CharacterAttribute->Life, wLifeMax);
    wMana = NormalizeResourceCurrent(CharacterAttribute->Mana, wManaMax);

    if (wLifeMax > 0)
    {
        if (wLife > 0 && (wLife / (float)wLifeMax) < 0.2f)
        {
            PlayBuffer(SOUND_HEART);
        }
    }

    float fLife = 0.f;
    float fMana = 0.f;

    fLife = GetMissingResourceRatio(wLife, wLifeMax);
    fMana = GetMissingResourceRatio(wMana, wManaMax);

    float width, height;
    float x, y;
    float fY, fH, fV;

    // life
    width = 45.f;
    x = 158;
    height = 39.f;
    y = (float)REFERENCE_HEIGHT - 48.f;

    fY = y + (fLife * height);
    fH = height - (fLife * height);
    fV = fLife;
    if (g_isCharacterBuff((&Hero->Object), eDeBuff_Poison))
    {
        RenderBitmap(IMAGE_GAUGE_GREEN, x, fY, width, fH, 0.f, fV * height / 64.f, width / 64.f, (1.0f - fV) * height / 64.f);
    }
    else
    {
        RenderBitmap(IMAGE_GAUGE_RED, x, fY, width, fH, 0.f, fV * height / 64.f, width / 64.f, (1.0f - fV) * height / 64.f);
    }

    SEASON3B::RenderNumber(x + 25, REFERENCE_HEIGHT - 18, ToRenderableResourceNumber(wLife));

    wchar_t strTipText[256];
    if (SEASON3B::CheckMouseIn(x, y, width, height) == true)
    {
        mu_swprintf(strTipText, I18N::Game::LifeDD, ToRenderableResourceNumber(wLife), ToRenderableResourceNumber(wLifeMax));
        RenderTipText((int)x, (int)418, strTipText);
    }

    // mana
    width = 45.f;
    x = 256.f + 128.f + 53.f;
    height = 39.f;
    y = (float)REFERENCE_HEIGHT - 48.f;

    fY = y + (fMana * height);
    fH = height - (fMana * height);
    fV = fMana;
    RenderBitmap(IMAGE_GAUGE_BLUE, x, fY, width, fH, 0.f, fV * height / 64.f, width / 64.f, (1.0f - fV) * height / 64.f);

    SEASON3B::RenderNumber(x + 30, REFERENCE_HEIGHT - 18, ToRenderableResourceNumber(wMana));

    // mana
    if (SEASON3B::CheckMouseIn(x, y, width, height) == true)
    {
        mu_swprintf(strTipText, I18N::Game::ManaDD359, ToRenderableResourceNumber(wMana), ToRenderableResourceNumber(wManaMax));
        RenderTipText((int)x, (int)418, strTipText);
    }
}

void SEASON3B::CNewUIMainFrameWindow::RenderGuageAG()
{
    float x, y, width, height;
    float fY, fH, fV;

    DWORD dwMaxSkillMana, dwSkillMana;

    if (gCharacterManager.IsMasterLevel(Hero->Class) == true)
    {
        dwMaxSkillMana = std::max<DWORD>(1, NormalizeResourceMax(Master_Level_Data.wMaxBP, CharacterAttribute->SkillManaMax));
    }
    else
    {
        dwMaxSkillMana = std::max<DWORD>(1, NormalizeResourceMax(CharacterAttribute->SkillManaMax, Master_Level_Data.wMaxBP));
    }
    dwSkillMana = NormalizeResourceCurrent(CharacterAttribute->SkillMana, dwMaxSkillMana);

    float fSkillMana = GetMissingResourceRatio(dwSkillMana, dwMaxSkillMana);

    width = 16.f, height = 39.f;
    x = 256 + 128 + 36; y = (float)REFERENCE_HEIGHT - 49.f;
    fY = y + (fSkillMana * height);
    fH = height - (fSkillMana * height);
    fV = fSkillMana;

    RenderBitmap(IMAGE_GAUGE_AG, x, fY, width, fH, 0.f, fV * height / 64.f, width / 16.f, (1.0f - fV) * height / 64.f);
    SEASON3B::RenderNumber(x + 10, REFERENCE_HEIGHT - 18, ToRenderableResourceNumber(dwSkillMana));

    if (SEASON3B::CheckMouseIn(x, y, width, height) == true)
    {
        wchar_t strTipText[256];

        mu_swprintf(strTipText, I18N::Game::AGDD, ToRenderableResourceNumber(dwSkillMana), ToRenderableResourceNumber(dwMaxSkillMana));
        RenderTipText((int)x - 20, (int)418, strTipText);
    }
}

void SEASON3B::CNewUIMainFrameWindow::RenderGuageSD()
{
    float x, y, width, height;
    float fY, fH, fV;
    DWORD wMaxShield, wShield;

    //Master_Level_Data.wMaxShield
    if (gCharacterManager.IsMasterLevel(Hero->Class) == true)
    {
        wMaxShield = std::max<DWORD>(1, NormalizeResourceMax(Master_Level_Data.wMaxShield, CharacterAttribute->ShieldMax));
    }
    else
    {
        wMaxShield = std::max<DWORD>(1, NormalizeResourceMax(CharacterAttribute->ShieldMax, Master_Level_Data.wMaxShield));
    }
    wShield = NormalizeResourceCurrent(CharacterAttribute->Shield, wMaxShield);

    float fShield = GetMissingResourceRatio(wShield, wMaxShield);

    width = 16.f, height = 39.f;
    x = 204; y = (float)REFERENCE_HEIGHT - 49.f;
    fY = y + (fShield * height);
    fH = height - (fShield * height);
    fV = fShield;

    RenderBitmap(IMAGE_GAUGE_SD, x, fY, width, fH, 0.f, fV * height / 64.f, width / 16.f, (1.0f - fV) * height / 64.f);
    SEASON3B::RenderNumber(x + 15, REFERENCE_HEIGHT - 18, ToRenderableResourceNumber(wShield));

    height = 39.f;
    y = (float)REFERENCE_HEIGHT - 10.f - 39.f;
    if (SEASON3B::CheckMouseIn(x, y, width, height) == true)
    {
        wchar_t strTipText[256];

        mu_swprintf(strTipText, I18N::Game::SDDD, ToRenderableResourceNumber(wShield), ToRenderableResourceNumber(wMaxShield));
        RenderTipText((int)x - 20, (int)418, strTipText);
    }
}

void SEASON3B::CNewUIMainFrameWindow::RenderExperience()
{
    __int64 wLevel;
    __int64 dwNexExperience;
    __int64 dwExperience;
    double x, y, width, height;
    const auto buildExpSegment = [](const double ratio, int& digit, double& fraction)
    {
        const double clampedRatio = std::clamp(ratio, 0.0, 1.0);
        if (clampedRatio >= 1.0)
        {
            digit = 9;
            fraction = 1.0;
            return;
        }

        const double scaled = clampedRatio * 10.0;
        digit = std::clamp(static_cast<int>(scaled), 0, 9);
        fraction = scaled - static_cast<double>(static_cast<long long>(scaled));
        fraction = std::clamp(fraction, 0.0, 1.0);
    };

    if (gCharacterManager.IsMasterExperienceActive(CharacterAttribute->Class, CharacterAttribute->Level) == true)
    {
        wLevel = (__int64)Master_Level_Data.nMLevel;
        dwNexExperience = (__int64)Master_Level_Data.lNext_MasterLevel_Experince;
        dwExperience = (__int64)Master_Level_Data.lMasterLevel_Experince;
    }
    else
    {
        wLevel = CharacterAttribute->Level;
        dwNexExperience = CharacterAttribute->NextExperience;
        dwExperience = CharacterAttribute->Experience;
    }

    if (gCharacterManager.IsMasterExperienceActive(CharacterAttribute->Class, CharacterAttribute->Level) == true)
    {
        x = 0; y = 470; width = 6; height = 4;

        __int64 iTotalLevel = wLevel + 400;
        __int64 iTOverLevel = iTotalLevel - 255;
        __int64 iBaseExperience = 0;

        __int64 iData_Master =	// A
            (
                (
                    (__int64)9 + (__int64)iTotalLevel
                    )
                * (__int64)iTotalLevel
                * (__int64)iTotalLevel
                * (__int64)10
                )
            +
            (
                (
                    (__int64)9 + (__int64)iTOverLevel
                    )
                * (__int64)iTOverLevel
                * (__int64)iTOverLevel
                * (__int64)1000
                );
        iBaseExperience = (iData_Master - (__int64)3892250000) / (__int64)2;	// B

        const __int64 lowerBound = iBaseExperience;
        __int64 upperBound = dwNexExperience;
        if (upperBound < lowerBound)
        {
            upperBound = lowerBound;
        }

        const double fNeedExp = static_cast<double>(upperBound - lowerBound);
        const double fClampedExp = std::clamp(static_cast<double>(dwExperience), static_cast<double>(lowerBound), static_cast<double>(upperBound));
        const double fRatio = (fNeedExp > 0.0) ? std::clamp((fClampedExp - static_cast<double>(lowerBound)) / fNeedExp, 0.0, 1.0) : 0.0;
        int iExp = 0;
        double fProgress = 0.0;
        buildExpSegment(fRatio, iExp, fProgress);

        if (m_bExpEffect == true)
        {
            double fPreProgress = 0.f;
            if (m_loPreExp < lowerBound)
            {
                x = 2.f; y = 473.f; width = fProgress * 629.f; height = 4.f;
                RenderBitmap(IMAGE_MASTER_GAUGE_BAR, x, y, width, height, 0.f, 0.f, 6.f / 8.f, 4.f / 4.f);
                glColor4f(1.f, 1.f, 1.f, 0.6f);
                RenderColor(x, y, width, height);
                EndRenderColor();
            }
            else
            {
                int iPreExpBarNum = 0;
                if (fNeedExp > 0.f)
                {
                    const double fPreClampedExp = std::clamp(static_cast<double>(m_loPreExp), static_cast<double>(lowerBound), static_cast<double>(upperBound));
                    const double fPreRatio = std::clamp((fPreClampedExp - static_cast<double>(lowerBound)) / fNeedExp, 0.0, 1.0);
                    buildExpSegment(fPreRatio, iPreExpBarNum, fPreProgress);
                }

                if (iExp > iPreExpBarNum)
                {
                    x = 2.f; y = 473.f; width = fProgress * 629.f; height = 4.f;
                    RenderBitmap(IMAGE_MASTER_GAUGE_BAR, x, y, width, height, 0.f, 0.f, 6.f / 8.f, 4.f / 4.f);
                    glColor4f(1.f, 1.f, 1.f, 0.6f);
                    RenderColor(x, y, width, height);
                    EndRenderColor();
                }
                else
                {
                    double fGapProgress = fProgress - fPreProgress;
                    fGapProgress = std::clamp(fGapProgress, 0.0, 1.0);
                    x = 2.f; y = 473.f; width = (double)fPreProgress * (double)629.f; height = 4.f;
                    RenderBitmap(IMAGE_MASTER_GAUGE_BAR, x, y, width, height, 0.f, 0.f, 6.f / 8.f, 4.f / 4.f);

                    x += width; width = (double)fGapProgress * (double)629.f;
                    RenderBitmap(IMAGE_MASTER_GAUGE_BAR, x, y, width, height, 0.f, 0.f, 6.f / 8.f, 4.f / 4.f);
                    glColor4f(1.f, 1.f, 1.f, 0.6f);
                    RenderColor(x, y, width, height);
                    EndRenderColor();
                }
            }
        }
        else
        {
            x = 2.f; y = 473.f; width = fProgress * 629.f; height = 4.f;
            RenderBitmap(IMAGE_MASTER_GAUGE_BAR, x, y, width, height, 0.f, 0.f, 6.f / 8.f, 4.f / 4.f);
        }

        x = 635.f; y = 469.f;
        SEASON3B::RenderNumber(x, y, iExp);

        x = 2.f; y = 473.f; width = 629.f; height = 4.f;
        if (SEASON3B::CheckMouseIn(x, y, width, height) == true)
        {
            wchar_t strTipText[256];

            mu_swprintf(strTipText, I18N::Game::EXPI64dI64d, dwExperience, dwNexExperience);
            RenderTipText(280, 418, strTipText);
        }
    }
    else
    {
        x = 0; y = 470; width = 6; height = 4;

        __int64 iPriorLevel = wLevel - 1;
        __int64 iPriorExperience = 0;

        if (iPriorLevel > 0)
        {
            iPriorExperience = (9 + iPriorLevel) * iPriorLevel * iPriorLevel * 10;

            if (iPriorLevel > 255)
            {
                const __int64 iLevelOverN = iPriorLevel - 255;
                iPriorExperience += (9 + iLevelOverN) * iLevelOverN * iLevelOverN * 1000;
            }
        }

        const __int64 lowerBound = iPriorExperience;
        __int64 upperBound = dwNexExperience;
        if (upperBound < lowerBound)
        {
            upperBound = lowerBound;
        }

        const double fNeedExp = static_cast<double>(upperBound - lowerBound);
        const double fClampedExp = std::clamp(static_cast<double>(dwExperience), static_cast<double>(lowerBound), static_cast<double>(upperBound));
        const double fRatio = (fNeedExp > 0.0) ? std::clamp((fClampedExp - static_cast<double>(lowerBound)) / fNeedExp, 0.0, 1.0) : 0.0;
        int iExp = 0;
        double fProgress = 0.0;
        buildExpSegment(fRatio, iExp, fProgress);

        if (m_bExpEffect == true)
        {
            double fPreProgress = 0.f;
            if (m_dwPreExp < lowerBound)
            {
                x = 2.f; y = 473.f; width = fProgress * 629.f; height = 4.f;
                RenderBitmap(IMAGE_GAUGE_EXBAR, x, y, width, height, 0.f, 0.f, 6.f / 8.f, 4.f / 4.f);
                glColor4f(1.f, 1.f, 1.f, 0.4f);
                RenderColor(x, y, width, height);
                EndRenderColor();
            }
            else
            {
                int iPreExpBarNum = 0;
                if (fNeedExp > 0.f)
                {
                    const double fPreClampedExp = std::clamp(static_cast<double>(m_dwPreExp), static_cast<double>(lowerBound), static_cast<double>(upperBound));
                    const double fPreRatio = std::clamp((fPreClampedExp - static_cast<double>(lowerBound)) / fNeedExp, 0.0, 1.0);
                    buildExpSegment(fPreRatio, iPreExpBarNum, fPreProgress);
                }

                if (iExp > iPreExpBarNum)
                {
                    x = 2.f; y = 473.f; width = fProgress * 629.f; height = 4.f;
                    RenderBitmap(IMAGE_GAUGE_EXBAR, x, y, width, height, 0.f, 0.f, 6.f / 8.f, 4.f / 4.f);
                    glColor4f(1.f, 1.f, 1.f, 0.4f);
                    RenderColor(x, y, width, height);
                    EndRenderColor();
                }
                else
                {
                    double fGapProgress = fProgress - fPreProgress;
                    fGapProgress = std::clamp(fGapProgress, 0.0, 1.0);
                    x = 2.f; y = 473.f; width = fPreProgress * 629.f; height = 4.f;
                    RenderBitmap(IMAGE_GAUGE_EXBAR, x, y, width, height, 0.f, 0.f, 6.f / 8.f, 4.f / 4.f);
                    x += width; width = fGapProgress * 629.f;
                    RenderBitmap(IMAGE_GAUGE_EXBAR, x, y, width, height, 0.f, 0.f, 6.f / 8.f, 4.f / 4.f);
                    glColor4f(1.f, 1.f, 1.f, 0.4f);
                    RenderColor(x, y, width, height);
                    EndRenderColor();
                }
            }
        }
        else
        {
            x = 2.f; y = 473.f; width = fProgress * 629.f; height = 4.f;
            RenderBitmap(IMAGE_GAUGE_EXBAR, x, y, width, height, 0.f, 0.f, 6.f / 8.f, 4.f / 4.f);
        }

        x = 635.f; y = 469.f;
        SEASON3B::RenderNumber(x, y, iExp);

        x = 2.f; y = 473.f; width = 629.f; height = 4.f;
        if (SEASON3B::CheckMouseIn(x, y, width, height) == true)
        {
            wchar_t strTipText[256];

            mu_swprintf(strTipText, I18N::Game::EXPI64dI64d, dwExperience, dwNexExperience);
            RenderTipText(280, 418, strTipText);
        }
    }
}

void SEASON3B::CNewUIMainFrameWindow::RenderHotKeyItemCount()
{
    m_ItemHotKey.RenderItemCount();
}

void SEASON3B::CNewUIMainFrameWindow::RenderButtons()
{
#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
    m_BtnCShop.Render();
#endif //defined PBG_ADD_INGAMESHOP_UI_MAINFRAME

    RenderCharInfoButton();
    m_BtnMyInven.Render();

    RenderFriendButton();

    m_BtnWindow.Render();
}

void SEASON3B::CNewUIMainFrameWindow::RenderCharInfoButton()
{
    m_BtnChaInfo.Render();

    if (g_QuestMng.IsQuestIndexByEtcListEmpty())
        return;

    if (g_Time.GetTimeCheck(5, 500))
        m_bButtonBlink = !m_bButtonBlink;

    if (m_bButtonBlink)
    {
        if (!(g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_QUEST_PROGRESS_ETC)
            || g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHARACTER)))
            RenderImage(IMAGE_MENU_BTN_CHAINFO, 489 + 30, REFERENCE_HEIGHT - 51, 30, 41, 0.0f, 41.f);
    }
}

void SEASON3B::CNewUIMainFrameWindow::RenderFriendButton()
{
    m_BtnFriend.Render();

    int iBlinkTemp = g_pFriendMenu->GetBlinkTemp();
    BOOL bIsAlertTime = (iBlinkTemp % 24 < 12);

    if (g_pFriendMenu->IsNewChatAlert() && bIsAlertTime)
    {
        RenderFriendButtonState();
    }
    if (g_pFriendMenu->IsNewMailAlert())
    {
        if (bIsAlertTime)
        {
            RenderFriendButtonState();

            if (iBlinkTemp % 24 == 11)
            {
                g_pFriendMenu->IncreaseLetterBlink();
            }
        }
    }
    else if (g_pLetterList->CheckNoReadLetter())
    {
        RenderFriendButtonState();
    }

    g_pFriendMenu->IncreaseBlinkTemp();
}

void SEASON3B::CNewUIMainFrameWindow::RenderFriendButtonState()
{
#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_FRIEND) == true)
    {
        RenderImage(IMAGE_MENU_BTN_FRIEND, 489 + (30 * 3), REFERENCE_HEIGHT - 51, 30, 41, 0.0f, 123.f);
    }
    else
    {
        RenderImage(IMAGE_MENU_BTN_FRIEND, 489 + (30 * 3), REFERENCE_HEIGHT - 51, 30, 41, 0.0f, 41.f);
    }
#else //defined PBG_ADD_INGAMESHOP_UI_MAINFRAME
    if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_FRIEND) == true)
    {
        RenderImage(IMAGE_MENU_BTN_FRIEND, 488 + 76, REFERENCE_HEIGHT - 51, 38, 42, 0.0f, 126.f);
    }
    else
    {
        RenderImage(IMAGE_MENU_BTN_FRIEND, 488 + 76, REFERENCE_HEIGHT - 51, 38, 42, 0.0f, 42.f);
    }
#endif//defined PBG_ADD_INGAMESHOP_UI_MAINFRAME
}

bool SEASON3B::CNewUIMainFrameWindow::UpdateMouseEvent()
{
    if (g_pNewUIHotKey->IsStateGameOver() == true)
    {
        return true;
    }

    if (BtnProcess() == true)
    {
        return false;
    }

    return true;
}

bool SEASON3B::CNewUIMainFrameWindow::BtnProcess()
{
    if (ProcessAzothWindowMouseEvents() == true)
    {
        return true;
    }

    if (ProcessJewelBankWindowMouseEvents() == true)
    {
        return true;
    }

    if (ProcessGameSettingsWindowMouseEvents() == true)
    {
        return true;
    }

    if (ProcessCommandsWindowMouseEvents() == true)
    {
        return true;
    }

    if (ProcessAccountGuardFormationWindowMouseEvents() == true)
    {
        return true;
    }

    if (ProcessAccountGuardWindowMouseEvents() == true)
    {
        return true;
    }

    if (g_pNewUIHotKey->CanUpdateKeyEventRelatedMyInventory() == true)
    {
        if (ProcessAzothHudMouseEvents() == true)
        {
            return true;
        }

        if (ProcessAccountGuardHudButtons() == true)
        {
            return true;
        }

        if (m_BtnMyInven.UpdateMouseEvent() == true)
        {
            g_pNewUISystem->Toggle(SEASON3B::INTERFACE_INVENTORY);
            PlayBuffer(SOUND_CLICK01);
            return true;
        }
    }
    else if (g_pNewUIHotKey->CanUpdateKeyEvent() == true)
    {
        if (ProcessAzothHudMouseEvents() == true)
        {
            return true;
        }

        if (ProcessAccountGuardHudButtons() == true)
        {
            return true;
        }

        if (m_BtnMyInven.UpdateMouseEvent() == true)
        {
            g_pNewUISystem->Toggle(SEASON3B::INTERFACE_INVENTORY);
            PlayBuffer(SOUND_CLICK01);
            return true;
        }
        else if (m_BtnChaInfo.UpdateMouseEvent() == true)
        {
            g_pNewUISystem->Toggle(SEASON3B::INTERFACE_CHARACTER);

            PlayBuffer(SOUND_CLICK01);

            if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_CHARACTER))
                g_QuestMng.SendQuestIndexByEtcSelection();

            return true;
        }
        else if (m_BtnFriend.UpdateMouseEvent() == true)
        {
            if (gMapManager.InChaosCastle() == true)
            {
                PlayBuffer(SOUND_CLICK01);
                return true;
            }

            int iLevel = CharacterAttribute->Level;

            if (iLevel < 6)
            {
                if (g_pSystemLogBox->CheckChatRedundancy(I18N::Game::YouMustBeAtLeastLevel6ToUseTheMyFriendFunction) == FALSE)
                {
                    g_pSystemLogBox->AddText(I18N::Game::YouMustBeAtLeastLevel6ToUseTheMyFriendFunction, SEASON3B::TYPE_SYSTEM_MESSAGE);
                }
            }
            else
            {
                g_pNewUISystem->Toggle(SEASON3B::INTERFACE_FRIEND);
            }
            PlayBuffer(SOUND_CLICK01);
            return true;
        }
        else if (m_BtnWindow.UpdateMouseEvent() == true)
        {
            g_pNewUISystem->Toggle(SEASON3B::INTERFACE_WINDOW_MENU);
            PlayBuffer(SOUND_CLICK01);
            return true;
        }

#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
        else if (m_BtnCShop.UpdateMouseEvent() == true)
        {
            if (g_pInGameShop->IsInGameShopOpen() == false)
                return false;

#ifdef KJH_MOD_SHOP_SCRIPT_DOWNLOAD
            if (g_InGameShopSystem->IsScriptDownload() == true)
            {
                if (g_InGameShopSystem->ScriptDownload() == false)
                    return false;
            }

            if (g_InGameShopSystem->IsBannerDownload() == true)
            {
                g_InGameShopSystem->BannerDownload();
            }
#endif // KJH_MOD_SHOP_SCRIPT_DOWNLOAD

            if (g_pNewUISystem->IsVisible(SEASON3B::INTERFACE_INGAMESHOP) == false)
            {
                if (g_InGameShopSystem->GetIsRequestShopOpenning() == false)
                {
                    SocketClient->ToGameServer()->SendCashShopOpenState(0);
                    g_InGameShopSystem->SetIsRequestShopOpenning(true);

#ifdef KJH_MOD_SHOP_SCRIPT_DOWNLOAD
                    g_pMainFrame->SetBtnState(MAINFRAME_BTN_PARTCHARGE, true);
#endif // KJH_MOD_SHOP_SCRIPT_DOWNLOAD
                }
            }
            else
            {
                SocketClient->ToGameServer()->SendCashShopOpenState(1);
                g_pNewUISystem->Hide(SEASON3B::INTERFACE_INGAMESHOP);
            }

            return true;
        }
#endif //PBG_ADD_INGAMESHOP_UI_MAINFRAME
    }

    return false;
}

bool SEASON3B::CNewUIMainFrameWindow::UpdateKeyEvent()
{
    if (g_isAzothWindowOpen && SEASON3B::IsPress(VK_ESCAPE) == true)
    {
        CloseAzothWindow();
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    if (g_isJewelBankWindowOpen && SEASON3B::IsPress(VK_ESCAPE) == true)
    {
        CloseJewelBankWindow();
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    if (g_isGameSettingsWindowOpen && SEASON3B::IsPress(VK_ESCAPE) == true)
    {
        CloseGameSettingsWindow();
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    if (g_isCommandsWindowOpen && SEASON3B::IsPress(VK_ESCAPE) == true)
    {
        CloseCommandsWindow();
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    if (g_isAccountGuardFormationWindowOpen && SEASON3B::IsPress(VK_ESCAPE) == true)
    {
        CloseAccountGuardFormationWindow();
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    if (g_isAccountGuardWindowOpen && SEASON3B::IsPress(VK_ESCAPE) == true)
    {
        CloseAccountGuardWindow();
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    if (m_ItemHotKey.UpdateKeyEvent() == false)
    {
        return false;
    }
    return true;
}

bool SEASON3B::CNewUIMainFrameWindow::Update()
{
    if (m_bExpEffect == true)
    {
        if (timeGetTime() - m_dwExpEffectTime > 2000)
        {
            m_bExpEffect = false;
            m_dwExpEffectTime = 0;
            m_dwGetExp = 0;
        }
    }

    return true;
}

float SEASON3B::CNewUIMainFrameWindow::GetLayerDepth()
{
    return 10.6f;
}

float SEASON3B::CNewUIMainFrameWindow::GetKeyEventOrder()
{
    return 2.9f;
}

void SEASON3B::CNewUIMainFrameWindow::SetItemHotKey(int iHotKey, int iItemType, int iItemLevel)
{
    m_ItemHotKey.SetHotKey(iHotKey, iItemType, iItemLevel);
}

int SEASON3B::CNewUIMainFrameWindow::GetItemHotKey(int iHotKey)
{
    return m_ItemHotKey.GetHotKey(iHotKey);
}

int SEASON3B::CNewUIMainFrameWindow::GetItemHotKeyLevel(int iHotKey)
{
    return m_ItemHotKey.GetHotKeyLevel(iHotKey);
}

void SEASON3B::CNewUIMainFrameWindow::UseHotKeyItemRButton()
{
    m_ItemHotKey.UseItemRButton();
}

void SEASON3B::CNewUIMainFrameWindow::UpdateItemHotKey()
{
    m_ItemHotKey.UpdateKeyEvent();
}

void SEASON3B::CNewUIMainFrameWindow::ResetSkillHotKey()
{
    g_pSkillList->Reset();
}

void SEASON3B::CNewUIMainFrameWindow::LoadSkillHotKeysLocal()
{
    g_pSkillList->LoadLocalHotKeys();
}

void SEASON3B::CNewUIMainFrameWindow::SetSkillHotKey(int iHotKey, int iSkillType)
{
    g_pSkillList->SetHotKeyFromServer(iHotKey, iSkillType);
}

int SEASON3B::CNewUIMainFrameWindow::GetSkillHotKey(int iHotKey)
{
    return g_pSkillList->GetHotKey(iHotKey);
}

int SEASON3B::CNewUIMainFrameWindow::GetSkillHotKeyIndex(int iSkillType)
{
    return g_pSkillList->GetSkillIndex(iSkillType);
}

SEASON3B::CNewUIItemHotKey::CNewUIItemHotKey()
{
    for (int i = 0; i < HOTKEY_COUNT; ++i)
    {
        m_iHotKeyItemType[i] = -1;
        m_iHotKeyItemLevel[i] = 0;
    }
}

SEASON3B::CNewUIItemHotKey::~CNewUIItemHotKey()
{
}

bool SEASON3B::CNewUIItemHotKey::UpdateKeyEvent()
{
    int iIndex = -1;

    if (SEASON3B::IsPress('Q') == true)
    {
        iIndex = GetHotKeyItemIndex(HOTKEY_Q);
    }
    else if (SEASON3B::IsPress('W') == true)
    {
        iIndex = GetHotKeyItemIndex(HOTKEY_W);
    }
    else if (SEASON3B::IsPress('E') == true)
    {
        iIndex = GetHotKeyItemIndex(HOTKEY_E);
    }
    else if (SEASON3B::IsPress('R') == true)
    {
        iIndex = GetHotKeyItemIndex(HOTKEY_R);
    }

    if (iIndex != -1)
    {
        ITEM* pItem = NULL;
        pItem = g_pMyInventory->FindItem(iIndex);
        if ((pItem->Type >= ITEM_POTION + 78 && pItem->Type <= ITEM_POTION + 82))
        {
            std::list<eBuffState> secretPotionbufflist;
            secretPotionbufflist.push_back(eBuff_SecretPotion1);
            secretPotionbufflist.push_back(eBuff_SecretPotion2);
            secretPotionbufflist.push_back(eBuff_SecretPotion3);
            secretPotionbufflist.push_back(eBuff_SecretPotion4);
            secretPotionbufflist.push_back(eBuff_SecretPotion5);

            if (g_isCharacterBufflist((&Hero->Object), secretPotionbufflist) != eBuffNone) {
                SEASON3B::CreateOkMessageBox(I18N::Game::YouCannotUseThisItemWhileThePotionEffectsRemainActive, RGBA(255, 30, 0, 255));
            }
            else {
                SendRequestUse(iIndex, 0);
            }
        }
        else

        {
            SendRequestUse(iIndex, 0);
        }
        return false;
    }

    return true;
}

int SEASON3B::CNewUIItemHotKey::GetHotKeyItemIndex(int iType, bool bItemCount)
{
    int iStartItemType = 0, iEndItemType = 0;
    int i, j;

    switch (iType)
    {
    case HOTKEY_Q:
        if (GetHotKeyCommonItem(iType, iStartItemType, iEndItemType) == false)
        {
            if (m_iHotKeyItemType[iType] >= ITEM_SMALL_MANA_POTION && m_iHotKeyItemType[iType] <= ITEM_LARGE_MANA_POTION)
            {
                iStartItemType = ITEM_LARGE_MANA_POTION; iEndItemType = ITEM_SMALL_MANA_POTION;
            }
            else
            {
                iStartItemType = ITEM_LARGE_HEALING_POTION; iEndItemType = ITEM_APPLE;
            }
        }
        break;
    case HOTKEY_W:
        if (GetHotKeyCommonItem(iType, iStartItemType, iEndItemType) == false)
        {
            if (m_iHotKeyItemType[iType] >= ITEM_APPLE && m_iHotKeyItemType[iType] <= ITEM_LARGE_HEALING_POTION)
            {
                iStartItemType = ITEM_LARGE_HEALING_POTION; iEndItemType = ITEM_APPLE;
            }
            else
            {
                iStartItemType = ITEM_LARGE_MANA_POTION; iEndItemType = ITEM_SMALL_MANA_POTION;
            }
        }
        break;
    case HOTKEY_E:
        if (GetHotKeyCommonItem(iType, iStartItemType, iEndItemType) == false)
        {
            if (m_iHotKeyItemType[iType] >= ITEM_APPLE && m_iHotKeyItemType[iType] <= ITEM_LARGE_HEALING_POTION)
            {
                iStartItemType = ITEM_LARGE_HEALING_POTION; iEndItemType = ITEM_APPLE;
            }
            else if (m_iHotKeyItemType[iType] >= ITEM_SMALL_MANA_POTION && m_iHotKeyItemType[iType] <= ITEM_LARGE_MANA_POTION)
            {
                iStartItemType = ITEM_LARGE_MANA_POTION; iEndItemType = ITEM_SMALL_MANA_POTION;
            }
            else
            {
                iStartItemType = ITEM_ANTIDOTE; iEndItemType = ITEM_ANTIDOTE;
            }
        }
        break;
    case HOTKEY_R:
        if (GetHotKeyCommonItem(iType, iStartItemType, iEndItemType) == false)
        {
            if (m_iHotKeyItemType[iType] >= ITEM_APPLE && m_iHotKeyItemType[iType] <= ITEM_LARGE_HEALING_POTION)
            {
                iStartItemType = ITEM_LARGE_HEALING_POTION; iEndItemType = ITEM_APPLE;
            }
            else if (m_iHotKeyItemType[iType] >= ITEM_SMALL_MANA_POTION && m_iHotKeyItemType[iType] <= ITEM_LARGE_MANA_POTION)
            {
                iStartItemType = ITEM_LARGE_MANA_POTION; iEndItemType = ITEM_SMALL_MANA_POTION;
            }
            else
            {
                iStartItemType = ITEM_LARGE_SHIELD_POTION; iEndItemType = ITEM_SMALL_SHIELD_POTION;
            }
        }
        break;
    }

    int iItemCount = 0;
    ITEM* pItem = NULL;

    int iNumberofItems = g_pMyInventory->GetInventoryCtrl()->GetNumberOfItems();
    for (i = iStartItemType; i >= iEndItemType; --i)
    {
        if (bItemCount)
        {
            for (j = 0; j < iNumberofItems; ++j)
            {
                pItem = g_pMyInventory->GetInventoryCtrl()->GetItem(j);
                if (pItem == NULL)
                {
                    continue;
                }

                if (
                    (pItem->Type == i && pItem->Level == m_iHotKeyItemLevel[iType])
                    || (pItem->Type == i && (pItem->Type >= ITEM_APPLE && pItem->Type <= ITEM_LARGE_HEALING_POTION))
                    )
                {
                    if (pItem->Type == ITEM_ALE
                        || pItem->Type == ITEM_TOWN_PORTAL_SCROLL
                        || pItem->Type == ITEM_POTION + 20
                        )
                    {
                        iItemCount++;
                    }
                    else
                    {
                        iItemCount += pItem->Durability;
                    }
                }
            }
        }
        else
        {
            int iIndex = -1;
            if (i >= ITEM_APPLE && i <= ITEM_LARGE_HEALING_POTION)
            {
                iIndex = g_pMyInventory->FindItemReverseIndex(i);
            }
            else
            {
                iIndex = g_pMyInventory->FindItemReverseIndex(i, m_iHotKeyItemLevel[iType]);
            }

            if (-1 != iIndex)
            {
                pItem = g_pMyInventory->FindItem(iIndex);
                if ((pItem->Type != ITEM_SIEGE_POTION
                    && pItem->Type != ITEM_TOWN_PORTAL_SCROLL
                    && pItem->Type != ITEM_POTION + 20)
                    || pItem->Level == m_iHotKeyItemLevel[iType]
                    )
                {
                    return iIndex;
                }
            }
        }
    }

    if (bItemCount == true)
    {
        return iItemCount;
    }

    return -1;
}

bool SEASON3B::CNewUIItemHotKey::GetHotKeyCommonItem(IN int iHotKey, OUT int& iStart, OUT int& iEnd)
{
    switch (m_iHotKeyItemType[iHotKey])
    {
    case ITEM_SIEGE_POTION:
    case ITEM_ANTIDOTE:
    case ITEM_ALE:
    case ITEM_TOWN_PORTAL_SCROLL:
    case ITEM_POTION + 20:
    case ITEM_JACK_OLANTERN_BLESSINGS:
    case ITEM_JACK_OLANTERN_WRATH:
    case ITEM_JACK_OLANTERN_CRY:
    case ITEM_JACK_OLANTERN_FOOD:
    case ITEM_JACK_OLANTERN_DRINK:
    case ITEM_POTION + 70:
    case ITEM_POTION + 71:
    case ITEM_POTION + 78:
    case ITEM_POTION + 79:
    case ITEM_POTION + 80:
    case ITEM_POTION + 81:
    case ITEM_POTION + 82:
    case ITEM_POTION + 94:
    case ITEM_CHERRY_BLOSSOM_WINE:
    case ITEM_CHERRY_BLOSSOM_RICE_CAKE:
    case ITEM_CHERRY_BLOSSOM_FLOWER_PETAL:
    case ITEM_POTION + 133:
        if (m_iHotKeyItemType[iHotKey] != ITEM_POTION + 20 || m_iHotKeyItemLevel[iHotKey] == 0)
        {
            iStart = iEnd = m_iHotKeyItemType[iHotKey];
            return true;
        }
        break;
    default:
        if (m_iHotKeyItemType[iHotKey] >= ITEM_SMALL_SHIELD_POTION && m_iHotKeyItemType[iHotKey] <= ITEM_LARGE_SHIELD_POTION)
        {
            iStart = ITEM_LARGE_SHIELD_POTION; iEnd = ITEM_SMALL_SHIELD_POTION;
            return true;
        }
        else if (m_iHotKeyItemType[iHotKey] >= ITEM_SMALL_COMPLEX_POTION && m_iHotKeyItemType[iHotKey] <= ITEM_LARGE_COMPLEX_POTION)
        {
            iStart = ITEM_LARGE_COMPLEX_POTION; iEnd = ITEM_SMALL_COMPLEX_POTION;
            return true;
        }
        break;
    }
    return false;
}

int SEASON3B::CNewUIItemHotKey::GetHotKeyItemCount(int iType)
{
    return 0;
}

void SEASON3B::CNewUIItemHotKey::SetHotKey(int iHotKey, int iItemType, int iItemLevel)
{
    if (iHotKey != -1 && CNewUIMyInventory::CanRegisterItemHotKey(iItemType) == true
        )
    {
        m_iHotKeyItemType[iHotKey] = iItemType;
        m_iHotKeyItemLevel[iHotKey] = iItemLevel;
    }
    else
    {
        m_iHotKeyItemType[iHotKey] = -1;
        m_iHotKeyItemLevel[iHotKey] = 0;
    }
}

int SEASON3B::CNewUIItemHotKey::GetHotKey(int iHotKey)
{
    if (iHotKey != -1)
    {
        return m_iHotKeyItemType[iHotKey];
    }

    return -1;
}

int SEASON3B::CNewUIItemHotKey::GetHotKeyLevel(int iHotKey)
{
    if (iHotKey != -1)
    {
        return m_iHotKeyItemLevel[iHotKey];
    }

    return 0;
}

void SEASON3B::CNewUIItemHotKey::RenderItems()
{
    float x, y, width, height;

    for (int i = 0; i < HOTKEY_COUNT; ++i)
    {
        int iIndex = GetHotKeyItemIndex(i);
        if (iIndex != -1)
        {
            ITEM* pItem = g_pMyInventory->FindItem(iIndex);
            if (pItem)
            {
                x = 10 + (i * 38); y = 443; width = 20; height = 20;
                RenderItem3D(x, y, width, height, pItem->Type, pItem->Level, 0, 0);
            }
        }
    }
}

void SEASON3B::CNewUIItemHotKey::RenderItemCount()
{
    float x, y, width, height;

    glColor4f(1.f, 1.f, 1.f, 1.f);

    for (int i = 0; i < HOTKEY_COUNT; ++i)
    {
        int iCount = GetHotKeyItemIndex(i, true);
        if (iCount > 0)
        {
            x = 30 + (i * 38); y = 457; width = 8; height = 9;
            SEASON3B::RenderNumber(x, y, iCount);
        }
    }
}

void SEASON3B::CNewUIItemHotKey::UseItemRButton()
{
    int x, y, width, height;

    for (int i = 0; i < HOTKEY_COUNT; ++i)
    {
        x = 10 + (i * 38); y = 445; width = 20; height = 20;
        if (SEASON3B::CheckMouseIn(x, y, width, height) == true)
        {
            if (MouseRButtonPush)
            {
                MouseRButtonPush = false;
                int iIndex = GetHotKeyItemIndex(i);
                if (iIndex != -1)
                {
                    SendRequestUse(iIndex, 0);
                    break;
                }
            }
        }
    }
}

SEASON3B::CNewUISkillList::CNewUISkillList()
{
    m_pNewUIMng = NULL;
    Reset();
}

SEASON3B::CNewUISkillList::~CNewUISkillList()
{
    Release();
}

bool SEASON3B::CNewUISkillList::Create(CNewUIManager* pNewUIMng, CNewUI3DRenderMng* pNewUI3DRenderMng)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(SEASON3B::INTERFACE_SKILL_LIST, this);

    m_pNewUI3DRenderMng = pNewUI3DRenderMng;

    LoadImages();

    Show(true);

    return true;
}

void SEASON3B::CNewUISkillList::Release()
{
    if (m_pNewUI3DRenderMng)
    {
        m_pNewUI3DRenderMng->DeleteUI2DEffectObject(UI2DEffectCallback);
    }

    UnloadImages();

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }
}

void SEASON3B::CNewUISkillList::Reset()
{
    m_bSkillList = false;
    m_bHotKeySkillListUp = false;

    m_bRenderSkillInfo = false;
    m_iRenderSkillInfoType = 0;
    m_iRenderSkillInfoPosX = 0;
    m_iRenderSkillInfoPosY = 0;

    for (int i = 0; i < SKILLHOTKEY_COUNT; ++i)
    {
        m_iHotKeySkillType[i] = -1;
    }

    m_EventState = EVENT_NONE;
}

void SEASON3B::CNewUISkillList::SaveLocalHotKeys() const
{
    const auto path = ResolveLocalSkillHotKeyPath();
    if (path.empty())
    {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
        return;
    }

    SkillHotKeyCache cache{};
    for (int index = 0; index < SKILLHOTKEY_COUNT; ++index)
    {
        cache.SkillIds[index] = ResolveSkillIdFromHotKeySlot(m_iHotKeySkillType[index]);
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        return;
    }

    file.write(reinterpret_cast<const char*>(&cache), sizeof(cache));
}

void SEASON3B::CNewUISkillList::LoadLocalHotKeys()
{
    const auto path = ResolveLocalSkillHotKeyPath();
    if (path.empty() || !std::filesystem::exists(path))
    {
        return;
    }

    SkillHotKeyCache cache{};
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return;
    }

    file.read(reinterpret_cast<char*>(&cache), sizeof(cache));
    if (file.gcount() != sizeof(cache)
        || cache.Magic != kSkillHotKeyCacheMagic
        || cache.Version != kSkillHotKeyCacheVersion)
    {
        return;
    }

    for (int index = 0; index < SKILLHOTKEY_COUNT; ++index)
    {
        m_iHotKeySkillType[index] = -1;
    }

    for (int index = 0; index < SKILLHOTKEY_COUNT; ++index)
    {
        const int skillSlot = ResolveHotKeySlotFromSkillId(cache.SkillIds[index]);
        if (skillSlot == -1)
        {
            continue;
        }

        SetHotKeyInternal(index, skillSlot, false);
    }
}

void SEASON3B::CNewUISkillList::LoadImages()
{
    LoadBitmap(L"Interface\\newui_skill.jpg", IMAGE_SKILL1, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_skill2.jpg", IMAGE_SKILL2, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_command.jpg", IMAGE_COMMAND, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_skillbox.jpg", IMAGE_SKILLBOX, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_skillbox2.jpg", IMAGE_SKILLBOX_USE, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_non_skill.jpg", IMAGE_NON_SKILL1, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_non_skill2.jpg", IMAGE_NON_SKILL2, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_non_command.jpg", IMAGE_NON_COMMAND, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_skill3.jpg", IMAGE_SKILL3, GL_LINEAR);
    LoadBitmap(L"Interface\\newui_non_skill3.jpg", IMAGE_NON_SKILL3, GL_LINEAR);
}

void SEASON3B::CNewUISkillList::UnloadImages()
{
    DeleteBitmap(IMAGE_SKILL1);
    DeleteBitmap(IMAGE_SKILL2);
    DeleteBitmap(IMAGE_COMMAND);
    DeleteBitmap(IMAGE_SKILLBOX);
    DeleteBitmap(IMAGE_SKILLBOX_USE);
    DeleteBitmap(IMAGE_NON_SKILL1);
    DeleteBitmap(IMAGE_NON_SKILL2);
    DeleteBitmap(IMAGE_NON_COMMAND);
    DeleteBitmap(IMAGE_SKILL3);
    DeleteBitmap(IMAGE_NON_SKILL3);
}

bool SEASON3B::CNewUISkillList::UpdateMouseEvent()
{
#ifdef MOD_SKILLLIST_UPDATEMOUSE_BLOCK
    if (GFxProcess::GetInstancePtr()->GetUISelect() == 1)
    {
        return true;
    }
#endif //MOD_SKILLLIST_UPDATEMOUSE_BLOCK

    if (g_isCharacterBuff((&Hero->Object), eBuff_DuelWatch))
    {
        m_bSkillList = false;
        return true;
    }

    BYTE bySkillNumber = CharacterAttribute->SkillNumber;
    BYTE bySkillMasterNumber = CharacterAttribute->SkillMasterNumber;

    float x, y, width, height;

    m_bRenderSkillInfo = false;

    if (bySkillNumber <= 0)
    {
        return true;
    }

    x = 385.f; y = 431.f; width = 32.f; height = 38.f;
    if (m_EventState == EVENT_NONE && MouseLButtonPush == false
        && SEASON3B::CheckMouseIn(x, y, width, height) == true)
    {
        m_EventState = EVENT_BTN_HOVER_CURRENTSKILL;
        return true;
    }
    if (m_EventState == EVENT_BTN_HOVER_CURRENTSKILL && MouseLButtonPush == false
        && SEASON3B::CheckMouseIn(x, y, width, height) == false)
    {
        m_EventState = EVENT_NONE;
        return true;
    }
    if (m_EventState == EVENT_BTN_HOVER_CURRENTSKILL && (MouseLButtonPush == true || MouseLButtonDBClick == true)
        && SEASON3B::CheckMouseIn(x, y, width, height) == true)
    {
        m_EventState = EVENT_BTN_DOWN_CURRENTSKILL;
        return false;
    }
    if (m_EventState == EVENT_BTN_DOWN_CURRENTSKILL)
    {
        if (MouseLButtonPush == false && MouseLButtonDBClick == false)
        {
            if (SEASON3B::CheckMouseIn(x, y, width, height) == true)
            {
                m_bSkillList = !m_bSkillList;
                PlayBuffer(SOUND_CLICK01);
                m_EventState = EVENT_NONE;
                return false;
            }
            m_EventState = EVENT_NONE;
            return true;
        }
    }

    if (m_EventState == EVENT_BTN_HOVER_CURRENTSKILL)
    {
        m_bRenderSkillInfo = true;
        m_iRenderSkillInfoType = Hero->CurrentSkill;
        m_iRenderSkillInfoPosX = x - 5;
        m_iRenderSkillInfoPosY = y;

        return false;
    }
    else if (m_EventState == EVENT_BTN_DOWN_CURRENTSKILL)
    {
        return false;
    }

    x = 222.f; y = 431.f; width = 32.f * 5.f; height = 38.f;
    if (m_EventState == EVENT_NONE && MouseLButtonPush == false
        && SEASON3B::CheckMouseIn(x, y, width, height) == true)
    {
        m_EventState = EVENT_BTN_HOVER_SKILLHOTKEY;
        return true;
    }
    if (m_EventState == EVENT_BTN_HOVER_SKILLHOTKEY && MouseLButtonPush == false
        && SEASON3B::CheckMouseIn(x, y, width, height) == false)
    {
        m_EventState = EVENT_NONE;
        return true;
    }
    if (m_EventState == EVENT_BTN_HOVER_SKILLHOTKEY && MouseLButtonPush == true
        && SEASON3B::CheckMouseIn(x, y, width, height) == true)
    {
        m_EventState = EVENT_BTN_DOWN_SKILLHOTKEY;
        return false;
    }

    x = 190.f; y = 431.f; width = 32.f; height = 38.f;
    int iStartIndex = (m_bHotKeySkillListUp == true) ? 6 : 1;
    for (int i = 0, iIndex = iStartIndex; i < 5; ++i, iIndex++)
    {
        x += width;

        if (iIndex == 10)
        {
            iIndex = 0;
        }
        if (SEASON3B::CheckMouseIn(x, y, width, height) == true)
        {
            if (m_iHotKeySkillType[iIndex] == -1)
            {
                if (m_EventState == EVENT_BTN_HOVER_SKILLHOTKEY)
                {
                    m_bRenderSkillInfo = false;
                    m_iRenderSkillInfoType = -1;
                }
                if (m_EventState == EVENT_BTN_DOWN_SKILLHOTKEY && MouseLButtonPush == false)
                {
                    m_EventState = EVENT_NONE;
                }
                continue;
            }

            WORD bySkillType = CharacterAttribute->Skill[m_iHotKeySkillType[iIndex]];

            if (bySkillType == 0 || (bySkillType >= AT_SKILL_STUN && bySkillType <= AT_SKILL_REMOVAL_BUFF))
                continue;

            BYTE bySkillUseType = SkillAttribute[bySkillType].SkillUseType;

            if (bySkillUseType == SKILL_USE_TYPE_MASTERLEVEL)
            {
                continue;
            }

            if (m_EventState == EVENT_BTN_HOVER_SKILLHOTKEY)
            {
                m_bRenderSkillInfo = true;
                m_iRenderSkillInfoType = m_iHotKeySkillType[iIndex];
                m_iRenderSkillInfoPosX = x - 5;
                m_iRenderSkillInfoPosY = y;
                return true;
            }
            if (m_EventState == EVENT_BTN_DOWN_SKILLHOTKEY)
            {
                if (MouseLButtonPush == false)
                {
                    if (m_iRenderSkillInfoType == m_iHotKeySkillType[iIndex])
                    {
                        m_EventState = EVENT_NONE;
                        m_wHeroPriorSkill = CharacterAttribute->Skill[Hero->CurrentSkill];
                        Hero->CurrentSkill = m_iHotKeySkillType[iIndex];
                        PlayBuffer(SOUND_CLICK01);
                        return false;
                    }
                    else
                    {
                        m_EventState = EVENT_NONE;
                    }
                }
            }
        }
    }

    x = 222.f; y = 431.f; width = 32.f * 5.f; height = 38.f;
    if (m_EventState == EVENT_BTN_DOWN_SKILLHOTKEY)
    {
        if (MouseLButtonPush == false && SEASON3B::CheckMouseIn(x, y, width, height) == false)
        {
            m_EventState = EVENT_NONE;
            return true;
        }
        return false;
    }

    if (m_bSkillList == false)
        return true;

    WORD bySkillType = 0;

    int iSkillCount = 0;
    bool bMouseOnSkillList = false;

    x = 385.f; y = 390; width = 32; height = 38;
    float fOrigX = 385.f;

    EVENT_STATE PrevEventState = m_EventState;

    for (int i = 0; i < MAX_MAGIC; ++i)
    {
        bySkillType = CharacterAttribute->Skill[i];

        if (bySkillType == 0 || (bySkillType >= AT_SKILL_STUN && bySkillType <= AT_SKILL_REMOVAL_BUFF))
            continue;

        BYTE bySkillUseType = SkillAttribute[bySkillType].SkillUseType;

        if (bySkillUseType == SKILL_USE_TYPE_MASTERLEVEL)
        {
            continue;
        }

        if (iSkillCount == 18)
        {
            y -= height;
        }

        if (iSkillCount < 14)
        {
            int iRemainder = iSkillCount % 2;
            int iQuotient = iSkillCount / 2;

            if (iRemainder == 0)
            {
                x = fOrigX + iQuotient * width;
            }
            else
            {
                x = fOrigX - (iQuotient + 1) * width;
            }
        }
        else if (iSkillCount >= 14 && iSkillCount < 18)
        {
            x = fOrigX - (8 * width) - ((iSkillCount - 14) * width);
        }
        else
        {
            x = fOrigX - (12 * width) + ((iSkillCount - 17) * width);
        }

        iSkillCount++;

        if (SEASON3B::CheckMouseIn(x, y, width, height) == true)
        {
            bMouseOnSkillList = true;
            if (m_EventState == EVENT_NONE && MouseLButtonPush == false)
            {
                m_EventState = EVENT_BTN_HOVER_SKILLLIST;
                break;
            }
        }

        if (m_EventState == EVENT_BTN_HOVER_SKILLLIST && MouseLButtonPush == true
            && SEASON3B::CheckMouseIn(x, y, width, height) == true)
        {
            m_EventState = EVENT_BTN_DOWN_SKILLLIST;
            break;
        }

        if (m_EventState == EVENT_BTN_HOVER_SKILLLIST && MouseLButtonPush == false
            && SEASON3B::CheckMouseIn(x, y, width, height) == true)
        {
            m_bRenderSkillInfo = true;
            m_iRenderSkillInfoType = i;
            m_iRenderSkillInfoPosX = x;
            m_iRenderSkillInfoPosY = y;
        }

        if (m_EventState == EVENT_BTN_DOWN_SKILLLIST && MouseLButtonPush == false
            && m_iRenderSkillInfoType == i && SEASON3B::CheckMouseIn(x, y, width, height) == true)
        {
            m_EventState = EVENT_NONE;

            m_wHeroPriorSkill = CharacterAttribute->Skill[Hero->CurrentSkill];

            Hero->CurrentSkill = i;
            m_bSkillList = false;

            PlayBuffer(SOUND_CLICK01);
            return false;
        }
    }

    if (PrevEventState != m_EventState)
    {
        if (m_EventState == EVENT_NONE || m_EventState == EVENT_BTN_HOVER_SKILLLIST)
            return true;
        return false;
    }

    if (Hero->m_pPet != NULL)
    {
        x = 353.f; y = 352; width = 32; height = 38;
        for (int i = AT_PET_COMMAND_DEFAULT; i < AT_PET_COMMAND_END; ++i)
        {
            if (SEASON3B::CheckMouseIn(x, y, width, height) == true)
            {
                bMouseOnSkillList = true;

                if (m_EventState == EVENT_NONE && MouseLButtonPush == false)
                {
                    m_EventState = EVENT_BTN_HOVER_SKILLLIST;
                    return true;
                }
                if (m_EventState == EVENT_BTN_HOVER_SKILLLIST && MouseLButtonPush == true)
                {
                    m_EventState = EVENT_BTN_DOWN_SKILLLIST;
                    return false;
                }

                if (m_EventState == EVENT_BTN_HOVER_SKILLLIST)
                {
                    m_bRenderSkillInfo = true;
                    m_iRenderSkillInfoType = i;
                    m_iRenderSkillInfoPosX = x;
                    m_iRenderSkillInfoPosY = y;
                }
                if (m_EventState == EVENT_BTN_DOWN_SKILLLIST && MouseLButtonPush == false
                    && m_iRenderSkillInfoType == i)
                {
                    m_EventState = EVENT_NONE;

                    m_wHeroPriorSkill = CharacterAttribute->Skill[Hero->CurrentSkill];

                    Hero->CurrentSkill = i;
                    m_bSkillList = false;
                    PlayBuffer(SOUND_CLICK01);
                    return false;
                }
            }
            x += width;
        }
    }

    if (bMouseOnSkillList == false && m_EventState == EVENT_BTN_HOVER_SKILLLIST)
    {
        m_EventState = EVENT_NONE;
        return true;
    }
    if (bMouseOnSkillList == false && MouseLButtonPush == false
        && m_EventState == EVENT_BTN_DOWN_SKILLLIST)
    {
        m_EventState = EVENT_NONE;
        return false;
    }
    if (m_EventState == EVENT_BTN_DOWN_SKILLLIST)
    {
        if (MouseLButtonPush == false)
        {
            m_EventState = EVENT_NONE;
            return true;
        }
        return false;
    }

    return true;
}

bool SEASON3B::CNewUISkillList::UpdateKeyEvent()
{
    for (int i = 0; i < 9; ++i)
    {
        if (SEASON3B::IsPress('1' + i))
        {
            UseHotKey(i + 1);
        }
    }

    if (SEASON3B::IsPress('0'))
    {
        UseHotKey(0);
    }

    if (m_EventState == EVENT_BTN_HOVER_SKILLLIST)
    {
        if (SEASON3B::IsRepeat(VK_CONTROL))
        {
            for (int i = 0; i < 9; ++i)
            {
                if (SEASON3B::IsPress('1' + i))
                {
                    SetHotKey(i + 1, m_iRenderSkillInfoType);

                    return false;
                }
            }

            if (SEASON3B::IsPress('0'))
            {
                SetHotKey(0, m_iRenderSkillInfoType);

                return false;
            }
        }
    }

    if (SEASON3B::IsRepeat(VK_SHIFT))
    {
        for (int i = 0; i < 4; ++i)
        {
            if (SEASON3B::IsPress('1' + i))
            {
                Hero->CurrentSkill = AT_PET_COMMAND_DEFAULT + i;
                return false;
            }
        }
    }

    return true;
}

bool SEASON3B::CNewUISkillList::IsArrayUp(BYTE bySkill)
{
    for (int i = 0; i < SKILLHOTKEY_COUNT; ++i)
    {
        if (m_iHotKeySkillType[i] == bySkill)
        {
            if (i == 0 || i > 5)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    return false;
}

bool SEASON3B::CNewUISkillList::IsArrayIn(BYTE bySkill)
{
    for (int i = 0; i < SKILLHOTKEY_COUNT; ++i)
    {
        if (m_iHotKeySkillType[i] == bySkill)
        {
            return true;
        }
    }

    return false;
}

void SEASON3B::CNewUISkillList::SetHotKey(int iHotKey, int iSkillType)
{
    SetHotKeyInternal(iHotKey, iSkillType, true);
}

void SEASON3B::CNewUISkillList::SetHotKeyFromServer(int iHotKey, int iSkillType)
{
    SetHotKeyInternal(iHotKey, iSkillType, false);
}

void SEASON3B::CNewUISkillList::SetHotKeyInternal(int iHotKey, int iSkillType, bool saveLocal)
{
    if (iHotKey < 0 || iHotKey >= SKILLHOTKEY_COUNT)
    {
        return;
    }

    for (int i = 0; i < SKILLHOTKEY_COUNT; ++i)
    {
        if (iSkillType != -1 && m_iHotKeySkillType[i] == iSkillType)
        {
            m_iHotKeySkillType[i] = -1;
            break;
        }
    }

    m_iHotKeySkillType[iHotKey] = iSkillType;

    if (saveLocal)
    {
        SaveLocalHotKeys();
    }
}

int SEASON3B::CNewUISkillList::GetHotKey(int iHotKey)
{
    return m_iHotKeySkillType[iHotKey];
}

int SEASON3B::CNewUISkillList::GetSkillIndex(int iSkillType)
{
    // special handling for skills with different skill id for the trigger
    if (iSkillType == AT_SKILL_NOVA_BEGIN)
    {
        iSkillType = AT_SKILL_NOVA;
    }

    int iReturn = -1;
    for (int i = 0; i < MAX_MAGIC; ++i)
    {
        if (CharacterAttribute->Skill[i] == iSkillType)
        {
            iReturn = i;
            break;
        }
    }

    return iReturn;
}

void SEASON3B::CNewUISkillList::UseHotKey(int iHotKey)
{
    if (m_iHotKeySkillType[iHotKey] != -1)
    {
        if (m_iHotKeySkillType[iHotKey] >= AT_PET_COMMAND_DEFAULT && m_iHotKeySkillType[iHotKey] < AT_PET_COMMAND_END)
        {
            if (Hero->m_pPet == NULL)
            {
                return;
            }
        }

        auto wHotKeySkill = CharacterAttribute->Skill[m_iHotKeySkillType[iHotKey]];

        if (wHotKeySkill == 0)
        {
            return;
        }

        m_wHeroPriorSkill = CharacterAttribute->Skill[Hero->CurrentSkill];

        Hero->CurrentSkill = m_iHotKeySkillType[iHotKey];

        auto bySkill = CharacterAttribute->Skill[Hero->CurrentSkill];

        if (
            g_pOption->IsAutoAttack() == true
            && gMapManager.WorldActive != WD_6STADIUM
            && gMapManager.InChaosCastle() == false
            && (bySkill == AT_SKILL_TELEPORT || bySkill == AT_SKILL_TELEPORT_ALLY))
        {
            SelectedCharacter = -1;
            Attacking = -1;
        }
    }
}

bool SEASON3B::CNewUISkillList::Update()
{
    if (IsArrayIn(Hero->CurrentSkill) == true)
    {
        if (IsArrayUp(Hero->CurrentSkill) == true)
        {
            m_bHotKeySkillListUp = true;
        }
        else
        {
            m_bHotKeySkillListUp = false;
        }
    }

    if (Hero->m_pPet == NULL)
    {
        if (Hero->CurrentSkill >= AT_PET_COMMAND_DEFAULT && Hero->CurrentSkill < AT_PET_COMMAND_END)
        {
            Hero->CurrentSkill = 0;
        }
    }

    return true;
}

void SEASON3B::CNewUISkillList::RenderCurrentSkillAndHotSkillList()
{
    int i;
    float x, y, width, height;

    BYTE bySkillNumber = CharacterAttribute->SkillNumber;

    if (bySkillNumber > 0)
    {
        int iStartSkillIndex = 1;
        if (m_bHotKeySkillListUp)
        {
            iStartSkillIndex = 6;
        }

        x = 190; y = 431; width = 32; height = 38;
        for (i = 0; i < 5; ++i)
        {
            x += width;

            int iIndex = iStartSkillIndex + i;
            if (iIndex == 10)
            {
                iIndex = 0;
            }

            if (m_iHotKeySkillType[iIndex] == -1)
            {
                continue;
            }

            if (m_iHotKeySkillType[iIndex] >= AT_PET_COMMAND_DEFAULT && m_iHotKeySkillType[iIndex] < AT_PET_COMMAND_END)
            {
                if (Hero->m_pPet == NULL)
                {
                    continue;
                }
            }

            if (Hero->CurrentSkill == m_iHotKeySkillType[iIndex])
            {
                SEASON3B::RenderImage(IMAGE_SKILLBOX_USE, x, y, width, height);
            }
            RenderSkillIcon(m_iHotKeySkillType[iIndex], x + 6, y + 6, 20, 28);
        }

        x = 392; y = 437; width = 20; height = 28;
        RenderSkillIcon(Hero->CurrentSkill, x, y, width, height);
    }
}

bool SEASON3B::CNewUISkillList::Render()
{
    int i;
    float x, y, width, height;

    BYTE bySkillNumber = CharacterAttribute->SkillNumber;

    if (bySkillNumber > 0)
    {
        if (m_bSkillList == true)
        {
            x = 385; y = 390; width = 32; height = 38;
            float fOrigX = 385.f;
            int iSkillType = 0;
            int iSkillCount = 0;

            for (i = 0; i < MAX_MAGIC; ++i)
            {
                iSkillType = CharacterAttribute->Skill[i];

                if (iSkillType != 0 && (iSkillType < AT_SKILL_STUN || iSkillType > AT_SKILL_REMOVAL_BUFF))
                {
                    BYTE bySkillUseType = SkillAttribute[iSkillType].SkillUseType;

                    if (bySkillUseType == SKILL_USE_TYPE_MASTER || bySkillUseType == SKILL_USE_TYPE_MASTERLEVEL)
                    {
                        continue;
                    }

                    if (iSkillCount == 18)
                    {
                        y -= height;
                    }

                    if (iSkillCount < 14)
                    {
                        int iRemainder = iSkillCount % 2;
                        int iQuotient = iSkillCount / 2;

                        if (iRemainder == 0)
                        {
                            x = fOrigX + iQuotient * width;
                        }
                        else
                        {
                            x = fOrigX - (iQuotient + 1) * width;
                        }
                    }
                    else if (iSkillCount >= 14 && iSkillCount < 18)
                    {
                        x = fOrigX - (8 * width) - ((iSkillCount - 14) * width);
                    }
                    else
                    {
                        x = fOrigX - (12 * width) + ((iSkillCount - 17) * width);
                    }

                    iSkillCount++;

                    if (i == Hero->CurrentSkill)
                    {
                        SEASON3B::RenderImage(IMAGE_SKILLBOX_USE, x, y, width, height);
                    }
                    else
                    {
                        SEASON3B::RenderImage(IMAGE_SKILLBOX, x, y, width, height);
                    }

                    RenderSkillIcon(i, x + 6, y + 6, 20, 28);
                }
            }
            RenderPetSkill();
        }
    }

    if (m_bRenderSkillInfo == true && m_pNewUI3DRenderMng)
    {
        m_pNewUI3DRenderMng->RenderUI2DEffect(INVENTORY_CAMERA_Z_ORDER, UI2DEffectCallback, this, 0, 0);

        m_bRenderSkillInfo = false;
    }

    return true;
}

void SEASON3B::CNewUISkillList::RenderSkillInfo()
{
    UI::Skills::Tooltip::Render(m_iRenderSkillInfoPosX + 15, m_iRenderSkillInfoPosY - 10, m_iRenderSkillInfoType);
}

float SEASON3B::CNewUISkillList::GetLayerDepth()
{
    return 5.2f;
}

WORD SEASON3B::CNewUISkillList::GetHeroPriorSkill()
{
    return m_wHeroPriorSkill;
}

void SEASON3B::CNewUISkillList::SetHeroPriorSkill(BYTE bySkill)
{
    m_wHeroPriorSkill = bySkill;
}

void SEASON3B::CNewUISkillList::RenderPetSkill()
{
    if (Hero->m_pPet == NULL)
    {
        return;
    }

    float x, y, width, height;

    x = 353.f; y = 352; width = 32; height = 38;
    for (int i = AT_PET_COMMAND_DEFAULT; i < AT_PET_COMMAND_END; ++i)
    {
        if (i == Hero->CurrentSkill)
        {
            SEASON3B::RenderImage(IMAGE_SKILLBOX_USE, x, y, width, height);
        }
        else
        {
            SEASON3B::RenderImage(IMAGE_SKILLBOX, x, y, width, height);
        }

        RenderSkillIcon(i, x + 6, y + 6, 20, 28);
        x += width;
    }
}

void SEASON3B::CNewUISkillList::RenderSkillIcon(int iIndex, float x, float y, float width, float height)
{
    auto bySkillType = CharacterAttribute->Skill[iIndex];

    if (bySkillType == 0)
    {
        return;
    }

    if (iIndex >= AT_PET_COMMAND_DEFAULT)
    {
        bySkillType = (ActionSkillType)iIndex;
    }

    bool bCantSkill = false;

    BYTE bySkillUseType = SkillAttribute[bySkillType].SkillUseType;
    int Skill_Icon = SkillAttribute[bySkillType].Magic_Icon;

    if (!gSkillManager.AreSkillAttributeRequirementsMet(bySkillType))
    {
        bCantSkill = true;
    }

    if (IsCanBCSkill(bySkillType) == false)
    {
        bCantSkill = true;
    }
    if (g_isCharacterBuff((&Hero->Object), eBuff_AddSkill) && bySkillUseType == SKILL_USE_TYPE_BRAND)
    {
        bCantSkill = true;
    }
    auto isSittingOnPet = (Hero->Helper.Type == MODEL_HORN_OF_UNIRIA || Hero->Helper.Type == MODEL_HORN_OF_DINORANT || Hero->Helper.Type == MODEL_HORN_OF_FENRIR);
    if (bySkillType == AT_SKILL_IMPALE && !isSittingOnPet)
    {
        bCantSkill = true;
    }

    if (bySkillType == AT_SKILL_IMPALE && isSittingOnPet)
    {
        int iTypeL = CharacterMachine->Equipment[EQUIPMENT_WEAPON_LEFT].Type;
        int iTypeR = CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type;
        if ((iTypeL < ITEM_SPEAR || iTypeL >= ITEM_BOW) && (iTypeR < ITEM_SPEAR || iTypeR >= ITEM_BOW))
        {
            bCantSkill = true;
        }
    }

    if (isSittingOnPet
        && ((bySkillType >= AT_SKILL_BLOCKING && bySkillType <= AT_SKILL_SLASH)
            || bySkillType == AT_SKILL_FALLING_SLASH_STR
            || bySkillType == AT_SKILL_LUNGE_STR
            || bySkillType == AT_SKILL_CYCLONE_STR
            || bySkillType == AT_SKILL_CYCLONE_STR_MG
            || bySkillType == AT_SKILL_SLASH_STR
            ))
    {
        bCantSkill = true;
    }

    if ((bySkillType == AT_SKILL_POWER_SLASH || bySkillType == AT_SKILL_POWER_SLASH_STR)
        && isSittingOnPet)
    {
        bCantSkill = true;
    }

    if (bySkillType == AT_SKILL_PARTY_TELEPORT && PartyNumber <= 0)
    {
        bCantSkill = true;
    }

    if (bySkillType == AT_SKILL_PARTY_TELEPORT && (IsDoppelGanger1() || IsDoppelGanger2() || IsDoppelGanger3() || IsDoppelGanger4()))
    {
        bCantSkill = true;
    }

    if (bySkillType == AT_SKILL_EARTHSHAKE || bySkillType == AT_SKILL_EARTHSHAKE_STR || bySkillType == AT_SKILL_EARTHSHAKE_MASTERY)
    {
        BYTE byDarkHorseLife = 0;
        byDarkHorseLife = CharacterMachine->Equipment[EQUIPMENT_HELPER].Durability;
        if (byDarkHorseLife == 0 || Hero->Helper.Type != MODEL_DARK_HORSE_ITEM)
        {
            bCantSkill = true;
        }
    }
#ifdef PJH_FIX_SPRIT
    /*???*/
    if (bySkillType >= AT_PET_COMMAND_DEFAULT && bySkillType < AT_PET_COMMAND_END)
    {
        int iCharisma = CharacterAttribute->Charisma + CharacterAttribute->AddCharisma;
        PET_INFO PetInfo;
        giPetManager::GetPetInfo(PetInfo, 421 - PET_TYPE_DARK_SPIRIT);
        int RequireCharisma = (185 + (PetInfo.m_wLevel * 15));
        if (RequireCharisma > iCharisma)
        {
            bCantSkill = true;
        }
    }
#endif //PJH_FIX_SPRIT
    if ((bySkillType == AT_SKILL_INFINITY_ARROW)
        || (bySkillType == AT_SKILL_INFINITY_ARROW_STR)
        || (bySkillType == AT_SKILL_EXPANSION_OF_WIZARDRY)
        || (bySkillType == AT_SKILL_EXPANSION_OF_WIZARDRY_STR)
        || (bySkillType == AT_SKILL_EXPANSION_OF_WIZARDRY_MASTERY)
        )
    {
        if ((g_isCharacterBuff((&Hero->Object), eBuff_InfinityArrow)) || (g_isCharacterBuff((&Hero->Object), eBuff_SwellOfMagicPower)))
        {
            bCantSkill = true;
        }
    }

    if (bySkillType == AT_SKILL_FIRE_SLASH || bySkillType == AT_SKILL_FIRE_SLASH_STR)
    {
        WORD Strength;
        const WORD wRequireStrength = 596;
        Strength = CharacterAttribute->Strength + CharacterAttribute->AddStrength;
        if (Strength < wRequireStrength)
        {
            bCantSkill = true;
        }
        int iTypeL = CharacterMachine->Equipment[EQUIPMENT_WEAPON_LEFT].Type;
        int iTypeR = CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type;

        if (!(iTypeR != -1 && (iTypeR < ITEM_STAFF || iTypeR >= ITEM_STAFF + MAX_ITEM_INDEX) && (iTypeL < ITEM_STAFF || iTypeL >= ITEM_STAFF + MAX_ITEM_INDEX)))
        {
            bCantSkill = true;
        }
    }

    switch (bySkillType)
    {
        //case AT_SKILL_PIERCING:
    case AT_SKILL_ICE_ARROW:
    case AT_SKILL_ICE_ARROW_STR:
    {
        WORD  Dexterity;
        const WORD wRequireDexterity = 646;
        Dexterity = CharacterAttribute->Dexterity + CharacterAttribute->AddDexterity;
        if (Dexterity < wRequireDexterity)
        {
            bCantSkill = true;
        }
    }break;
    }

    if (bySkillType == AT_SKILL_TWISTING_SLASH
        || bySkillType == AT_SKILL_TWISTING_SLASH_STR
        || bySkillType == AT_SKILL_TWISTING_SLASH_STR_MG
        || bySkillType == AT_SKILL_TWISTING_SLASH_MASTERY
        || bySkillType == AT_SKILL_RAGEFUL_BLOW
        || bySkillType == AT_SKILL_RAGEFUL_BLOW_STR
        || bySkillType == AT_SKILL_RAGEFUL_BLOW_MASTERY
        || bySkillType == AT_SKILL_DEATHSTAB
        || bySkillType == AT_SKILL_DEATHSTAB_STR
        )
    {
        int iTypeL = CharacterMachine->Equipment[EQUIPMENT_WEAPON_LEFT].Type;
        int iTypeR = CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type;

        if (!(iTypeR != -1 && (iTypeR < ITEM_STAFF || iTypeR >= ITEM_STAFF + MAX_ITEM_INDEX) && (iTypeL < ITEM_STAFF || iTypeL >= ITEM_STAFF + MAX_ITEM_INDEX)))
        {
            bCantSkill = true;
        }
    }

    if (gMapManager.InChaosCastle() == true)
    {
        if (bySkillType == AT_SKILL_EARTHSHAKE
            || bySkillType == AT_SKILL_EARTHSHAKE_STR
            || bySkillType == AT_SKILL_EARTHSHAKE_MASTERY
            || bySkillType == AT_SKILL_RIDER
            || (static_cast<int>(bySkillType) >= static_cast<int>(AT_PET_COMMAND_DEFAULT) && static_cast<int>(bySkillType) <= static_cast<int>(AT_PET_COMMAND_TARGET))
            )
        {
            bCantSkill = true;
        }
    }
    else
    {
        if (bySkillType == AT_SKILL_EARTHSHAKE
            || bySkillType == AT_SKILL_EARTHSHAKE_STR
            || bySkillType == AT_SKILL_EARTHSHAKE_MASTERY)
        {
            BYTE byDarkHorseLife = 0;
            byDarkHorseLife = CharacterMachine->Equipment[EQUIPMENT_HELPER].Durability;
            if (byDarkHorseLife == 0)
            {
                bCantSkill = true;
            }
        }
    }

    if (!g_CMonkSystem.IsSwordformGlovesUseSkill(bySkillType))
    {
        bCantSkill = true;
    }
    if (g_CMonkSystem.IsRideNotUseSkill(bySkillType, Hero->Helper.Type))
    {
        bCantSkill = true;
    }

    ITEM* pLeftRing = &CharacterMachine->Equipment[EQUIPMENT_RING_LEFT];
    ITEM* pRightRing = &CharacterMachine->Equipment[EQUIPMENT_RING_RIGHT];

    if (g_CMonkSystem.IsChangeringNotUseSkill(pLeftRing->Type, pRightRing->Type, pLeftRing->Level, pRightRing->Level)
        && (gCharacterManager.GetBaseClass(Hero->Class) == CLASS_RAGEFIGHTER))
    {
        bCantSkill = true;
    }

    float fU, fV;
    int iKindofSkill = 0;

    if (!g_csItemOption.IsNonWeaponSkillOrIsSkillEquipped(bySkillType))
    {
        bCantSkill = true;
    }

    if (static_cast<int>(bySkillType) >= static_cast<int>(AT_PET_COMMAND_DEFAULT) && static_cast<int>(bySkillType) <= static_cast<int>(AT_PET_COMMAND_END))
    {
        fU = ((static_cast<int>(bySkillType) - AT_PET_COMMAND_DEFAULT) % 8) * width / 256.f;
        fV = ((static_cast<int>(bySkillType) - AT_PET_COMMAND_DEFAULT) / 8) * height / 256.f;
        iKindofSkill = KOS_COMMAND;
    }
    else if (bySkillType == AT_SKILL_PLASMA_STORM_FENRIR)
    {
        fU = 4 * width / 256.f;
        fV = 0.f;
        iKindofSkill = KOS_COMMAND;
    }
    else if ((bySkillType >= AT_SKILL_ALICE_DRAINLIFE && bySkillType <= AT_SKILL_ALICE_THORNS))
    {
        fU = ((bySkillType - AT_SKILL_ALICE_DRAINLIFE) % 8) * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType >= AT_SKILL_ALICE_SLEEP && bySkillType <= AT_SKILL_ALICE_BLIND)
    {
        fU = ((bySkillType - AT_SKILL_ALICE_SLEEP + 4) % 8) * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType == AT_SKILL_ALICE_BERSERKER)
    {
        fU = 10 * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType >= AT_SKILL_ALICE_WEAKNESS && bySkillType <= AT_SKILL_ALICE_ENERVATION)
    {
        fU = (bySkillType - AT_SKILL_ALICE_WEAKNESS + 8) * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType >= AT_SKILL_SUMMON_EXPLOSION && bySkillType <= AT_SKILL_SUMMON_REQUIEM)
    {
        fU = ((bySkillType - AT_SKILL_SUMMON_EXPLOSION + 6) % 8) * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType == AT_SKILL_SUMMON_POLLUTION)
    {
        fU = 11 * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType == AT_SKILL_STRIKE_OF_DESTRUCTION)
    {
        fU = 7 * width / 256.f;
        fV = 2 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType == AT_SKILL_CHAOTIC_DISEIER)
    {
        fU = 3 * width / 256.f;
        fV = 8 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType == AT_SKILL_RECOVER)
    {
        fU = 9 * width / 256.f;
        fV = 2 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType == AT_SKILL_MULTI_SHOT)
    {
        if (gCharacterManager.GetEquipedBowType_Skill() == BOWTYPE_NONE)
        {
            bCantSkill = true;
        }

        fU = 0 * width / 256.f;
        fV = 8 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType == AT_SKILL_FLAME_STRIKE)
    {
        int iTypeL = CharacterMachine->Equipment[EQUIPMENT_WEAPON_LEFT].Type;
        int iTypeR = CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type;

        if (!(iTypeR != -1 && (iTypeR < ITEM_STAFF || iTypeR >= ITEM_STAFF + MAX_ITEM_INDEX) && (iTypeL < ITEM_STAFF || iTypeL >= ITEM_STAFF + MAX_ITEM_INDEX)))
        {
            bCantSkill = true;
        }

        fU = 1 * width / 256.f;
        fV = 8 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType == AT_SKILL_GIGANTIC_STORM)
    {
        fU = 2 * width / 256.f;
        fV = 8 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType == AT_SKILL_LIGHTNING_SHOCK)
    {
        fU = 2 * width / 256.f;
        fV = 3 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType == AT_SKILL_EXPANSION_OF_WIZARDRY)
    {
        fU = 8 * width / 256.f;
        fV = 2 * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillUseType == 4)
    {
        fU = (width / 256.f) * (Skill_Icon % 12);
        fV = (height / 256.f) * ((Skill_Icon / 12) + 4);
        iKindofSkill = KOS_SKILL2;
    }
    else if (bySkillType >= AT_SKILL_KILLING_BLOW)
    {
        fU = ((bySkillType - AT_SKILL_KILLING_BLOW) % 12) * width / 256.f;
        fV = ((bySkillType - AT_SKILL_KILLING_BLOW) / 12) * height / 256.f;
        iKindofSkill = KOS_SKILL3;
    }
    else if (bySkillType >= AT_SKILL_SPIRAL_SLASH)
    {
        fU = ((bySkillType - AT_SKILL_SPIRAL_SLASH) % 8) * width / 256.f;
        fV = ((bySkillType - AT_SKILL_SPIRAL_SLASH) / 8) * height / 256.f;
        iKindofSkill = KOS_SKILL2;
    }
    else
    {
        fU = ((bySkillType - 1) % 8) * width / 256.f;
        fV = ((bySkillType - 1) / 8) * height / 256.f;
        iKindofSkill = KOS_SKILL1;
    }
    int iSkillIndex = 0;
    switch (iKindofSkill)
    {
    case KOS_COMMAND:
    {
        iSkillIndex = IMAGE_COMMAND;
    }break;
    case KOS_SKILL1:
    {
        iSkillIndex = IMAGE_SKILL1;
    }break;
    case KOS_SKILL2:
    {
        iSkillIndex = IMAGE_SKILL2;
    }break;
    case KOS_SKILL3:
    {
        iSkillIndex = IMAGE_SKILL3;
    }break;
    }

    if (bySkillType >= AT_SKILL_MASTER_BEGIN)
    {
        if (bCantSkill)
        {
            RenderImage(BITMAP_INTERFACE_MASTER_BEGIN + 3, x, y, width, height, (20.f / 512.f) * (Skill_Icon % 25), ((28.f / 512.f) * ((Skill_Icon / 25))), 20.f / 512.f, 28.f / 512.f);
        }
        else
        {
            RenderImage(BITMAP_INTERFACE_MASTER_BEGIN + 2, x, y, width, height, (20.f / 512.f)* (Skill_Icon % 25), ((28.f / 512.f)* ((Skill_Icon / 25))), 20.f / 512.f, 28.f / 512.f);
        }
    }
    else
    {
        if (bCantSkill == true)
        {
            iSkillIndex += 6;
        }

        if (iSkillIndex != 0)
        {
            RenderBitmap(iSkillIndex, x, y, width, height, fU, fV, width / 256.f, height / 256.f);
        }
    }

    int iHotKey = -1;
    for (int i = 0; i < SKILLHOTKEY_COUNT; ++i)
    {
        if (m_iHotKeySkillType[i] == iIndex)
        {
            iHotKey = i;
            break;
        }
    }

    if (iHotKey != -1)
    {
        glColor3f(1.f, 0.9f, 0.8f);
        SEASON3B::RenderNumber(x + 20, y + 20, iHotKey);
        glColor3f(1.f, 1.f, 1.f);
    }

    if ((bySkillType == AT_SKILL_CHAIN_DRIVE
        || bySkillType == AT_SKILL_CHAIN_DRIVE_STR
        || bySkillType == AT_SKILL_DRAGON_KICK
        || bySkillType == AT_SKILL_DRAGON_ROAR
        || bySkillType == AT_SKILL_DRAGON_ROAR_STR) && (bCantSkill))
        return;

    if ((bySkillType != AT_SKILL_INFINITY_ARROW)
        && (bySkillType != AT_SKILL_INFINITY_ARROW_STR)
        && (bySkillType != AT_SKILL_EXPANSION_OF_WIZARDRY)
        && (bySkillType != AT_SKILL_EXPANSION_OF_WIZARDRY_STR)
        && (bySkillType != AT_SKILL_EXPANSION_OF_WIZARDRY_MASTERY)
        )
    {
        RenderSkillDelay(iIndex, x, y, width, height);
    }
}

void SEASON3B::CNewUISkillList::RenderSkillDelay(int iIndex, float x, float y, float width, float height)
{
    int iSkillDelay = CharacterAttribute->SkillDelay[iIndex];
    if (iSkillDelay > 0)
    {
        int iSkillType = CharacterAttribute->Skill[iIndex];

        if (iSkillType == AT_SKILL_PLASMA_STORM_FENRIR)
        {
            if (!CheckAttack())
            {
                return;
            }
        }

        int iSkillMaxDelay = SkillAttribute[iSkillType].Delay;

        auto fPersent = (float)(iSkillDelay / (float)iSkillMaxDelay);

        EnableAlphaTest();
        glColor4f(1.f, 0.5f, 0.5f, 0.5f);
        float fdeltaH = height * fPersent;
        RenderColor(x, y + height - fdeltaH, width, fdeltaH);
        EndRenderColor();
    }
}

bool SEASON3B::CNewUISkillList::IsSkillListUp()
{
    return m_bHotKeySkillListUp;
}

void SEASON3B::CNewUISkillList::ResetMouseLButton()
{
    MouseLButton = false;
    MouseLButtonPop = false;
    MouseLButtonPush = false;
}

void SEASON3B::CNewUISkillList::UI2DEffectCallback(LPVOID pClass, DWORD dwParamA, DWORD dwParamB)
{
    if (pClass)
    {
        auto* pSkillList = (CNewUISkillList*)(pClass);
        pSkillList->RenderSkillInfo();
    }
}

void SEASON3B::CNewUIMainFrameWindow::SetPreExp_Wide(__int64 dwPreExp)
{
    m_loPreExp = dwPreExp;
}

void SEASON3B::CNewUIMainFrameWindow::SetGetExp_Wide(__int64 dwGetExp)
{
    m_loGetExp = dwGetExp;

    if (m_loGetExp > 0)
    {
        m_bExpEffect = true;
        m_dwExpEffectTime = timeGetTime();
    }
}

void SEASON3B::CNewUIMainFrameWindow::SetPreExp(__int64 dwPreExp)
{
    m_dwPreExp = dwPreExp;
}

void SEASON3B::CNewUIMainFrameWindow::SetGetExp(__int64 dwGetExp)
{
    m_dwGetExp = dwGetExp;

    if (m_dwGetExp > 0)
    {
        m_bExpEffect = true;
        m_dwExpEffectTime = timeGetTime();
    }
}

void SEASON3B::CNewUIMainFrameWindow::SetBtnState(int iBtnType, bool bStateDown)
{
    switch (iBtnType)
    {
#ifdef PBG_ADD_INGAMESHOP_UI_MAINFRAME
    case MAINFRAME_BTN_PARTCHARGE:
    {
        if (bStateDown)
        {
            m_BtnCShop.UnRegisterButtonState();
            m_BtnCShop.RegisterButtonState(BUTTON_STATE_UP, IMAGE_MENU_BTN_CSHOP, 2);
            m_BtnCShop.RegisterButtonState(BUTTON_STATE_OVER, IMAGE_MENU_BTN_CSHOP, 3);
            m_BtnCShop.RegisterButtonState(BUTTON_STATE_DOWN, IMAGE_MENU_BTN_CSHOP, 2);
            m_BtnCShop.ChangeImgIndex(IMAGE_MENU_BTN_CSHOP, 2);
        }
        else
        {
            m_BtnCShop.UnRegisterButtonState();
            m_BtnCShop.RegisterButtonState(BUTTON_STATE_UP, IMAGE_MENU_BTN_CSHOP, 0);
            m_BtnCShop.RegisterButtonState(BUTTON_STATE_OVER, IMAGE_MENU_BTN_CSHOP, 1);
            m_BtnCShop.RegisterButtonState(BUTTON_STATE_DOWN, IMAGE_MENU_BTN_CSHOP, 2);
            m_BtnCShop.ChangeImgIndex(IMAGE_MENU_BTN_CSHOP, 0);
        }
    }
    break;
#endif //defined defined PBG_ADD_INGAMESHOP_UI_MAINFRAME
    case MAINFRAME_BTN_CHAINFO:
    {
        if (bStateDown)
        {
            m_BtnChaInfo.UnRegisterButtonState();
            m_BtnChaInfo.RegisterButtonState(BUTTON_STATE_UP, IMAGE_MENU_BTN_CHAINFO, 2);
            m_BtnChaInfo.RegisterButtonState(BUTTON_STATE_OVER, IMAGE_MENU_BTN_CHAINFO, 3);
            m_BtnChaInfo.RegisterButtonState(BUTTON_STATE_DOWN, IMAGE_MENU_BTN_CHAINFO, 2);
            m_BtnChaInfo.ChangeImgIndex(IMAGE_MENU_BTN_CHAINFO, 2);
        }
        else
        {
            m_BtnChaInfo.UnRegisterButtonState();
            m_BtnChaInfo.RegisterButtonState(BUTTON_STATE_UP, IMAGE_MENU_BTN_CHAINFO, 0);
            m_BtnChaInfo.RegisterButtonState(BUTTON_STATE_OVER, IMAGE_MENU_BTN_CHAINFO, 1);
            m_BtnChaInfo.RegisterButtonState(BUTTON_STATE_DOWN, IMAGE_MENU_BTN_CHAINFO, 2);
            m_BtnChaInfo.ChangeImgIndex(IMAGE_MENU_BTN_CHAINFO, 0);
        }
    }
    break;
    case MAINFRAME_BTN_MYINVEN:
    {
        if (bStateDown)
        {
            m_BtnMyInven.UnRegisterButtonState();
            m_BtnMyInven.RegisterButtonState(BUTTON_STATE_UP, IMAGE_MENU_BTN_MYINVEN, 2);
            m_BtnMyInven.RegisterButtonState(BUTTON_STATE_OVER, IMAGE_MENU_BTN_MYINVEN, 3);
            m_BtnMyInven.RegisterButtonState(BUTTON_STATE_DOWN, IMAGE_MENU_BTN_MYINVEN, 2);
            m_BtnMyInven.ChangeImgIndex(IMAGE_MENU_BTN_MYINVEN, 2);
        }
        else
        {
            m_BtnMyInven.UnRegisterButtonState();
            m_BtnMyInven.RegisterButtonState(BUTTON_STATE_UP, IMAGE_MENU_BTN_MYINVEN, 0);
            m_BtnMyInven.RegisterButtonState(BUTTON_STATE_OVER, IMAGE_MENU_BTN_MYINVEN, 1);
            m_BtnMyInven.RegisterButtonState(BUTTON_STATE_DOWN, IMAGE_MENU_BTN_MYINVEN, 2);
            m_BtnMyInven.ChangeImgIndex(IMAGE_MENU_BTN_MYINVEN, 0);
        }
    }
    break;
    case MAINFRAME_BTN_FRIEND:
    {
        if (bStateDown)
        {
            m_BtnFriend.UnRegisterButtonState();
            m_BtnFriend.RegisterButtonState(BUTTON_STATE_UP, IMAGE_MENU_BTN_FRIEND, 2);
            m_BtnFriend.RegisterButtonState(BUTTON_STATE_OVER, IMAGE_MENU_BTN_FRIEND, 3);
            m_BtnFriend.RegisterButtonState(BUTTON_STATE_DOWN, IMAGE_MENU_BTN_FRIEND, 2);
            m_BtnFriend.ChangeImgIndex(IMAGE_MENU_BTN_FRIEND, 2);
        }
        else
        {
            m_BtnFriend.UnRegisterButtonState();
            m_BtnFriend.RegisterButtonState(BUTTON_STATE_UP, IMAGE_MENU_BTN_FRIEND, 0);
            m_BtnFriend.RegisterButtonState(BUTTON_STATE_OVER, IMAGE_MENU_BTN_FRIEND, 1);
            m_BtnFriend.RegisterButtonState(BUTTON_STATE_DOWN, IMAGE_MENU_BTN_FRIEND, 2);
            m_BtnFriend.ChangeImgIndex(IMAGE_MENU_BTN_FRIEND, 0);
        }
    }
    break;
    case MAINFRAME_BTN_WINDOW:
    {
        if (bStateDown)
        {
            m_BtnWindow.UnRegisterButtonState();
            m_BtnWindow.RegisterButtonState(BUTTON_STATE_UP, IMAGE_MENU_BTN_WINDOW, 2);
            m_BtnWindow.RegisterButtonState(BUTTON_STATE_OVER, IMAGE_MENU_BTN_WINDOW, 3);
            m_BtnWindow.RegisterButtonState(BUTTON_STATE_DOWN, IMAGE_MENU_BTN_WINDOW, 2);
            m_BtnWindow.ChangeImgIndex(IMAGE_MENU_BTN_WINDOW, 2);
        }
        else
        {
            m_BtnWindow.UnRegisterButtonState();
            m_BtnWindow.RegisterButtonState(BUTTON_STATE_UP, IMAGE_MENU_BTN_WINDOW, 0);
            m_BtnWindow.RegisterButtonState(BUTTON_STATE_OVER, IMAGE_MENU_BTN_WINDOW, 1);
            m_BtnWindow.RegisterButtonState(BUTTON_STATE_DOWN, IMAGE_MENU_BTN_WINDOW, 2);
            m_BtnWindow.ChangeImgIndex(IMAGE_MENU_BTN_WINDOW, 0);
        }
    }
    break;
    }
}
