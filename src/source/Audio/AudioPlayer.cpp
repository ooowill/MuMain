#include "stdafx.h"
#include "Audio/AudioPlayer.h"
#include "Core/Platform/PathResolve.h"

#include "Data/GameConfig/GameConfig.h"
#include "App/Platform/Windows/Winmain.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <cstring>
#include <string>

extern bool Destroy;

namespace
{
    MIX_Mixer*  g_Mixer          = nullptr;
    MIX_Track*  g_MusicTrack     = nullptr;
    MIX_Track*  g_AmbientTrack   = nullptr;
    MIX_Audio*  g_CurrentAudio   = nullptr;
    MIX_Audio*  g_AmbientAudio   = nullptr;
    std::string g_CurrentPath;
    std::string g_AmbientPath;
    int         g_ConfigVolume   = AudioPlayer::DefaultVolumeLevel;
    float       g_MainFadeGain   = 1.f;
    float       g_AmbientGain    = 0.f;

    bool IsReady()
    {
        return g_Mixer != nullptr && g_MusicTrack != nullptr;
    }

    bool IsAmbientReady()
    {
        return g_Mixer != nullptr && g_AmbientTrack != nullptr;
    }

    float ClampGain(float gain)
    {
        if (gain < 0.f)
            return 0.f;
        if (gain > 1.f)
            return 1.f;
        return gain;
    }

    float ConfigVolumeGain()
    {
        return static_cast<float>(AudioPlayer::ClampVolume(g_ConfigVolume)) / static_cast<float>(AudioPlayer::MaxVolumeLevel);
    }

    void ApplyMainTrackGain()
    {
        if (!IsReady())
            return;
        MIX_SetTrackGain(g_MusicTrack, ConfigVolumeGain() * ClampGain(g_MainFadeGain));
    }

    void ApplyAmbientTrackGain()
    {
        if (!IsAmbientReady())
            return;
        MIX_SetTrackGain(g_AmbientTrack, ConfigVolumeGain() * ClampGain(g_AmbientGain));
    }

    void ReleaseCurrentAudio()
    {
        if (g_CurrentAudio)
        {
            MIX_DestroyAudio(g_CurrentAudio);
            g_CurrentAudio = nullptr;
        }
        g_CurrentPath.clear();
    }

    void ReleaseAmbientAudio()
    {
        if (g_AmbientAudio)
        {
            MIX_DestroyAudio(g_AmbientAudio);
            g_AmbientAudio = nullptr;
        }
        g_AmbientPath.clear();
    }

    bool LoadAndStartMusic(const char* path)
    {
#ifdef _WIN32
        MIX_Audio* audio = MIX_LoadAudio(g_Mixer, path, /*predecode=*/false);
#else
        // Music paths are Windows-spelled (backslashes, mixed case); resolve
        // them against the case-sensitive filesystem.
        MIX_Audio* audio = MIX_LoadAudio(g_Mixer, MuResolvePath(path).c_str(), /*predecode=*/false);
#endif
        if (!audio)
            return false;

        if (!MIX_SetTrackAudio(g_MusicTrack, audio))
        {
            MIX_DestroyAudio(audio);
            return false;
        }

        ReleaseCurrentAudio();
        g_CurrentAudio = audio;
        g_CurrentPath  = path;

        // Match the legacy wzAudioPlay(name, 1) semantics: play once.
        // On failure, clear g_CurrentPath so a subsequent PlayMp3 with the
        // same name doesn't early-return on the "already playing" check
        // and is allowed to retry.
        if (!MIX_PlayTrack(g_MusicTrack, 0))
        {
            g_CurrentPath.clear();
            return false;
        }
        ApplyMainTrackGain();
        return true;
    }

    bool LoadAndStartAmbient(const char* path)
    {
#ifdef _WIN32
        MIX_Audio* audio = MIX_LoadAudio(g_Mixer, path, /*predecode=*/false);
#else
        MIX_Audio* audio = MIX_LoadAudio(g_Mixer, MuResolvePath(path).c_str(), /*predecode=*/false);
#endif
        if (!audio)
            return false;

        if (!MIX_SetTrackAudio(g_AmbientTrack, audio))
        {
            MIX_DestroyAudio(audio);
            return false;
        }

        ReleaseAmbientAudio();
        g_AmbientAudio = audio;
        g_AmbientPath = path;

        ApplyAmbientTrackGain();
        if (!MIX_PlayTrack(g_AmbientTrack, -1))
        {
            g_AmbientPath.clear();
            return false;
        }
        return true;
    }
}

namespace AudioPlayer
{
    int ClampVolume(int level)
    {
        if (level < MinVolumeLevel || level > MaxVolumeLevel)
            return DefaultVolumeLevel;
        return level;
    }

