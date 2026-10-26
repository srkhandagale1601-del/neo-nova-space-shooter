#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <GL/freeglut.h>

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

#include "Core/Config.hpp"
#include "Audio/SoundEngine.hpp"

// ======================================================
// GAME STATES & USER SETTINGS
// ======================================================

GameState currentGameState = STATE_MAIN_MENU;
GameState previousState = STATE_MAIN_MENU;

float masterVolume = 0.80f;
float sfxVolume    = 0.85f;
float musicVolume  = 0.55f;
int screenShakeSetting = SHAKE_NORMAL;
int shipAccentSetting = ACCENT_CYAN;

// ======================================================
// SCREEN SHAKE
// ======================================================

float screenShake = 0.0f;

void triggerShake(float intensity)
{
    if (screenShakeSetting == SHAKE_OFF)
        return;

    float mult = (screenShakeSetting == SHAKE_HEAVY) ? 1.6f : 1.0f;
    float scaled = intensity * mult;
    if (scaled > screenShake)
    {
        screenShake = scaled;
    }
}

#include "Graphics/Renderer.hpp"
#include "Graphics/ParticleSystem.hpp"

#include "Graphics/Starfield.hpp"

#include "Entities/Powerup.hpp"

// ======================================================
// PLAYER DEFINITION
// ======================================================

float playerX = WIDTH / 2.0f;
float playerY = 60.0f;

const float PLAYER_WIDTH = 54.0f;
const float PLAYER_HEIGHT = 46.0f;
const float BASE_PLAYER_SPEED = 8.5f;

int invulnerableTimer = 0;
int animFrame = 0;

// Active Power-up Timers
int shieldHealth = 0;      // > 0 means shield active (absorbs hits)
int tripleShotTimer = 0;   // Frames remaining
int pentaShotTimer = 0;    // Frames remaining (5-way barrage!)
int rapidFireTimer = 0;    // Frames remaining
int speedBoostTimer = 0;   // Frames remaining (hyper-thruster)
int timeSlowTimer = 0;     // Frames remaining (chrono-warp)
int empBombsStock = 1;     // Number of EMP bombs in storage

// Continuous firing state & cadence
bool spacePressed = false;
int shootCooldown = 0;
int cannonAlternator = 0;

// Keyboard navigation state
bool leftPressed = false;
bool rightPressed = false;
bool upPressed = false;
bool downPressed = false;

// ======================================================
// BULLETS
// ======================================================

const int MAX_BULLETS = 90;

struct Bullet
{
    float x;
    float y;
    float vx;
    float vy;
    bool isEnemy;
    int type; // 0 = standard, 1 = heavy/plasma, 2 = boss projectile
    bool active;
};

Bullet bullets[MAX_BULLETS];

const float BULLET_WIDTH = 5.0f;
const float BULLET_HEIGHT = 16.0f;
const float BULLET_SPEED = 14.0f;

// ======================================================
// ENEMIES & BOSS ARCHITECTURE
// ======================================================

const int MAX_ENEMIES = 10;

struct Enemy
{
    float x;
    float y;
    float speed;
    float wobbleOffset;
    int shootTimer;
    int type; // 0 = Scout (Red), 1 = Interceptor (Purple), 2 = Heavy Raider (Orange)
    int hp;
    bool active;
};

Enemy enemies[MAX_ENEMIES];

const float ENEMY_WIDTH = 48.0f;
const float ENEMY_HEIGHT = 38.0f;

// --- BOSS CAPITAL SHIP ---
struct BossShip
{
    bool active;
    float x;
    float y;
    float targetY;
    float vx;
    float width;
    float height;
    int hp;
    int maxHp;
    int shootTimer;
    int specialAttackTimer;
    int phase;
    const char* name;
};

BossShip currentBoss;

// ======================================================
// GAME PROGRESSION & SCORING
// ======================================================

int score = 0;
int highScore = 0;
int lives = 3;
int wave = 1;

int comboCount = 0;
int comboTimer = 0; // Frames before combo expires

bool gameOver = false;
bool paused = false;

// Forward declarations
void initializeGame();
void nextSector();
void triggerEmpBomb();

// ======================================================
// DRAW PLAYER SHIP
// ======================================================

