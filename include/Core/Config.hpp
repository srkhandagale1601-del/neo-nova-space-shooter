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
    STATE_LEADERBOARD,   // Dev 3 - Phase 1
    STATE_SECTOR_CLEAR
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

enum ShipArchetypeType
{
    SHIP_VANGUARD = 0,   // Vanguard Interceptor: High speed, rapid cadence, 3 HP
    SHIP_AEGIS,          // Aegis Dreadnought: Tank, innate forcefield, 6 HP
    SHIP_MIRAGE,         // Mirage Gunship: Micro-missile spread, 4 HP
    SHIP_ARCHETYPE_COUNT
};
extern int selectedShipArchetype;
