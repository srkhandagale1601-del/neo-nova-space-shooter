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
#include "Graphics/CGLab.hpp"

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
    cgFlash = std::max(cgFlash, intensity * 0.03f);   // impact flash scales with the hit
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
int hiwPage = 0;     // current page of the HOW IT WORKS (code explained) screen
int hintTimer = 0;   // frames the on-screen control hint stays visible after a new game

// Ship physics (inertia) + transform bookkeeping for the CG Lab panel
float shipVx = 0.0f, shipVy = 0.0f;
float shipBankDeg = 0.0f, shipSx = 1.0f, shipSy = 1.0f;
float shipM[6] = {1, 0, 0, 1, 0, 0};   // a b c d tx ty of  M = T(p) R S T(-p)

// 3D meshes (built once, rendered with the software 3D pipeline in CGLab)
Mesh heavyMesh, bossCoreMesh, menuMesh, gemMesh;

// Debug/educational state shared with the CG Lab panel
int lockedEnemy = -1;
float lockedT = 0.0f;
int sweepOc0 = 0, sweepOc1 = 0;

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
    float vx;        // horizontal velocity (drives the banking transform)
    float startY;    // y where the Bezier path starts
    float cx[4];     // x of the 4 Bezier control points (y is evenly spaced)
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

    // --- Transformation pipeline:  M = T(pivot) * R(bank) * S(squash) * T(-pivot)
    float pivotY = playerY + PLAYER_HEIGHT * 0.4f;
    shipBankDeg = std::max(-24.0f, std::min(24.0f, -shipVx * 2.4f));
    shipSx = 1.0f - fabsf(shipVx) * 0.012f;
    shipSy = 1.0f + fabsf(shipVy) * 0.014f;
    {
        float th = shipBankDeg * 3.14159265f / 180.0f;
        float a = cosf(th) * shipSx, b = sinf(th) * shipSx;
        float c = -sinf(th) * shipSy, d = cosf(th) * shipSy;
        shipM[0] = a; shipM[1] = b; shipM[2] = c; shipM[3] = d;
        shipM[4] = playerX - (a * playerX + c * pivotY);
        shipM[5] = pivotY - (b * playerX + d * pivotY);
    }
    glPushMatrix();
    glTranslatef(playerX, pivotY, 0.0f);
    glRotatef(shipBankDeg, 0.0f, 0.0f, 1.0f);
    glScalef(shipSx, shipSy, 1.0f);
    glTranslatef(-playerX, -pivotY, 0.0f);

    // --- Animated Thruster Engine Flame ---
    float flameMult = (speedBoostTimer > 0) ? 1.8f : 1.0f;
    float flameHeight = (12.0f + (rand() % 14)) * flameMult;

    // Hyper-Thruster Warp Particles
    if (speedBoostTimer > 0)
    {
        spawnEngineTrail(playerX - 12.0f, playerY - 4.0f, 0.1f, 0.9f, 1.0f);
        spawnEngineTrail(playerX + 12.0f, playerY - 4.0f, 0.1f, 0.9f, 1.0f);
    }

    // Soft halo behind the ship so it pops from the background

    // Additive engine light
    if (speedBoostTimer > 0) drawGlow(playerX, playerY - 8.0f, 44.0f, 0.1f, 0.8f, 1.0f, 0.55f);
    else drawGlow(playerX, playerY - 8.0f, 36.0f, 1.0f, 0.5f, 0.1f, 0.45f);

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

    const float X = playerX, Y = playerY;
    const float HW = PLAYER_WIDTH / 2.0f;

    // Ground shadow so the plane clearly floats above the scenery
    glColor4f(0.0f, 0.0f, 0.0f, 0.20f);
    drawCircleFill(X + 14.0f, Y + 4.0f, 17.0f, 18);

    // --- Wings: light grey, with accent-coloured tips ---
    glColor3f(0.93f, 0.95f, 0.98f);
    glBegin(GL_POLYGON);
        glVertex2f(X - HW, Y - 2.0f);  glVertex2f(X - 5.0f, Y + 26.0f);
        glVertex2f(X + 5.0f, Y + 26.0f); glVertex2f(X + HW, Y - 2.0f);
        glVertex2f(X + HW - 5.0f, Y - 9.0f); glVertex2f(X - HW + 5.0f, Y - 9.0f);
    glEnd();
    glColor3f(accR, accG, accB);
    glBegin(GL_TRIANGLES);
        glVertex2f(X - HW, Y - 2.0f); glVertex2f(X - HW + 11.0f, Y + 8.0f); glVertex2f(X - HW + 5.0f, Y - 9.0f);
        glVertex2f(X + HW, Y - 2.0f); glVertex2f(X + HW - 11.0f, Y + 8.0f); glVertex2f(X + HW - 5.0f, Y - 9.0f);
    glEnd();

    // --- Tail fins (red) ---
    glColor3f(0.88f, 0.15f, 0.15f);
    glBegin(GL_TRIANGLES);
        glVertex2f(X - 4.0f, Y + 2.0f); glVertex2f(X - 14.0f, Y - 12.0f); glVertex2f(X - 4.0f, Y - 8.0f);
        glVertex2f(X + 4.0f, Y + 2.0f); glVertex2f(X + 14.0f, Y - 12.0f); glVertex2f(X + 4.0f, Y - 8.0f);
    glEnd();

    // --- Fuselage (white) + red nose cone ---
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_POLYGON);
        glVertex2f(X, Y + PLAYER_HEIGHT);
        glVertex2f(X + 7.0f, Y + 34.0f); glVertex2f(X + 8.0f, Y + 6.0f); glVertex2f(X + 5.0f, Y - 8.0f);
        glVertex2f(X - 5.0f, Y - 8.0f);  glVertex2f(X - 8.0f, Y + 6.0f); glVertex2f(X - 7.0f, Y + 34.0f);
    glEnd();
    glColor3f(0.9f, 0.15f, 0.15f);
    glBegin(GL_TRIANGLES);
        glVertex2f(X, Y + PLAYER_HEIGHT); glVertex2f(X + 6.2f, Y + 36.0f); glVertex2f(X - 6.2f, Y + 36.0f);
    glEnd();

    // --- Cockpit window ---
    glColor3f(0.15f, 0.5f, 0.95f);
    drawCircleFill(X, Y + 24.0f, 5.0f, 16);
    glColor3f(0.8f, 0.93f, 1.0f);
    drawCircleFill(X - 1.5f, Y + 25.5f, 2.0f, 10);

    // --- Weapon pods at the wing tips: colour shows the active power-up ---
    if (rapidFireTimer > 0)       glColor3f(1.0f, 0.85f, 0.0f);
    else if (pentaShotTimer > 0)  glColor3f(0.85f, 0.2f, 1.0f);
    else if (tripleShotTimer > 0) glColor3f(0.1f, 0.85f, 0.2f);
    else                          glColor3f(0.35f, 0.4f, 0.5f);
    drawCircleFill(X - HW + 4.0f, Y + 2.0f, 3.5f, 10);
    drawCircleFill(X + HW - 4.0f, Y + 2.0f, 3.5f, 10);

    // --- Dark cartoon outline around wings and fuselage ---
    glColor4f(0.08f, 0.1f, 0.18f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(X - HW, Y - 2.0f);  glVertex2f(X - 5.0f, Y + 26.0f);
        glVertex2f(X + 5.0f, Y + 26.0f); glVertex2f(X + HW, Y - 2.0f);
        glVertex2f(X + HW - 5.0f, Y - 9.0f); glVertex2f(X - HW + 5.0f, Y - 9.0f);
    glEnd();
    glBegin(GL_LINE_LOOP);
        glVertex2f(X, Y + PLAYER_HEIGHT);
        glVertex2f(X + 7.0f, Y + 34.0f); glVertex2f(X + 8.0f, Y + 6.0f); glVertex2f(X + 5.0f, Y - 8.0f);
        glVertex2f(X - 5.0f, Y - 8.0f);  glVertex2f(X - 8.0f, Y + 6.0f); glVertex2f(X - 7.0f, Y + 34.0f);
    glEnd();
    glLineWidth(1.0f);

    // --- Glowing Energy Shield Bubble (if active) ---
    if (shieldHealth > 0)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        float shieldRadius = PLAYER_WIDTH * 0.74f;
        float pulse = 0.5f + 0.35f * sinf(animFrame * 0.16f);

        drawGlow(playerX, playerY + PLAYER_HEIGHT / 2.0f, shieldRadius * 1.1f, 0.1f, 0.7f, 1.0f, 0.16f);
        glColor4f(0.1f, 0.85f, 1.0f, pulse);
        if (cgRaster)
        {
            // Midpoint circle algorithm, plotted pixel by pixel
            glPointSize(3.0f);
            midpointCircle((int)playerX, (int)(playerY + PLAYER_HEIGHT / 2.0f), (int)shieldRadius);
        }
        else
        {
            glLineWidth(2.5f);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 48; i++)
            {
                float theta = i * 2.0f * 3.14159265f / 48.0f;
                glVertex2f(playerX + cosf(theta) * shieldRadius,
                           (playerY + PLAYER_HEIGHT / 2.0f) + sinf(theta) * shieldRadius);
            }
            glEnd();
            glLineWidth(1.0f);
        }

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glPopMatrix();
}

// ======================================================
// DRAW BULLETS
// ======================================================