void drawPlayer()
{
    if (invulnerableTimer > 0 && (animFrame / 4) % 2 == 1)
    {
        return; // Blinking invulnerability
    }

    // Determine ship accent colors based on user settings
    float accR = 0.2f, accG = 0.85f, accB = 1.0f; // Default Neon Cyan
    if (shipAccentSetting == ACCENT_EMERALD) { accR = 0.2f; accG = 1.0f; accB = 0.4f; }
    else if (shipAccentSetting == ACCENT_GOLD) { accR = 1.0f; accG = 0.85f; accB = 0.1f; }
    else if (shipAccentSetting == ACCENT_CRIMSON) { accR = 1.0f; accG = 0.2f; accB = 0.25f; }

    // --- Animated Thruster Engine Flame ---
    float flameMult = (speedBoostTimer > 0) ? 1.8f : 1.0f;
    float flameHeight = (12.0f + (rand() % 14)) * flameMult;

    // Hyper-Thruster Warp Particles
    if (speedBoostTimer > 0)
    {
        spawnEngineTrail(playerX - 12.0f, playerY - 4.0f, 0.1f, 0.9f, 1.0f);
        spawnEngineTrail(playerX + 12.0f, playerY - 4.0f, 0.1f, 0.9f, 1.0f);
    }

    // Outer Thruster Flame
    if (speedBoostTimer > 0)
        glColor3f(0.1f, 0.85f, 1.0f); // Electric Blue afterburner
    else
        glColor3f(1.0f, 0.45f, 0.05f); // Orange normal flame

    glBegin(GL_TRIANGLES);
        glVertex2f(playerX - 10.0f, playerY);
        glVertex2f(playerX, playerY - flameHeight);
        glVertex2f(playerX + 10.0f, playerY);
    glEnd();

    // Inner Yellow Thruster Core
    glColor3f(1.0f, 0.95f, 0.4f);
    glBegin(GL_TRIANGLES);
        glVertex2f(playerX - 5.0f, playerY);
        glVertex2f(playerX, playerY - flameHeight * 0.65f);
        glVertex2f(playerX + 5.0f, playerY);
    glEnd();

    // --- Main Wings (Deep Armored Indigo) ---
    glColor3f(0.10f, 0.25f, 0.55f);
    glBegin(GL_POLYGON);
        glVertex2f(playerX, playerY + PLAYER_HEIGHT);
        glVertex2f(playerX + PLAYER_WIDTH / 2.0f, playerY - 4.0f);
        glVertex2f(playerX + PLAYER_WIDTH * 0.35f, playerY + 8.0f);
        glVertex2f(playerX - PLAYER_WIDTH * 0.35f, playerY + 8.0f);
        glVertex2f(playerX - PLAYER_WIDTH / 2.0f, playerY - 4.0f);
    glEnd();

    // --- Wing Armor Trim Panels ---
    glColor3f(0.06f, 0.15f, 0.35f);
    glBegin(GL_TRIANGLES);
        glVertex2f(playerX - PLAYER_WIDTH / 2.0f, playerY - 4.0f);
        glVertex2f(playerX - PLAYER_WIDTH * 0.25f, playerY + 12.0f);
        glVertex2f(playerX - PLAYER_WIDTH * 0.45f, playerY + 14.0f);

        glVertex2f(playerX + PLAYER_WIDTH / 2.0f, playerY - 4.0f);
        glVertex2f(playerX + PLAYER_WIDTH * 0.25f, playerY + 12.0f);
        glVertex2f(playerX + PLAYER_WIDTH * 0.45f, playerY + 14.0f);
    glEnd();

    // --- Fuselage Center (Custom Accent Color) ---
    glColor3f(accR, accG, accB);
    glBegin(GL_TRIANGLES);
        glVertex2f(playerX, playerY + PLAYER_HEIGHT);
        glVertex2f(playerX - 14.0f, playerY + 2.0f);
        glVertex2f(playerX + 14.0f, playerY + 2.0f);
    glEnd();

    // --- Wingtip Laser Cannons ---
    if (rapidFireTimer > 0)
        glColor3f(1.0f, 0.9f, 0.1f); // Golden glowing cannons in rapid fire
    else if (pentaShotTimer > 0)
        glColor3f(0.95f, 0.2f, 1.0f); // Magenta glowing cannons in penta shot
    else if (tripleShotTimer > 0)
        glColor3f(0.2f, 1.0f, 0.3f); // Green glowing cannons in triple shot
    else
        glColor3f(0.95f, 0.25f, 0.25f);

    glBegin(GL_QUADS);
        // Left Cannon
        glVertex2f(playerX - PLAYER_WIDTH / 2.0f - 2.0f, playerY);
        glVertex2f(playerX - PLAYER_WIDTH / 2.0f + 2.0f, playerY);
        glVertex2f(playerX - PLAYER_WIDTH / 2.0f + 2.0f, playerY + 16.0f);
        glVertex2f(playerX - PLAYER_WIDTH / 2.0f - 2.0f, playerY + 16.0f);

        // Right Cannon
        glVertex2f(playerX + PLAYER_WIDTH / 2.0f - 2.0f, playerY);
        glVertex2f(playerX + PLAYER_WIDTH / 2.0f + 2.0f, playerY);
        glVertex2f(playerX + PLAYER_WIDTH / 2.0f + 2.0f, playerY + 16.0f);
        glVertex2f(playerX + PLAYER_WIDTH / 2.0f - 2.0f, playerY + 16.0f);
    glEnd();

    // --- Cockpit Canopy (Ice Blue / Crystal Diamond) ---
    glColor3f(0.92f, 0.98f, 1.0f);
    glBegin(GL_TRIANGLES);
        glVertex2f(playerX, playerY + PLAYER_HEIGHT - 6.0f);
        glVertex2f(playerX - 6.5f, playerY + 16.0f);
        glVertex2f(playerX + 6.5f, playerY + 16.0f);
    glEnd();

    // --- Glowing Energy Shield Bubble (if active) ---
    if (shieldHealth > 0)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        float shieldRadius = PLAYER_WIDTH * 0.74f;
        float pulse = 0.5f + 0.35f * sinf(animFrame * 0.16f);

        glColor4f(0.1f, 0.85f, 1.0f, pulse);
        glLineWidth(2.5f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 28; i++)
        {
            float theta = i * 2.0f * 3.14159265f / 28.0f;
            float sx = playerX + cosf(theta) * shieldRadius;
            float sy = (playerY + PLAYER_HEIGHT / 2.0f) + sinf(theta) * shieldRadius;
            glVertex2f(sx, sy);
        }
        glEnd();
        glLineWidth(1.0f);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

// ======================================================
// DRAW BULLETS
// ======================================================

void drawBullet(const Bullet &b)
{
    if (!b.active)
        return;

    if (b.isEnemy)
    {
        if (b.type == 2) // Boss heavy orb
        {
            glColor3f(1.0f, 0.1f, 0.85f);
            glPointSize(10.0f);
            glBegin(GL_POINTS);
                glVertex2f(b.x, b.y);
            glEnd();

            glColor3f(1.0f, 1.0f, 1.0f);
            glPointSize(5.0f);
            glBegin(GL_POINTS);
                glVertex2f(b.x, b.y);
            glEnd();
        }
        else
        {
            // Enemy Crimson Laser Orb
            glColor3f(1.0f, 0.15f, 0.15f);
            glPointSize(7.0f);
            glBegin(GL_POINTS);
                glVertex2f(b.x, b.y);
            glEnd();

            glColor3f(1.0f, 0.85f, 0.85f);
            glPointSize(3.0f);
            glBegin(GL_POINTS);
                glVertex2f(b.x, b.y);
            glEnd();
        }
    }
    else
    {
        // Player Laser Beam slanted with horizontal velocity
        float slantX = b.vx * 1.25f;

        // Outer glow color
        if (pentaShotTimer > 0)
            glColor3f(0.95f, 0.2f, 1.0f); // Magenta
        else if (tripleShotTimer > 0)
            glColor3f(0.2f, 1.0f, 0.4f);  // Emerald green
        else if (rapidFireTimer > 0)
            glColor3f(1.0f, 0.5f, 0.1f);  // Orange plasma
        else
            glColor3f(0.2f, 0.95f, 1.0f); // Cyan default

        glBegin(GL_QUADS);
            glVertex2f(b.x - BULLET_WIDTH / 2.0f, b.y);
            glVertex2f(b.x + BULLET_WIDTH / 2.0f, b.y);
            glVertex2f(b.x + BULLET_WIDTH / 2.0f + slantX, b.y + BULLET_HEIGHT);
            glVertex2f(b.x - BULLET_WIDTH / 2.0f + slantX, b.y + BULLET_HEIGHT);
        glEnd();

        // Hot White Core
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
            glVertex2f(b.x - BULLET_WIDTH * 0.25f, b.y + 2.0f);
            glVertex2f(b.x + BULLET_WIDTH * 0.25f, b.y + 2.0f);
            glVertex2f(b.x + BULLET_WIDTH * 0.25f + slantX * 0.7f, b.y + BULLET_HEIGHT - 2.0f);
            glVertex2f(b.x - BULLET_WIDTH * 0.25f + slantX * 0.7f, b.y + BULLET_HEIGHT - 2.0f);
        glEnd();
    }
}

// ======================================================
// DRAW ENEMIES
// ======================================================

void drawEnemy(const Enemy &e)
{
    if (!e.active)
        return;

    if (e.type == 0)
    {
        // --- TYPE 0: Crimson Scout Fighter ---
        glColor3f(0.92f, 0.18f, 0.24f);
        glBegin(GL_POLYGON);
            glVertex2f(e.x, e.y);
            glVertex2f(e.x + ENEMY_WIDTH / 2.0f, e.y + ENEMY_HEIGHT * 0.7f);
            glVertex2f(e.x + ENEMY_WIDTH * 0.35f, e.y + ENEMY_HEIGHT);
            glVertex2f(e.x - ENEMY_WIDTH * 0.35f, e.y + ENEMY_HEIGHT);
            glVertex2f(e.x - ENEMY_WIDTH / 2.0f, e.y + ENEMY_HEIGHT * 0.7f);
        glEnd();

        // Dark armor plate
        glColor3f(0.45f, 0.05f, 0.12f);
        glBegin(GL_TRIANGLES);
            glVertex2f(e.x, e.y + 8.0f);
            glVertex2f(e.x - 12.0f, e.y + ENEMY_HEIGHT - 4.0f);
            glVertex2f(e.x + 12.0f, e.y + ENEMY_HEIGHT - 4.0f);
        glEnd();

        // Visor Eyes
        glColor3f(1.0f, 0.95f, 0.1f);
        glPointSize(4.0f);
        glBegin(GL_POINTS);
            glVertex2f(e.x - 9.0f, e.y + 18.0f);
            glVertex2f(e.x + 9.0f, e.y + 18.0f);
        glEnd();
    }
    else if (e.type == 1)
    {
        // --- TYPE 1: Elite Purple Interceptor ---
        glColor3f(0.72f, 0.12f, 0.92f);
        glBegin(GL_TRIANGLES);
            glVertex2f(e.x, e.y - 2.0f);
            glVertex2f(e.x + ENEMY_WIDTH / 2.0f + 4.0f, e.y + ENEMY_HEIGHT);
            glVertex2f(e.x - ENEMY_WIDTH / 2.0f - 4.0f, e.y + ENEMY_HEIGHT);
        glEnd();

        // Center Hull
        glColor3f(0.35f, 0.05f, 0.6f);
        glBegin(GL_POLYGON);
            glVertex2f(e.x, e.y + 4.0f);
            glVertex2f(e.x + 10.0f, e.y + ENEMY_HEIGHT - 2.0f);
            glVertex2f(e.x - 10.0f, e.y + ENEMY_HEIGHT - 2.0f);
        glEnd();

        // Reactor Eye
        glColor3f(0.1f, 1.0f, 0.95f);
        glPointSize(5.0f);
        glBegin(GL_POINTS);
            glVertex2f(e.x, e.y + 15.0f);
        glEnd();
    }
    else
    {
        // --- TYPE 2: Heavy Armored Raider (Orange/Gold) ---
        glColor3f(1.0f, 0.45f, 0.1f);
        glBegin(GL_POLYGON);
            glVertex2f(e.x, e.y - 4.0f);
            glVertex2f(e.x + ENEMY_WIDTH * 0.55f, e.y + 12.0f);
            glVertex2f(e.x + ENEMY_WIDTH * 0.45f, e.y + ENEMY_HEIGHT);
            glVertex2f(e.x - ENEMY_WIDTH * 0.45f, e.y + ENEMY_HEIGHT);
            glVertex2f(e.x - ENEMY_WIDTH * 0.55f, e.y + 12.0f);
        glEnd();

        glColor3f(0.2f, 0.1f, 0.05f);
        glBegin(GL_QUADS);
            glVertex2f(e.x - 8.0f, e.y + 10.0f);
            glVertex2f(e.x + 8.0f, e.y + 10.0f);
            glVertex2f(e.x + 8.0f, e.y + ENEMY_HEIGHT - 6.0f);
            glVertex2f(e.x - 8.0f, e.y + ENEMY_HEIGHT - 6.0f);
        glEnd();
    }
}

// ======================================================
// DRAW BOSS CAPITAL SHIP
// ======================================================

void drawBoss(const BossShip &b)
{
    if (!b.active)
        return;

    float hw = b.width / 2.0f;
    float hh = b.height / 2.0f;

    // Glowing Hull Armor
    glColor3f(0.85f, 0.1f, 0.3f);
    glBegin(GL_POLYGON);
        glVertex2f(b.x, b.y - hh);
        glVertex2f(b.x + hw * 0.7f, b.y - hh * 0.3f);
        glVertex2f(b.x + hw, b.y + hh * 0.5f);
        glVertex2f(b.x + hw * 0.8f, b.y + hh);
        glVertex2f(b.x - hw * 0.8f, b.y + hh);
        glVertex2f(b.x - hw, b.y + hh * 0.5f);
        glVertex2f(b.x - hw * 0.7f, b.y - hh * 0.3f);
    glEnd();

    // Secondary Hull Plates
    glColor3f(0.25f, 0.05f, 0.15f);
    glBegin(GL_POLYGON);
        glVertex2f(b.x, b.y - hh * 0.5f);
        glVertex2f(b.x + hw * 0.5f, b.y + hh * 0.3f);
        glVertex2f(b.x - hw * 0.5f, b.y + hh * 0.3f);
    glEnd();

    // Reactor Core pulsing
    float pulse = 0.5f + 0.5f * sinf(animFrame * 0.2f);
    glColor3f(1.0f * pulse, 0.8f * pulse, 0.1f);
    glPointSize(12.0f);
    glBegin(GL_POINTS);
        glVertex2f(b.x, b.y);
    glEnd();

    // Left and Right Wing Cannons
    glColor3f(1.0f, 0.3f, 0.3f);
    glBegin(GL_QUADS);
        glVertex2f(b.x - hw * 0.85f - 3.0f, b.y - hh * 0.2f);
        glVertex2f(b.x - hw * 0.85f + 3.0f, b.y - hh * 0.2f);
        glVertex2f(b.x - hw * 0.85f + 3.0f, b.y + 12.0f);
        glVertex2f(b.x - hw * 0.85f - 3.0f, b.y + 12.0f);

        glVertex2f(b.x + hw * 0.85f - 3.0f, b.y - hh * 0.2f);
        glVertex2f(b.x + hw * 0.85f + 3.0f, b.y - hh * 0.2f);
        glVertex2f(b.x + hw * 0.85f + 3.0f, b.y + 12.0f);
        glVertex2f(b.x + hw * 0.85f - 3.0f, b.y + 12.0f);
    glEnd();

    // Boss Health Bar Floating Overhead
    float barWidth = 240.0f;
    float barHeight = 12.0f;
    float barX = WIDTH / 2.0f - barWidth / 2.0f;
    float barY = HEIGHT - 75.0f;

    // Background bar
    glColor4f(0.1f, 0.1f, 0.15f, 0.85f);
    glBegin(GL_QUADS);
        glVertex2f(barX - 2.0f, barY - 2.0f);
        glVertex2f(barX + barWidth + 2.0f, barY - 2.0f);
        glVertex2f(barX + barWidth + 2.0f, barY + barHeight + 2.0f);
        glVertex2f(barX - 2.0f, barY + barHeight + 2.0f);
    glEnd();

    // Filled health bar
    float fillPct = (float)b.hp / (float)b.maxHp;
    if (fillPct < 0.0f) fillPct = 0.0f;

    glColor3f(1.0f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
        glVertex2f(barX, barY);
        glVertex2f(barX + barWidth * fillPct, barY);
        glVertex2f(barX + barWidth * fillPct, barY + barHeight);
        glVertex2f(barX, barY + barHeight);
    glEnd();

    // Border
    glColor3f(1.0f, 0.8f, 0.2f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(barX, barY);
        glVertex2f(barX + barWidth, barY);
        glVertex2f(barX + barWidth, barY + barHeight);
        glVertex2f(barX, barY + barHeight);
    glEnd();

    // Boss Name Tag
    char bossTag[64];
    sprintf(bossTag, "%s [%d / %d]", b.name, b.hp, b.maxHp);
    renderGlText(barX, barY + 16.0f, bossTag, GLUT_BITMAP_HELVETICA_12);
}

// ======================================================
// DRAW HUD
// ======================================================

void drawHUD()
{
    char scoreText[64];
    char highText[64];
    char sectorText[64];
    char livesText[64];
    char comboText[64];
    char empText[64];

    sprintf(scoreText, "SCORE: %d", score);
    sprintf(highText, "HIGH: %d", highScore);
    sprintf(sectorText, "%s", sectorThemes[currentSector % NUM_SECTORS].name);
    sprintf(livesText, "LIVES: %d", lives);
    sprintf(empText, "EMP [B]: %d", empBombsStock);

    // Top HUD Bar background banner
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.03f, 0.05f, 0.12f, 0.82f);
    glBegin(GL_QUADS);
        glVertex2f(0.0f, HEIGHT - 50.0f);
        glVertex2f(WIDTH, HEIGHT - 50.0f);
        glVertex2f(WIDTH, HEIGHT);
        glVertex2f(0.0f, HEIGHT);
    glEnd();

    // Bottom Border Accent Line
    glColor3f(0.2f, 0.7f, 1.0f);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
        glVertex2f(0.0f, HEIGHT - 50.0f);
        glVertex2f(WIDTH, HEIGHT - 50.0f);
    glEnd();
    glLineWidth(1.0f);

    // Top text elements
    glColor3f(0.3f, 0.95f, 1.0f);
    renderGlText(20, HEIGHT - 32, scoreText);

    glColor3f(1.0f, 0.85f, 0.2f);
    renderGlText(160, HEIGHT - 32, highText);

    glColor3f(0.9f, 0.65f, 1.0f);
    renderGlText(WIDTH / 2 - 80, HEIGHT - 32, sectorText);

    // EMP Bomb count
    glColor3f(0.4f, 0.8f, 1.0f);
    renderGlText(WIDTH - 240, HEIGHT - 32, empText);

    // Lives
    if (lives > 1)
        glColor3f(0.2f, 1.0f, 0.4f);
    else
        glColor3f(1.0f, 0.3f, 0.3f);
    renderGlText(WIDTH - 110, HEIGHT - 32, livesText);

    // Active Power-ups Bar
    float powerUpX = 20.0f;
    float powerUpY = HEIGHT - 72.0f;

    if (shieldHealth > 0)
    {
        char buf[32];
        sprintf(buf, "[SHIELD: %d]", shieldHealth);
        glColor3f(0.1f, 0.85f, 1.0f);
        renderGlText(powerUpX, powerUpY, buf, GLUT_BITMAP_HELVETICA_12);
        powerUpX += 95.0f;
    }

    if (pentaShotTimer > 0)
    {
        char buf[32];
        sprintf(buf, "[PENTA: %ds]", pentaShotTimer / 60);
        glColor3f(0.95f, 0.2f, 1.0f);
        renderGlText(powerUpX, powerUpY, buf, GLUT_BITMAP_HELVETICA_12);
        powerUpX += 95.0f;
    }
    else if (tripleShotTimer > 0)
    {
        char buf[32];
        sprintf(buf, "[TRIPLE: %ds]", tripleShotTimer / 60);
        glColor3f(1.0f, 0.85f, 0.1f);
        renderGlText(powerUpX, powerUpY, buf, GLUT_BITMAP_HELVETICA_12);
        powerUpX += 95.0f;
    }

    if (rapidFireTimer > 0)
    {
        char buf[32];
        sprintf(buf, "[RAPID: %ds]", rapidFireTimer / 60);
        glColor3f(1.0f, 0.3f, 0.3f);
        renderGlText(powerUpX, powerUpY, buf, GLUT_BITMAP_HELVETICA_12);
        powerUpX += 95.0f;
    }

    if (speedBoostTimer > 0)
    {
        char buf[32];
        sprintf(buf, "[SPEED: %ds]", speedBoostTimer / 60);
        glColor3f(0.0f, 1.0f, 0.85f);
        renderGlText(powerUpX, powerUpY, buf, GLUT_BITMAP_HELVETICA_12);
        powerUpX += 95.0f;
    }

    if (timeSlowTimer > 0)
    {
        char buf[32];
        sprintf(buf, "[CHRONO: %ds]", timeSlowTimer / 60);
        glColor3f(0.3f, 0.7f, 1.0f);
        renderGlText(powerUpX, powerUpY, buf, GLUT_BITMAP_HELVETICA_12);
        powerUpX += 95.0f;
    }

    // Combo Multiplier indicator
    if (comboCount > 1 && comboTimer > 0)
    {
        sprintf(comboText, "COMBO x%d!", comboCount);
        float comboAlpha = (float)comboTimer / 120.0f;
        glColor4f(1.0f, 0.9f, 0.1f, comboAlpha);
        renderGlText(WIDTH - 130, HEIGHT - 72, comboText, GLUT_BITMAP_HELVETICA_18);
    }

    // Sector Transition Banner Announcement
    if (sectorAnnouncementTimer > 0)
    {
        float bannerAlpha = (sectorAnnouncementTimer > 40) ? 0.9f : ((float)sectorAnnouncementTimer / 40.0f);
        glColor4f(0.05f, 0.08f, 0.18f, bannerAlpha * 0.85f);
        glBegin(GL_QUADS);
            glVertex2f(0, HEIGHT / 2.0f - 45.0f);
            glVertex2f(WIDTH, HEIGHT / 2.0f - 45.0f);
            glVertex2f(WIDTH, HEIGHT / 2.0f + 45.0f);
            glVertex2f(0, HEIGHT / 2.0f + 45.0f);
        glEnd();

        glColor4f(0.2f, 0.9f, 1.0f, bannerAlpha);
        renderGlText(WIDTH / 2 - 120, HEIGHT / 2 + 10, sectorThemes[currentSector % NUM_SECTORS].name, GLUT_BITMAP_TIMES_ROMAN_24);

        glColor4f(1.0f, 0.85f, 0.2f, bannerAlpha);
        renderGlText(WIDTH / 2 - 110, HEIGHT / 2 - 20, sectorThemes[currentSector % NUM_SECTORS].subtitle, GLUT_BITMAP_HELVETICA_12);
    }
}

// ======================================================
// INTERACTIVE MENUS (MAIN MENU, PAUSE, SETTINGS, HOW-TO)
// ======================================================

int menuSelectedIdx = 0;

void drawButton(float x, float y, float w, float h, const char* label, bool selected)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (selected)
    {
        // Glowing Golden/Cyan highlight
        glColor4f(0.12f, 0.45f, 0.75f, 0.85f);
        glBegin(GL_QUADS);
            glVertex2f(x, y);
            glVertex2f(x + w, y);
            glVertex2f(x + w, y + h);
            glVertex2f(x, y + h);
        glEnd();

        glColor3f(1.0f, 0.85f, 0.2f);
        glLineWidth(2.5f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(x, y);
            glVertex2f(x + w, y);
            glVertex2f(x + w, y + h);
            glVertex2f(x, y + h);
        glEnd();
        glLineWidth(1.0f);

        glColor3f(1.0f, 1.0f, 1.0f);
    }
    else
    {
        glColor4f(0.06f, 0.12f, 0.25f, 0.65f);
        glBegin(GL_QUADS);
            glVertex2f(x, y);
            glVertex2f(x + w, y);
            glVertex2f(x + w, y + h);
            glVertex2f(x, y + h);
        glEnd();

        glColor3f(0.3f, 0.6f, 0.85f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(x, y);
            glVertex2f(x + w, y);
            glVertex2f(x + w, y + h);
            glVertex2f(x, y + h);
        glEnd();

        glColor3f(0.85f, 0.9f, 0.95f);
    }

    renderGlText(x + 22.0f, y + 14.0f, label, GLUT_BITMAP_HELVETICA_18);
}

void drawSlider(float x, float y, float w, float h, const char* label, float value, bool selected)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Box background
    if (selected)
        glColor4f(0.15f, 0.35f, 0.65f, 0.85f);
    else
        glColor4f(0.06f, 0.12f, 0.25f, 0.65f);

    glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
    glEnd();

    // Slider track
    float trackX = x + 160.0f;
    float trackW = w - 180.0f;
    float trackY = y + h / 2.0f - 4.0f;
    float trackH = 8.0f;

    glColor3f(0.1f, 0.15f, 0.25f);
    glBegin(GL_QUADS);
        glVertex2f(trackX, trackY);
        glVertex2f(trackX + trackW, trackY);
        glVertex2f(trackX + trackW, trackY + trackH);
        glVertex2f(trackX, trackY + trackH);
    glEnd();

    // Active fill
    glColor3f(0.2f, 0.85f, 1.0f);
    glBegin(GL_QUADS);
        glVertex2f(trackX, trackY);
        glVertex2f(trackX + trackW * value, trackY);
        glVertex2f(trackX + trackW * value, trackY + trackH);
        glVertex2f(trackX, trackY + trackH);
    glEnd();

    // Slider knob
    glColor3f(1.0f, 0.9f, 0.2f);
    glBegin(GL_QUADS);
        glVertex2f(trackX + trackW * value - 6.0f, trackY - 4.0f);
        glVertex2f(trackX + trackW * value + 6.0f, trackY - 4.0f);
        glVertex2f(trackX + trackW * value + 6.0f, trackY + trackH + 4.0f);
        glVertex2f(trackX + trackW * value - 6.0f, trackY + trackH + 4.0f);
    glEnd();

    // Label and percent text
    char displayStr[64];
    sprintf(displayStr, "%s: %d%%", label, (int)(value * 100.0f));
    glColor3f(selected ? 1.0f : 0.85f, 1.0f, selected ? 0.3f : 0.9f);
    renderGlText(x + 15.0f, y + 14.0f, displayStr, GLUT_BITMAP_HELVETICA_12);
}

void drawMainMenu()
{
    // Title Glow
    float pulse = 0.8f + 0.2f * sinf(animFrame * 0.08f);
    glColor3f(0.2f * pulse, 0.9f * pulse, 1.0f);
    renderGlText(WIDTH / 2 - 180, HEIGHT - 110, "NEO NOVA: 2D SPACE SHOOTER", GLUT_BITMAP_TIMES_ROMAN_24);

    glColor3f(1.0f, 0.85f, 0.2f);
    renderGlText(WIDTH / 2 - 115, HEIGHT - 145, "- ENHANCED SECTOR EDITION -", GLUT_BITMAP_HELVETICA_12);

    float bw = 240.0f;
    float bh = 44.0f;
    float bx = WIDTH / 2.0f - bw / 2.0f;
    float startY = HEIGHT - 240.0f;
    float gap = 58.0f;

    drawButton(bx, startY, bw, bh, "  START MISSION", menuSelectedIdx == 0);
    drawButton(bx, startY - gap, bw, bh, "     SETTINGS", menuSelectedIdx == 1);
    drawButton(bx, startY - gap * 2, bw, bh, "  HOW TO PLAY", menuSelectedIdx == 2);
    drawButton(bx, startY - gap * 3, bw, bh, "   QUIT GAME", menuSelectedIdx == 3);

    glColor3f(0.6f, 0.7f, 0.85f);
    renderGlText(WIDTH / 2 - 160, 40, "Controls: Arrows / WASD to Select, Space / Enter to Confirm", GLUT_BITMAP_HELVETICA_12);
}

void drawPauseMenu()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.02f, 0.03f, 0.07f, 0.75f);
    glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f(WIDTH, 0);
        glVertex2f(WIDTH, HEIGHT);
        glVertex2f(0, HEIGHT);
    glEnd();

    glColor3f(1.0f, 0.85f, 0.2f);
    renderGlText(WIDTH / 2 - 85, HEIGHT - 120, "GAME PAUSED", GLUT_BITMAP_TIMES_ROMAN_24);

    float bw = 240.0f;
    float bh = 44.0f;
    float bx = WIDTH / 2.0f - bw / 2.0f;
    float startY = HEIGHT - 210.0f;
    float gap = 55.0f;

    drawButton(bx, startY, bw, bh, "    RESUME", menuSelectedIdx == 0);
    drawButton(bx, startY - gap, bw, bh, "    RESTART", menuSelectedIdx == 1);
    drawButton(bx, startY - gap * 2, bw, bh, "    SETTINGS", menuSelectedIdx == 2);
    drawButton(bx, startY - gap * 3, bw, bh, "   MAIN MENU", menuSelectedIdx == 3);

    glColor3f(0.7f, 0.7f, 0.8f);
    renderGlText(WIDTH / 2 - 95, 45, "Press 'P' or 'Space' to Resume", GLUT_BITMAP_HELVETICA_12);
}

