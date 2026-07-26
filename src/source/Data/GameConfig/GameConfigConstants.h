#pragma once

namespace CfgSections
{
    inline constexpr wchar_t CfgSectionWindow[]     = L"Window";
    inline constexpr wchar_t CfgSectionGraphics[]   = L"Graphics";
    inline constexpr wchar_t CfgSectionAudio[]      = L"Audio";
    inline constexpr wchar_t CfgSectionUI[]         = L"UI";
    inline constexpr wchar_t CfgSectionLogin[]      = L"LOGIN";
    inline constexpr wchar_t CfgSectionConnectionSettings[] = L"CONNECTION SETTINGS";
    inline constexpr wchar_t CfgSectionCamera[] = L"Camera";
    inline constexpr wchar_t CfgSectionGamePerformance[] = L"Game Performance";
}

namespace CfgKeys
{
    // Window
    inline constexpr wchar_t CfgKeyWidth[]      = L"Width";
    inline constexpr wchar_t CfgKeyHeight[]     = L"Height";
    inline constexpr wchar_t CfgKeyWindowed[]   = L"Windowed";

    // Audio — volume 0 = off, >0 = on (no separate Enabled flag).
    inline constexpr wchar_t CfgKeySoundVolume[]  = L"SoundVolume";
    inline constexpr wchar_t CfgKeyMusicVolume[] = L"MusicVolume";

    // Login
    inline constexpr wchar_t CfgKeyRememberMe[]        = L"RememberMe";
    inline constexpr wchar_t CfgKeyLanguage[]          = L"Language";
    inline constexpr wchar_t CfgKeyEncryptedUsername[] = L"EncryptedUsername";
    inline constexpr wchar_t CfgKeyEncryptedPassword[] = L"EncryptedPassword";

    // Connection
    inline constexpr wchar_t CfgKeyServerIP[]   = L"ServerIP";
    inline constexpr wchar_t CfgKeyServerPort[] = L"ServerPort";

    // UI
    inline constexpr wchar_t CfgKeyUILocale[] = L"Locale";

    // Camera
    inline constexpr wchar_t CfgKeyZoom[] = L"Zoom";

    // Game Performance
    inline constexpr wchar_t CfgKeyHideWorldObjects[] = L"HideWorldObjects";
    inline constexpr wchar_t CfgKeyDisableHeavyEffects[] = L"DisableHeavyEffects";
    inline constexpr wchar_t CfgKeyReduceCharacterGlow[] = L"ReduceCharacterGlow";
    inline constexpr wchar_t CfgKeyHideWings[] = L"HideWings";
    inline constexpr wchar_t CfgKeyHideMountsPets[] = L"HideMountsPets";
    inline constexpr wchar_t CfgKeySimplifyOtherPlayers[] = L"SimplifyOtherPlayers";
}

namespace CfgDefaults
{
    inline constexpr int  CfgDefaultWindowWidth  = 1024;
    inline constexpr int  CfgDefaultWindowHeight = 768;
    inline constexpr bool CfgDefaultWindowed     = true;

    inline constexpr int  CfgDefaultSoundVolume = 5;
    inline constexpr int  CfgDefaultMusicVolume = 5;

    inline constexpr bool CfgDefaultRememberMe = false;
    inline constexpr wchar_t CfgDefaultLanguage[] = L"Eng";
    inline constexpr wchar_t CfgDefaultEncryptedUsername[] = L"";
    inline constexpr wchar_t CfgDefaultEncryptedPassword[] = L"";

    inline constexpr wchar_t CfgDefaultServerIP[] = L"127.127.127.127";
    inline constexpr int CfgDefaultServerPort = 44406;

    inline constexpr int CfgDefaultZoom = 1735;  // OrbitalCamera DEFAULT_RADIUS — matches Default-cam camera-to-Hero distance

    // I18N locale code; "en" is the default the resx generator falls back to.
    inline constexpr wchar_t CfgDefaultUILocale[] = L"en";

    inline constexpr bool CfgDefaultHideWorldObjects = false;
    inline constexpr bool CfgDefaultDisableHeavyEffects = false;
    inline constexpr bool CfgDefaultReduceCharacterGlow = false;
    inline constexpr bool CfgDefaultHideWings = false;
    inline constexpr bool CfgDefaultHideMountsPets = false;
    inline constexpr bool CfgDefaultSimplifyOtherPlayers = false;
}
