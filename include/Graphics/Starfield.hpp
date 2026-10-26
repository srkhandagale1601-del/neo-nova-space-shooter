#pragma once

// ======================================================
// SECTOR THEMES & LEVEL DEFINITIONS
// ======================================================

struct SectorTheme
{
    const char* name;
    const char* subtitle;
    float bgTopR, bgTopG, bgTopB;
    float bgBotR, bgBotG, bgBotB;
    float nebulaR, nebulaG, nebulaB;
    float starR, starG, starB;
    float starSpeedMult;
    int targetScore; // Score needed to spawn sector boss
    int tempo;
};

const int NUM_SECTORS = 5;
extern SectorTheme sectorThemes[NUM_SECTORS];

extern int currentSector;
extern int sectorAnnouncementTimer;

// Starfield & Nebula Methods
void initStars();
void updateStars();
void drawBackgroundAndStars();