void drawSettingsMenu()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.02f, 0.03f, 0.08f, 0.88f);
    glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f(WIDTH, 0);
        glVertex2f(WIDTH, HEIGHT);
        glVertex2f(0, HEIGHT);
    glEnd();

    glColor3f(0.2f, 0.9f, 1.0f);
    renderGlText(WIDTH / 2 - 90, HEIGHT - 75, "GAME SETTINGS", GLUT_BITMAP_TIMES_ROMAN_24);

    float sw = 420.0f;
    float sh = 38.0f;
    float sx = WIDTH / 2.0f - sw / 2.0f;
    float startY = HEIGHT - 150.0f;
    float gap = 50.0f;

    // Sliders
    drawSlider(sx, startY, sw, sh, "Master Volume", masterVolume, menuSelectedIdx == 0);
    drawSlider(sx, startY - gap, sw, sh, "SFX / Bullet Volume", sfxVolume, menuSelectedIdx == 1);
    drawSlider(sx, startY - gap * 2, sw, sh, "Music / Game Noise", musicVolume, menuSelectedIdx == 2);

    // Screen Shake Setting Toggle
    const char* shakeStr = (screenShakeSetting == SHAKE_NORMAL) ? "Screen Shake: NORMAL" :
                           (screenShakeSetting == SHAKE_HEAVY) ? "Screen Shake: HEAVY" : "Screen Shake: OFF";
    drawButton(sx, startY - gap * 3, sw, sh, shakeStr, menuSelectedIdx == 3);

    // Ship Accent Color Toggle
    const char* shipAccentStr = (shipAccentSetting == ACCENT_CYAN) ? "Ship Hull: NEON CYAN" :
                                (shipAccentSetting == ACCENT_EMERALD) ? "Ship Hull: EMERALD" :
                                (shipAccentSetting == ACCENT_GOLD) ? "Ship Hull: GOLD" : "Ship Hull: CRIMSON";
    drawButton(sx, startY - gap * 4, sw, sh, shipAccentStr, menuSelectedIdx == 4);

    // Back button
    drawButton(sx, startY - gap * 5.2f, sw, sh, "       < BACK / APPLY >", menuSelectedIdx == 5);

    glColor3f(0.65f, 0.75f, 0.9f);
    renderGlText(WIDTH / 2 - 195, 35, "Use Left/Right to adjust volumes. Up/Down to navigate. Enter to toggle.", GLUT_BITMAP_HELVETICA_12);
}