    void Initialize()
    {
        if (IsReady())
            return;

        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
            return;

        if (!MIX_Init())
        {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return;
        }

        g_Mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (!g_Mixer)
        {
            MIX_Quit();
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return;
        }

        g_MusicTrack = MIX_CreateTrack(g_Mixer);
        if (!g_MusicTrack)
        {
            MIX_DestroyMixer(g_Mixer);
            g_Mixer = nullptr;
            MIX_Quit();
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return;
        }

        g_AmbientTrack = MIX_CreateTrack(g_Mixer);
        if (!g_AmbientTrack)
        {
            MIX_DestroyTrack(g_MusicTrack);
            g_MusicTrack = nullptr;
            MIX_DestroyMixer(g_Mixer);
            g_Mixer = nullptr;
            MIX_Quit();
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            return;
        }

        SetMusicVolume(GameConfig::GetInstance().GetMusicVolume());
    }

    void Shutdown()
    {
        // Order matters: the track holds a pointer to g_CurrentAudio, so the
        // track must be destroyed before its audio (otherwise we leave the
        // track briefly pointed at freed memory).  The mixer goes last since
        // both the track and the audio were created against it.
        if (g_MusicTrack)
        {
            MIX_DestroyTrack(g_MusicTrack);
            g_MusicTrack = nullptr;
        }

        if (g_AmbientTrack)
        {
            MIX_DestroyTrack(g_AmbientTrack);
            g_AmbientTrack = nullptr;
        }

        ReleaseCurrentAudio();
        ReleaseAmbientAudio();

        if (g_Mixer)
        {
            MIX_DestroyMixer(g_Mixer);
            g_Mixer = nullptr;
        }

        MIX_Quit();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    void SetMusicVolume(int level)
    {
        g_ConfigVolume = ClampVolume(level);
        ApplyMainTrackGain();
        ApplyAmbientTrackGain();
    }

    void SetMainMusicFade(float gain)
    {
        g_MainFadeGain = ClampGain(gain);
        ApplyMainTrackGain();
    }

    void PlayAmbientLoop(const char* path, float gain)
    {
        if (Destroy) return;
        if (!m_MusicOnOff) return;
        if (!IsAmbientReady()) return;

        g_AmbientGain = ClampGain(gain);
        ApplyAmbientTrackGain();

        if (g_AmbientPath == path && MIX_TrackPlaying(g_AmbientTrack))
            return;

        MIX_StopTrack(g_AmbientTrack, 0);
        LoadAndStartAmbient(path);
    }

    void StopAmbient(const char* path)
    {
        if (!IsAmbientReady()) return;
        if (g_AmbientPath.empty()) return;
        if (path != nullptr && g_AmbientPath != path) return;

        MIX_StopTrack(g_AmbientTrack, 0);
        MIX_SetTrackAudio(g_AmbientTrack, nullptr);
        ReleaseAmbientAudio();
        g_AmbientGain = 0.f;
    }

    void SetAmbientGain(float gain)
    {
        g_AmbientGain = ClampGain(gain);
        ApplyAmbientTrackGain();
    }
}

// ---------------------------------------------------------------------------
// Legacy free-function wrappers (declared in Winmain.h).
// Preserved verbatim semantics — gating on m_MusicOnOff and Destroy, plus
// the "no-op if same track is already loaded" behavior.
// ---------------------------------------------------------------------------

void StopMusic()
{
    if (!m_MusicOnOff) return;
    if (!IsReady()) return;
    MIX_StopTrack(g_MusicTrack, 0);
}

void StopMp3(const char* Name, BOOL bEnforce)
{
    if (!m_MusicOnOff && !bEnforce) return;
    if (!IsReady()) return;
    if (g_CurrentPath.empty()) return;

    if (std::strcmp(Name, g_CurrentPath.c_str()) == 0)
    {
        MIX_StopTrack(g_MusicTrack, 0);
        // Unbind before releasing: the track keeps a pointer to the audio
        // even when stopped, and ReleaseCurrentAudio destroys it.
        MIX_SetTrackAudio(g_MusicTrack, nullptr);
        ReleaseCurrentAudio();
    }
}

void PlayMp3(const char* Name, BOOL bEnforce)
{
    if (Destroy) return;
    if (!m_MusicOnOff && !bEnforce) return;
    if (!IsReady()) return;

    if (g_CurrentPath == Name)
        return;

    // Stop the prior track before swapping (matches the original
    // wzAudio WZAOPT_STOPBEFOREPLAY behavior).
    MIX_StopTrack(g_MusicTrack, 0);

    LoadAndStartMusic(Name);
}

bool IsEndMp3()
{
    if (!IsReady()) return false;
    if (!g_CurrentAudio) return false;
    return !MIX_TrackPlaying(g_MusicTrack);
}

int GetMp3PlayPosition()
{
    if (!IsReady() || !g_CurrentAudio) return 0;

    const Sint64 duration = MIX_GetAudioDuration(g_CurrentAudio);
    if (duration <= 0) return 0;

    const Sint64 position = MIX_GetTrackPlaybackPosition(g_MusicTrack);
    return static_cast<int>((position * 100) / duration);
}