void drawBullet(const Bullet &b)
{
    if (!b.active)
        return;

    glEnable(GL_POINT_SMOOTH);
    if (b.isEnemy)
    {
        if (b.type == 2) // Boss heavy orb
        {
            drawGlow(b.x, b.y, 24.0f, 1.0f, 0.1f, 0.85f, 0.55f);
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
            drawGlow(b.x, b.y, 17.0f, 1.0f, 0.15f, 0.1f, 0.5f);
            glColor3f(0.1f, 0.02f, 0.02f);          // dark ring = readable on any background
            glPointSize(12.0f);
            glBegin(GL_POINTS); glVertex2f(b.x, b.y); glEnd();
            glColor3f(1.0f, 0.15f, 0.15f);
            glPointSize(8.0f);
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

        // Dynamic light around the bolt
        {
            float lr = 1.0f, lg = 0.85f, lb = 0.1f;
            if (pentaShotTimer > 0) { lr = 0.95f; lg = 0.2f; lb = 1.0f; }
            else if (tripleShotTimer > 0) { lr = 0.2f; lg = 1.0f; lb = 0.4f; }
            else if (rapidFireTimer > 0) { lr = 1.0f; lg = 0.5f; lb = 0.1f; }
            drawGlow(b.x + slantX * 0.5f, b.y + BULLET_HEIGHT * 0.5f, 16.0f, lr, lg, lb, 0.30f);
        }

        // Outer glow color
        if (pentaShotTimer > 0)
            glColor3f(0.95f, 0.2f, 1.0f); // Magenta
        else if (tripleShotTimer > 0)
            glColor3f(0.2f, 1.0f, 0.4f);  // Emerald green
        else if (rapidFireTimer > 0)
            glColor3f(1.0f, 0.5f, 0.1f);  // Orange plasma
        else
            glColor3f(1.0f, 0.85f, 0.1f); // Bright yellow default (clear on green and blue)

        {
            // dark outline quad under the bolt
            GLfloat cc[4]; glGetFloatv(GL_CURRENT_COLOR, cc);
            glColor3f(0.1f, 0.12f, 0.2f);
            glBegin(GL_QUADS);
                glVertex2f(b.x - BULLET_WIDTH / 2.0f - 2.0f, b.y - 2.0f);
                glVertex2f(b.x + BULLET_WIDTH / 2.0f + 2.0f, b.y - 2.0f);
                glVertex2f(b.x + BULLET_WIDTH / 2.0f + 2.0f + slantX, b.y + BULLET_HEIGHT + 2.0f);
                glVertex2f(b.x - BULLET_WIDTH / 2.0f - 2.0f + slantX, b.y + BULLET_HEIGHT + 2.0f);
            glEnd();
            glColor3f(cc[0], cc[1], cc[2]);
        }
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

        // Bright round tip: the bolt reads as a glowing capsule
        glColor3f(1.0f, 1.0f, 0.85f);
        drawCircleFill(b.x + slantX, b.y + BULLET_HEIGHT, 3.2f, 12);
    }
}

// ======================================================
// DRAW ENEMIES
// ======================================================

// Enemy plane = the SAME fighter design as the player's plane, flipped upside down so its nose
// points DOWN at you. Same silhouette = the direction of travel is obvious at a glance.
//   body = fuselage colour, wing = wing colour, trim = nose/tips/fins colour
static void drawEnemyPlane(float x, float y, float w, float h,
                           float br, float bg, float bb,     // body
                           float wr, float wg, float wb,     // wings
                           float damage)
{
    const float HW = 27.0f, H = 46.0f;       // the design is drawn in a 54 x 46 box (nose at +H)

    // ground shadow
    glColor4f(0.0f, 0.0f, 0.0f, 0.18f);
    drawCircleFill(x + 12.0f, y + h * 0.45f - 6.0f, h * 0.42f, 16);

    glPushMatrix();
    glTranslatef(x, y + h, 0.0f);            // tail end of the plane
    glScalef(w / 54.0f, -h / 46.0f, 1.0f);   // negative Y scale = vertical flip: nose now points DOWN

    // engine flame behind the plane (flickers)
    float fl = 8.0f + 5.0f * sinf(animFrame * 0.6f + x * 0.1f);
    glColor3f(1.0f, 0.55f, 0.1f);
    glBegin(GL_TRIANGLES);
        glVertex2f(-5.0f, -8.0f); glVertex2f(5.0f, -8.0f); glVertex2f(0.0f, -8.0f - fl * 1.4f);
    glEnd();
    glColor3f(1.0f, 0.95f, 0.45f);
    glBegin(GL_TRIANGLES);
        glVertex2f(-2.5f, -8.0f); glVertex2f(2.5f, -8.0f); glVertex2f(0.0f, -8.0f - fl * 0.8f);
    glEnd();

    // wings
    glColor3f(wr, wg, wb);
    glBegin(GL_POLYGON);
        glVertex2f(-HW, -2.0f); glVertex2f(-5.0f, 26.0f); glVertex2f(5.0f, 26.0f); glVertex2f(HW, -2.0f);
        glVertex2f(HW - 5.0f, -9.0f); glVertex2f(-HW + 5.0f, -9.0f);
    glEnd();
    // dark wing tips + tail fins
    glColor3f(wr * 0.55f, wg * 0.55f, wb * 0.55f);
    glBegin(GL_TRIANGLES);
        glVertex2f(-HW, -2.0f); glVertex2f(-HW + 11.0f, 8.0f); glVertex2f(-HW + 5.0f, -9.0f);
        glVertex2f(HW, -2.0f);  glVertex2f(HW - 11.0f, 8.0f);  glVertex2f(HW - 5.0f, -9.0f);
        glVertex2f(-4.0f, 2.0f); glVertex2f(-14.0f, -12.0f); glVertex2f(-4.0f, -8.0f);
        glVertex2f(4.0f, 2.0f);  glVertex2f(14.0f, -12.0f);  glVertex2f(4.0f, -8.0f);
    glEnd();

    // fuselage
    glColor3f(br, bg, bb);
    glBegin(GL_POLYGON);
        glVertex2f(0.0f, H);
        glVertex2f(7.0f, 34.0f); glVertex2f(8.0f, 6.0f); glVertex2f(5.0f, -8.0f);
        glVertex2f(-5.0f, -8.0f); glVertex2f(-8.0f, 6.0f); glVertex2f(-7.0f, 34.0f);
    glEnd();
    // nose cone
    glColor3f(wr * 0.8f, wg * 0.8f, wb * 0.8f);
    glBegin(GL_TRIANGLES);
        glVertex2f(0.0f, H); glVertex2f(6.2f, 36.0f); glVertex2f(-6.2f, 36.0f);
    glEnd();
    // cockpit (dark glass with a highlight)
    glColor3f(0.12f, 0.16f, 0.28f);
    drawCircleFill(0.0f, 24.0f, 5.0f, 14);
    glColor3f(0.65f, 0.88f, 1.0f);
    drawCircleFill(-1.5f, 25.5f, 2.0f, 8);

    // outlines
    glColor3f(0.08f, 0.1f, 0.18f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(-HW, -2.0f); glVertex2f(-5.0f, 26.0f); glVertex2f(5.0f, 26.0f); glVertex2f(HW, -2.0f);
        glVertex2f(HW - 5.0f, -9.0f); glVertex2f(-HW + 5.0f, -9.0f);
    glEnd();
    glBegin(GL_LINE_LOOP);
        glVertex2f(0.0f, H);
        glVertex2f(7.0f, 34.0f); glVertex2f(8.0f, 6.0f); glVertex2f(5.0f, -8.0f);
        glVertex2f(-5.0f, -8.0f); glVertex2f(-8.0f, 6.0f); glVertex2f(-7.0f, 34.0f);
    glEnd();
    glLineWidth(1.0f);
    glPopMatrix();

    // damage: dark smoke on a hurt heavy bomber
    if (damage > 0.0f)
    {
        glColor4f(0.1f, 0.1f, 0.1f, 0.55f);
        drawCircleFill(x - 8.0f, y + h * 0.7f, 6.0f, 10);
        drawCircleFill(x + 6.0f, y + h * 0.85f, 5.0f, 10);
    }
}

void drawEnemy(const Enemy &e)
{
    if (!e.active)
        return;

    // Per-enemy model transform: bank into the turn (rotation) + breathing (uniform scale)
    float tilt = 0.0f;
    float breathe = 1.0f;
    float pivY = e.y + ENEMY_HEIGHT * 0.5f;
    glPushMatrix();
    glTranslatef(e.x, pivY, 0.0f);
    glRotatef(tilt, 0.0f, 0.0f, 1.0f);
    glScalef(breathe, breathe, 1.0f);
    glTranslatef(-e.x, -pivY, 0.0f);

    if (e.type == 0)
    {
        // Scout: red and white jet (the common, easy enemy)
        drawEnemyPlane(e.x, e.y, ENEMY_WIDTH, ENEMY_HEIGHT, 1.0f, 1.0f, 1.0f, 0.90f, 0.15f, 0.15f, 0.0f);
    }
    else if (e.type == 1)
    {
        // Interceptor: yellow and orange (faster, shoots more)
        drawEnemyPlane(e.x, e.y, ENEMY_WIDTH + 6.0f, ENEMY_HEIGHT, 1.0f, 0.93f, 0.35f, 0.95f, 0.55f, 0.05f, 0.0f);
    }
    else
    {
        // Heavy bomber: big dark-blue plane, takes two hits (smoke after the first)
        drawEnemyPlane(e.x, e.y - 6.0f, ENEMY_WIDTH + 24.0f, ENEMY_HEIGHT + 12.0f,
                       0.45f, 0.5f, 0.62f, 0.2f, 0.25f, 0.4f, (e.hp < 2) ? 1.0f : 0.0f);
    }

    glPopMatrix();
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

    // Reactor core: pulsing light + lit 3D mesh
    float pulse = 0.5f + 0.5f * sinf(animFrame * 0.2f);
    drawGlow(b.x, b.y, 80.0f, 1.0f, 0.55f, 0.1f, 0.35f + 0.25f * pulse);
    glColor3f(1.0f, 0.8f, 0.2f);
    drawCircleFill(b.x, b.y, 22.0f, 28);
    glColor3f(1.0f, 0.4f + 0.4f * pulse, 0.1f);
    drawCircleFill(b.x, b.y, 14.0f, 24);
    glColor3f(0.1f, 0.12f, 0.2f);
    glLineWidth(2.0f); drawCircleOutline(b.x, b.y, 22.0f, 28); glLineWidth(1.0f);

    // Hierarchical model: boss frame -> rotating ring -> pods -> orbiting satellites.
    // Each child inherits (and adds to) its parent's transform via the matrix stack.
    glPushMatrix();
    glTranslatef(b.x, b.y, 0.0f);

    glColor4f(1.0f, 0.8f, 0.2f, 0.55f);
    if (cgRaster)
    {
        glPointSize(2.0f);
        midpointCircle(0, 0, 78);
    }
    else
    {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 64; i++)
        {
            float t = i * 2.0f * 3.14159265f / 64.0f;
            glVertex2f(cosf(t) * 78.0f, sinf(t) * 78.0f);
        }
        glEnd();
    }

    glRotatef(animFrame * 1.4f, 0.0f, 0.0f, 1.0f);            // ring spins
    for (int k = 0; k < 3; k++)
    {
        glPushMatrix();
        glRotatef(k * 120.0f, 0.0f, 0.0f, 1.0f);              // spread pods
        glTranslatef(78.0f, 0.0f, 0.0f);                      // move out to the ring
        glRotatef(-animFrame * 3.0f, 0.0f, 0.0f, 1.0f);       // pod spins on its own axis

        glColor3f(0.9f, 0.25f, 0.35f);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(0.0f, 0.0f);
            for (int i = 0; i <= 6; i++)
            {
                float t = i * 2.0f * 3.14159265f / 6.0f;
                glVertex2f(cosf(t) * 11.0f, sinf(t) * 11.0f);
            }
        glEnd();
        glColor3f(0.2f, 0.04f, 0.1f);
        glBegin(GL_TRIANGLES);
            glVertex2f(-5.0f, -4.0f); glVertex2f(5.0f, -4.0f); glVertex2f(0.0f, 6.0f);
        glEnd();

        glPushMatrix();                                       // satellite = child of the pod
        glRotatef(animFrame * 6.0f, 0.0f, 0.0f, 1.0f);
        glTranslatef(19.0f, 0.0f, 0.0f);
        glColor3f(1.0f, 0.9f, 0.3f);
        glBegin(GL_QUADS);
            glVertex2f(-3.0f, -3.0f); glVertex2f(3.0f, -3.0f); glVertex2f(3.0f, 3.0f); glVertex2f(-3.0f, 3.0f);
        glEnd();
        glPopMatrix();

        glPopMatrix();
    }
    glPopMatrix();

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

// Score needed to trigger the boss of a sector (grows again after the 5th planet)
static int sectorTargetScore(int sec)
{
    return sectorThemes[sec % NUM_SECTORS].targetScore + (sec / NUM_SECTORS) * 1000;
}

static void drawHeart(float x, float y, float s, bool full)
{
    if (full) glColor3f(0.95f, 0.15f, 0.25f); else glColor4f(0.3f, 0.3f, 0.4f, 0.6f);
    drawCircleFill(x - 3.2f * s, y + 2.5f * s, 4.2f * s, 14);
    drawCircleFill(x + 3.2f * s, y + 2.5f * s, 4.2f * s, 14);
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 7.2f * s, y + 1.2f * s); glVertex2f(x + 7.2f * s, y + 1.2f * s); glVertex2f(x, y - 8.0f * s);
    glEnd();
}

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
    renderGlText(WIDTH / 2 - 110, HEIGHT - 32, sectorText);

    // EMP Bomb count
    glColor3f(0.4f, 0.8f, 1.0f);
    renderGlText(WIDTH - 235, HEIGHT - 32, empText, GLUT_BITMAP_HELVETICA_12);

    // Lives as hearts (grey = lost)
    glColor3f(1.0f, 1.0f, 1.0f);
    renderGlText(WIDTH - 150, HEIGHT - 32, "LIVES", GLUT_BITMAP_HELVETICA_12);
    for (int h = 0; h < 5; h++)
        drawHeart(WIDTH - 95.0f + h * 19.0f, HEIGHT - 28.0f, 1.0f, h < lives);

    // Boss progress bar: fills as you score toward the sector boss
    {
        float bx = WIDTH / 2.0f - 110.0f, by = HEIGHT - 70.0f, bw = 220.0f, bh = 10.0f;
        int tgt = sectorTargetScore(currentSector);
        int prev = (currentSector > 0) ? sectorTargetScore(currentSector - 1) : 0;
        float prog = (tgt > prev) ? (float)(score - prev) / (float)(tgt - prev) : 1.0f;
        prog = std::max(0.0f, std::min(1.0f, prog));
        if (currentBoss.active) prog = 1.0f;
        glColor4f(0.05f, 0.1f, 0.25f, 0.75f);
        glBegin(GL_QUADS);
            glVertex2f(bx - 2, by - 2); glVertex2f(bx + bw + 2, by - 2); glVertex2f(bx + bw + 2, by + bh + 2); glVertex2f(bx - 2, by + bh + 2);
        glEnd();
        if (currentBoss.active) glColor3f(0.95f, 0.2f, 0.2f); else glColor3f(1.0f, 0.7f, 0.1f);
        glBegin(GL_QUADS);
            glVertex2f(bx, by); glVertex2f(bx + bw * prog, by); glVertex2f(bx + bw * prog, by + bh); glVertex2f(bx, by + bh);
        glEnd();
        renderTextOutlined(bx + bw + 8.0f, by, currentBoss.active ? "BOSS FIGHT!" : "BOSS", GLUT_BITMAP_HELVETICA_12, 1.0f, 1.0f, 1.0f);
    }

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

    // First-time hint strip: tells a new player everything in one line
    if (hintTimer > 0)
    {
        float a = (hintTimer > 60) ? 1.0f : hintTimer / 60.0f;
        glColor4f(0.05f, 0.1f, 0.25f, 0.88f * a);
        glBegin(GL_QUADS);
            glVertex2f(0, 0); glVertex2f(WIDTH, 0); glVertex2f(WIDTH, 58.0f); glVertex2f(0, 58.0f);
        glEnd();
        renderTextOutlined(20, 36, "MOVE: Arrow keys / WASD     FIRE: hold SPACE     EMP bomb: B", GLUT_BITMAP_HELVETICA_18, 1.0f, 1.0f, 1.0f, a);
        renderTextOutlined(20, 12, "Shoot enemies BEFORE they reach the ground - each one that lands costs you a heart!", GLUT_BITMAP_HELVETICA_12, 1.0f, 0.9f, 0.3f, a);
    }

    // Sector Transition Banner Announcement
    if (sectorAnnouncementTimer > 0)
    {
        float bannerAlpha = (sectorAnnouncementTimer > 40) ? 0.9f : ((float)sectorAnnouncementTimer / 40.0f);
        glColor4f(0.05f, 0.08f, 0.18f, bannerAlpha * 0.85f);
        glBegin(GL_QUADS);
            glVertex2f(0, HEIGHT * 0.72f - 30.0f);
            glVertex2f(WIDTH, HEIGHT * 0.72f - 30.0f);
            glVertex2f(WIDTH, HEIGHT * 0.72f + 30.0f);
            glVertex2f(0, HEIGHT * 0.72f + 30.0f);
        glEnd();

        glColor4f(0.2f, 0.9f, 1.0f, bannerAlpha);
        renderGlText(WIDTH / 2 - 120, HEIGHT * 0.72f + 6, sectorThemes[currentSector % NUM_SECTORS].name, GLUT_BITMAP_TIMES_ROMAN_24);

        glColor4f(1.0f, 0.85f, 0.2f, bannerAlpha);
        renderGlText(WIDTH / 2 - 110, HEIGHT * 0.72f - 18, sectorThemes[currentSector % NUM_SECTORS].subtitle, GLUT_BITMAP_HELVETICA_12);
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

    // soft drop shadow
    glColor4f(0.0f, 0.0f, 0.0f, 0.25f);
    glBegin(GL_QUADS);
        glVertex2f(x + 3, y - 4); glVertex2f(x + w + 3, y - 4);
        glVertex2f(x + w + 3, y + h - 4); glVertex2f(x + 3, y + h - 4);
    glEnd();

    if (selected) glColor3f(1.0f, 0.62f, 0.10f);     // bright orange = "this one"
    else          glColor3f(0.97f, 0.98f, 1.0f);     // clean white
    glBegin(GL_QUADS);
        glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();

    glColor3f(0.08f, 0.15f, 0.35f);
    glLineWidth(selected ? 3.0f : 2.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
    glLineWidth(1.0f);

    // centred label (leading spaces in old labels are ignored)
    while (*label == ' ') label++;
    float tw = (float)glutBitmapLength(GLUT_BITMAP_HELVETICA_18, (const unsigned char*)label);
    if (selected) renderTextOutlined(x + (w - tw) / 2.0f, y + h / 2.0f - 6.0f, label, GLUT_BITMAP_HELVETICA_18, 1.0f, 1.0f, 1.0f);
    else { glColor3f(0.06f, 0.12f, 0.30f); renderGlText(x + (w - tw) / 2.0f, y + h / 2.0f - 6.0f, label, GLUT_BITMAP_HELVETICA_18); }
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
    float t = (float)animFrame;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Title banner: white translucent panel over the scenery
    glColor4f(1.0f, 1.0f, 1.0f, 0.78f);
    glBegin(GL_QUADS);
        glVertex2f(0, HEIGHT - 175.0f); glVertex2f(WIDTH, HEIGHT - 175.0f);
        glVertex2f(WIDTH, HEIGHT - 40.0f); glVertex2f(0, HEIGHT - 40.0f);
    glEnd();

    const char* title = "VECTOR STORM";
    float sc = 0.42f;
    float bob = 3.0f * sinf(t * 0.06f);
    renderStrokeTitle((WIDTH - strokeTextWidth(title, sc)) / 2.0f, HEIGHT - 115.0f + bob, title, sc, 1.0f, 0.55f, 0.05f);

    const char* sub = "Fly the plane. Shoot the RED enemy planes. Grab the power-up badges.";
    float sw = (float)glutBitmapLength(GLUT_BITMAP_HELVETICA_18, (const unsigned char*)sub);
    glColor3f(0.06f, 0.15f, 0.40f);
    renderGlText((WIDTH - sw) / 2.0f, HEIGHT - 155.0f, sub, GLUT_BITMAP_HELVETICA_18);

    float bw = 280.0f;
    float bh = 40.0f;
    float bx = WIDTH / 2.0f - bw / 2.0f;
    float startY = HEIGHT - 228.0f;
    float gap = 50.0f;

    drawButton(bx, startY, bw, bh, "START GAME", menuSelectedIdx == 0);
    drawButton(bx, startY - gap, bw, bh, "SETTINGS", menuSelectedIdx == 1);
    drawButton(bx, startY - gap * 2, bw, bh, "HOW TO PLAY", menuSelectedIdx == 2);
    drawButton(bx, startY - gap * 3, bw, bh, "HOW IT WORKS (CODE)", menuSelectedIdx == 3);
    drawButton(bx, startY - gap * 4, bw, bh, "CG CONCEPTS", menuSelectedIdx == 4);
    drawButton(bx, startY - gap * 5, bw, bh, "QUIT", menuSelectedIdx == 5);

    // Demo: YOU (left) vs an ENEMY (right) so the idea is clear before playing
    {
        float sx = playerX, sy = playerY;
        playerX = 120.0f; playerY = 230.0f + 12.0f * sinf(t * 0.05f);
        drawPlayer();
        playerX = sx; playerY = sy;
        renderTextOutlined(100.0f, 170.0f, "YOU", GLUT_BITMAP_HELVETICA_18, 1.0f, 1.0f, 1.0f);
        drawEnemyPlane(680.0f, 250.0f + 12.0f * sinf(t * 0.05f + 2.0f), ENEMY_WIDTH, ENEMY_HEIGHT, 1.0f, 1.0f, 1.0f, 0.90f, 0.15f, 0.15f, 0.0f);
        renderTextOutlined(645.0f, 170.0f, "ENEMY", GLUT_BITMAP_HELVETICA_18, 1.0f, 1.0f, 1.0f);
        renderTextOutlined(618.0f, 150.0f, "shoot it!", GLUT_BITMAP_HELVETICA_12, 1.0f, 0.9f, 0.3f);
    }

    // key hint on a dark strip
    glColor4f(0.05f, 0.1f, 0.25f, 0.85f);
    glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(WIDTH, 0); glVertex2f(WIDTH, 52.0f); glVertex2f(0, 52.0f);
    glEnd();
    const char* hint = "UP / DOWN (or W / S) to choose     ENTER or SPACE to confirm";
    float hw = (float)glutBitmapLength(GLUT_BITMAP_HELVETICA_18, (const unsigned char*)hint);
    glColor3f(1.0f, 1.0f, 1.0f);
    renderGlText((WIDTH - hw) / 2.0f, 20.0f, hint, GLUT_BITMAP_HELVETICA_18);
}

void drawCgInfo()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.02f, 0.03f, 0.08f, 0.93f);
    glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(WIDTH, 0); glVertex2f(WIDTH, HEIGHT); glVertex2f(0, HEIGHT);
    glEnd();

    glColor3f(0.2f, 0.9f, 1.0f);
    renderGlText(WIDTH / 2 - 150, HEIGHT - 50, "COMPUTER GRAPHICS CONCEPTS USED", GLUT_BITMAP_TIMES_ROMAN_24);

    static const char* rows[][2] = {
        {"2D Transformations",   "Translate/Rotate/Scale about a pivot: ship banks & squashes, enemies tilt into turns"},
        {"Hierarchical model",   "Boss: frame -> spinning ring -> pods -> satellites using glPushMatrix / glPopMatrix"},
        {"Bezier curves",        "Enemy flight paths = cubic Bezier (de Casteljau).  F4 shows the control polygons"},
        {"3D pipeline",          "Rotation matrices, perspective divide, back-face culling, painter's sort (Raiders, gems)"},
        {"Lighting",             "Lambert diffuse + Blinn-Phong specular on meshes; flat-shaded faces"},
        {"Gouraud shading",      "Glows are triangle fans: centre bright, rim transparent, colour interpolated"},
        {"Dynamic lights",       "Every explosion spawns a short-lived additive point light"},
        {"Texture mapping",      "Procedural fBm noise texture sphere-mapped on a lit, rotating planet"},
        {"Perspective",          "Warp starfield (x/z projection) and synthwave floor grid with a vanishing point"},
        {"Scan conversion",      "Bresenham line + Midpoint circle (F5 draws them pixel by pixel)"},
        {"Line clipping",        "Cohen-Sutherland clips the radar sweep line to the radar window"},
        {"Viewport transform",   "Radar = same world re-rendered into a second glViewport with scissor test"},
        {"Blending",             "Alpha blending for UI, additive blending (GL_ONE) for light and fire"},
        {"Post-processing",      "Vignette, CRT scanlines (F3), impact flash, chromatic-split title"},
        {"Polygon modes",        "F2 switches everything to wireframe: see the triangles behind the picture"}
    };
    float y = HEIGHT - 92.0f;
    for (int i = 0; i < 15; i++)
    {
        glColor3f(1.0f, 0.85f, 0.2f);
        renderGlText(40.0f, y, rows[i][0], GLUT_BITMAP_HELVETICA_12);
        glColor3f(0.82f, 0.9f, 1.0f);
        renderGlText(200.0f, y, rows[i][1], GLUT_BITMAP_HELVETICA_12);
        y -= 25.0f;
    }

    glColor3f(0.4f, 1.0f, 0.7f);
    renderGlText(40.0f, 82.0f, "In-game keys:  F1 CG Lab panel   F2 Wireframe   F3 CRT   F4 Bezier/hitbox debug   F5 Raster algorithms", GLUT_BITMAP_HELVETICA_12);

    drawButton(WIDTH / 2.0f - 100.0f, 30.0f, 200.0f, 38.0f, "    < RETURN >", true);
}

