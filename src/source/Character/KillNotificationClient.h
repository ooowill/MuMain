#pragma once

#include <string>

namespace KillNotificationClient
{
    bool HandleKillMessage(const wchar_t* message);
    void RenderProfileAvatar(const std::wstring& characterName, float x, float y, float size, float alpha = 1.0f);
    void Reset();
    void Tick();
    void Render();
}
