#pragma once

namespace GuildProfileClient
{
    constexpr unsigned int MaxGuildLevel = 9999;

    void Reset();
    bool HandleProfileMessage(const wchar_t* message);
    void SetGuildLevel(int guildKey, unsigned int level);
    bool GetGuildLevel(int guildKey, unsigned int& level);
}