// ------------------------------------------------------
// CG overlays: target lock (Bresenham), debug view, radar (2nd viewport), lab panel
// ------------------------------------------------------

static void rectLoop(float x0, float y0, float x1, float y1)
{
    glBegin(GL_LINE_LOOP);
        glVertex2f(x0, y0); glVertex2f(x1, y0); glVertex2f(x1, y1); glVertex2f(x0, y1);
    glEnd();
}

void drawTargetLock()
{
    lockedEnemy = -1;
    float best = 1e9f;
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (!enemies[i].active || enemies[i].y < playerY || enemies[i].y > HEIGHT - 50.0f) continue;
        float dx = enemies[i].x - playerX, dy = enemies[i].y - playerY;
        float d = dx * dx + dy * dy;
        if (d < best) { best = d; lockedEnemy = i; }
    }
    if (lockedEnemy < 0) return;

    const Enemy& e = enemies[lockedEnemy];
    float ex = e.x, ey = e.y + ENEMY_HEIGHT * 0.5f;
    lockedT = std::max(0.0f, std::min(1.0f, (e.startY - e.y) / (e.startY + 40.0f)));

    glEnable(GL_BLEND);
    glColor4f(0.3f, 1.0f, 0.55f, 0.55f);
    if (cgRaster)
    {
        glPointSize(3.0f);
        bresenhamLine((int)playerX, (int)(playerY + PLAYER_HEIGHT), (int)ex, (int)ey);
    }
    else
    {
        glBegin(GL_LINES);
            glVertex2f(playerX, playerY + PLAYER_HEIGHT);
            glVertex2f(ex, ey);
        glEnd();
    }

    // Reticle: spinning transform around the target
    glPushMatrix();
    glTranslatef(ex, ey, 0.0f);
    glRotatef(animFrame * 3.0f, 0.0f, 0.0f, 1.0f);
    glColor4f(0.3f, 1.0f, 0.55f, 0.9f);
    if (cgRaster)
    {
        glPointSize(2.5f);
        midpointCircle(0, 0, 28);
    }
    else
    {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 40; i++)
        {
            float a = i * 2.0f * 3.14159265f / 40.0f;
            glVertex2f(cosf(a) * 28.0f, sinf(a) * 28.0f);
        }
        glEnd();
    }
    glBegin(GL_LINES);
    for (int k = 0; k < 4; k++)
    {
        float a = k * 3.14159265f / 2.0f;
        glVertex2f(cosf(a) * 28.0f, sinf(a) * 28.0f);
        glVertex2f(cosf(a) * 38.0f, sinf(a) * 38.0f);
    }
    glEnd();
    glPopMatrix();
}

