#pragma once

// ======================================================
// SOUND SYNTHESIS ENGINE (WinMM Procedural Synthesizer)
// ======================================================

enum SfxType
{
    SFX_LASER_DEFAULT = 0,
    SFX_LASER_SPREAD,
    SFX_LASER_PENTA,
    SFX_LASER_ENEMY,
    SFX_EXPLOSION_SMALL,
    SFX_EXPLOSION_LARGE,
    SFX_POWERUP,
    SFX_SHIELD_HIT,
    SFX_SPEED_BOOST,
    SFX_EMP_BOMB,
    SFX_LEVEL_UP,
    SFX_GAME_OVER,
    SFX_UI_CLICK,
    SFX_ALARM
};

// Global music tempo and sector theme trackers
extern int currentMusicTempo;
extern int sectorThemeIndex;

// Audio Lifecycle & Controls
void initAudio();
void shutdownAudio();
void playSfx(SfxType type, float vol = 1.0f);