void drawHowToPlay()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.02f, 0.03f, 0.08f, 0.92f);
    glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f(WIDTH, 0);
        glVertex2f(WIDTH, HEIGHT);
        glVertex2f(0, HEIGHT);
    glEnd();

    glColor3f(0.2f, 0.9f, 1.0f);
    renderGlText(WIDTH / 2 - 80, HEIGHT - 65, "HOW TO PLAY", GLUT_BITMAP_TIMES_ROMAN_24);

    float leftX = 80.0f;
    float y = HEIGHT - 110.0f;

    glColor3f(1.0f, 0.85f, 0.2f);
    renderGlText(leftX, y, "FLIGHT CONTROLS & WEAPONS:", GLUT_BITMAP_HELVETICA_18);
    y -= 25.0f;
    glColor3f(0.9f, 0.95f, 1.0f);
    renderGlText(leftX + 20, y, "* Move Left / Right : 'A' / 'D' or Left / Right Arrow Keys", GLUT_BITMAP_HELVETICA_12);
    y -= 20.0f;
    renderGlText(leftX + 20, y, "* Continuous Fire   : Hold 'Spacebar' (Auto-fire cadence with momentum)", GLUT_BITMAP_HELVETICA_12);
    y -= 20.0f;
    renderGlText(leftX + 20, y, "* EMP Shockwave     : Press 'B' (Clears screen of enemy fire & inflicts heavy damage)", GLUT_BITMAP_HELVETICA_12);
    y -= 20.0f;
    renderGlText(leftX + 20, y, "* Pause / Resume    : Press 'P' or 'ESC'", GLUT_BITMAP_HELVETICA_12);
    y -= 20.0f;
    renderGlText(leftX + 20, y, "* Quick Restart     : Press 'R'", GLUT_BITMAP_HELVETICA_12);

    y -= 35.0f;
    glColor3f(1.0f, 0.85f, 0.2f);
    renderGlText(leftX, y, "COLLECTIBLE POWER-UPS:", GLUT_BITMAP_HELVETICA_18);
    y -= 25.0f;
    glColor3f(0.1f, 0.85f, 1.0f);
    renderGlText(leftX + 20, y, "[S] SHIELD FORCEFIELD : Absorbs 2-3 direct hits before hull damage", GLUT_BITMAP_HELVETICA_12);
    y -= 20.0f;
    glColor3f(1.0f, 0.85f, 0.1f);
    renderGlText(leftX + 20, y, "[3] TRIPLE CANNONS    : 3-way spread laser barrage (10s)", GLUT_BITMAP_HELVETICA_12);
    y -= 20.0f;
    glColor3f(0.95f, 0.2f, 1.0f);
    renderGlText(leftX + 20, y, "[5] PENTA-BLAST       : 5-way devastating nova spread (10s)", GLUT_BITMAP_HELVETICA_12);
    y -= 20.0f;
    glColor3f(1.0f, 0.3f, 0.2f);
    renderGlText(leftX + 20, y, "[R] RAPID OVERCHARGE  : Triples blaster fire cadence (10s)", GLUT_BITMAP_HELVETICA_12);
    y -= 20.0f;
    glColor3f(0.0f, 1.0f, 0.85f);
    renderGlText(leftX + 20, y, "[W] WARP HYPER-THRUST : +60% Flight Speed with glowing afterburners (12s)", GLUT_BITMAP_HELVETICA_12);
    y -= 20.0f;
    glColor3f(0.2f, 0.7f, 1.0f);
    renderGlText(leftX + 20, y, "[T] CHRONO-WARP       : Slows enemy motion and hostile lasers to 40% (8s)", GLUT_BITMAP_HELVETICA_12);
    y -= 20.0f;
    glColor3f(0.2f, 1.0f, 0.3f);
    renderGlText(leftX + 20, y, "[+] NANO-REPAIR       : Restores +1 ship hull life", GLUT_BITMAP_HELVETICA_12);

    float bw = 200.0f;
    float bh = 38.0f;
    float bx = WIDTH / 2.0f - bw / 2.0f;
    drawButton(bx, 30.0f, bw, bh, "    < RETURN >", true);
}

void drawGameOver()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.06f, 0.02f, 0.04f, 0.84f);
    glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f(WIDTH, 0);
        glVertex2f(WIDTH, HEIGHT);
        glVertex2f(0, HEIGHT);
    glEnd();

    glColor3f(1.0f, 0.22f, 0.22f);
    renderGlText(WIDTH / 2 - 80, HEIGHT / 2 + 95, "GAME OVER", GLUT_BITMAP_TIMES_ROMAN_24);

    char finalScore[64];
    char finalWave[64];
    sprintf(finalScore, "Final Score: %d", score);
    sprintf(finalWave, "Sector Reached: %s", sectorThemes[currentSector % NUM_SECTORS].name);

    glColor3f(1.0f, 1.0f, 1.0f);
    renderGlText(WIDTH / 2 - 60, HEIGHT / 2 + 45, finalScore);
    renderGlText(WIDTH / 2 - 110, HEIGHT / 2 + 15, finalWave);

    if (score >= highScore && score > 0)
    {
        glColor3f(1.0f, 0.9f, 0.1f);
        renderGlText(WIDTH / 2 - 85, HEIGHT / 2 - 20, "*** NEW HIGH SCORE! ***", GLUT_BITMAP_HELVETICA_12);
    }

    float bw = 240.0f;
    float bh = 42.0f;
    float bx = WIDTH / 2.0f - bw / 2.0f;

    drawButton(bx, HEIGHT / 2.0f - 80.0f, bw, bh, "  RETRY MISSION", menuSelectedIdx == 0);
    drawButton(bx, HEIGHT / 2.0f - 135.0f, bw, bh, "    MAIN MENU", menuSelectedIdx == 1);
}

// ======================================================
// SHOOT BULLETS (WITH MOMENTUM & MULTI-CANNON SPREADS)
// ======================================================

void spawnPlayerBullet(float x, float y, float vx, float vy)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].active)
        {
            bullets[i].x = x;
            bullets[i].y = y;
            bullets[i].vx = vx;
            bullets[i].vy = vy;
            bullets[i].isEnemy = false;
            bullets[i].type = 0;
            bullets[i].active = true;
            break;
        }
    }
}

