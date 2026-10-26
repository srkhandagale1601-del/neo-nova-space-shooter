#pragma once

const int MAX_PARTICLES = 400;

struct Particle
{
    float x;
    float y;
    float vx;
    float vy;
    float r, g, b;
    float size;
    int life;
    int maxLife;
    bool active;
};

void initParticles();
void spawnExplosion(float x, float y, float r, float g, float b, int count, float speedMultiplier = 1.0f);
void spawnEngineTrail(float x, float y, float r, float g, float b, float vyOffset = -3.5f);
void updateParticles();
void drawParticles();

const int MAX_FLOATING_TEXTS = 24;

struct FloatingText
{
    float x;
    float y;
    char text[40];
    float r, g, b;
    int life;
    int maxLife;
    bool active;
};

void initFloatingTexts();
void addFloatingText(float x, float y, const char* str, float r, float g, float b);
void updateFloatingTexts();
void drawFloatingTexts();
