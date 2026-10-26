#pragma once

// ======================================================
// WINDOW CONFIGURATION
// ======================================================

const int WIDTH = 800;
const int HEIGHT = 600;

// ======================================================
// GAME STATES
// ======================================================

enum GameState
{
    STATE_MAIN_MENU = 0,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_SETTINGS,
    STATE_HOW_TO_PLAY,
    STATE_GAME_OVER,
    STATE_SECTOR_CLEAR,
    STATE_CG_INFO,
    STATE_HOW_IT_WORKS
};

// ======================================================
// SETTINGS & AUDIO CONTROLS
// ======================================================

extern float masterVolume;
extern float sfxVolume;
extern float musicVolume;

enum ScreenShakeLevel
{
    SHAKE_OFF = 0,
    SHAKE_NORMAL,
    SHAKE_HEAVY,
    SHAKE_COUNT
};
extern int screenShakeSetting;

enum ShipAccentColor
{
    ACCENT_CYAN = 0,
    ACCENT_EMERALD,
    ACCENT_GOLD,
    ACCENT_CRIMSON,
    ACCENT_COUNT
};
extern int shipAccentSetting;
