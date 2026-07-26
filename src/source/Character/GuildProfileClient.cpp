#include "stdafx.h"
#include "Character/GuildProfileClient.h"

#include <algorithm>
#include <climits>
#include <cwchar>
#include <unordered_map>

namespace
{
    constexpr wchar_t kGuildProfilePrefix[] = L"#GLD1|";

    std::unordered_map<int, unsigned int> g_guildLevels;

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
}

void GuildProfileClient::Reset()
{
    g_guildLevels.clear();
}

bool GuildProfileClient::HandleProfileMessage(const wchar_t* message)
{
    if (message == nullptr)
    {
        return false;
    }

    const size_t prefixLength = std::wcslen(kGuildProfilePrefix);
    if (std::wcsncmp(message, kGuildProfilePrefix, prefixLength) != 0)
    {
        return false;
    }

    const wchar_t* cursor = message + prefixLength;
    unsigned long guildKey = 0;
    unsigned long level = 0;
    if (TryParseUnsigned(cursor, guildKey)
        && TryConsumeSeparator(cursor)
        && TryParseUnsigned(cursor, level)
        && guildKey <= static_cast<unsigned long>(INT_MAX))
    {
        SetGuildLevel(
            static_cast<int>(guildKey),
            static_cast<unsigned int>(std::min<unsigned long>(level, MaxGuildLevel)));
    }

    return true;
}

void GuildProfileClient::SetGuildLevel(int guildKey, unsigned int level)
{
    if (guildKey <= 0)
    {
        return;
    }

    g_guildLevels[guildKey] = std::min(level, MaxGuildLevel);
}

bool GuildProfileClient::GetGuildLevel(int guildKey, unsigned int& level)
{
    const auto it = g_guildLevels.find(guildKey);
    if (it == g_guildLevels.end())
    {
        return false;
    }

    level = it->second;
    return true;
}
