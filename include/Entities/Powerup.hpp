#pragma once

enum PowerUpType
{
    PU_SHIELD = 0,
    PU_TRIPLE_SHOT,
    PU_PENTA_SHOT,
    PU_RAPID_FIRE,
    PU_SPEED_BOOST,
    PU_EMP_BOMB,
    PU_TIME_SLOW,
    PU_EXTRA_LIFE,
    PU_COUNT
};

const int MAX_POWERUPS = 8;
const float POWERUP_SIZE = 24.0f;

struct PowerUp
{
    float x;
    float y;
    float vy;
    PowerUpType type;
    float pulse;
    bool active;
};

extern PowerUp powerUps[MAX_POWERUPS];

void initPowerUps();
void spawnPowerUp(float x, float y);
void updatePowerUps();
void drawPowerUps();
