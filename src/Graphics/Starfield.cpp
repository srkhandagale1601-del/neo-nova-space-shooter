#include "Graphics/Starfield.hpp"
#include "Core/Config.hpp"
#include <GL/freeglut.h>
#include <cstdlib>
#include <cmath>

SectorTheme sectorThemes[NUM_SECTORS] = {
    // Sector 1: Orion Abyss (Deep Indigo / Ice Cyan)
    {
        "SECTOR 1: ORION ABYSS",
        "DEEP SPACE RECONNAISSANCE",
        0.015f, 0.015f, 0.055f,
        0.035f, 0.045f, 0.095f,
        0.1f, 0.5f, 0.9f,
        0.75f, 0.9f, 1.0f,
        1.0f,
        350,
        120
    },
    // Sector 2: Solar Flare (Fiery Crimson & Amber)
    {
        "SECTOR 2: SOLAR FLARE",
        "SOLAR STORM HAZARD ZONE",
        0.065f, 0.015f, 0.02f,
        0.12f, 0.04f, 0.02f,
        1.0f, 0.45f, 0.1f,
        1.0f, 0.85f, 0.4f,
        1.35f,
        850,
        132
    },
    // Sector 3: Toxic Asteroid Belt (Emerald & Acid Teal)
    {
        "SECTOR 3: ACID VOID",
        "BIO-CORROSIVE ASTEROID EXPANSE",
        0.012f, 0.045f, 0.03f,
        0.025f, 0.08f, 0.05f,
        0.1f, 0.95f, 0.45f,
        0.4f, 1.0f, 0.7f,
        1.15f,
        1500,
        126
    },
    // Sector 4: Cyber Warp Zone (Synthwave Magenta & Cyan)
    {
        "SECTOR 4: WARP STREAM",
        "HYPER-SPACE QUANTUM CORRIDOR",
        0.045f, 0.012f, 0.07f,
        0.08f, 0.02f, 0.11f,
        0.95f, 0.15f, 0.85f,
        0.9f, 0.5f, 1.0f,
        2.2f,
        2400,
        140
    },
    // Sector 5: Supernova Core (Omega Sector)
    {
        "SECTOR 5: OMEGA CORE",
        "APEX FLAGSHIP DETECTED",
        0.07f, 0.05f, 0.015f,
        0.12f, 0.08f, 0.03f,
        1.0f, 0.85f, 0.2f,
        1.0f, 0.95f, 0.7f,
        1.6f,
        3600,
        145
    }
};

int currentSector = 0;
int sectorAnnouncementTimer = 0;

const int MAX_STARS = 140;

struct Star
{
    float x;
    float y;
    float speed;
    float size;
    float brightness;
};

static Star stars[MAX_STARS];

const int MAX_NEBULAS = 6;
struct NebulaCloud
{
    float x;
    float y;
    float vy;
    float radius;
    float r, g, b, alpha;
};
static NebulaCloud nebulas[MAX_NEBULAS];

void initStars()
{
    for (int i = 0; i < MAX_STARS; i++)
    {
        stars[i].x = (float)(rand() % WIDTH);
        stars[i].y = (float)(rand() % HEIGHT);

        int layer = rand() % 3;
        if (layer == 0)
        {
            stars[i].speed = 0.6f + (rand() % 6) / 10.0f;
            stars[i].size = 1.0f;
            stars[i].brightness = 0.45f + (rand() % 3) / 10.0f;
        }
        else if (layer == 1)
        {
            stars[i].speed = 1.6f + (rand() % 8) / 10.0f;
            stars[i].size = 2.0f;
            stars[i].brightness = 0.75f + (rand() % 3) / 10.0f;
        }
        else
        {
            stars[i].speed = 3.2f + (rand() % 16) / 10.0f;
            stars[i].size = 3.0f;
            stars[i].brightness = 1.0f;
        }
    }

    for (int i = 0; i < MAX_NEBULAS; i++)
    {
        nebulas[i].x = (float)(rand() % WIDTH);
        nebulas[i].y = (float)(rand() % HEIGHT);
        nebulas[i].vy = -0.35f - (rand() % 25) / 100.0f;
        nebulas[i].radius = 120.0f + (rand() % 100);
        nebulas[i].alpha = 0.08f + (rand() % 8) / 100.0f;
    }
}

void updateStars()
{
    SectorTheme &thm = sectorThemes[currentSector % NUM_SECTORS];
    float speedMult = thm.starSpeedMult;

    for (int i = 0; i < MAX_STARS; i++)
    {
        stars[i].y -= stars[i].speed * speedMult;
        if (stars[i].y < 0)
        {
            stars[i].y = (float)HEIGHT;
            stars[i].x = (float)(rand() % WIDTH);
        }
    }

    for (int i = 0; i < MAX_NEBULAS; i++)
    {
        nebulas[i].y += nebulas[i].vy * speedMult;
        if (nebulas[i].y < -nebulas[i].radius)
        {
            nebulas[i].y = HEIGHT + nebulas[i].radius;
            nebulas[i].x = (float)(rand() % WIDTH);
        }
    }
}

void drawBackgroundAndStars()
{
    SectorTheme &thm = sectorThemes[currentSector % NUM_SECTORS];

    glBegin(GL_QUADS);
        glColor3f(thm.bgTopR, thm.bgTopG, thm.bgTopB);
        glVertex2f(0.0f, (float)HEIGHT);
        glVertex2f((float)WIDTH, (float)HEIGHT);
        glColor3f(thm.bgBotR, thm.bgBotG, thm.bgBotB);
        glVertex2f((float)WIDTH, 0.0f);
        glVertex2f(0.0f, 0.0f);
    glEnd();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (int i = 0; i < MAX_NEBULAS; i++)
    {
        glColor4f(thm.nebulaR, thm.nebulaG, thm.nebulaB, nebulas[i].alpha);
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(nebulas[i].x, nebulas[i].y);
            glColor4f(thm.nebulaR * 0.5f, thm.nebulaG * 0.5f, thm.nebulaB * 0.5f, 0.0f);
            for (int a = 0; a <= 16; a++)
            {
                float angle = a * 2.0f * 3.14159265f / 16.0f;
                glVertex2f(nebulas[i].x + cosf(angle) * nebulas[i].radius,
                           nebulas[i].y + sinf(angle) * nebulas[i].radius);
            }
        glEnd();
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < MAX_STARS; i++)
    {
        glColor4f(thm.starR, thm.starG, thm.starB, stars[i].brightness);
        glPointSize(stars[i].size);

        glBegin(GL_POINTS);
            glVertex2f(stars[i].x, stars[i].y);
        glEnd();
    }
}
