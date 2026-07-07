#include "stdafx.h"
#include "Character/AzothClient.h"

#include <algorithm>
#include <cwchar>
#include <iterator>

#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"

namespace
{
    constexpr wchar_t kBalancePrefix[] = L"#AZO1|";
    constexpr wchar_t kAutoSettingsPrefix[] = L"#AZO2|";
    constexpr wchar_t kNpcShopPricePrefix[] = L"#AZO3|";
    unsigned long long g_balance = 0;
    bool g_autoConversionEnabled = false;
    unsigned long long g_autoConversionReserveZen = 0;
    unsigned int g_autoConversionSettingsVersion = 0;
    bool g_requestedSessionBalance = false;
    unsigned long long g_npcShopPrices[256] = {};
    bool g_hasNpcShopPrice[256] = {};

    bool TryParseUnsigned(const wchar_t* text, unsigned long long& value)
    {
        if (text == nullptr || text[0] == L'\0')
        {
            return false;
        }

        unsigned long long parsedValue = 0;
        bool hasDigit = false;
        for (const wchar_t* cursor = text; *cursor != L'\0'; ++cursor)
        {
            if (*cursor < L'0' || *cursor > L'9')
            {
                break;
            }

            hasDigit = true;
            const unsigned int digit = static_cast<unsigned int>(*cursor - L'0');
            if (parsedValue > (AzothClient::MaxDisplayBalance - digit) / 10ULL)
            {
                parsedValue = AzothClient::MaxDisplayBalance;
                break;
            }

            parsedValue = (parsedValue * 10ULL) + digit;
        }

        if (!hasDigit)
        {
            return false;
        }

        value = std::min(parsedValue, AzothClient::MaxDisplayBalance);
        return true;
    }

    bool TryParseAutoSettings(const wchar_t* text, bool& enabled, unsigned long long& reserveZen)
    {
        if (text == nullptr || text[0] == L'\0')
        {
            return false;
        }

        enabled = text[0] == L'1';
        const wchar_t* separator = std::wcschr(text, L'|');
        if (separator == nullptr)
        {
            return false;
        }

        unsigned long long parsedReserveZen = 0;
        if (!TryParseUnsigned(separator + 1, parsedReserveZen))
        {
            return false;
        }

        reserveZen = std::min(parsedReserveZen, AzothClient::MaxAutoConversionReserveZen);
        return true;
    }

    bool TryParseNpcShopPrices(const wchar_t* text)
    {
        if (text == nullptr)
        {
            return false;
        }

        if (text[0] == L'C' && text[1] == L'\0')
        {
            AzothClient::ClearNpcShopPrices();
            return true;
        }

        const wchar_t* cursor = text;
        while (*cursor != L'\0')
        {
            unsigned long long slot = 0;
            if (!TryParseUnsigned(cursor, slot) || slot > 255)
            {
                return true;
            }

            while (*cursor >= L'0' && *cursor <= L'9')
            {
                ++cursor;
            }

            if (*cursor != L':')
            {
                return true;
            }

            ++cursor;
            unsigned long long price = 0;
            if (!TryParseUnsigned(cursor, price))
            {
                return true;
            }

            AzothClient::SetNpcShopPrice(static_cast<int>(slot), price);

            while (*cursor >= L'0' && *cursor <= L'9')
            {
                ++cursor;
            }

            if (*cursor == L',')
            {
                ++cursor;
                continue;
            }

            break;
        }

        return true;
    }

    void SendAzothCommand(const wchar_t* command)
    {
        if (Hero == nullptr || command == nullptr || command[0] == L'\0')
        {
            return;
        }

        wchar_t text[128] = {};
        std::wcsncpy(text, command, std::size(text) - 1);
        SendMacroChat(text);
    }
}

unsigned long long AzothClient::GetBalance()
{
    return g_balance;
}

void AzothClient::SetBalance(unsigned long long balance)
{
    g_balance = std::min(balance, MaxDisplayBalance);
}

bool AzothClient::IsAutoConversionEnabled()
{
    return g_autoConversionEnabled;
}

unsigned long long AzothClient::GetAutoConversionReserveZen()
{
    return g_autoConversionReserveZen;
}

unsigned int AzothClient::GetAutoConversionSettingsVersion()
{
    return g_autoConversionSettingsVersion;
}

void AzothClient::SetAutoConversionSettings(bool enabled, unsigned long long reserveZen)
{
    g_autoConversionEnabled = enabled;
    g_autoConversionReserveZen = std::min(reserveZen, MaxAutoConversionReserveZen);
    ++g_autoConversionSettingsVersion;
}

void AzothClient::Tick()
{
    if (Hero == nullptr)
    {
        g_balance = 0;
        g_requestedSessionBalance = false;
        return;
    }

    if (!g_requestedSessionBalance)
    {
        g_requestedSessionBalance = true;
        RequestBalance();
    }
}