void drawDebugOverlay()
{
    glEnable(GL_BLEND);
    glLineWidth(1.0f);
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        const Enemy& e = enemies[i];
        if (!e.active) continue;

        // Evaluated Bezier curve
        glColor4f(0.2f, 0.9f, 1.0f, 0.4f);
        glBegin(GL_LINE_STRIP);
        for (int k = 0; k <= 40; k++)
        {
            float t = k / 40.0f;
            glVertex2f(bezier1D(e.cx[0], e.cx[1], e.cx[2], e.cx[3], t), e.startY - t * (e.startY + 40.0f));
        }
        glEnd();

        // Control polygon + control points
        glColor4f(1.0f, 0.9f, 0.2f, 0.35f);
        glBegin(GL_LINE_STRIP);
        for (int k = 0; k < 4; k++) glVertex2f(e.cx[k], e.startY - (k / 3.0f) * (e.startY + 40.0f));
        glEnd();
        glColor4f(1.0f, 0.9f, 0.2f, 0.9f);
        glPointSize(5.0f);
        glBegin(GL_POINTS);
        for (int k = 0; k < 4; k++) glVertex2f(e.cx[k], e.startY - (k / 3.0f) * (e.startY + 40.0f));
        glEnd();

        // Collision box (AABB)
        glColor4f(0.2f, 1.0f, 0.3f, 0.9f);
        rectLoop(e.x - ENEMY_WIDTH / 2, e.y, e.x + ENEMY_WIDTH / 2, e.y + ENEMY_HEIGHT);
    }
    glColor4f(0.2f, 1.0f, 0.3f, 0.9f);
    rectLoop(playerX - PLAYER_WIDTH * 0.375f, playerY, playerX + PLAYER_WIDTH * 0.375f, playerY + PLAYER_HEIGHT * 0.85f);
    if (currentBoss.active)
        rectLoop(currentBoss.x - currentBoss.width / 2, currentBoss.y, currentBoss.x + currentBoss.width / 2, currentBoss.y + currentBoss.height);
}

void drawRadar()
{
    float sx = cgWinW / (float)WIDTH, sy = cgWinH / (float)HEIGHT;
    int rw = (int)(150 * sx), rh = (int)(112 * sy);
    int px = cgWinW - rw - (int)(8 * sx);
    int py = (int)((HEIGHT - 96 - 112) * sy);

    // Viewport transform: the SAME world projection is now mapped into a small window
    glEnable(GL_SCISSOR_TEST);
    glScissor(px, py, rw, rh);
    glViewport(px, py, rw, rh);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.01f, 0.07f, 0.06f, 0.72f);
    glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(WIDTH, 0); glVertex2f(WIDTH, HEIGHT); glVertex2f(0, HEIGHT);
    glEnd();

    glColor4f(0.1f, 0.8f, 0.5f, 0.25f);
    glBegin(GL_LINES);
    for (int x = 100; x < WIDTH; x += 100) { glVertex2f((float)x, 0); glVertex2f((float)x, (float)HEIGHT); }
    for (int y = 100; y < HEIGHT; y += 100) { glVertex2f(0, (float)y); glVertex2f((float)WIDTH, (float)y); }
    glEnd();

    // Rotating sweep, clipped to the radar window with Cohen-Sutherland
    for (int k = 0; k < 6; k++)
    {
        float ang = animFrame * 0.05f - k * 0.09f;
        float x0 = WIDTH / 2.0f, y0 = HEIGHT / 2.0f;
        float x1 = x0 + cosf(ang) * 1200.0f, y1 = y0 + sinf(ang) * 1200.0f;
        int oc0 = 0, oc1 = 0;
        if (k == 0) { clipLineCohenSutherland(x0, y0, x1, y1, 0, 0, (float)WIDTH, (float)HEIGHT, &sweepOc0, &sweepOc1); }
        else        { clipLineCohenSutherland(x0, y0, x1, y1, 0, 0, (float)WIDTH, (float)HEIGHT, &oc0, &oc1); }
        glColor4f(0.2f, 1.0f, 0.6f, 0.85f - k * 0.14f);
        glBegin(GL_LINES);
            glVertex2f(x0, y0); glVertex2f(x1, y1);
        glEnd();
    }

    // Blips
    glPointSize(4.0f);
    glColor3f(1.0f, 0.25f, 0.25f);
    glBegin(GL_POINTS);
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].active && enemies[i].y < HEIGHT + 40) glVertex2f(enemies[i].x, std::max(0.0f, enemies[i].y));
    glEnd();
    glPointSize(2.0f);
    glColor3f(1.0f, 0.6f, 0.2f);
    glBegin(GL_POINTS);
    for (int i = 0; i < MAX_BULLETS; i++)
        if (bullets[i].active && bullets[i].isEnemy) glVertex2f(bullets[i].x, bullets[i].y);
    glEnd();
    if (currentBoss.active)
    {
        glPointSize(8.0f);
        glColor3f(1.0f, 0.9f, 0.2f);
        glBegin(GL_POINTS); glVertex2f(currentBoss.x, std::min((float)HEIGHT, currentBoss.y)); glEnd();
    }
    glPointSize(5.0f);
    glColor3f(0.3f, 0.95f, 1.0f);
    glBegin(GL_POINTS); glVertex2f(playerX, playerY); glEnd();

    glColor4f(0.3f, 1.0f, 0.7f, 0.95f);
    glLineWidth(3.0f);
    rectLoop(2.0f, 2.0f, WIDTH - 2.0f, HEIGHT - 2.0f);
    glLineWidth(1.0f);

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, cgWinW, cgWinH);

    glColor3f(0.3f, 1.0f, 0.7f);
    renderGlText(WIDTH - 148.0f, HEIGHT - 108.0f, "RADAR", GLUT_BITMAP_HELVETICA_10);
}