void shoot()
{
    float currentSpeed = BASE_PLAYER_SPEED * (speedBoostTimer > 0 ? 1.6f : 1.0f);
    float playerVx = 0.0f;
    if (leftPressed && !rightPressed)
        playerVx = -currentSpeed;
    else if (rightPressed && !leftPressed)
        playerVx = currentSpeed;

    float inheritedVx = playerVx * 0.42f;

    if (pentaShotTimer > 0)
    {
        // 5-Way Nova Barrage!
        spawnPlayerBullet(playerX - 22.0f, playerY + 15.0f, inheritedVx - 5.5f, BULLET_SPEED * 0.9f);
        spawnPlayerBullet(playerX - 12.0f, playerY + 28.0f, inheritedVx - 2.8f, BULLET_SPEED * 0.95f);
        spawnPlayerBullet(playerX, playerY + PLAYER_HEIGHT, inheritedVx, BULLET_SPEED);
        spawnPlayerBullet(playerX + 12.0f, playerY + 28.0f, inheritedVx + 2.8f, BULLET_SPEED * 0.95f);
        spawnPlayerBullet(playerX + 22.0f, playerY + 15.0f, inheritedVx + 5.5f, BULLET_SPEED * 0.9f);

        playSfx(SFX_LASER_PENTA, 0.85f);
    }
    else if (tripleShotTimer > 0)
    {
        // 3-Way Laser Spread
        spawnPlayerBullet(playerX - 18.0f, playerY + 20.0f, inheritedVx - 3.4f, BULLET_SPEED * 0.95f);
        spawnPlayerBullet(playerX, playerY + PLAYER_HEIGHT, inheritedVx, BULLET_SPEED);
        spawnPlayerBullet(playerX + 18.0f, playerY + 20.0f, inheritedVx + 3.4f, BULLET_SPEED * 0.95f);

        playSfx(SFX_LASER_SPREAD, 0.75f);
    }
    else
    {
        // Alternating Left / Right Wingtip Cannons
        float cannonOffset = (cannonAlternator == 0) ? -22.0f : 22.0f;
        cannonAlternator = 1 - cannonAlternator;

        spawnPlayerBullet(playerX + cannonOffset, playerY + 14.0f, inheritedVx, BULLET_SPEED);
        playSfx(SFX_LASER_DEFAULT, 0.65f);
    }
}

void spawnEnemyBullet(float x, float y, float vx = 0.0f, float vy = -5.5f, int type = 0)
{
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (!bullets[i].active)
        {
            bullets[i].x = x;
            bullets[i].y = y;
            bullets[i].vx = vx;
            bullets[i].vy = vy;
            bullets[i].isEnemy = true;
            bullets[i].type = type;
            bullets[i].active = true;
            break;
        }
    }
}

// EMP Shockwave Bomb: clears all enemy bullets and heavily damages foes
void triggerEmpBomb()
{
    if (empBombsStock <= 0)
        return;

    empBombsStock--;
    playSfx(SFX_EMP_BOMB, 1.0f);
    triggerShake(12.0f);

    // Wipe all enemy bullets
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (bullets[i].active && bullets[i].isEnemy)
        {
            bullets[i].active = false;
            spawnExplosion(bullets[i].x, bullets[i].y, 0.2f, 0.8f, 1.0f, 6);
        }
    }

    // Heavy damage to all enemies
    for (int j = 0; j < MAX_ENEMIES; j++)
    {
        if (enemies[j].active)
        {
            enemies[j].active = false;
            score += 15;
            spawnExplosion(enemies[j].x, enemies[j].y, 0.4f, 0.9f, 1.0f, 25);
            addFloatingText(enemies[j].x, enemies[j].y, "+15 EMP", 0.4f, 0.8f, 1.0f);
        }
    }

    // Damage Boss if present
    if (currentBoss.active)
    {
        currentBoss.hp -= 120;
        spawnExplosion(currentBoss.x, currentBoss.y, 1.0f, 0.3f, 1.0f, 40);
        addFloatingText(currentBoss.x, currentBoss.y + 40.0f, "EMP -120 HP!", 0.2f, 0.9f, 1.0f);
    }

    addFloatingText(WIDTH / 2.0f - 60.0f, HEIGHT / 2.0f, "EMP SHOCKWAVE DETONATED!", 0.3f, 0.9f, 1.0f);
}

// ======================================================
// COLLISION DETECTION (AABB)
// ======================================================

bool checkCollision(
    float x1, float y1,
    float w1, float h1,
    float x2, float y2,
    float w2, float h2)
{
    return (x1 - w1 / 2.0f < x2 + w2 / 2.0f) &&
           (x1 + w1 / 2.0f > x2 - w2 / 2.0f) &&
           (y1 < y2 + h2) &&
           (y1 + h1 > y2);
}

// ======================================================
// ENEMY & BOSS SPAWNING
// ======================================================

void resetEnemy(int i)
{
    enemies[i].x = 50.0f + rand() % (WIDTH - 100);
    enemies[i].y = (float)HEIGHT + 50.0f + (rand() % 350);

    float baseSpeed = 1.3f + currentSector * 0.4f;
    if (baseSpeed > 4.8f) baseSpeed = 4.8f;

    enemies[i].speed = baseSpeed + (rand() % 20) / 10.0f;
    enemies[i].wobbleOffset = (float)(rand() % 100);
    enemies[i].shootTimer = 55 + rand() % 110;

    int typeRoll = rand() % 10;
    if (currentSector >= 2 && typeRoll > 6)
        enemies[i].type = 2; // Heavy Raider
    else if (currentSector >= 1 && typeRoll > 3)
        enemies[i].type = 1; // Interceptor
    else
        enemies[i].type = 0; // Scout

    if (enemies[i].type == 1)
    {
        enemies[i].speed += 0.8f;
        enemies[i].shootTimer = 40 + rand() % 70;
        enemies[i].hp = 1;
    }
    else if (enemies[i].type == 2)
    {
        enemies[i].speed *= 0.85f;
        enemies[i].shootTimer = 50 + rand() % 60;
        enemies[i].hp = 2;
    }
    else
    {
        enemies[i].hp = 1;
    }

    enemies[i].active = true;
}

void spawnBoss()
{
    currentBoss.active = true;
    currentBoss.width = 160.0f;
    currentBoss.height = 70.0f;
    currentBoss.x = WIDTH / 2.0f;
    currentBoss.y = HEIGHT + 100.0f;
    currentBoss.targetY = HEIGHT - 140.0f;
    currentBoss.vx = 2.2f;

    int sector = currentSector % NUM_SECTORS;
    currentBoss.maxHp = 180 + sector * 120;
    currentBoss.hp = currentBoss.maxHp;
    currentBoss.shootTimer = 40;
    currentBoss.specialAttackTimer = 160;
    currentBoss.phase = 0;

    const char* bossNames[5] = {
        "CRIMSON DREAD-SCOUT",
        "SOLAR DESTROYER HELIOS",
        "VENOM HIVE CARRIER",
        "QUANTUM SHADOW FLAGSHIP",
        "OMEGA MOTHERSHIP PRIME"
    };
    currentBoss.name = bossNames[sector];

    playSfx(SFX_ALARM, 1.0f);
    addFloatingText(WIDTH / 2.0f - 110.0f, HEIGHT / 2.0f, "WARNING: CAPITAL SHIP INBOUND!", 1.0f, 0.2f, 0.2f);
}

// Next Sector Warp Transition
void nextSector()
{
    currentSector++;
    sectorThemeIndex = currentSector % NUM_SECTORS;
    currentMusicTempo = sectorThemes[sectorThemeIndex].tempo;
    sectorAnnouncementTimer = 150; // 2.5 seconds banner

    playSfx(SFX_LEVEL_UP, 1.0f);
    triggerShake(8.0f);

    // Free power-up reward on clearing sector!
    shieldHealth = std::max(shieldHealth, 2);
    if (empBombsStock < 3) empBombsStock++;

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        resetEnemy(i);
        enemies[i].y += i * 80.0f;
    }
}

// ======================================================
// INITIALIZE GAME
// ======================================================

void initializeGame()
{
    playerX = WIDTH / 2.0f;
    playerY = 60.0f;

    score = 0;
    lives = 3;
    wave = 1;
    currentSector = 0;
    sectorThemeIndex = 0;
    currentMusicTempo = sectorThemes[0].tempo;
    sectorAnnouncementTimer = 120;

    comboCount = 0;
    comboTimer = 0;

    gameOver = false;
    paused = false;

    invulnerableTimer = 0;
    animFrame = 0;
    screenShake = 0.0f;

    shieldHealth = 0;
    tripleShotTimer = 0;
    pentaShotTimer = 0;
    rapidFireTimer = 0;
    speedBoostTimer = 0;
    timeSlowTimer = 0;
    empBombsStock = 1;

    spacePressed = false;
    shootCooldown = 0;
    cannonAlternator = 0;

    leftPressed = false;
    rightPressed = false;
    upPressed = false;
    downPressed = false;

    currentBoss.active = false;

    for (int i = 0; i < MAX_BULLETS; i++)
    {
        bullets[i].active = false;
    }

    initParticles();
    initFloatingTexts();
    initPowerUps();
    initStars();

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        resetEnemy(i);
        enemies[i].y += i * 85.0f;
    }
}

// ======================================================
// UPDATE GAME LOOP
// ======================================================

