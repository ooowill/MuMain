#pragma once

// Music playback subsystem (SDL_mixer-backed).  The legacy free
// functions (PlayMp3, StopMp3, StopMusic, IsEndMp3, GetMp3PlayPosition)
// are declared in Winmain.h to preserve existing call sites; their
// definitions live in AudioPlayer.cpp.

namespace AudioPlayer
{
    // Music volume levels are stored as a 0..MaxVolumeLevel scale in
    // the project's config; SDL_mixer expects a 0.0..1.0 gain.
    constexpr int MinVolumeLevel = 0;
    constexpr int MaxVolumeLevel = 10;
    constexpr int DefaultVolumeLevel = 5;

    // Initialize SDL audio + the mixer device, then apply the saved
    // music volume from GameConfig.  Safe to call once at startup.
    void Initialize();

    // Tear the mixer down.  Safe to call even if Initialize() failed.
    void Shutdown();

    // Apply a music volume on the project's 0..10 scale.  Out-of-range
    // values fall back to DefaultVolumeLevel.
    void SetMusicVolume(int level);

    // Runtime gain for the current map music, multiplied by the saved music
    // volume.  Used for local fades such as the Lorencia bar ambience.
    void SetMainMusicFade(float gain);

    // Secondary looped music layer for local ambience.  It respects the saved
    // music volume and is intentionally separate from PlayMp3/StopMp3 so map
    // music can keep playing once while ambience fades in and out.
    void PlayAmbientLoop(const char* path, float gain);
    void StopAmbient(const char* path = nullptr);
    void SetAmbientGain(float gain);

    int ClampVolume(int level);
}