void AzothClient::ClearNpcShopPrices()
{
    std::fill(std::begin(g_npcShopPrices), std::end(g_npcShopPrices), 0ULL);
    std::fill(std::begin(g_hasNpcShopPrice), std::end(g_hasNpcShopPrice), false);
}

void AzothClient::SetNpcShopPrice(int slot, unsigned long long price)
{
    if (slot < 0 || slot > 255)
    {
        return;
    }

    g_npcShopPrices[slot] = std::min(price, MaxDisplayBalance);
    g_hasNpcShopPrice[slot] = true;
}

bool AzothClient::GetNpcShopPrice(int slot, unsigned long long& price)
{
    if (slot < 0 || slot > 255 || !g_hasNpcShopPrice[slot])
    {
        return false;
    }

    price = g_npcShopPrices[slot];
    return true;
}

void AzothClient::FormatAmount(unsigned long long amount, wchar_t* text, size_t textLength)
{
    if (text == nullptr || textLength == 0)
    {
        return;
    }

    wchar_t raw[32] = {};
    swprintf_s(raw, L"%llu", amount);

    const size_t rawLength = std::wcslen(raw);
    const size_t commas = rawLength > 0 ? (rawLength - 1) / 3 : 0;
    const size_t needed = rawLength + commas;
    if (needed + 1 > textLength)
    {
        std::wcsncpy(text, raw, textLength - 1);
        text[textLength - 1] = L'\0';
        return;
    }

    size_t write = needed;
    text[write--] = L'\0';
    int groupCount = 0;
    for (size_t read = rawLength; read > 0; --read)
    {
        if (groupCount == 3)
        {
            text[write--] = L',';
            groupCount = 0;
        }

        text[write--] = raw[read - 1];
        ++groupCount;
    }
}

bool AzothClient::HandleBalanceMessage(const wchar_t* senderName, const wchar_t* message)
{
    (void)senderName;

    if (message == nullptr)
    {
        return false;
    }

    const size_t balancePrefixLength = std::wcslen(kBalancePrefix);
    if (std::wcsncmp(message, kBalancePrefix, balancePrefixLength) == 0)
    {
        unsigned long long balance = 0;
        if (TryParseUnsigned(message + balancePrefixLength, balance))
        {
            SetBalance(balance);
        }

        return true;
    }

    const size_t npcShopPricePrefixLength = std::wcslen(kNpcShopPricePrefix);
    if (std::wcsncmp(message, kNpcShopPricePrefix, npcShopPricePrefixLength) == 0)
    {
        return TryParseNpcShopPrices(message + npcShopPricePrefixLength);
    }

    const size_t autoSettingsPrefixLength = std::wcslen(kAutoSettingsPrefix);
    if (std::wcsncmp(message, kAutoSettingsPrefix, autoSettingsPrefixLength) != 0)
    {
        return false;
    }

    bool enabled = false;
    unsigned long long reserveZen = 0;
    if (TryParseAutoSettings(message + autoSettingsPrefixLength, enabled, reserveZen))
    {
        SetAutoConversionSettings(enabled, reserveZen);
    }

    return true;
}

void AzothClient::RequestBalance()
{
    SendAzothCommand(L"/azoth sync");
}

void AzothClient::ConvertCurrent(long long zen)
{
    if (zen <= 0)
    {
        ConvertCurrentMax();
        return;
    }

    wchar_t command[128] = {};
    swprintf_s(command, L"/azoth convert %lld", zen);
    SendAzothCommand(command);
}

void AzothClient::ConvertCurrentMax()
{
    SendAzothCommand(L"/azoth max");
}

void AzothClient::ConvertAccountCharacters()
{
    SendAzothCommand(L"/azoth all");
}

void AzothClient::ConfigureAutoConversion(bool enabled, unsigned long long reserveZen)
{
    if (!enabled)
    {
        SendAzothCommand(L"/azoth auto off");
        return;
    }

    wchar_t command[128] = {};
    swprintf_s(command, L"/azoth auto %llu", std::min(reserveZen, MaxAutoConversionReserveZen));
    SendAzothCommand(command);
}

void AzothClient::ApplyAutoConversionSettings(bool enabled, unsigned long long reserveZen)
{
    if (!enabled)
    {
        SetAutoConversionSettings(false, 0);
        SendAzothCommand(L"/azoth auto off");
        return;
    }

    const unsigned long long safeReserveZen = std::min(reserveZen, MaxAutoConversionReserveZen);
    SetAutoConversionSettings(true, safeReserveZen);

    wchar_t command[128] = {};
    swprintf_s(command, L"/azoth auto %llu apply", safeReserveZen);
    SendAzothCommand(command);
}