void update(int value)
{
    if (currentGameState == STATE_PLAYING && !gameOver && !paused)
    {
        animFrame++;

        // Timers & Cooldowns
        if (invulnerableTimer > 0) invulnerableTimer--;
        if (tripleShotTimer > 0)   tripleShotTimer--;
        if (pentaShotTimer > 0)    pentaShotTimer--;
        if (rapidFireTimer > 0)    rapidFireTimer--;
        if (speedBoostTimer > 0)   speedBoostTimer--;
        if (timeSlowTimer > 0)     timeSlowTimer--;
        if (shootCooldown > 0)     shootCooldown--;
        if (sectorAnnouncementTimer > 0) sectorAnnouncementTimer--;

        // Combo decay
        if (comboTimer > 0)
        {
            comboTimer--;
            if (comboTimer <= 0)
            {
                comboCount = 0;
            }
        }

        // Continuous Firing Handling
        if (spacePressed && shootCooldown <= 0)
        {
            shoot();
            // Cadence: Rapid = 5 frames, Normal = 10 frames
            shootCooldown = (rapidFireTimer > 0) ? 5 : 10;
        }

        // Background & Particles
        updateStars();
        updateParticles();
        updateFloatingTexts();
        updatePowerUps();

        // Player Movement with Speed Boost multiplier
        float currentSpeed = BASE_PLAYER_SPEED * (speedBoostTimer > 0 ? 1.6f : 1.0f);
        if (leftPressed)  playerX -= currentSpeed;
        if (rightPressed) playerX += currentSpeed;

        if (playerX < PLAYER_WIDTH / 2.0f)
            playerX = PLAYER_WIDTH / 2.0f;
        if (playerX > WIDTH - PLAYER_WIDTH / 2.0f)
            playerX = WIDTH - PLAYER_WIDTH / 2.0f;

        // Bullets Movement
        float enemySpeedScale = (timeSlowTimer > 0) ? 0.45f : 1.0f;

        for (int i = 0; i < MAX_BULLETS; i++)
        {
            if (bullets[i].active)
            {
                float bulletScale = bullets[i].isEnemy ? enemySpeedScale : 1.0f;
                bullets[i].x += bullets[i].vx * bulletScale;
                bullets[i].y += bullets[i].vy * bulletScale;

                if (bullets[i].y > HEIGHT + 25.0f || bullets[i].y < -25.0f ||
                    bullets[i].x < -25.0f || bullets[i].x > WIDTH + 25.0f)
                {
                    bullets[i].active = false;
                }
            }
        }

        // Check if sector threshold reached to spawn Boss
        SectorTheme &thm = sectorThemes[currentSector % NUM_SECTORS];
        if (!currentBoss.active && score >= thm.targetScore)
        {
            spawnBoss();
        }

        // --- Boss Update ---
        if (currentBoss.active)
        {
            if (currentBoss.y > currentBoss.targetY)
            {
                currentBoss.y -= 1.5f;
            }
            else
            {
                currentBoss.x += currentBoss.vx * enemySpeedScale;
                if (currentBoss.x > WIDTH - currentBoss.width / 2.0f || currentBoss.x < currentBoss.width / 2.0f)
                {
                    currentBoss.vx = -currentBoss.vx;
                }

                currentBoss.shootTimer--;
                if (currentBoss.shootTimer <= 0)
                {
                    // Fire multi-cannons
                    spawnEnemyBullet(currentBoss.x - 40.0f, currentBoss.y - 20.0f, -1.5f, -6.0f, 2);
                    spawnEnemyBullet(currentBoss.x, currentBoss.y - 30.0f, 0.0f, -6.5f, 2);
                    spawnEnemyBullet(currentBoss.x + 40.0f, currentBoss.y - 20.0f, 1.5f, -6.0f, 2);

                    playSfx(SFX_LASER_ENEMY, 0.7f);
                    currentBoss.shootTimer = (timeSlowTimer > 0) ? 70 : 35;
                }

                currentBoss.specialAttackTimer--;
                if (currentBoss.specialAttackTimer <= 0)
                {
                    // Radial burst attack
                    for (int a = 0; a < 8; a++)
                    {
                        float angle = a * 3.14159265f / 4.0f;
                        spawnEnemyBullet(currentBoss.x, currentBoss.y, cosf(angle) * 3.5f, sinf(angle) * 3.5f - 2.0f, 2);
                    }
                    playSfx(SFX_LASER_ENEMY, 0.9f);
                    currentBoss.specialAttackTimer = (timeSlowTimer > 0) ? 240 : 130;
                }
            }
        }

        // --- Move Enemies & Enemy Shooting ---
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (!enemies[i].active)
                continue;

            enemies[i].y -= enemies[i].speed * enemySpeedScale;
            enemies[i].x += sinf(enemies[i].y * 0.035f + enemies[i].wobbleOffset) * 1.25f * enemySpeedScale;

            if (enemies[i].x < ENEMY_WIDTH / 2.0f)
                enemies[i].x = ENEMY_WIDTH / 2.0f;
            if (enemies[i].x > WIDTH - ENEMY_WIDTH / 2.0f)
                enemies[i].x = WIDTH - ENEMY_WIDTH / 2.0f;

            // Enemy Shooting
            enemies[i].shootTimer--;
            if (enemies[i].shootTimer <= 0 && enemies[i].y < HEIGHT - 40.0f && enemies[i].y > 150.0f)
            {
                spawnEnemyBullet(enemies[i].x, enemies[i].y);
                playSfx(SFX_LASER_ENEMY, 0.35f);
                enemies[i].shootTimer = (enemies[i].type == 1) ? (50 + rand() % 60) : (80 + rand() % 80);
                if (timeSlowTimer > 0) enemies[i].shootTimer *= 2;
            }

            // Enemy reaches bottom defense line
            if (enemies[i].y < 0)
            {
                lives--;
                triggerShake(6.0f);
                playSfx(SFX_EXPLOSION_LARGE, 0.8f);
                spawnExplosion(enemies[i].x, 15.0f, 1.0f, 0.2f, 0.1f, 20);
                addFloatingText(enemies[i].x, 25.0f, "-1 HULL INTEGRITY", 1.0f, 0.2f, 0.2f);

                resetEnemy(i);

                if (lives <= 0)
                {
                    gameOver = true;
                    currentGameState = STATE_GAME_OVER;
                    playSfx(SFX_GAME_OVER, 1.0f);
                    if (score > highScore) highScore = score;
                }
            }
        }

        // --- Bullet - Enemy Collisions ---
        for (int i = 0; i < MAX_BULLETS; i++)
        {
            if (!bullets[i].active || bullets[i].isEnemy)
                continue;

            // Check Boss Collision
            if (currentBoss.active && checkCollision(
                bullets[i].x, bullets[i].y, BULLET_WIDTH, BULLET_HEIGHT,
                currentBoss.x, currentBoss.y, currentBoss.width, currentBoss.height))
            {
                bullets[i].active = false;
                currentBoss.hp--;
                spawnExplosion(bullets[i].x, bullets[i].y, 1.0f, 0.8f, 0.2f, 6);

                if (currentBoss.hp <= 0)
                {
                    currentBoss.active = false;
                    score += 250;
                    if (score > highScore) highScore = score;

                    playSfx(SFX_EXPLOSION_LARGE, 1.0f);
                    triggerShake(14.0f);
                    spawnExplosion(currentBoss.x, currentBoss.y, 1.0f, 0.4f, 0.1f, 60, 2.0f);
                    addFloatingText(currentBoss.x, currentBoss.y, "BOSS DESTROYED! +250 PTS", 1.0f, 0.9f, 0.2f);

                    // Spawn guaranteed powerups
                    spawnPowerUp(currentBoss.x - 30.0f, currentBoss.y);
                    spawnPowerUp(currentBoss.x + 30.0f, currentBoss.y);

                    // Transition to Next Sector!
                    nextSector();
                }
                continue;
            }

            // Regular Enemies Collision
            for (int j = 0; j < MAX_ENEMIES; j++)
            {
                if (!enemies[j].active)
                    continue;

                if (checkCollision(
                    bullets[i].x, bullets[i].y, BULLET_WIDTH, BULLET_HEIGHT,
                    enemies[j].x, enemies[j].y, ENEMY_WIDTH, ENEMY_HEIGHT))
                {
                    bullets[i].active = false;
                    enemies[j].hp--;

                    if (enemies[j].hp <= 0)
                    {
                        enemies[j].active = false;

                        // Combo multiplier
                        comboCount++;
                        if (comboCount > 5) comboCount = 5;
                        comboTimer = 130; // ~2 seconds to keep combo alive

                        int basePoints = (enemies[j].type == 2) ? 35 : (enemies[j].type == 1) ? 25 : 10;
                        int pointAward = basePoints * comboCount;
                        score += pointAward;
                        if (score > highScore) highScore = score;

                        char ptsStr[24];
                        if (comboCount > 1)
                            sprintf(ptsStr, "+%d (x%d)", pointAward, comboCount);
                        else
                            sprintf(ptsStr, "+%d", pointAward);

                        addFloatingText(enemies[j].x, enemies[j].y + 15.0f, ptsStr, 1.0f, 0.9f, 0.2f);
                        triggerShake(enemies[j].type == 2 ? 5.0f : enemies[j].type == 1 ? 3.5f : 2.0f);
                        playSfx(enemies[j].type == 2 ? SFX_EXPLOSION_LARGE : SFX_EXPLOSION_SMALL, 0.7f);

                        // Visual Explosion
                        if (enemies[j].type == 2)
                            spawnExplosion(enemies[j].x, enemies[j].y + ENEMY_HEIGHT / 2.0f, 1.0f, 0.5f, 0.1f, 32);
                        else if (enemies[j].type == 1)
                            spawnExplosion(enemies[j].x, enemies[j].y + ENEMY_HEIGHT / 2.0f, 0.8f, 0.2f, 1.0f, 26);
                        else
                            spawnExplosion(enemies[j].x, enemies[j].y + ENEMY_HEIGHT / 2.0f, 1.0f, 0.35f, 0.15f, 20);

                        // Power-up Drop Chance
                        spawnPowerUp(enemies[j].x, enemies[j].y);

                        resetEnemy(j);
                    }
                    else
                    {
                        // Armored hit spark
                        spawnExplosion(bullets[i].x, bullets[i].y, 1.0f, 0.9f, 0.4f, 8);
                    }
                    break;
                }
            }
        }

        // --- Enemy Bullet - Player Collisions ---
        if (invulnerableTimer <= 0)
        {
            for (int i = 0; i < MAX_BULLETS; i++)
            {
                if (!bullets[i].active || !bullets[i].isEnemy)
                    continue;

                if (checkCollision(
                    bullets[i].x, bullets[i].y, 8.0f, 8.0f,
                    playerX, playerY, PLAYER_WIDTH * 0.7f, PLAYER_HEIGHT * 0.8f))
                {
                    bullets[i].active = false;

                    if (shieldHealth > 0)
                    {
                        shieldHealth--;
                        triggerShake(4.5f);
                        playSfx(SFX_SHIELD_HIT, 0.85f);
                        spawnExplosion(playerX, playerY + PLAYER_HEIGHT / 2.0f, 0.2f, 0.85f, 1.0f, 18);
                        addFloatingText(playerX, playerY + 50.0f, "SHIELD HIT!", 0.2f, 0.85f, 1.0f);
                    }
                    else
                    {
                        lives--;
                        comboCount = 0;
                        triggerShake(9.0f);
                        playSfx(SFX_EXPLOSION_LARGE, 0.9f);
                        spawnExplosion(playerX, playerY + PLAYER_HEIGHT / 2.0f, 1.0f, 0.3f, 0.1f, 32);
                        addFloatingText(playerX, playerY + 50.0f, "-1 HULL INTEGRITY", 1.0f, 0.2f, 0.2f);
                        invulnerableTimer = 90;

                        if (lives <= 0)
                        {
                            gameOver = true;
                            currentGameState = STATE_GAME_OVER;
                            playSfx(SFX_GAME_OVER, 1.0f);
                            if (score > highScore) highScore = score;
                        }
                    }
                    break;
                }
            }
        }

        // --- Player - Enemy Collisions ---
        if (invulnerableTimer <= 0)
        {
            for (int j = 0; j < MAX_ENEMIES; j++)
            {
                if (!enemies[j].active)
                    continue;

                if (checkCollision(
                    playerX, playerY, PLAYER_WIDTH * 0.75f, PLAYER_HEIGHT * 0.85f,
                    enemies[j].x, enemies[j].y, ENEMY_WIDTH * 0.85f, ENEMY_HEIGHT * 0.85f))
                {
                    if (shieldHealth > 0)
                    {
                        shieldHealth--;
                        triggerShake(6.0f);
                        playSfx(SFX_SHIELD_HIT, 0.9f);
                        spawnExplosion(playerX, playerY + PLAYER_HEIGHT / 2.0f, 0.2f, 0.85f, 1.0f, 25);
                        spawnExplosion(enemies[j].x, enemies[j].y + ENEMY_HEIGHT / 2.0f, 1.0f, 0.2f, 0.1f, 15);
                        addFloatingText(playerX, playerY + 50.0f, "SHIELD DEFLECT!", 0.2f, 0.85f, 1.0f);
                    }
                    else
                    {
                        lives--;
                        comboCount = 0;
                        triggerShake(11.0f);
                        playSfx(SFX_EXPLOSION_LARGE, 1.0f);
                        spawnExplosion(playerX, playerY + PLAYER_HEIGHT / 2.0f, 1.0f, 0.8f, 0.1f, 38);
                        spawnExplosion(enemies[j].x, enemies[j].y + ENEMY_HEIGHT / 2.0f, 1.0f, 0.2f, 0.1f, 22);
                        addFloatingText(playerX, playerY + 50.0f, "-1 HULL INTEGRITY", 1.0f, 0.2f, 0.2f);
                        invulnerableTimer = 100;

                        if (lives <= 0)
                        {
                            gameOver = true;
                            currentGameState = STATE_GAME_OVER;
                            playSfx(SFX_GAME_OVER, 1.0f);
                            if (score > highScore) highScore = score;
                        }
                    }

                    resetEnemy(j);
                    break;
                }
            }
        }

        // --- Player - Power-Up Collectibles ---
        for (int i = 0; i < MAX_POWERUPS; i++)
        {
            if (!powerUps[i].active)
                continue;

            if (checkCollision(
                playerX, playerY, PLAYER_WIDTH, PLAYER_HEIGHT,
                powerUps[i].x, powerUps[i].y, POWERUP_SIZE, POWERUP_SIZE))
            {
                powerUps[i].active = false;
                triggerShake(2.5f);
                playSfx(SFX_POWERUP, 0.85f);
                spawnExplosion(powerUps[i].x, powerUps[i].y, 1.0f, 1.0f, 1.0f, 20, 0.8f);

                switch (powerUps[i].type)
                {
                    case PU_SHIELD:
                        shieldHealth = 3;
                        addFloatingText(playerX, playerY + 50.0f, "SHIELD ONLINE! (3 HITS)", 0.1f, 0.85f, 1.0f);
                        break;
                    case PU_TRIPLE_SHOT:
                        tripleShotTimer = 660; // 11s
                        addFloatingText(playerX, playerY + 50.0f, "TRIPLE CANNONS!", 1.0f, 0.85f, 0.1f);
                        break;
                    case PU_PENTA_SHOT:
                        pentaShotTimer = 600; // 10s
                        addFloatingText(playerX, playerY + 50.0f, "PENTA NOVA CANNONS!", 0.95f, 0.2f, 1.0f);
                        break;
                    case PU_RAPID_FIRE:
                        rapidFireTimer = 600; // 10s
                        addFloatingText(playerX, playerY + 50.0f, "RAPID OVERCHARGE!", 1.0f, 0.3f, 0.3f);
                        break;
                    case PU_SPEED_BOOST:
                        speedBoostTimer = 720; // 12s
                        playSfx(SFX_SPEED_BOOST, 0.9f);
                        addFloatingText(playerX, playerY + 50.0f, "HYPER-THRUSTER SPEED!", 0.0f, 1.0f, 0.85f);
                        break;
                    case PU_EMP_BOMB:
                        empBombsStock++;
                        addFloatingText(playerX, playerY + 50.0f, "+1 EMP BOMB [PRESS B]!", 0.4f, 0.6f, 1.0f);
                        break;
                    case PU_TIME_SLOW:
                        timeSlowTimer = 480; // 8s
                        addFloatingText(playerX, playerY + 50.0f, "CHRONO-WARP ENGAGED!", 0.3f, 0.7f, 1.0f);
                        break;
                    case PU_EXTRA_LIFE:
                        if (lives < 5) lives++;
                        addFloatingText(playerX, playerY + 50.0f, "+1 EXTRA LIFE!", 0.2f, 1.0f, 0.3f);
                        break;
                    default:
                        break;
                }
            }
        }
    }
    else
    {
        // Update background & particles even while paused / menus
        animFrame++;
        updateStars();
        updateParticles();
        updateFloatingTexts();
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0); // ~60 FPS
}

