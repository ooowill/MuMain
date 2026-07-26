#include "stdafx.h"
#include "Character/JewelBankClient.h"

#include <algorithm>
#include <array>
#include <cwchar>
#include <cstdio>
#include <iterator>

#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"

namespace
{
    constexpr wchar_t kJewelBankPrefix[] = L"#JWB1|";
    std::array<unsigned long long, JewelBankClient::JewelCount> g_counts{};
    bool g_autoStoreEnabled = false;
    unsigned int g_stateVersion = 0;
    bool g_requestedSessionState = false;

    bool TryParseCount(const wchar_t*& cursor, unsigned long long& value)
    {
        if (cursor == nullptr || cursor[0] == L'\0')
        {
            return false;
        }

        unsigned long long parsedValue = 0;
        bool hasDigit = false;
        while (*cursor >= L'0' && *cursor <= L'9')
        {
            hasDigit = true;
            const unsigned int digit = static_cast<unsigned int>(*cursor - L'0');
            if (parsedValue > (JewelBankClient::MaxDisplayCount - digit) / 10ULL)
            {
                parsedValue = JewelBankClient::MaxDisplayCount;
            }
            else
            {
                parsedValue = (parsedValue * 10ULL) + digit;
            }

            ++cursor;
        }

        if (!hasDigit)
        {
            return false;
        }

        value = std::min(parsedValue, JewelBankClient::MaxDisplayCount);
        return true;
    }

    bool TryParseState(const wchar_t* text, bool& autoStore, std::array<unsigned long long, JewelBankClient::JewelCount>& counts)
    {
        if (text == nullptr || (text[0] != L'0' && text[0] != L'1') || text[1] != L'|')
        {
            return false;
        }

        autoStore = text[0] == L'1';
        const wchar_t* cursor = text + 2;
        for (int index = 0; index < JewelBankClient::JewelCount; ++index)
        {
            unsigned long long count = 0;
            if (!TryParseCount(cursor, count))
            {
                return false;
            }

            counts[static_cast<size_t>(index)] = count;
            if (index + 1 < JewelBankClient::JewelCount)
            {
                if (*cursor != L'|')
                {
                    return false;
                }

                ++cursor;
            }
        }

        return true;
    }

    void SendJewelBankCommand(const wchar_t* command)
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

unsigned long long JewelBankClient::GetCount(int index)
{
    if (index < 0 || index >= JewelCount)
    {
        return 0;
    }

    return g_counts[static_cast<size_t>(index)];
}

bool JewelBankClient::IsAutoStoreEnabled()
{
    return g_autoStoreEnabled;
}

unsigned int JewelBankClient::GetStateVersion()
{
    return g_stateVersion;
}

void JewelBankClient::SetState(bool autoStore, const unsigned long long* counts, int count)
{
    g_autoStoreEnabled = autoStore;
    for (int index = 0; index < JewelCount; ++index)
    {
        g_counts[static_cast<size_t>(index)] = (counts != nullptr && index < count)
            ? std::min(counts[index], MaxDisplayCount)
            : 0ULL;
    }

    ++g_stateVersion;
}

void JewelBankClient::Tick()
{
    if (Hero == nullptr)
    {
        g_counts.fill(0);
        g_autoStoreEnabled = false;
        g_requestedSessionState = false;
        return;
    }

    if (!g_requestedSessionState)
    {
        g_requestedSessionState = true;
        RequestSync();
    }
}

bool JewelBankClient::HandleBankMessage(const wchar_t* senderName, const wchar_t* message)
{
    (void)senderName;

    if (message == nullptr)
    {
        return false;
    }

    const size_t prefixLength = std::wcslen(kJewelBankPrefix);
    if (std::wcsncmp(message, kJewelBankPrefix, prefixLength) != 0)
    {
        return false;
    }

    bool autoStore = false;
    std::array<unsigned long long, JewelCount> counts{};
    if (TryParseState(message + prefixLength, autoStore, counts))
    {
        SetState(autoStore, counts.data(), static_cast<int>(counts.size()));
    }

    return true;
}

void JewelBankClient::RequestSync()
{
    SendJewelBankCommand(L"/jewelbank sync");
}

void JewelBankClient::StoreAll()
{
    SendJewelBankCommand(L"/jewelbank store all");
}

void JewelBankClient::WithdrawAll()
{
    SendJewelBankCommand(L"/jewelbank withdraw all");
}

void JewelBankClient::Store(short jewelKey)
{
    wchar_t command[64] = {};
    std::swprintf(command, std::size(command), L"/jewelbank store %d", static_cast<int>(jewelKey));
    SendJewelBankCommand(command);
}

void JewelBankClient::Withdraw(short jewelKey)
{
    wchar_t command[64] = {};
    std::swprintf(command, std::size(command), L"/jewelbank withdraw %d", static_cast<int>(jewelKey));
    SendJewelBankCommand(command);
}

void JewelBankClient::SetAutoStore(bool enabled)
{
    g_autoStoreEnabled = enabled;
    ++g_stateVersion;
    SendJewelBankCommand(enabled ? L"/jewelbank auto on" : L"/jewelbank auto off");
}