void drawCgLabPanel()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float x = 10.0f, w = 292.0f, top = HEIGHT - 92.0f, h = 262.0f;
    glColor4f(0.02f, 0.05f, 0.1f, 0.82f);
    glBegin(GL_QUADS);
        glVertex2f(x, top - h); glVertex2f(x + w, top - h); glVertex2f(x + w, top); glVertex2f(x, top);
    glEnd();
    glColor3f(0.2f, 0.9f, 1.0f);
    rectLoop(x, top - h, x + w, top);

    char buf[96];
    float y = top - 18.0f;
    glColor3f(0.2f, 0.95f, 1.0f);
    sprintf(buf, "CG LAB  [F1]                  %3.0f FPS", cgFps);
    renderGlText(x + 10, y, buf, GLUT_BITMAP_HELVETICA_12); y -= 22.0f;

    glColor3f(1.0f, 0.85f, 0.2f);
    renderGlText(x + 10, y, "Ship model matrix  M = T(p) R(bank) S T(-p)", GLUT_BITMAP_HELVETICA_10); y -= 15.0f;
    glColor3f(0.9f, 0.95f, 1.0f);
    sprintf(buf, "| %6.3f %6.3f %8.2f |", shipM[0], shipM[2], shipM[4]);
    renderGlText(x + 20, y, buf, GLUT_BITMAP_HELVETICA_12); y -= 15.0f;
    sprintf(buf, "| %6.3f %6.3f %8.2f |", shipM[1], shipM[3], shipM[5]);
    renderGlText(x + 20, y, buf, GLUT_BITMAP_HELVETICA_12); y -= 15.0f;
    renderGlText(x + 20, y, "| 0.000  0.000     1.00 |", GLUT_BITMAP_HELVETICA_12); y -= 16.0f;
    sprintf(buf, "bank %.1f deg   scale (%.3f, %.3f)", shipBankDeg, shipSx, shipSy);
    renderGlText(x + 10, y, buf, GLUT_BITMAP_HELVETICA_12); y -= 24.0f;

    glColor3f(1.0f, 0.85f, 0.2f);
    renderGlText(x + 10, y, "Bezier path of locked target (de Casteljau)", GLUT_BITMAP_HELVETICA_10); y -= 15.0f;
    glColor3f(0.9f, 0.95f, 1.0f);
    if (lockedEnemy >= 0)
    {
        const Enemy& e = enemies[lockedEnemy];
        sprintf(buf, "t = %.2f   P(t).x = %.1f", lockedT, e.x);
        renderGlText(x + 10, y, buf, GLUT_BITMAP_HELVETICA_12); y -= 15.0f;
        sprintf(buf, "ctrl x: %.0f  %.0f  %.0f  %.0f", e.cx[0], e.cx[1], e.cx[2], e.cx[3]);
        renderGlText(x + 10, y, buf, GLUT_BITMAP_HELVETICA_12); y -= 24.0f;
    }
    else
    {
        renderGlText(x + 10, y, "(no target)", GLUT_BITMAP_HELVETICA_12); y -= 39.0f;
    }

    glColor3f(1.0f, 0.85f, 0.2f);
    renderGlText(x + 10, y, "Cohen-Sutherland (radar sweep end-point)", GLUT_BITMAP_HELVETICA_10); y -= 15.0f;
    glColor3f(0.9f, 0.95f, 1.0f);
    sprintf(buf, "outcode start %d%d%d%d   end %d%d%d%d",
            (sweepOc0 >> 3) & 1, (sweepOc0 >> 2) & 1, (sweepOc0 >> 1) & 1, sweepOc0 & 1,
            (sweepOc1 >> 3) & 1, (sweepOc1 >> 2) & 1, (sweepOc1 >> 1) & 1, sweepOc1 & 1);
    renderGlText(x + 10, y, buf, GLUT_BITMAP_HELVETICA_12); y -= 24.0f;

    const char* names[4] = {"[F2] Wireframe (GL_LINE)", "[F3] CRT scanlines", "[F4] Bezier + hitbox debug", "[F5] Bresenham / Midpoint raster"};
    bool states[4] = {cgWireframe, cgCRT, cgDebug, cgRaster};
    for (int i = 0; i < 4; i++)
    {
        if (states[i]) glColor3f(0.3f, 1.0f, 0.5f); else glColor3f(0.55f, 0.6f, 0.7f);
        sprintf(buf, "%s : %s", names[i], states[i] ? "ON" : "off");
        renderGlText(x + 10, y, buf, GLUT_BITMAP_HELVETICA_12); y -= 15.0f;
    }
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

// ======================================================
// HOW TO PLAY (plain-language rules, with the real enemy sprites)
// ======================================================

void drawHowToPlay()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 0.93f);
    glBegin(GL_QUADS);
        glVertex2f(20, 20); glVertex2f(WIDTH - 20, 20); glVertex2f(WIDTH - 20, HEIGHT - 20); glVertex2f(20, HEIGHT - 20);
    glEnd();

    const char* title = "HOW TO PLAY";
    renderStrokeTitle((WIDTH - strokeTextWidth(title, 0.3f)) / 2.0f, HEIGHT - 75.0f, title, 0.3f, 1.0f, 0.55f, 0.05f);

    const float L = 50.0f;
    float y = HEIGHT - 110.0f;
    glColor3f(0.8f, 0.35f, 0.0f);
    renderGlText(L, y, "GOAL", GLUT_BITMAP_HELVETICA_18);
    glColor3f(0.06f, 0.12f, 0.30f);
    renderGlText(L + 70, y, "Shoot enemy planes. Fill the BOSS bar, beat the boss, fly on to the next sector.", GLUT_BITMAP_HELVETICA_12);
    y -= 24.0f;
    glColor3f(0.8f, 0.35f, 0.0f);
    renderGlText(L, y, "KEYS", GLUT_BITMAP_HELVETICA_18);
    glColor3f(0.06f, 0.12f, 0.30f);
    renderGlText(L + 70, y, "Arrows or WASD = move     SPACE (hold) = fire     B = EMP bomb     P = pause", GLUT_BITMAP_HELVETICA_12);
    y -= 24.0f;
    glColor3f(0.8f, 0.1f, 0.1f);
    renderGlText(L, y, "DANGER", GLUT_BITMAP_HELVETICA_18);
    glColor3f(0.06f, 0.12f, 0.30f);
    renderGlText(L + 70, y, "Lose a heart if: a red bullet hits you, a plane hits you, or an enemy reaches the ground.", GLUT_BITMAP_HELVETICA_12);

    // Enemy guide: the real sprites
    y -= 40.0f;
    glColor3f(0.8f, 0.35f, 0.0f);
    renderGlText(L, y, "ENEMIES", GLUT_BITMAP_HELVETICA_18);

    static const char* names[3] = {"SCOUT  (10 pts)", "INTERCEPTOR  (25 pts)", "BOMBER  (35 pts)"};
    static const char* line1[3] = {"Flies straight down.", "Fires AIMED shots", "Big. Needs 2 hits."};
    static const char* line2[3] = {"Shoots straight down.", "right at your plane.", "Fires 3 bullets in a fan."};
    for (int i = 0; i < 3; i++)
    {
        float cx = 150.0f + i * 250.0f, cy = y - 80.0f;
        if (i == 0)      drawEnemyPlane(cx, cy, ENEMY_WIDTH, ENEMY_HEIGHT, 1.0f, 1.0f, 1.0f, 0.90f, 0.15f, 0.15f, 0.0f);
        else if (i == 1) drawEnemyPlane(cx, cy, ENEMY_WIDTH + 6.0f, ENEMY_HEIGHT, 1.0f, 0.93f, 0.35f, 0.95f, 0.55f, 0.05f, 0.0f);
        else             drawEnemyPlane(cx, cy - 6.0f, ENEMY_WIDTH + 24.0f, ENEMY_HEIGHT + 12.0f, 0.45f, 0.5f, 0.62f, 0.2f, 0.25f, 0.4f, 0.0f);
        glColor3f(0.06f, 0.12f, 0.30f);
        renderGlText(cx - 70.0f, cy - 22.0f, names[i], GLUT_BITMAP_HELVETICA_12);
        renderGlText(cx - 70.0f, cy - 40.0f, line1[i], GLUT_BITMAP_HELVETICA_12);
        renderGlText(cx - 70.0f, cy - 56.0f, line2[i], GLUT_BITMAP_HELVETICA_12);
    }

    // Power-up guide: the real badges
    y -= 190.0f;
    glColor3f(0.8f, 0.35f, 0.0f);
    renderGlText(L, y, "POWER-UPS", GLUT_BITMAP_HELVETICA_18);
    glColor3f(0.06f, 0.12f, 0.30f);
    renderGlText(L + 100, y, "Fly into a badge to grab it. Most last about 10 seconds.", GLUT_BITMAP_HELVETICA_12);
    static const char* letters[8] = {"S", "3", "5", "R", "W", "B", "T", "+"};
    static const char* caps[8] = {"Shield", "3-Shot", "5-Shot", "Rapid", "Speed", "EMP Bomb", "Slow-Mo", "+1 Life"};
    static const float cols[8][3] = {{0.1f, 0.85f, 1.0f}, {1.0f, 0.8f, 0.1f}, {0.85f, 0.2f, 1.0f}, {1.0f, 0.25f, 0.2f},
                                     {0.0f, 0.85f, 0.7f}, {0.4f, 0.5f, 1.0f}, {0.2f, 0.7f, 1.0f}, {0.2f, 0.8f, 0.3f}};
    for (int i = 0; i < 8; i++)
    {
        float bx = 82.0f + i * 90.0f, by = y - 38.0f;
        glColor3f(0.08f, 0.1f, 0.2f);
        drawCircleFill(bx, by, 15.5f, 22);
        glColor3f(cols[i][0], cols[i][1], cols[i][2]);
        drawCircleFill(bx, by, 13.0f, 22);
        glColor3f(1.0f, 1.0f, 1.0f);
        renderGlText(bx - 5.0f, by - 6.0f, letters[i], GLUT_BITMAP_HELVETICA_18);
        glColor3f(0.06f, 0.12f, 0.30f);
        renderGlText(bx - 22.0f, by - 32.0f, caps[i], GLUT_BITMAP_HELVETICA_12);
    }

    drawButton(WIDTH / 2.0f - 100.0f, 34.0f, 200.0f, 38.0f, "BACK", true);
}

// ======================================================
// HOW IT WORKS: the game's real code, explained line by line
// ======================================================

