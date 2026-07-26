#include "stdafx.h"
#include "MuHelperData.h"

#include <algorithm>

namespace MUHelper
{
	constexpr int DEFAULT_HUNTING_RANGE = 6;
	constexpr int DEFAULT_OBTAINING_RANGE = 8;
	constexpr BYTE CUSTOM_CONFIG_MARKER_0 = 'P';
	constexpr BYTE CUSTOM_CONFIG_MARKER_1 = 'G';
	constexpr BYTE CUSTOM_CONFIG_MIN_VERSION = 1;
	constexpr BYTE CUSTOM_CONFIG_VERSION = 2;

	enum CustomGuardFlag : WORD
	{
		CUSTOM_FLAG_PICK_USABLE = 1u << 0,
		CUSTOM_FLAG_PICK_COMMON = 1u << 1,
		CUSTOM_FLAG_PICK_EVENT = 1u << 2,
		CUSTOM_FLAG_AUTO_AZOTH = 1u << 3,
		CUSTOM_FLAG_AUTO_RESET = 1u << 4,
		CUSTOM_FLAG_AUTO_DISTRIBUTE = 1u << 5,
	};

	BYTE ClampPercent(int value)
	{
		return static_cast<BYTE>(std::clamp(value, 0, 100));
	}

	void ConfigDataSerDe::Serialize(const ConfigData& gameData, PRECEIVE_MUHELPER_DATA& netData)
	{
		memset(&netData, 0, sizeof(netData));

		netData.HuntingRange = static_cast<BYTE>(gameData.iHuntingRange & 0x0F);
		netData.DistanceMin = static_cast<BYTE>(gameData.iMaxSecondsAway & 0x0F);
		netData.LongDistanceAttack = gameData.bLongRangeCounterAttack ? 1 : 0;
		netData.OriginalPosition = gameData.bReturnToOriginalPosition ? 1 : 0;

		netData.BasicSkill1 = static_cast<WORD>(gameData.aiSkill[0] & 0xFFFF);
		netData.ActivationSkill1 = static_cast<WORD>(gameData.aiSkill[1] & 0xFFFF);
		netData.ActivationSkill2 = static_cast<WORD>(gameData.aiSkill[2] & 0xFFFF);

		netData.DelayMinSkill1 = static_cast<WORD>(gameData.aiSkillInterval[1] & 0xFFFF);
		netData.DelayMinSkill2 = static_cast<WORD>(gameData.aiSkillInterval[2] & 0xFFFF);

		if (gameData.aiSkillCondition[1] & ON_TIMER)
		{
			netData.Skill1Delay = 1;
		}
		if (gameData.aiSkillCondition[1] & ON_CONDITION)
		{
			netData.Skill1Con = 1;
		}
		if (gameData.aiSkillCondition[1] & ON_MOBS_NEARBY)
		{
			netData.Skill1PreCon = 0;
		}
		else if (gameData.aiSkillCondition[1] & ON_MOBS_ATTACKING)
		{
			netData.Skill1PreCon = 1;
		}

		if (gameData.aiSkillCondition[1] & ON_MORE_THAN_TWO_MOBS)
		{
			netData.Skill1SubCon = 0;
		}
		else if (gameData.aiSkillCondition[1] & ON_MORE_THAN_THREE_MOBS)
		{
			netData.Skill1SubCon = 1;
		}
		else if (gameData.aiSkillCondition[1] & ON_MORE_THAN_FOUR_MOBS)
		{
			netData.Skill1SubCon = 2;
		}
		else if (gameData.aiSkillCondition[1] & ON_MORE_THAN_FIVE_MOBS)
		{
			netData.Skill1SubCon = 3;
		}

		if (gameData.aiSkillCondition[2] & ON_TIMER)
		{
			netData.Skill2Delay = 1;
		}
		if (gameData.aiSkillCondition[2] & ON_CONDITION)
		{
			netData.Skill2Con = 1;
		}
		if (gameData.aiSkillCondition[2] & ON_MOBS_NEARBY)
		{
			netData.Skill2PreCon = 0;
		}
		else if (gameData.aiSkillCondition[2] & ON_MOBS_ATTACKING)
		{
			netData.Skill2PreCon = 1;
		}

		if (gameData.aiSkillCondition[2] & ON_MORE_THAN_TWO_MOBS)
		{
			netData.Skill2SubCon = 0;
		}
		else if (gameData.aiSkillCondition[2] & ON_MORE_THAN_THREE_MOBS)
		{
			netData.Skill2SubCon = 1;
		}
		else if (gameData.aiSkillCondition[2] & ON_MORE_THAN_FOUR_MOBS)
		{
			netData.Skill2SubCon = 2;
		}
		else if (gameData.aiSkillCondition[2] & ON_MORE_THAN_FIVE_MOBS)
		{
			netData.Skill2SubCon = 3;
		}

		netData.Combo = gameData.bUseCombo ? 1 : 0;

		netData.BuffSkill0NumberID = static_cast<WORD>(gameData.aiBuff[0] & 0xFFFF);
		netData.BuffSkill1NumberID = static_cast<WORD>(gameData.aiBuff[1] & 0xFFFF);
		netData.BuffSkill2NumberID = static_cast<WORD>(gameData.aiBuff[2] & 0xFFFF);

		netData.BuffDuration = gameData.bBuffDuration ? 1 : 0;
		netData.BuffDurationforAllPartyMembers = gameData.bBuffDurationParty ? 1 : 0;
		netData.CastingBuffMin = static_cast<WORD>(gameData.iBuffCastInterval);

		netData.AutoHeal = gameData.bAutoHeal ? 1 : 0;
		netData.HPStatusAutoPotion = static_cast<BYTE>((gameData.iPotionThreshold / 10) & 0x0F);
		netData.HPStatusAutoHeal = static_cast<BYTE>((gameData.iHealThreshold / 10) & 0x0F);
		netData.AutoPotion = gameData.bUseHealPotion ? 1 : 0;
		netData.DrainLife = gameData.bUseDrainLife ? 1 : 0;
		netData.Party = gameData.bSupportParty ? 1 : 0;
		netData.PreferenceOfPartyHeal = gameData.bAutoHealParty ? 1 : 0;

		netData.HPStatusOfPartyMembers = static_cast<BYTE>((gameData.iHealPartyThreshold / 10) & 0x0F);
		netData.HPStatusDrainLife = static_cast<BYTE>((gameData.iHealThreshold / 10) & 0x0F);

		netData.UseDarkSpirits = gameData.bUseDarkRaven ? 1 : 0;
		netData.PetAttack = static_cast<BYTE>(gameData.iDarkRavenMode);

		netData.RepairItem = gameData.bRepairItem ? 1 : 0;
		netData.ObtainRange = static_cast<BYTE>(gameData.iObtainingRange & 0x0F);
		netData.PickAllNearItems = gameData.bPickAllItems ? 1 : 0;
		netData.PickSelectedItems = gameData.bPickSelectItems ? 1 : 0;
		netData.Zen = gameData.bPickZen ? 1 : 0;
		netData.JewelOrGem = gameData.bPickJewel ? 1 : 0;
		netData.ExcellentItem = gameData.bPickExcellent ? 1 : 0;
		netData.SetItem = gameData.bPickAncient ? 1 : 0;
		netData.AddExtraItem = gameData.bPickExtraItems ? 1 : 0;

		memset(netData.ExtraItems, 0, sizeof(netData.ExtraItems));
		int iItemIndex = 0;
		for (const auto& wsItem : gameData.aExtraItems)
		{
			if (iItemIndex >= 12)
			{
				break;
			}

			size_t n = wcstombs(netData.ExtraItems[iItemIndex], wsItem.c_str(), 15);
			if (n == (size_t)-1 || n == 15)
			{
				memset(netData.ExtraItems[iItemIndex], 0, 15);
			}
			iItemIndex++;
		}

		// byte 33, client-local. server echoes unchanged, see MuHelperData.h
		netData.bUseSelfDefense = gameData.bUseSelfDefense ? 1 : 0;
		netData.bAutoAcceptFriend = gameData.bAutoAcceptFriend ? 1 : 0;
		netData.bAutoAcceptGuild = gameData.bAutoAcceptGuild ? 1 : 0;
		netData.bFallbackBasicAttack = gameData.bFallbackBasicAttack ? 1 : 0;

		WORD customFlags = 0;
		customFlags |= gameData.bPickOnlyUsableItems ? CUSTOM_FLAG_PICK_USABLE : 0;
		customFlags |= gameData.bPickCommonItems ? CUSTOM_FLAG_PICK_COMMON : 0;
		customFlags |= gameData.bPickEventItems ? CUSTOM_FLAG_PICK_EVENT : 0;
		customFlags |= gameData.bAutoConvertZenToAzoth ? CUSTOM_FLAG_AUTO_AZOTH : 0;
		customFlags |= gameData.bAutoReset ? CUSTOM_FLAG_AUTO_RESET : 0;
		customFlags |= gameData.bAutoDistributePoints ? CUSTOM_FLAG_AUTO_DISTRIBUTE : 0;

		netData._UnusedPadding[0] = CUSTOM_CONFIG_MARKER_0;
		netData._UnusedPadding[1] = CUSTOM_CONFIG_MARKER_1;
		netData._UnusedPadding[2] = CUSTOM_CONFIG_VERSION;
		netData._UnusedPadding[3] = static_cast<BYTE>(customFlags & 0xFF);
		netData._UnusedPadding[4] = static_cast<BYTE>((customFlags >> 8) & 0xFF);
		netData._UnusedPadding[5] = static_cast<BYTE>(std::clamp(gameData.iMinimumOptionLevel, 0, 16));
		for (int index = 0; index < 5; ++index)
		{
			netData._UnusedPadding[6 + index] = ClampPercent(gameData.aAutoPointPercent[index]);
		}
		for (int index = 11; index <= 15; ++index)
		{
			netData._UnusedPadding[index] = 0;
		}
	}