// ======================================================
// DISPLAY CALLBACK
// ======================================================

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // Dynamic Sector Background Gradient and Parallax Starfield
    drawBackgroundAndStars();

    // Camera Screen Shake
    glPushMatrix();

    if (screenShake > 0.01f)
    {
        float shakeOffsetX = ((rand() % 100) / 50.0f - 1.0f) * screenShake;
        float shakeOffsetY = ((rand() % 100) / 50.0f - 1.0f) * screenShake;
        glTranslatef(shakeOffsetX, shakeOffsetY, 0.0f);
        screenShake *= 0.88f; // Shake decay
    }

    if (currentGameState == STATE_PLAYING || currentGameState == STATE_PAUSED)
    {
        // Power-ups on screen
        drawPowerUps();

        // Particles
        drawParticles();

        // Player
        if (!gameOver)
        {
            drawPlayer();
        }

        // Bullets
        for (int i = 0; i < MAX_BULLETS; i++)
        {
            drawBullet(bullets[i]);
        }

        // Enemies
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            drawEnemy(enemies[i]);
        }

        // Boss
        drawBoss(currentBoss);

        // Floating texts
        drawFloatingTexts();
    }
    else
    {
        drawParticles();
    }

    glPopMatrix();

    // HUD and Overlay Screens
    if (currentGameState == STATE_PLAYING)
    {
        drawHUD();
    }
    else if (currentGameState == STATE_MAIN_MENU)
    {
        drawMainMenu();
    }
    else if (currentGameState == STATE_PAUSED)
    {
        drawHUD();
        drawPauseMenu();
    }
    else if (currentGameState == STATE_SETTINGS)
    {
        drawSettingsMenu();
    }
    else if (currentGameState == STATE_HOW_TO_PLAY)
    {
        drawHowToPlay();
    }
    else if (currentGameState == STATE_GAME_OVER)
    {
        drawHUD();
        drawGameOver();
    }

    glutSwapBuffers();
}

// ======================================================
// INPUT HANDLING: KEYBOARD & MOUSE
// ======================================================

void keyboardDown(unsigned char key, int x, int y)
{
    switch (key)
    {
        case 'a':
        case 'A':
            leftPressed = true;
            if (currentGameState == STATE_SETTINGS)
            {
                // Adjust volume left
                if (menuSelectedIdx == 0) { masterVolume = std::max(0.0f, masterVolume - 0.05f); playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 1) { sfxVolume = std::max(0.0f, sfxVolume - 0.05f); playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 2) { musicVolume = std::max(0.0f, musicVolume - 0.05f); playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 3) { screenShakeSetting = (screenShakeSetting + SHAKE_COUNT - 1) % SHAKE_COUNT; playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 4) { shipAccentSetting = (shipAccentSetting + ACCENT_COUNT - 1) % ACCENT_COUNT; playSfx(SFX_UI_CLICK, 0.5f); }
            }
            break;

        case 'd':
        case 'D':
            rightPressed = true;
            if (currentGameState == STATE_SETTINGS)
            {
                // Adjust volume right
                if (menuSelectedIdx == 0) { masterVolume = std::min(1.0f, masterVolume + 0.05f); playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 1) { sfxVolume = std::min(1.0f, sfxVolume + 0.05f); playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 2) { musicVolume = std::min(1.0f, musicVolume + 0.05f); playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 3) { screenShakeSetting = (screenShakeSetting + 1) % SHAKE_COUNT; playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 4) { shipAccentSetting = (shipAccentSetting + 1) % ACCENT_COUNT; playSfx(SFX_UI_CLICK, 0.5f); }
            }
            break;

        case 'w':
        case 'W':
            upPressed = true;
            playSfx(SFX_UI_CLICK, 0.5f);
            menuSelectedIdx = std::max(0, menuSelectedIdx - 1);
            break;

        case 's':
        case 'S':
            downPressed = true;
            playSfx(SFX_UI_CLICK, 0.5f);
            if (currentGameState == STATE_MAIN_MENU) menuSelectedIdx = std::min(3, menuSelectedIdx + 1);
            else if (currentGameState == STATE_PAUSED) menuSelectedIdx = std::min(3, menuSelectedIdx + 1);
            else if (currentGameState == STATE_SETTINGS) menuSelectedIdx = std::min(5, menuSelectedIdx + 1);
            else if (currentGameState == STATE_GAME_OVER) menuSelectedIdx = std::min(1, menuSelectedIdx + 1);
            break;

        case 'b':
        case 'B':
            if (currentGameState == STATE_PLAYING && !gameOver && !paused)
            {
                triggerEmpBomb();
            }
            break;

        case ' ': // Spacebar
        case 13:  // Enter Key
            if (currentGameState == STATE_PLAYING)
            {
                spacePressed = true;
            }
            else if (currentGameState == STATE_MAIN_MENU)
            {
                playSfx(SFX_UI_CLICK, 0.8f);
                if (menuSelectedIdx == 0)
                {
                    initializeGame();
                    currentGameState = STATE_PLAYING;
                }
                else if (menuSelectedIdx == 1)
                {
                    previousState = STATE_MAIN_MENU;
                    menuSelectedIdx = 0;
                    currentGameState = STATE_SETTINGS;
                }
                else if (menuSelectedIdx == 2)
                {
                    currentGameState = STATE_HOW_TO_PLAY;
                }
                else if (menuSelectedIdx == 3)
                {
                    shutdownAudio();
                    exit(0);
                }
            }
            else if (currentGameState == STATE_PAUSED)
            {
                playSfx(SFX_UI_CLICK, 0.8f);
                if (menuSelectedIdx == 0) currentGameState = STATE_PLAYING;
                else if (menuSelectedIdx == 1) { initializeGame(); currentGameState = STATE_PLAYING; }
                else if (menuSelectedIdx == 2) { previousState = STATE_PAUSED; menuSelectedIdx = 0; currentGameState = STATE_SETTINGS; }
                else if (menuSelectedIdx == 3) { currentGameState = STATE_MAIN_MENU; menuSelectedIdx = 0; }
            }
            else if (currentGameState == STATE_SETTINGS)
            {
                playSfx(SFX_UI_CLICK, 0.8f);
                if (menuSelectedIdx == 3) screenShakeSetting = (screenShakeSetting + 1) % SHAKE_COUNT;
                else if (menuSelectedIdx == 4) shipAccentSetting = (shipAccentSetting + 1) % ACCENT_COUNT;
                else if (menuSelectedIdx == 5) { currentGameState = previousState; menuSelectedIdx = 0; }
            }
            else if (currentGameState == STATE_HOW_TO_PLAY)
            {
                playSfx(SFX_UI_CLICK, 0.8f);
                currentGameState = STATE_MAIN_MENU;
                menuSelectedIdx = 0;
            }
            else if (currentGameState == STATE_GAME_OVER)
            {
                playSfx(SFX_UI_CLICK, 0.8f);
                if (menuSelectedIdx == 0) { initializeGame(); currentGameState = STATE_PLAYING; }
                else if (menuSelectedIdx == 1) { currentGameState = STATE_MAIN_MENU; menuSelectedIdx = 0; }
            }
            break;

        case 'p':
        case 'P':
            if (currentGameState == STATE_PLAYING)
            {
                currentGameState = STATE_PAUSED;
                menuSelectedIdx = 0;
                playSfx(SFX_UI_CLICK, 0.6f);
            }
            else if (currentGameState == STATE_PAUSED)
            {
                currentGameState = STATE_PLAYING;
                playSfx(SFX_UI_CLICK, 0.6f);
            }
            break;

        case 'r':
        case 'R':
            if (currentGameState == STATE_PLAYING || currentGameState == STATE_PAUSED || currentGameState == STATE_GAME_OVER)
            {
                initializeGame();
                currentGameState = STATE_PLAYING;
            }
            break;

        case 27: // ESC key
            if (currentGameState == STATE_PLAYING)
            {
                currentGameState = STATE_PAUSED;
                menuSelectedIdx = 0;
            }
            else if (currentGameState == STATE_PAUSED)
            {
                currentGameState = STATE_PLAYING;
            }
            else if (currentGameState == STATE_SETTINGS)
            {
                currentGameState = previousState;
                menuSelectedIdx = 0;
            }
            else if (currentGameState == STATE_HOW_TO_PLAY)
            {
                currentGameState = STATE_MAIN_MENU;
                menuSelectedIdx = 0;
            }
            else if (currentGameState == STATE_MAIN_MENU)
            {
                shutdownAudio();
                exit(0);
            }
            break;
    }
}

