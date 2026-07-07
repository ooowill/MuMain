#include "stdafx.h"
#include "Character/KillNotificationClient.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <urlmon.h>
#ifdef _MSC_VER
#pragma comment(lib, "urlmon.lib")
#endif
#endif

#include "Engine/Object/ZzzInterface.h"
#include "Render/Sprites/GlobalBitmap.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "UI/Legacy/UIControls.h"

namespace
{
    constexpr wchar_t kKillPrefix[] = L"#KIL1|";
    constexpr wchar_t kAvatarBaseUrl[] = L"https://user.muonline.pt/kill-avatar/";
    constexpr bool kEnableRemoteAvatarTextures = false;

    constexpr float kBannerWidth = 238.0f;
    constexpr float kBannerHeight = 36.0f;
    constexpr float kBannerMarginRight = 9.0f;
    constexpr float kBannerTop = 66.0f;
    constexpr float kBannerStackGap = 4.0f;
    constexpr float kBannerTextureU = 1.0f;
    constexpr float kBannerTextureV = 154.0f / 256.0f;
    constexpr float kAvatarSize = 25.0f;
    constexpr float kLeftAvatarX = 5.0f;
    constexpr float kRightAvatarX = kBannerWidth - kLeftAvatarX - kAvatarSize;
    constexpr float kAvatarY = 5.0f;
    constexpr int kMaxNotifications = 3;
    constexpr int kFadeInMs = 260;
    constexpr int kHoldMs = 3100;
    constexpr int kFadeOutMs = 420;
    constexpr int kTotalMs = kFadeInMs + kHoldMs + kFadeOutMs;

    using Clock = std::chrono::steady_clock;

    struct KillNotification
    {
        bool IsSentinel = false;
        std::wstring KillerName;
        std::wstring VictimName;
        std::wstring KillerAvatarUrl;
        std::wstring VictimAvatarUrl;
        Clock::time_point CreatedAt = Clock::now();
    };

    struct AvatarCacheEntry
    {
        std::wstring LocalPath;
        bool DownloadStarted = false;
        bool DownloadFinished = false;
        bool DownloadSucceeded = false;
        bool LoadAttempted = false;
        GLuint Texture = BITMAP_UNKNOWN;
    };

    std::deque<KillNotification> g_notifications;
    std::unordered_map<std::wstring, AvatarCacheEntry> g_avatarCache;
    std::mutex g_avatarMutex;
    GLuint g_bannerTexture = BITMAP_UNKNOWN;

    int HexValue(wchar_t ch)
    {
        if (ch >= L'0' && ch <= L'9')
        {
            return ch - L'0';
        }

        if (ch >= L'a' && ch <= L'f')
        {
            return 10 + ch - L'a';
        }

        if (ch >= L'A' && ch <= L'F')
        {
            return 10 + ch - L'A';
        }

        return -1;
    }

    std::wstring Utf8BytesToWide(const std::string& bytes)
    {
        if (bytes.empty())
        {
            return {};
        }

#ifdef _WIN32
        const int inputLength = static_cast<int>(bytes.size());
        const int outputLength = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), inputLength, nullptr, 0);
        if (outputLength > 0)
        {
            std::wstring output(static_cast<size_t>(outputLength), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, bytes.data(), inputLength, output.data(), outputLength);
            return output;
        }