struct HiwPage
{
    const char* title;
    int nCode;
    const char* code[13];
    int nNotes;
    const char* notes[6];
};

static const HiwPage hiwPages[] = {
    { "1. THE GAME LOOP - everything runs 60 times per second", 13, {
        "void update(int value) {                          // main.cpp",
        "    if (currentGameState == STATE_PLAYING) {",
        "        // move plane, shoot, move enemies, check collisions",
        "    }",
        "    glutPostRedisplay();             // ask GLUT to redraw the screen",
        "    glutTimerFunc(16, update, 0);    // call update() again in 16 ms = ~60 FPS",
        "}",
        "void display() {",
        "    glClear(GL_COLOR_BUFFER_BIT);    // erase the previous frame",
        "    drawBackgroundAndStars();        // far things first, near things last",
        "    drawPlayer();  drawEnemy(...);   // plane, enemies, bullets on top",
        "    drawHUD();                       // score and hearts last = always visible",
        "    glutSwapBuffers();               // show the finished picture" }, 5, {
        "Line 1-7: update() is the BRAIN. It only changes numbers (positions, health, score). It draws nothing.",
        "Line 5-6: glutTimerFunc schedules update() again, so the loop never stops. 16 ms x 60 = 1 second.",
        "Line 8-13: display() is the PAINTER. It only reads the numbers and draws them.",
        "Line 10-12: painter's order - whatever is drawn later covers what was drawn before.",
        "Line 13: double buffering - we draw off-screen, then swap, so you never see a half-drawn frame." } },

    { "2. MOVING THE PLANE - smooth, with inertia", 8, {
        "float targetVx = (rightPressed - leftPressed) * currentSpeed;   // simplified",
        "shipVx += (targetVx - shipVx) * 0.2f;   // slide 20% of the way to the target each frame",
        "playerX += shipVx;                       // move by the current velocity",
        "if (playerX < PLAYER_WIDTH / 2.0f) playerX = PLAYER_WIDTH / 2.0f;   // left wall",
        "if (playerX > WIDTH - PLAYER_WIDTH / 2.0f) playerX = WIDTH - PLAYER_WIDTH / 2.0f;",
        "if (playerY > HEIGHT * 0.5f) playerY = HEIGHT * 0.5f;   // stay in the lower half",
        "shipBankDeg = -shipVx * 2.4f;            // tilt the plane when it turns",
        "glRotatef(shipBankDeg, 0, 0, 1);         // rotate the drawing around the plane" }, 5, {
        "Line 1: pressing a key only sets the TARGET speed. The plane does not teleport to it.",
        "Line 2: every frame it closes 20% of the gap - fast start, soft stop. That is the inertia feel.",
        "Line 3: position = position + velocity. All movement in the game works like this.",
        "Line 4-6: clamping keeps the plane inside the window and low enough to see enemies coming.",
        "Line 7-8: the faster you slide sideways, the more the plane banks - a 2D rotation transform." } },

    { "3. SHOOTING - a cooldown and a pool of bullets", 13, {
        "if (spacePressed && shootCooldown <= 0) {",
        "    shoot();",
        "    shootCooldown = (rapidFireTimer > 0) ? 5 : 10;   // frames between shots",
        "}",
        "void spawnPlayerBullet(float x, float y, float vx, float vy) {",
        "    for (int i = 0; i < MAX_BULLETS; i++) {",
        "        if (!bullets[i].active) {              // find a free slot in the pool",
        "            bullets[i].x = x;   bullets[i].y = y;",
        "            bullets[i].vx = vx; bullets[i].vy = vy;",
        "            bullets[i].isEnemy = false;  bullets[i].active = true;",
        "            break;                             // one bullet only",
        "        }  }  }",
        "bullets[i].x += bullets[i].vx;  bullets[i].y += bullets[i].vy;   // in update()" }, 5, {
        "Line 1-4: holding SPACE fires every 10 frames (6 shots a second). Rapid-fire makes it 5.",
        "Line 5-12: bullets live in a fixed array of 90. We reuse a free slot instead of allocating memory.",
        "Line 10: isEnemy decides who a bullet can hurt - yours hit enemies, theirs hit you.",
        "Line 13: every frame each active bullet moves by its velocity (up for you, down for them).",
        "Bullets that leave the window are switched off (active = false) so the slot can be reused." } },

    { "4. ENEMY MOVEMENT AND AIMING", 7, {
        "enemies[i].y -= enemies[i].speed;                                  // always fly forward (down)",
        "// speed 1.7+ is faster than the ground scroll (0.7), so planes visibly fly FORWARD",
        "enemies[i].x = homeX + sinf(animFrame * 0.03f + phase) * 40.0f;       // sway left and right",
        "// Interceptor aiming:",
        "float dx = playerX - enemies[i].x,  dy = playerY - enemies[i].y;",
        "float len = sqrtf(dx * dx + dy * dy);               // distance to the player",
        "spawnEnemyBullet(x, y, dx / len * 4.0f, dy / len * 4.0f);   // unit direction x speed" }, 4, {
        "Line 1-2: enemies always move down the screen, faster than the ground scrolls, so they fly forward.",
        "Line 3: sin() gives a smooth left-right sway around the spot where the plane was spawned (homeX).",
        "Line 5-7: Pythagoras gives the distance; dividing by it gives a length-1 direction; times 4 = speed.",
        "Scouts shoot straight down, Interceptors aim at you, Bombers fire a 3-bullet fan." } },

    { "5. HIT DETECTION - invisible rectangles (AABB)", 11, {
        "bool checkCollision(float x1, float y1, float w1, float h1,",
        "                    float x2, float y2, float w2, float h2) {",
        "    return (x1 - w1/2 < x2 + w2/2) &&    // A's left edge is left of B's right edge",
        "           (x1 + w1/2 > x2 - w2/2) &&    // A's right edge is right of B's left edge",
        "           (y1 < y2 + h2) &&             // A's bottom is below B's top",
        "           (y1 + h1 > y2);               // A's top is above B's bottom",
        "}",
        "// used like this (your bullet vs an enemy):",
        "if (checkCollision(bullets[i].x, bullets[i].y, BULLET_WIDTH, BULLET_HEIGHT,",
        "                   enemies[j].x, enemies[j].y, ENEMY_WIDTH, ENEMY_HEIGHT)) {",
        "    bullets[i].active = false;  enemies[j].hp--;  }" }, 5, {
        "Every object is treated as an invisible box. Two boxes touch only if they overlap on BOTH axes.",
        "Line 3-6: four simple comparisons. If all four are true, the boxes overlap = a hit.",
        "Line 9-11: the bullet disappears and the enemy loses 1 HP; at 0 HP it explodes.",
        "Your plane's hit-box is smaller than its drawing (70-85%), so near misses feel fair.",
        "The same function checks: enemy bullets vs you, enemy planes vs you, you vs power-up badges." } },

    { "6. SCORE, COMBO AND LIVES", 10, {
        "int basePoints = (enemies[j].type == 2) ? 35 : (enemies[j].type == 1) ? 25 : 10;",
        "comboCount++;  if (comboCount > 5) comboCount = 5;   // max x5",
        "comboTimer = 130;                                    // ~2 seconds to keep it alive",
        "score += basePoints * comboCount;",
        "// every frame in update():",
        "if (comboTimer > 0) { comboTimer--; if (comboTimer <= 0) comboCount = 0; }",
        "// when you are hit:",
        "if (shieldHealth > 0) shieldHealth--;      // the shield absorbs it",
        "else { lives--; invulnerableTimer = 90; }  // otherwise lose a heart, 1.5 s of safety",
        "if (lives <= 0) currentGameState = STATE_GAME_OVER;" }, 5, {
        "Line 1: Scout 10, Interceptor 25, Bomber 35 points. Harder enemies are worth more.",
        "Line 2-4: kill enemies one after another and the multiplier grows up to x5.",
        "Line 6: when the timer runs out the combo resets, so keep shooting to keep it.",
        "Line 8-9: the shield takes the hit first. Without it you lose a heart (you start with 5) and blink.",
        "An enemy that reaches the ground also costs a heart (and resets your combo) - shoot it before it lands." } },

    { "7. DRAWING THE PLANE - just polygons and triangles", 10, {
        "glColor3f(1, 1, 1);                       // white",
        "glBegin(GL_POLYGON);                      // fuselage = one filled polygon",
        "    glVertex2f(X, Y + PLAYER_HEIGHT);     // nose",
        "    glVertex2f(X + 7, Y + 34);  glVertex2f(X + 8, Y + 6);  glVertex2f(X + 5, Y - 8);",
        "    glVertex2f(X - 5, Y - 8);   glVertex2f(X - 8, Y + 6);  glVertex2f(X - 7, Y + 34);",
        "glEnd();",
        "glColor3f(0.9f, 0.15f, 0.15f);            // red nose cone (drawn after = on top)",
        "glBegin(GL_TRIANGLES); ... glEnd();",
        "glColor3f(0.08f, 0.1f, 0.18f);  glLineWidth(2.0f);",
        "glBegin(GL_LINE_LOOP); ... glEnd();       // dark outline = cartoon look" }, 5, {
        "A computer draws everything from vertices (x, y points). glBegin/glEnd group them into a shape.",
        "Line 2-6: the polygon joins the points in order; OpenGL fills the inside with the current colour.",
        "Line 7-8: shapes are drawn in order; later shapes cover earlier ones (wings, body, nose, cockpit).",
        "Line 9-10: the same points drawn as a LINE_LOOP in a dark colour give the outline.",
        "When banking, glRotatef turns the whole drawing, so the same coordinates are reused every frame." } },

    { "8. THE SCROLLING GROUND - flat 2D layers", 8, {
        "sceneScroll += 0.7f;                          // every frame: the whole world moves down slowly",
        "float sy = worldY - sceneScroll;              // where a tree is on the screen",
        "unsigned h = cell * 2654435761u;              // hash: same cell -> same tree, always",
        "if (h % 10 < 5) tree(x, sy, size);            // some cells get a tree, others a house, bush...",
        "float seaEdge(float wy) { return 590 + 42*sinf(wy*0.007f) + 22*sinf(wy*0.019f); }",
        "glBegin(GL_TRIANGLE_STRIP);                   // the sea = a strip that follows seaEdge()",
        "drawCircleFill(x, y, radius, 18);             // a cloud = several overlapping circles",
        "clouds[i].y -= clouds[i].speed * 1.6f;        // clouds move faster than the ground = depth" }, 5, {
        "Line 1-2: scrolling is just moving the world down a little each frame and subtracting it from positions.",
        "Line 3-4: trees, houses and boats come from a hash of the cell number, so the world is endless",
        "and identical when you fly back over it - nothing has to be stored in memory.",
        "Line 5-6: sin() waves make the curvy coastline and the winding path; a strip of triangles fills it.",
        "Line 7-8: two speeds (ground slow, clouds fast) give a feeling of depth without any 3D maths." } },

    { "9. POWER-UPS, BOSS AND SECTORS", 8, {
        "if (rand() % 100 < 32) spawnPowerUp(x, y);       // 32% drop chance when an enemy dies",
        "case PU_SHIELD:      shieldHealth = 3;      break;",
        "case PU_TRIPLE_SHOT: tripleShotTimer = 660; break;   // 11 seconds at 60 FPS",
        "case PU_EXTRA_LIFE:  if (lives < 5) lives++; break;",
        "if (!currentBoss.active && score >= sectorThemes[currentSector].targetScore) spawnBoss();",
        "currentBoss.maxHp = 40 + sector * 15;          // later bosses are tougher",
        "if (currentBoss.hp <= 0) { score += 250; nextSector(); }",
        "void nextSector() { currentSector++; shieldHealth = 2; empBombsStock++; ... }" }, 5, {
        "Line 1: a little randomness keeps drops exciting. The badge is a circle with a letter, readable at once.",
        "Line 2-4: each power-up just sets a number or a timer; update() counts timers down every frame.",
        "Line 5: reaching the target score (the BOSS bar in the top HUD) starts the boss fight.",
        "Line 6-7: beating the boss gives 250 points and starts the next sector with a free shield and bomb.",
        "Each sector is a new planet (Earth, desert, ice, volcano, space) with its own world, speed and enemy mix." } },

    { "10. WHERE EVERYTHING LIVES - project map", 9, {
        "main.cpp                         game logic: update(), display(), input, HUD, menus, planes",
        "src/Graphics/Starfield.cpp       the scrolling world: grass, sea, path, scenery, clouds",
        "src/Graphics/CGLab.cpp           glow, lights, perspective helpers, 3D mesh renderer",
        "src/Graphics/ParticleSystem.cpp  explosions, smoke trails, floating score text",
        "src/Graphics/Renderer.cpp        text and circle drawing helpers used everywhere",
        "src/Entities/Powerup.cpp         power-up badges: spawn, fall, draw",
        "src/Audio/SoundEngine.cpp        synthesised sound effects and music",
        "include/Core/Config.hpp          window size and the list of game states",
        "build.bat                        compiles every file with g++ and links freeglut + OpenGL" }, 4, {
        "Rule of thumb: update() changes data, display() draws data, the input functions only set flags.",
        "To change difficulty: edit resetEnemy() (speed, HP, shoot timers) in main.cpp.",
        "To change the look: edit drawPlayer() / drawEnemyPlane(), or the colours in Starfield.cpp.",
        "To add a power-up: add it to the enum in Powerup.hpp, draw its badge, handle it in the switch in update()." } }
};
static const int HIW_PAGES = (int)(sizeof(hiwPages) / sizeof(hiwPages[0]));