void keyboardUp(unsigned char key, int x, int y)
{
    switch (key)
    {
        case 'a':
        case 'A':
            leftPressed = false;
            break;

        case 'd':
        case 'D':
            rightPressed = false;
            break;

        case 'w':
        case 'W':
            upPressed = false;
            break;

        case 's':
        case 'S':
            downPressed = false;
            break;

        case ' ':
            spacePressed = false;
            break;
    }
}

void specialKeyDown(int key, int x, int y)
{
    switch (key)
    {
        case GLUT_KEY_LEFT:
            leftPressed = true;
            if (currentGameState == STATE_SETTINGS)
            {
                if (menuSelectedIdx == 0) { masterVolume = std::max(0.0f, masterVolume - 0.05f); playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 1) { sfxVolume = std::max(0.0f, sfxVolume - 0.05f); playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 2) { musicVolume = std::max(0.0f, musicVolume - 0.05f); playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 3) { screenShakeSetting = (screenShakeSetting + SHAKE_COUNT - 1) % SHAKE_COUNT; playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 4) { shipAccentSetting = (shipAccentSetting + ACCENT_COUNT - 1) % ACCENT_COUNT; playSfx(SFX_UI_CLICK, 0.5f); }
            }
            break;

        case GLUT_KEY_RIGHT:
            rightPressed = true;
            if (currentGameState == STATE_SETTINGS)
            {
                if (menuSelectedIdx == 0) { masterVolume = std::min(1.0f, masterVolume + 0.05f); playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 1) { sfxVolume = std::min(1.0f, sfxVolume + 0.05f); playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 2) { musicVolume = std::min(1.0f, musicVolume + 0.05f); playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 3) { screenShakeSetting = (screenShakeSetting + 1) % SHAKE_COUNT; playSfx(SFX_UI_CLICK, 0.5f); }
                else if (menuSelectedIdx == 4) { shipAccentSetting = (shipAccentSetting + 1) % ACCENT_COUNT; playSfx(SFX_UI_CLICK, 0.5f); }
            }
            break;

        case GLUT_KEY_UP:
            upPressed = true;
            playSfx(SFX_UI_CLICK, 0.5f);
            menuSelectedIdx = std::max(0, menuSelectedIdx - 1);
            break;

        case GLUT_KEY_DOWN:
            downPressed = true;
            playSfx(SFX_UI_CLICK, 0.5f);
            if (currentGameState == STATE_MAIN_MENU) menuSelectedIdx = std::min(3, menuSelectedIdx + 1);
            else if (currentGameState == STATE_PAUSED) menuSelectedIdx = std::min(3, menuSelectedIdx + 1);
            else if (currentGameState == STATE_SETTINGS) menuSelectedIdx = std::min(5, menuSelectedIdx + 1);
            else if (currentGameState == STATE_GAME_OVER) menuSelectedIdx = std::min(1, menuSelectedIdx + 1);
            break;
    }
}

void specialKeyUp(int key, int x, int y)
{
    switch (key)
    {
        case GLUT_KEY_LEFT:
            leftPressed = false;
            break;

        case GLUT_KEY_RIGHT:
            rightPressed = false;
            break;

        case GLUT_KEY_UP:
            upPressed = false;
            break;

        case GLUT_KEY_DOWN:
            downPressed = false;
            break;
    }
}

// Mouse Click Interaction
void mouseClick(int button, int state, int mouseX, int mouseY)
{
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN)
        return;

    // Convert mouse coordinates to OpenGL Ortho (Y flipped)
    float glX = (float)mouseX;
    float glY = (float)(HEIGHT - mouseY);

    if (currentGameState == STATE_MAIN_MENU)
    {
        float bw = 240.0f;
        float bh = 44.0f;
        float bx = WIDTH / 2.0f - bw / 2.0f;
        float startY = HEIGHT - 240.0f;
        float gap = 58.0f;

        for (int i = 0; i < 4; i++)
        {
            float btnY = startY - gap * i;
            if (glX >= bx && glX <= bx + bw && glY >= btnY && glY <= btnY + bh)
            {
                menuSelectedIdx = i;
                playSfx(SFX_UI_CLICK, 0.8f);
                if (i == 0) { initializeGame(); currentGameState = STATE_PLAYING; }
                else if (i == 1) { previousState = STATE_MAIN_MENU; menuSelectedIdx = 0; currentGameState = STATE_SETTINGS; }
                else if (i == 2) { currentGameState = STATE_HOW_TO_PLAY; }
                else if (i == 3) { shutdownAudio(); exit(0); }
                break;
            }
        }
    }
    else if (currentGameState == STATE_PAUSED)
    {
        float bw = 240.0f;
        float bh = 44.0f;
        float bx = WIDTH / 2.0f - bw / 2.0f;
        float startY = HEIGHT - 210.0f;
        float gap = 55.0f;

        for (int i = 0; i < 4; i++)
        {
            float btnY = startY - gap * i;
            if (glX >= bx && glX <= bx + bw && glY >= btnY && glY <= btnY + bh)
            {
                menuSelectedIdx = i;
                playSfx(SFX_UI_CLICK, 0.8f);
                if (i == 0) currentGameState = STATE_PLAYING;
                else if (i == 1) { initializeGame(); currentGameState = STATE_PLAYING; }
                else if (i == 2) { previousState = STATE_PAUSED; menuSelectedIdx = 0; currentGameState = STATE_SETTINGS; }
                else if (i == 3) { currentGameState = STATE_MAIN_MENU; menuSelectedIdx = 0; }
                break;
            }
        }
    }
    else if (currentGameState == STATE_SETTINGS)
    {
        float sw = 420.0f;
        float sh = 38.0f;
        float sx = WIDTH / 2.0f - sw / 2.0f;
        float startY = HEIGHT - 150.0f;
        float gap = 50.0f;

        // Sliders 0, 1, 2
        for (int i = 0; i < 3; i++)
        {
            float sY = startY - gap * i;
            if (glX >= sx && glX <= sx + sw && glY >= sY && glY <= sY + sh)
            {
                menuSelectedIdx = i;
                float trackX = sx + 160.0f;
                float trackW = sw - 180.0f;
                float ratio = (glX - trackX) / trackW;
                if (ratio < 0.0f) ratio = 0.0f;
                if (ratio > 1.0f) ratio = 1.0f;

                if (i == 0) masterVolume = ratio;
                else if (i == 1) sfxVolume = ratio;
                else if (i == 2) musicVolume = ratio;

                playSfx(SFX_UI_CLICK, 0.7f);
                return;
            }
        }

        // Screen Shake Toggle
        float shakeY = startY - gap * 3;
        if (glX >= sx && glX <= sx + sw && glY >= shakeY && glY <= shakeY + sh)
        {
            menuSelectedIdx = 3;
            screenShakeSetting = (screenShakeSetting + 1) % SHAKE_COUNT;
            playSfx(SFX_UI_CLICK, 0.7f);
            return;
        }

        // Ship Accent Toggle
        float shipY = startY - gap * 4;
        if (glX >= sx && glX <= sx + sw && glY >= shipY && glY <= shipY + sh)
        {
            menuSelectedIdx = 4;
            shipAccentSetting = (shipAccentSetting + 1) % ACCENT_COUNT;
            playSfx(SFX_UI_CLICK, 0.7f);
            return;
        }

        // Back Button
        float backY = startY - gap * 5.2f;
        if (glX >= sx && glX <= sx + sw && glY >= backY && glY <= backY + sh)
        {
            menuSelectedIdx = 5;
            playSfx(SFX_UI_CLICK, 0.8f);
            currentGameState = previousState;
            menuSelectedIdx = 0;
            return;
        }
    }
    else if (currentGameState == STATE_HOW_TO_PLAY)
    {
        float bw = 200.0f;
        float bh = 38.0f;
        float bx = WIDTH / 2.0f - bw / 2.0f;
        if (glX >= bx && glX <= bx + bw && glY >= 30.0f && glY <= 30.0f + bh)
        {
            playSfx(SFX_UI_CLICK, 0.8f);
            currentGameState = STATE_MAIN_MENU;
            menuSelectedIdx = 0;
        }
    }
    else if (currentGameState == STATE_GAME_OVER)
    {
        float bw = 240.0f;
        float bh = 42.0f;
        float bx = WIDTH / 2.0f - bw / 2.0f;
        float rY = HEIGHT / 2.0f - 80.0f;
        float mY = HEIGHT / 2.0f - 135.0f;

        if (glX >= bx && glX <= bx + bw && glY >= rY && glY <= rY + bh)
        {
            playSfx(SFX_UI_CLICK, 0.8f);
            initializeGame();
            currentGameState = STATE_PLAYING;
        }
        else if (glX >= bx && glX <= bx + bw && glY >= mY && glY <= mY + bh)
        {
            playSfx(SFX_UI_CLICK, 0.8f);
            currentGameState = STATE_MAIN_MENU;
            menuSelectedIdx = 0;
        }
    }
}

// ======================================================
// WINDOW RESIZE / RESHAPE
// ======================================================

void reshape(int width, int height)
{
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, 0, HEIGHT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// ======================================================
// OPENGL INITIALIZATION
// ======================================================

void init()
{
    glClearColor(0.015f, 0.015f, 0.055f, 1.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, 0, HEIGHT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// ======================================================
// MAIN ENTRY POINT
// ======================================================

int main(int argc, char** argv)
{
    srand((unsigned int)time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutInitWindowPosition(100, 50);
    glutCreateWindow("NEO NOVA: 2D Space Shooter - Enhanced Sector Edition");

    init();
    initAudio();
    initializeGame();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialKeyDown);
    glutSpecialUpFunc(specialKeyUp);
    glutMouseFunc(mouseClick);

    glutTimerFunc(16, update, 0);

    glutMainLoop();

    shutdownAudio();
    return 0;
}