#endif

        return std::wstring(bytes.begin(), bytes.end());
    }

    std::wstring PercentDecode(const std::wstring& value)
    {
        std::wstring decoded;
        std::string bytes;

        const auto flushBytes = [&]()
        {
            if (!bytes.empty())
            {
                decoded += Utf8BytesToWide(bytes);
                bytes.clear();
            }
        };

        for (size_t index = 0; index < value.size(); ++index)
        {
            const wchar_t ch = value[index];
            if (ch == L'%' && index + 2 < value.size())
            {
                const int high = HexValue(value[index + 1]);
                const int low = HexValue(value[index + 2]);
                if (high >= 0 && low >= 0)
                {
                    bytes.push_back(static_cast<char>((high << 4) | low));
                    index += 2;
                    continue;
                }
            }

            if (ch <= 0x7F)
            {
                bytes.push_back(static_cast<char>(ch));
            }
            else
            {
                flushBytes();
                decoded.push_back(ch);
            }
        }

        flushBytes();
        return decoded;
    }

    std::vector<std::wstring> SplitFields(const wchar_t* payload)
    {
        std::vector<std::wstring> fields;
        std::wstring current;
        for (const wchar_t* cursor = payload; cursor != nullptr && *cursor != L'\0'; ++cursor)
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
        return fields;
    }

    std::wstring TrimName(std::wstring name)
    {
        name.erase(std::remove_if(name.begin(), name.end(), [](wchar_t ch)
        {
            return ch == L'\r' || ch == L'\n' || ch == L'|';
        }), name.end());

        if (name.size() > 12)
        {
            name.resize(12);
        }

        return name;
    }

    std::wstring BuildAvatarUrl(const std::wstring& name)
    {
        std::wstring url = kAvatarBaseUrl;
        url += name;
        return url;
    }

    bool FileExists(const std::wstring& path)
    {
#ifdef _WIN32
        return _waccess(path.c_str(), 0) == 0;
#else
        return false;
#endif
    }

    std::wstring AvatarLocalPath(const std::wstring& url)
    {
        const auto hash = static_cast<unsigned long long>(std::hash<std::wstring>{}(url));
        wchar_t fileName[64] {};
        std::swprintf(fileName, sizeof(fileName) / sizeof(fileName[0]), L"%016llx.jpg", hash);
        return std::wstring(L"Data\\Cache\\KillAvatars\\") + fileName;
    }

    void StartAvatarDownload(const std::wstring& url, const std::wstring& localPath)
    {
#ifdef _WIN32
        CreateDirectoryW(L"Data\\Cache", nullptr);
        CreateDirectoryW(L"Data\\Cache\\KillAvatars", nullptr);

        std::thread([url, localPath]()
        {
            const HRESULT result = URLDownloadToFileW(nullptr, url.c_str(), localPath.c_str(), 0, nullptr);
            std::lock_guard<std::mutex> lock(g_avatarMutex);
            auto& entry = g_avatarCache[url];
            entry.DownloadFinished = true;
            entry.DownloadSucceeded = SUCCEEDED(result) && FileExists(localPath);
        }).detach();
#else
        std::lock_guard<std::mutex> lock(g_avatarMutex);
        auto& entry = g_avatarCache[url];
        entry.DownloadFinished = true;
        entry.DownloadSucceeded = false;
#endif
    }

    AvatarCacheEntry& EnsureAvatarEntryLocked(const std::wstring& url)
    {
        auto& entry = g_avatarCache[url];
        if (entry.LocalPath.empty())
        {
            entry.LocalPath = AvatarLocalPath(url);
        }

        if (!entry.DownloadStarted)
        {
            entry.DownloadStarted = true;
            if (FileExists(entry.LocalPath))
            {
                entry.DownloadFinished = true;
                entry.DownloadSucceeded = true;
            }
            else
            {
                StartAvatarDownload(url, entry.LocalPath);
            }
        }

        return entry;
    }

    GLuint GetAvatarTexture(const std::wstring& url)
    {
        if (url.empty())
        {
            return BITMAP_UNKNOWN;
        }

        // Remote profile images are normal web files; the MU loader expects converted client texture data.
        if (!kEnableRemoteAvatarTextures)
        {
            return BITMAP_UNKNOWN;
        }

        std::wstring localPath;
        {
            std::lock_guard<std::mutex> lock(g_avatarMutex);
            AvatarCacheEntry& entry = EnsureAvatarEntryLocked(url);
            if (entry.Texture != BITMAP_UNKNOWN)
            {
                return entry.Texture;
            }

            if (!entry.DownloadFinished || !entry.DownloadSucceeded || entry.LoadAttempted)
            {
                return BITMAP_UNKNOWN;
            }

            entry.LoadAttempted = true;
            localPath = entry.LocalPath;
        }

        Bitmaps.Convert_Format(localPath);
        const GLuint texture = Bitmaps.LoadImage(localPath, GL_LINEAR, GL_CLAMP_TO_EDGE);
        std::lock_guard<std::mutex> lock(g_avatarMutex);
        auto& entry = g_avatarCache[url];
        entry.Texture = texture;
        return texture;
    }

    GLuint GetBannerTexture()
    {
        if (g_bannerTexture == BITMAP_UNKNOWN)
        {
            g_bannerTexture = Bitmaps.LoadImage(L"Data\\Interface\\mu_kill_notification.tga", GL_LINEAR, GL_CLAMP_TO_EDGE);
        }

        return g_bannerTexture;
    }

    float AgeMs(const KillNotification& notification, Clock::time_point now)
    {
        return static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(now - notification.CreatedAt).count());
    }

    float AlphaForAge(float ageMs)
    {
        if (ageMs < kFadeInMs)
        {
            return std::max(0.0f, ageMs / static_cast<float>(kFadeInMs));
        }

        if (ageMs > kFadeInMs + kHoldMs)
        {
            const float fadeAge = ageMs - static_cast<float>(kFadeInMs + kHoldMs);
            return std::max(0.0f, 1.0f - (fadeAge / static_cast<float>(kFadeOutMs)));
        }

        return 1.0f;
    }

    std::wstring InitialFor(const std::wstring& name)
    {
        if (name.empty())
        {
            return L"?";
        }

        wchar_t initial[2] { static_cast<wchar_t>(std::towupper(name.front())), L'\0' };
        return initial;
    }

    void RenderAvatar(const std::wstring& url, const std::wstring& name, float x, float y, float size, float alpha)
    {
        const GLuint texture = GetAvatarTexture(url);
        if (texture != BITMAP_UNKNOWN)
        {
            RenderBitmap(static_cast<int>(texture), x, y, size, size, 0.0f, 0.0f, 1.0f, 1.0f, true, true, alpha);
            return;
        }

        const std::wstring initial = InitialFor(name);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->SetTextColor(245, 214, 150, static_cast<BYTE>(230.0f * alpha));
        g_pRenderText->RenderText(static_cast<int>(x), static_cast<int>(y + size * 0.36f), initial.c_str(), static_cast<int>(size), 0, RT3_SORT_CENTER);
    }

    void RenderNotification(const KillNotification& notification, int index, Clock::time_point now)
    {
        const float ageMs = AgeMs(notification, now);
        const float alpha = AlphaForAge(ageMs);
        if (alpha <= 0.0f)
        {
            return;
        }

        const float slideOffset = (1.0f - alpha) * 28.0f;
        const float x = REFERENCE_WIDTH - kBannerWidth - kBannerMarginRight + slideOffset;
        const float y = kBannerTop + (kBannerHeight + kBannerStackGap) * static_cast<float>(index);

        const GLuint banner = GetBannerTexture();
        if (banner != BITMAP_UNKNOWN)
        {
            RenderBitmap(static_cast<int>(banner), x, y, kBannerWidth, kBannerHeight, 0.0f, 0.0f, kBannerTextureU, kBannerTextureV, true, true, alpha);
        }

        RenderAvatar(notification.KillerAvatarUrl, notification.KillerName, x + kLeftAvatarX, y + kAvatarY, kAvatarSize, alpha);
        RenderAvatar(notification.VictimAvatarUrl, notification.VictimName, x + kRightAvatarX, y + kAvatarY, kAvatarSize, alpha);

        const BYTE textAlpha = static_cast<BYTE>(240.0f * alpha);
        g_pRenderText->SetBgColor(0);
        g_pRenderText->SetTextColor(255, 240, 205, textAlpha);
        g_pRenderText->RenderText(static_cast<int>(x + 36.0f), static_cast<int>(y + 10.0f), notification.KillerName.c_str(), 65, 0, RT3_SORT_CENTER);
        g_pRenderText->RenderText(static_cast<int>(x + 137.0f), static_cast<int>(y + 10.0f), notification.VictimName.c_str(), 65, 0, RT3_SORT_CENTER);

        if (notification.IsSentinel)
        {
            g_pRenderText->SetTextColor(255, 190, 105, static_cast<BYTE>(215.0f * alpha));
            g_pRenderText->RenderText(static_cast<int>(x + 91.0f), static_cast<int>(y + 23.0f), L"SENTINELA", 58, 0, RT3_SORT_CENTER);
        }
    }
}