	void ConfigDataSerDe::Deserialize(const PRECEIVE_MUHELPER_DATA& netData, ConfigData& gameData)
	{
		gameData.iHuntingRange = static_cast<int>(netData.HuntingRange);
		if (gameData.iHuntingRange <= 0)
		{
			gameData.iHuntingRange = DEFAULT_HUNTING_RANGE;
		}
		gameData.iHuntingRange = std::clamp(gameData.iHuntingRange, 1, DEFAULT_HUNTING_RANGE);

		gameData.iMaxSecondsAway = std::clamp<int>(netData.DistanceMin, 0, 999);
		gameData.bLongRangeCounterAttack = (bool)netData.LongDistanceAttack;
		gameData.bReturnToOriginalPosition = (bool)netData.OriginalPosition;

		gameData.aiSkill.fill(0);
		gameData.aiSkill[0] = static_cast<int>(netData.BasicSkill1);
		gameData.aiSkill[1] = static_cast<int>(netData.ActivationSkill1);
		gameData.aiSkill[2] = static_cast<int>(netData.ActivationSkill2);

		gameData.aiSkillInterval.fill(0);
		gameData.aiSkillInterval[1] = std::clamp<int>(netData.DelayMinSkill1, 0, 999);
		gameData.aiSkillInterval[2] = std::clamp<int>(netData.DelayMinSkill2, 0, 999);

		gameData.aiSkillCondition.fill(0);
		gameData.aiSkillCondition[1] |= netData.Skill1Delay ? ON_TIMER : 0;
		gameData.aiSkillCondition[1] |= netData.Skill1Con ? ON_CONDITION : 0;
		gameData.aiSkillCondition[1] |= netData.Skill1PreCon == 0 ? ON_MOBS_NEARBY : ON_MOBS_ATTACKING;
		gameData.aiSkillCondition[1] |= netData.Skill1SubCon == 0 ? ON_MORE_THAN_TWO_MOBS :
			netData.Skill1SubCon == 1 ? ON_MORE_THAN_THREE_MOBS :
			netData.Skill1SubCon == 2 ? ON_MORE_THAN_FOUR_MOBS :
			netData.Skill1SubCon == 3 ? ON_MORE_THAN_FIVE_MOBS :
			0;

		gameData.aiSkillCondition[2] |= netData.Skill2Delay ? ON_TIMER : 0;
		gameData.aiSkillCondition[2] |= netData.Skill2Con ? ON_CONDITION : 0;
		gameData.aiSkillCondition[2] |= netData.Skill2PreCon == 0 ? ON_MOBS_NEARBY : ON_MOBS_ATTACKING;
		gameData.aiSkillCondition[2] |= netData.Skill2SubCon == 0 ? ON_MORE_THAN_TWO_MOBS :
			netData.Skill2SubCon == 1 ? ON_MORE_THAN_THREE_MOBS :
			netData.Skill2SubCon == 2 ? ON_MORE_THAN_FOUR_MOBS :
			netData.Skill2SubCon == 3 ? ON_MORE_THAN_FIVE_MOBS :
			0;
		gameData.bUseCombo = (bool)netData.Combo;

		gameData.aiBuff.fill(0);
		gameData.aiBuff[0] = static_cast<int>(netData.BuffSkill0NumberID);
		gameData.aiBuff[1] = static_cast<int>(netData.BuffSkill1NumberID);
		gameData.aiBuff[2] = static_cast<int>(netData.BuffSkill2NumberID);

		gameData.bBuffDuration = (bool)netData.BuffDuration;
		gameData.bBuffDurationParty = (bool)netData.BuffDurationforAllPartyMembers;
		gameData.iBuffCastInterval = std::clamp<int>(netData.CastingBuffMin, 0, 999);

		gameData.bAutoHeal = (bool)netData.AutoHeal;
		gameData.iHealThreshold = static_cast<int>(netData.HPStatusAutoHeal) * 10;
		gameData.bUseDrainLife = static_cast<int>(netData.DrainLife);
		gameData.bUseHealPotion = (bool)netData.AutoPotion;
		gameData.iPotionThreshold = static_cast<int>(netData.HPStatusAutoPotion) * 10;
		gameData.bSupportParty = (bool)netData.Party;
		gameData.bAutoHealParty = (bool)netData.PreferenceOfPartyHeal;
		gameData.iHealPartyThreshold = static_cast<int>(netData.HPStatusOfPartyMembers) * 10;

		gameData.bUseDarkRaven = (bool)netData.UseDarkSpirits;
		gameData.iDarkRavenMode = static_cast<int>(netData.PetAttack);
		gameData.bRepairItem = (bool)netData.RepairItem;

		gameData.iObtainingRange = static_cast<int>(netData.ObtainRange);
		if (gameData.iObtainingRange <= 0)
		{
			gameData.iObtainingRange = DEFAULT_OBTAINING_RANGE;
		}
		gameData.iObtainingRange = std::clamp(gameData.iObtainingRange, 1, DEFAULT_OBTAINING_RANGE);
		gameData.bPickAllItems = (bool)netData.PickAllNearItems;
		gameData.bPickSelectItems = (bool)netData.PickSelectedItems;
		gameData.bPickZen = (bool)netData.Zen;
		gameData.bPickJewel = (bool)netData.JewelOrGem;
		gameData.bPickExcellent = (bool)netData.ExcellentItem;
		gameData.bPickAncient = (bool)netData.SetItem;
		gameData.bPickExtraItems = (bool)netData.AddExtraItem;

		gameData.aExtraItems.clear();

		wchar_t wsExtraItemBuffer[15 + 1];
		for (int i = 0; i < sizeof(netData.ExtraItems) / sizeof(netData.ExtraItems[0]); i++)
		{
			memset(wsExtraItemBuffer, 0, sizeof(wsExtraItemBuffer));

			size_t n = std::mbstowcs(wsExtraItemBuffer, &netData.ExtraItems[i][0], 15);
			if (n > 0 && n <= 15)
			{
				wsExtraItemBuffer[n] = L'\0';
				if (wsExtraItemBuffer[0] != L'\0')
				{
					gameData.aExtraItems.insert(std::wstring(wsExtraItemBuffer));
				}
			}
		}

		// byte 33, client-local. server echoes unchanged, see MuHelperData.h
		gameData.bUseSelfDefense = (bool)netData.bUseSelfDefense;
		gameData.bAutoAcceptFriend = (bool)netData.bAutoAcceptFriend;
		gameData.bAutoAcceptGuild = (bool)netData.bAutoAcceptGuild;
		gameData.bFallbackBasicAttack = (bool)netData.bFallbackBasicAttack;
		if (!gameData.bFallbackBasicAttack
			&& gameData.aiSkill[0] == 0
			&& gameData.aiSkill[1] == 0
			&& gameData.aiSkill[2] == 0)
		{
			gameData.bFallbackBasicAttack = true;
		}

		gameData.bPickOnlyUsableItems = false;
		gameData.bPickCommonItems = gameData.bPickAllItems;
		gameData.bPickEventItems = false;
		gameData.bAutoConvertZenToAzoth = false;
		gameData.bAutoReset = false;
		gameData.bAutoDistributePoints = true;
		gameData.iMinimumOptionLevel = 0;
		gameData.aAutoPointPercent = { 25, 25, 20, 30, 0 };

		if (netData._UnusedPadding[0] == CUSTOM_CONFIG_MARKER_0
			&& netData._UnusedPadding[1] == CUSTOM_CONFIG_MARKER_1
			&& netData._UnusedPadding[2] >= CUSTOM_CONFIG_MIN_VERSION)
		{
			const WORD customFlags = static_cast<WORD>(netData._UnusedPadding[3])
				| (static_cast<WORD>(netData._UnusedPadding[4]) << 8);
			gameData.bPickOnlyUsableItems = (customFlags & CUSTOM_FLAG_PICK_USABLE) != 0;
			gameData.bPickCommonItems = (customFlags & CUSTOM_FLAG_PICK_COMMON) != 0;
			gameData.bPickEventItems = (customFlags & CUSTOM_FLAG_PICK_EVENT) != 0;
			gameData.bAutoConvertZenToAzoth = (customFlags & CUSTOM_FLAG_AUTO_AZOTH) != 0;
			gameData.bAutoReset = (customFlags & CUSTOM_FLAG_AUTO_RESET) != 0;
			gameData.bAutoDistributePoints = true;
			gameData.iMinimumOptionLevel = std::clamp<int>(netData._UnusedPadding[5], 0, 16);
			for (int index = 0; index < 5; ++index)
			{
				gameData.aAutoPointPercent[index] = ClampPercent(netData._UnusedPadding[6 + index]);
			}
		}
	}

}