static void hiwNext() { if (hiwPage < HIW_PAGES - 1) hiwPage++; else { currentGameState = STATE_MAIN_MENU; menuSelectedIdx = 0; } }
static void hiwPrev() { if (hiwPage > 0) hiwPage--; }

// Button geometry shared by drawing and mouse clicks
static const float HIW_BTN_W = 150.0f, HIW_BTN_H = 36.0f, HIW_BTN_Y = 14.0f;

void drawHowItWorks()
{
    const HiwPage& p = hiwPages[hiwPage];
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // white card over the scenery
    glColor4f(1.0f, 1.0f, 1.0f, 0.94f);
    glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(WIDTH, 0); glVertex2f(WIDTH, HEIGHT); glVertex2f(0, HEIGHT);
    glEnd();

    // title strip
    glColor3f(0.08f, 0.15f, 0.38f);
    glBegin(GL_QUADS);
        glVertex2f(0, HEIGHT - 52.0f); glVertex2f(WIDTH, HEIGHT - 52.0f); glVertex2f(WIDTH, HEIGHT); glVertex2f(0, HEIGHT);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
    renderGlText(24.0f, HEIGHT - 33.0f, p.title, GLUT_BITMAP_HELVETICA_18);
    char pg[32];
    sprintf(pg, "page %d / %d", hiwPage + 1, HIW_PAGES);
    glColor3f(1.0f, 0.8f, 0.3f);
    renderGlText(WIDTH - 110.0f, HEIGHT - 33.0f, pg, GLUT_BITMAP_HELVETICA_12);

    // code block (monospace, numbered, comments in green)
    const float LH = 15.0f;
    float top = HEIGHT - 64.0f;
    float bottom = top - p.nCode * LH - 14.0f;
    glColor3f(0.07f, 0.10f, 0.20f);
    glBegin(GL_QUADS);
        glVertex2f(16.0f, bottom); glVertex2f(WIDTH - 16.0f, bottom); glVertex2f(WIDTH - 16.0f, top); glVertex2f(16.0f, top);
    glEnd();
    for (int k = 0; k < p.nCode; k++)
    {
        float y = top - 17.0f - k * LH;
        char num[8];
        sprintf(num, "%2d", k + 1);
        glColor3f(0.95f, 0.75f, 0.3f);
        renderGlText(22.0f, y, num, GLUT_BITMAP_8_BY_13);

        const char* line = p.code[k];
        const char* cmt = strstr(line, "//");
        int codeLen = cmt ? (int)(cmt - line) : (int)strlen(line);
        char buf[160];
        strncpy(buf, line, codeLen);
        buf[codeLen] = '\0';
        glColor3f(0.93f, 0.96f, 1.0f);
        renderGlText(50.0f, y, buf, GLUT_BITMAP_8_BY_13);
        if (cmt)
        {
            glColor3f(0.45f, 0.90f, 0.55f);
            renderGlText(50.0f + codeLen * 8.0f, y, cmt, GLUT_BITMAP_8_BY_13);
        }
    }

    // line-by-line explanation
    float y = bottom - 26.0f;
    glColor3f(0.8f, 0.35f, 0.0f);
    renderGlText(20.0f, y, "WHAT IT DOES", GLUT_BITMAP_HELVETICA_18);
    y -= 24.0f;
    for (int k = 0; k < p.nNotes; k++)
    {
        glColor3f(0.06f, 0.12f, 0.30f);
        renderGlText(26.0f, y, p.notes[k], GLUT_BITMAP_HELVETICA_12);
        y -= 21.0f;
    }

    drawButton(30.0f, HIW_BTN_Y, HIW_BTN_W, HIW_BTN_H, "< PREVIOUS", false);
    drawButton(WIDTH / 2.0f - HIW_BTN_W / 2.0f, HIW_BTN_Y, HIW_BTN_W, HIW_BTN_H, "MENU", false);
    drawButton(WIDTH - 30.0f - HIW_BTN_W, HIW_BTN_Y, HIW_BTN_W, HIW_BTN_H, hiwPage == HIW_PAGES - 1 ? "FINISH" : "NEXT >", true);
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

void spawnEnemyBullet(float x, float y, float vx = 0.0f, float vy = -4.2f, int type = 0)
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

// An enemy is simply switched off when it dies or leaves; the wave spawner brings the next row.
void resetEnemy(int i)
{
    enemies[i].active = false;
}

int waveNumber = 0;     // counts waves so the formation shape changes
int waveDelay = 50;     // short pause between waves

// Spawn one wave just above the screen. Planes are SPREAD OUT: random lanes, different heights and
// slightly different speeds, so they never fly in a straight line side by side.
void spawnWave()
{
    int n = 3 + std::min(2, currentSector);                       // 3 enemies at first, up to 5 later
    float baseSpeed = std::min(2.6f, 1.7f + currentSector * 0.15f);   // always faster than the ground scroll

    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;

    // shuffle the lanes so the order is different every wave
    int lane[5] = {0, 1, 2, 3, 4};
    for (int i = n - 1; i > 0; i--) { int j = rand() % (i + 1); std::swap(lane[i], lane[j]); }

    for (int i = 0; i < n; i++)
    {
        float x = WIDTH * (lane[i] + 1) / (float)(n + 1) + (rand() % 61 - 30);    // lane + random jitter
        x = std::max(50.0f, std::min((float)WIDTH - 50.0f, x));
        float y = HEIGHT + 50.0f + i * (55.0f + rand() % 60);                    // each plane at its own height

        Enemy& e = enemies[i];
        e.x = x; e.y = y; e.startY = y; e.vx = 0.0f;
        for (int k = 0; k < 4; k++) e.cx[k] = x;          // cx[0] = home lane for the sway
        e.speed = baseSpeed + (rand() % 40) / 100.0f;
        e.wobbleOffset = (float)(rand() % 100);
        e.shootTimer = 90 + rand() % 90;

        // The first plane of a wave is the "special" one in later sectors; the rest are scouts
        e.type = 0; e.hp = 1;
        if (i == 0 && currentSector >= 2) { e.type = 2; e.hp = 2; e.speed *= 0.9f; }
        else if (i == 0 && currentSector >= 1) { e.type = 1; }
        e.active = true;
    }
    waveNumber++;
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

    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
    waveNumber = 0;
    waveDelay = 150;   // breathing room while the sector banner shows
}

// ======================================================
// INITIALIZE GAME
// ======================================================

void initializeGame()
{
    playerX = WIDTH / 2.0f;
    playerY = 60.0f;
    shipVx = shipVy = 0.0f;

    score = 0;
    lives = 5;
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
    hintTimer = 900;
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

    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
    waveNumber = 0;
    waveDelay = 70;
}

// ======================================================
// UPDATE GAME LOOP
// ======================================================

void update(int value)
{
    if (currentGameState == STATE_PLAYING && !gameOver && !paused)
    {
        animFrame++;
        if (hintTimer > 0) hintTimer--;

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

        // Full 2D movement with inertia (velocity eases toward the input target)
        float targetVx = ((rightPressed ? 1.0f : 0.0f) - (leftPressed ? 1.0f : 0.0f)) * currentSpeed;
        float targetVy = ((upPressed ? 1.0f : 0.0f) - (downPressed ? 1.0f : 0.0f)) * currentSpeed * 0.8f;
        shipVx += (targetVx - shipVx) * 0.2f;
        shipVy += (targetVy - shipVy) * 0.2f;
        playerX += shipVx;
        playerY += shipVy;
        if (playerY < 28.0f) playerY = 28.0f;
        if (playerY > HEIGHT * 0.5f) playerY = HEIGHT * 0.5f;

        // Engine exhaust particles
        if (animFrame % 2 == 0)
        {
            spawnEngineTrail(playerX - 9.0f, playerY - 8.0f, 0.95f, 0.97f, 1.0f);
            spawnEngineTrail(playerX + 9.0f, playerY - 8.0f, 0.95f, 0.97f, 1.0f);
        }

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
        if (!currentBoss.active && score >= sectorTargetScore(currentSector))
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
                    currentBoss.shootTimer = (timeSlowTimer > 0) ? 110 : 60;
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
                    currentBoss.specialAttackTimer = (timeSlowTimer > 0) ? 360 : 210;
                }
            }
        }

        // --- Wave spawner: next formation arrives only when the screen is clear (boss fights have no minions) ---
        if (!currentBoss.active)
        {
            bool anyAlive = false;
            for (int i = 0; i < MAX_ENEMIES; i++) if (enemies[i].active) anyAlive = true;
            if (!anyAlive)
            {
                if (--waveDelay <= 0) { spawnWave(); waveDelay = 60; }
            }
        }

        // --- Move Enemies & Enemy Shooting ---
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (!enemies[i].active)
                continue;

            // Always fly forward (down the screen), faster than the ground scrolls, with a gentle sway
            enemies[i].y -= enemies[i].speed * enemySpeedScale;
            float swayX = enemies[i].cx[0] + sinf(animFrame * 0.03f + enemies[i].wobbleOffset) * 40.0f;
            enemies[i].vx = swayX - enemies[i].x;
            enemies[i].x = swayX;
            if (enemies[i].x < ENEMY_WIDTH / 2.0f) enemies[i].x = ENEMY_WIDTH / 2.0f;
            if (enemies[i].x > WIDTH - ENEMY_WIDTH / 2.0f) enemies[i].x = WIDTH - ENEMY_WIDTH / 2.0f;

            // Enemy Shooting
            enemies[i].shootTimer--;
            if (enemies[i].shootTimer <= 0 && enemies[i].y < HEIGHT - 40.0f && enemies[i].y > 150.0f)
            {
                if (enemies[i].type == 1)
                {
                    // Interceptor: aim at where the player is right now (normalise the direction vector)
                    float dx = playerX - enemies[i].x, dy = (playerY + 20.0f) - enemies[i].y;
                    float len = sqrtf(dx * dx + dy * dy);
                    if (len < 1.0f) len = 1.0f;
                    spawnEnemyBullet(enemies[i].x, enemies[i].y, dx / len * 4.0f, dy / len * 4.0f);
                }
                else if (enemies[i].type == 2)
                {
                    // Bomber: 3-way fan, you must slip between the bullets
                    spawnEnemyBullet(enemies[i].x, enemies[i].y, -1.5f, -4.2f);
                    spawnEnemyBullet(enemies[i].x, enemies[i].y, 0.0f, -4.5f);
                    spawnEnemyBullet(enemies[i].x, enemies[i].y, 1.5f, -4.2f);
                }
                else
                {
                    spawnEnemyBullet(enemies[i].x, enemies[i].y);
                }
                playSfx(SFX_LASER_ENEMY, 0.35f);
                int gap = (enemies[i].type == 1) ? (80 + rand() % 60) : (110 + rand() % 80);
                gap -= currentSector * 6;               // later sectors shoot a little more often
                enemies[i].shootTimer = std::max(60, gap);
                if (timeSlowTimer > 0) enemies[i].shootTimer *= 2;
            }

            // Enemy reaches the ground: it explodes there and you lose a heart
            if (enemies[i].y < 0)
            {
                enemies[i].active = false;
                comboCount = 0;
                lives--;
                triggerShake(6.0f);
                playSfx(SFX_EXPLOSION_LARGE, 0.8f);
                spawnExplosion(enemies[i].x, 15.0f, 1.0f, 0.2f, 0.1f, 20);
                addFloatingText(enemies[i].x - 40.0f, 30.0f, "-1 HEART: ENEMY LANDED!", 1.0f, 0.25f, 0.2f);

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

    cgFlash *= 0.9f;
    glutPostRedisplay();
    glutTimerFunc(16, update, 0); // ~60 FPS
}