bool KillNotificationClient::HandleKillMessage(const wchar_t* message)
{
    if (message == nullptr)
    {
        return false;
    }

    const size_t prefixLength = std::wcslen(kKillPrefix);
    if (std::wcsncmp(message, kKillPrefix, prefixLength) != 0)
    {
        return false;
    }

    const std::vector<std::wstring> fields = SplitFields(message + prefixLength);
    if (fields.size() < 3)
    {
        return true;
    }

    KillNotification notification;
    notification.IsSentinel = fields[0] == L"s" || fields[0] == L"sentinel";
    notification.KillerName = TrimName(PercentDecode(fields[1]));
    notification.VictimName = TrimName(PercentDecode(fields[2]));
    notification.KillerAvatarUrl = BuildAvatarUrl(notification.KillerName);
    notification.VictimAvatarUrl = BuildAvatarUrl(notification.VictimName);
    notification.CreatedAt = Clock::now();

    if (notification.KillerName.empty() || notification.VictimName.empty())
    {
        return true;
    }

    g_notifications.push_front(notification);
    while (g_notifications.size() > kMaxNotifications)
    {
        g_notifications.pop_back();
    }

    return true;
}

void KillNotificationClient::RenderProfileAvatar(const std::wstring& characterName, float x, float y, float size, float alpha)
{
    RenderAvatar(BuildAvatarUrl(TrimName(characterName)), characterName, x, y, size, alpha);
}

void KillNotificationClient::Reset()
{
    g_notifications.clear();
}

void KillNotificationClient::Tick()
{
    const auto now = Clock::now();
    while (!g_notifications.empty() && AgeMs(g_notifications.back(), now) >= kTotalMs)
    {
        g_notifications.pop_back();
    }
}

void KillNotificationClient::Render()
{
    Tick();

    const auto now = Clock::now();
    int index = 0;
    for (const KillNotification& notification : g_notifications)
    {
        RenderNotification(notification, index, now);
        ++index;
    }
}