// ======================================================
// DISPLAY CALLBACK
// ======================================================

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);
    updateFps();

    // Background: gradient, textured planet, perspective grid, warp stars
    drawBackgroundAndStars();

    // Camera Screen Shake (a translation applied to the whole world)
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
        if (cgWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        drawPowerUps();
        drawParticles();

        if (!gameOver)
        {
            drawPlayer();
        }

        for (int i = 0; i < MAX_BULLETS; i++)
            drawBullet(bullets[i]);

        for (int i = 0; i < MAX_ENEMIES; i++)
            drawEnemy(enemies[i]);

        drawBoss(currentBoss);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        if (cgDebug) drawDebugOverlay();
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
    else if (currentGameState == STATE_CG_INFO)
    {
        drawCgInfo();
    }
    else if (currentGameState == STATE_HOW_IT_WORKS)
    {
        drawHowItWorks();
    }
    else if (currentGameState == STATE_GAME_OVER)
    {
        drawHUD();
        drawGameOver();
    }

    if (currentGameState == STATE_PLAYING || currentGameState == STATE_PAUSED)
    {
        if (cgLabPanel) { drawRadar(); drawCgLabPanel(); }
    }

    // Post-processing
    drawVignette();
    if (cgCRT) drawScanlines();
    drawScreenFlash();

    if (cgWireframe && currentGameState == STATE_PLAYING)
    {
        glColor3f(1.0f, 0.9f, 0.3f);
        renderGlText(WIDTH / 2 - 120, 24, "WIREFRAME MODE  glPolygonMode(GL_LINE)", GLUT_BITMAP_HELVETICA_12);
    }

    glutSwapBuffers();
}

// ======================================================
// INPUT HANDLING: KEYBOARD & MOUSE
// ======================================================

void keyboardDown(unsigned char key, int x, int y)
{
    if (currentGameState == STATE_HOW_IT_WORKS)
    {
        playSfx(SFX_UI_CLICK, 0.6f);
        if (key == 27) { currentGameState = STATE_MAIN_MENU; menuSelectedIdx = 0; }
        else if (key == 'a' || key == 'A') hiwPrev();
        else if (key == 'd' || key == 'D' || key == ' ' || key == 13) hiwNext();
        return;
    }

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
            if (currentGameState != STATE_PLAYING)
            {
                playSfx(SFX_UI_CLICK, 0.5f);
                menuSelectedIdx = std::max(0, menuSelectedIdx - 1);
            }
            break;

        case 's':
        case 'S':
            downPressed = true;
            if (currentGameState != STATE_PLAYING) playSfx(SFX_UI_CLICK, 0.5f);
            if (currentGameState == STATE_MAIN_MENU) menuSelectedIdx = std::min(5, menuSelectedIdx + 1);
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
                    hiwPage = 0;
                    currentGameState = STATE_HOW_IT_WORKS;
                }
                else if (menuSelectedIdx == 4)
                {
                    currentGameState = STATE_CG_INFO;
                }
                else if (menuSelectedIdx == 5)
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
            else if (currentGameState == STATE_HOW_TO_PLAY || currentGameState == STATE_CG_INFO)
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
            else if (currentGameState == STATE_HOW_TO_PLAY || currentGameState == STATE_CG_INFO)
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
    if (currentGameState == STATE_HOW_IT_WORKS)
    {
        if (key == GLUT_KEY_LEFT || key == GLUT_KEY_UP) hiwPrev();
        else if (key == GLUT_KEY_RIGHT || key == GLUT_KEY_DOWN) hiwNext();
        playSfx(SFX_UI_CLICK, 0.6f);
        return;
    }

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

        case GLUT_KEY_F1: cgLabPanel = !cgLabPanel; break;
        case GLUT_KEY_F2: cgWireframe = !cgWireframe; break;
        case GLUT_KEY_F3: cgCRT = !cgCRT; break;
        case GLUT_KEY_F4: cgDebug = !cgDebug; break;
        case GLUT_KEY_F5: cgRaster = !cgRaster; break;

        case GLUT_KEY_UP:
            upPressed = true;
            if (currentGameState != STATE_PLAYING)
            {
                playSfx(SFX_UI_CLICK, 0.5f);
                menuSelectedIdx = std::max(0, menuSelectedIdx - 1);
            }
            break;

        case GLUT_KEY_DOWN:
            downPressed = true;
            if (currentGameState != STATE_PLAYING) playSfx(SFX_UI_CLICK, 0.5f);
            if (currentGameState == STATE_MAIN_MENU) menuSelectedIdx = std::min(5, menuSelectedIdx + 1);
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
    float glX = mouseX * (float)WIDTH / (float)std::max(1, cgWinW);
    float glY = HEIGHT - mouseY * (float)HEIGHT / (float)std::max(1, cgWinH);

    if (currentGameState == STATE_MAIN_MENU)
    {
        float bw = 280.0f;
        float bh = 40.0f;
        float bx = WIDTH / 2.0f - bw / 2.0f;
        float startY = HEIGHT - 228.0f;
        float gap = 50.0f;

        for (int i = 0; i < 6; i++)
        {
            float btnY = startY - gap * i;
            if (glX >= bx && glX <= bx + bw && glY >= btnY && glY <= btnY + bh)
            {
                menuSelectedIdx = i;
                playSfx(SFX_UI_CLICK, 0.8f);
                if (i == 0) { initializeGame(); currentGameState = STATE_PLAYING; }
                else if (i == 1) { previousState = STATE_MAIN_MENU; menuSelectedIdx = 0; currentGameState = STATE_SETTINGS; }
                else if (i == 2) { currentGameState = STATE_HOW_TO_PLAY; }
                else if (i == 3) { hiwPage = 0; currentGameState = STATE_HOW_IT_WORKS; }
                else if (i == 4) { currentGameState = STATE_CG_INFO; }
                else if (i == 5) { shutdownAudio(); exit(0); }
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
    else if (currentGameState == STATE_HOW_IT_WORKS)
    {
        bool inRow = (glY >= HIW_BTN_Y && glY <= HIW_BTN_Y + HIW_BTN_H);
        if (inRow && glX >= 30.0f && glX <= 30.0f + HIW_BTN_W) { playSfx(SFX_UI_CLICK, 0.8f); hiwPrev(); }
        else if (inRow && glX >= WIDTH / 2.0f - HIW_BTN_W / 2.0f && glX <= WIDTH / 2.0f + HIW_BTN_W / 2.0f)
        { playSfx(SFX_UI_CLICK, 0.8f); currentGameState = STATE_MAIN_MENU; menuSelectedIdx = 0; }
        else if (inRow && glX >= WIDTH - 30.0f - HIW_BTN_W && glX <= WIDTH - 30.0f) { playSfx(SFX_UI_CLICK, 0.8f); hiwNext(); }
    }
    else if (currentGameState == STATE_HOW_TO_PLAY || currentGameState == STATE_CG_INFO)
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
    cgWinW = width;
    cgWinH = height;
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
    glutCreateWindow("VECTOR STORM - A Computer Graphics Showcase Shooter");

    init();
    initPlanet();                                   // procedural texture -> GPU
    menuMesh = makeBipyramid(6, 0.75f, 1.5f);       // elongated hex crystal
    gemMesh = makeBipyramid(4, 1.0f, 1.0f);         // octahedron
    heavyMesh = makeBipyramid(6, 1.0f, 0.85f);      // Heavy Raider body
    bossCoreMesh = makeBipyramid(5, 1.0f, 1.25f);   // boss reactor
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