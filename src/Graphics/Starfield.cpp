#include "Graphics/Starfield.hpp"
#include "Core/Config.hpp"
#include "Graphics/CGLab.hpp"
#include <GL/freeglut.h>
#include <cstdlib>
#include <cmath>

SectorTheme sectorThemes[NUM_SECTORS] = {
    // Sector 1: Orion Abyss (Deep Indigo / Ice Cyan)
    {
        "SECTOR 1: EARTH COAST",
        "CLEAR SKIES - GOOD LUCK, PILOT",
        0.015f, 0.015f, 0.055f,
        0.035f, 0.045f, 0.095f,
        0.1f, 0.5f, 0.9f,
        0.75f, 0.9f, 1.0f,
        1.0f,
        120,
        120
    },
    // Sector 2: Solar Flare (Fiery Crimson & Amber)
    {
        "SECTOR 2: DESERT PLANET",
        "HOT, DRY AND FASTER ENEMIES",
        0.065f, 0.015f, 0.02f,
        0.12f, 0.04f, 0.02f,
        1.0f, 0.45f, 0.1f,
        1.0f, 0.85f, 0.4f,
        1.35f,
        280,
        132
    },
    // Sector 3: Toxic Asteroid Belt (Emerald & Acid Teal)
    {
        "SECTOR 3: ICE PLANET",
        "HEAVY BOMBERS APPEAR",
        0.012f, 0.045f, 0.03f,
        0.025f, 0.08f, 0.05f,
        0.1f, 0.95f, 0.45f,
        0.4f, 1.0f, 0.7f,
        1.15f,
        480,
        126
    },
    // Sector 4: Cyber Warp Zone (Synthwave Magenta & Cyan)
    {
        "SECTOR 4: VOLCANO PLANET",
        "DANGEROUS SKIES",
        0.045f, 0.012f, 0.07f,
        0.08f, 0.02f, 0.11f,
        0.95f, 0.15f, 0.85f,
        0.9f, 0.5f, 1.0f,
        2.2f,
        720,
        140
    },
    // Sector 5: Supernova Core (Omega Sector)
    {
        "SECTOR 5: DEEP SPACE",
        "DEFEAT THE FLAGSHIP",
        0.07f, 0.05f, 0.015f,
        0.12f, 0.08f, 0.03f,
        1.0f, 0.85f, 0.2f,
        1.0f, 0.95f, 0.7f,
        1.6f,
        1000,
        145
    }
};

int currentSector = 0;
int sectorAnnouncementTimer = 0;

// ======================================================================
// CARTOON 2D WORLDS  (one planet per sector, scrolling top -> bottom)
//   sector 1 Earth coast  : grass, sea, path, trees, houses, boats
//   sector 2 Desert world : sand, oasis lake, cacti, huts, rocks
//   sector 3 Ice planet   : snow, frozen sea, pines, igloos, icebergs
//   sector 4 Volcano world: ash ground, lava river, volcanoes, dark rocks
//   sector 5 Deep space   : stars, planets, asteroids
// Scenery is placed by a hash of the cell number, so the world is endless.
// ======================================================================

#include "Graphics/Renderer.hpp"
#include <algorithm>

static float sceneScroll = 0.0f;
static const float YMAX = 640.0f;

struct Cloud { float x, y, scale, speed, tint; };
static const int MAX_CLOUDS = 7;
static Cloud clouds[MAX_CLOUDS];

// colours of each planet's ground, bands, shore, water (top/bottom), path, ripples, clouds
struct Pal { float ground[3], band[3], shore[3], w1[3], w2[3], path[3], rip[3], cloud[3]; };
static const Pal PAL[4] = {
    // Earth
    {{0.40f, 0.72f, 0.28f}, {0.52f, 0.84f, 0.34f}, {0.94f, 0.85f, 0.56f}, {0.35f, 0.65f, 0.95f},
     {0.14f, 0.42f, 0.86f}, {0.86f, 0.68f, 0.42f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
    // Desert
    {{0.90f, 0.76f, 0.48f}, {0.95f, 0.83f, 0.56f}, {0.78f, 0.55f, 0.32f}, {0.25f, 0.78f, 0.75f},
     {0.10f, 0.52f, 0.62f}, {0.78f, 0.60f, 0.36f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.94f, 0.82f}},
    // Ice
    {{0.86f, 0.93f, 1.00f}, {0.95f, 0.98f, 1.00f}, {0.70f, 0.85f, 0.95f}, {0.30f, 0.55f, 0.85f},
     {0.12f, 0.30f, 0.62f}, {0.72f, 0.82f, 0.92f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},
    // Volcano
    {{0.30f, 0.20f, 0.19f}, {0.36f, 0.25f, 0.22f}, {0.55f, 0.20f, 0.10f}, {1.00f, 0.60f, 0.10f},
     {0.85f, 0.20f, 0.05f}, {0.20f, 0.14f, 0.14f}, {1.0f, 0.90f, 0.40f}, {0.40f, 0.36f, 0.36f}}
};

static void col3(const float* c) { glColor3f(c[0], c[1], c[2]); }

static unsigned hashCell(int c, int salt)
{
    unsigned h = (unsigned)c * 2654435761u + (unsigned)salt * 40503u;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
    return h;
}

static float seaEdge(float wy)    { return 590.0f + 42.0f * sinf(wy * 0.0070f) + 22.0f * sinf(wy * 0.0190f); }
static float pathCenter(float wy) { return 215.0f + 70.0f * sinf(wy * 0.0050f + 1.0f); }

void initStars()
{
    initWarp();
    sceneScroll = 0.0f;
    for (int i = 0; i < MAX_CLOUDS; i++)
    {
        clouds[i].x = (float)(rand() % WIDTH);
        clouds[i].y = (float)(rand() % (HEIGHT + 200));
        clouds[i].scale = 0.8f + (rand() % 50) / 100.0f;
        clouds[i].speed = 0.7f + (rand() % 40) / 100.0f;
        clouds[i].tint = (rand() % 2) ? 1.0f : 0.0f;
    }
}

void updateStars()
{
    SectorTheme &thm = sectorThemes[currentSector % NUM_SECTORS];
    float speedMult = 0.8f + 0.2f * thm.starSpeedMult;
    sceneScroll += 0.7f * speedMult;   // slower than any enemy, so enemies always move forward over the ground

    for (int i = 0; i < MAX_CLOUDS; i++)
    {
        clouds[i].y -= clouds[i].speed * 1.6f * speedMult;
        if (clouds[i].y < -80.0f)
        {
            clouds[i].y = HEIGHT + 80.0f;
            clouds[i].x = (float)(rand() % WIDTH);
        }
    }
}

// ---------------------------------------------------------------- scenery pieces
static void shadow(float x, float y, float r)
{
    glColor4f(0.0f, 0.0f, 0.0f, 0.18f);
    drawCircleFill(x + 6.0f, y - 5.0f, r, 14);
}

static void tree(float x, float y, float s)
{
    shadow(x, y, 17.0f * s);
    glColor3f(0.45f, 0.28f, 0.12f);
    glBegin(GL_QUADS);
        glVertex2f(x - 3.5f * s, y - 16.0f * s); glVertex2f(x + 3.5f * s, y - 16.0f * s);
        glVertex2f(x + 3.5f * s, y);             glVertex2f(x - 3.5f * s, y);
    glEnd();
    glColor3f(0.13f, 0.55f, 0.18f);
    drawCircleFill(x, y + 6.0f * s, 17.0f * s, 20);
    glColor3f(0.25f, 0.78f, 0.25f);
    drawCircleFill(x - 2.0f * s, y + 8.0f * s, 13.0f * s, 20);
}

static void pine(float x, float y, float s)
{
    shadow(x, y - 6.0f * s, 14.0f * s);
    glColor3f(0.40f, 0.26f, 0.14f);
    glBegin(GL_QUADS);
        glVertex2f(x - 3.0f * s, y - 18.0f * s); glVertex2f(x + 3.0f * s, y - 18.0f * s);
        glVertex2f(x + 3.0f * s, y - 6.0f * s);  glVertex2f(x - 3.0f * s, y - 6.0f * s);
    glEnd();
    for (int k = 0; k < 3; k++)
    {
        float by = y - 8.0f * s + k * 11.0f * s, w = (17.0f - k * 4.0f) * s;
        glColor3f(0.10f, 0.40f, 0.28f);
        glBegin(GL_TRIANGLES);
            glVertex2f(x - w, by); glVertex2f(x + w, by); glVertex2f(x, by + 17.0f * s);
        glEnd();
        glColor3f(0.95f, 0.98f, 1.0f);                     // snow cap
        glBegin(GL_TRIANGLES);
            glVertex2f(x - w * 0.45f, by + 9.0f * s); glVertex2f(x + w * 0.45f, by + 9.0f * s); glVertex2f(x, by + 17.0f * s);
        glEnd();
    }
}

static void cactus(float x, float y, float s)
{
    shadow(x, y, 12.0f * s);
    glColor3f(0.15f, 0.55f, 0.25f);
    glBegin(GL_QUADS);
        glVertex2f(x - 4.5f * s, y - 14.0f * s); glVertex2f(x + 4.5f * s, y - 14.0f * s);
        glVertex2f(x + 4.5f * s, y + 14.0f * s); glVertex2f(x - 4.5f * s, y + 14.0f * s);
        glVertex2f(x - 13.0f * s, y - 2.0f * s); glVertex2f(x - 4.5f * s, y - 2.0f * s);
        glVertex2f(x - 4.5f * s, y + 3.0f * s);  glVertex2f(x - 13.0f * s, y + 3.0f * s);
        glVertex2f(x + 4.5f * s, y + 4.0f * s);  glVertex2f(x + 13.0f * s, y + 4.0f * s);
        glVertex2f(x + 13.0f * s, y + 9.0f * s); glVertex2f(x + 4.5f * s, y + 9.0f * s);
    glEnd();
    drawCircleFill(x, y + 14.0f * s, 4.5f * s, 10);
    glColor3f(0.45f, 0.85f, 0.45f);
    glBegin(GL_QUADS);
        glVertex2f(x - 1.0f * s, y - 12.0f * s); glVertex2f(x + 1.0f * s, y - 12.0f * s);
        glVertex2f(x + 1.0f * s, y + 12.0f * s); glVertex2f(x - 1.0f * s, y + 12.0f * s);
    glEnd();
}

static void houseC(float x, float y, const float* wall, const float* roof)
{
    glColor4f(0.0f, 0.0f, 0.0f, 0.18f);
    glBegin(GL_QUADS);
        glVertex2f(x - 26.0f, y - 22.0f); glVertex2f(x + 36.0f, y - 22.0f);
        glVertex2f(x + 36.0f, y + 20.0f); glVertex2f(x - 26.0f, y + 20.0f);
    glEnd();
    col3(wall);
    glBegin(GL_QUADS);
        glVertex2f(x - 28.0f, y - 18.0f); glVertex2f(x + 28.0f, y - 18.0f);
        glVertex2f(x + 28.0f, y + 14.0f); glVertex2f(x - 28.0f, y + 14.0f);
    glEnd();
    col3(roof);
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 34.0f, y + 12.0f); glVertex2f(x + 34.0f, y + 12.0f); glVertex2f(x, y + 36.0f);
    glEnd();
    glColor3f(0.4f, 0.22f, 0.08f);
    glBegin(GL_QUADS);
        glVertex2f(x - 6.0f, y - 18.0f); glVertex2f(x + 6.0f, y - 18.0f);
        glVertex2f(x + 6.0f, y + 2.0f);  glVertex2f(x - 6.0f, y + 2.0f);
    glEnd();
    glColor3f(1.0f, 0.9f, 0.4f);
    glBegin(GL_QUADS);
        glVertex2f(x + 14.0f, y - 6.0f); glVertex2f(x + 22.0f, y - 6.0f);
        glVertex2f(x + 22.0f, y + 4.0f); glVertex2f(x + 14.0f, y + 4.0f);
    glEnd();
}

static void igloo(float x, float y)
{
    shadow(x, y, 24.0f);
    glColor3f(0.70f, 0.82f, 0.95f);
    drawCircleFill(x, y, 24.0f, 24);
    glColor3f(0.97f, 0.99f, 1.0f);
    drawCircleFill(x - 2.0f, y + 2.0f, 21.0f, 24);
    glColor3f(0.25f, 0.40f, 0.65f);
    drawCircleFill(x, y - 10.0f, 7.0f, 14);
}

static void flowers(float x, float y)
{
    static const float cols[3][3] = {{1.0f, 0.25f, 0.2f}, {1.0f, 0.85f, 0.15f}, {0.95f, 0.4f, 0.8f}};
    for (int k = 0; k < 3; k++)
    {
        glColor3f(cols[k][0], cols[k][1], cols[k][2]);
        drawCircleFill(x + (k - 1) * 11.0f, y + (k == 1 ? 5.0f : -2.0f), 4.0f, 10);
        glColor3f(1.0f, 1.0f, 0.9f);
        drawCircleFill(x + (k - 1) * 11.0f, y + (k == 1 ? 5.0f : -2.0f), 1.6f, 6);
    }
}

static void bush(float x, float y)
{
    glColor3f(0.12f, 0.50f, 0.20f);
    drawCircleFill(x - 7.0f, y, 8.0f, 14); drawCircleFill(x + 7.0f, y, 8.0f, 14);
    glColor3f(0.22f, 0.70f, 0.28f);
    drawCircleFill(x, y + 4.0f, 10.0f, 16);
    glColor3f(0.35f, 0.82f, 0.38f);
    drawCircleFill(x - 3.0f, y + 6.0f, 5.0f, 12);
}

static void rockC(float x, float y, float r, float cr, float cg, float cb)
{
    shadow(x, y, r);
    glColor3f(cr * 0.75f, cg * 0.75f, cb * 0.75f);
    drawCircleFill(x, y, r, 12);
    glColor3f(cr, cg, cb);
    drawCircleFill(x - r * 0.25f, y + r * 0.25f, r * 0.6f, 10);
}

static void volcano(float x, float y, float s)
{
    shadow(x, y - 4.0f * s, 26.0f * s);
    glColor3f(0.32f, 0.20f, 0.18f);
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 30.0f * s, y - 16.0f * s); glVertex2f(x + 30.0f * s, y - 16.0f * s); glVertex2f(x, y + 22.0f * s);
    glEnd();
    glColor3f(0.45f, 0.28f, 0.22f);
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 4.0f * s, y - 16.0f * s); glVertex2f(x + 30.0f * s, y - 16.0f * s); glVertex2f(x, y + 22.0f * s);
    glEnd();
    glColor3f(1.0f, 0.55f, 0.1f);                           // glowing crater
    drawCircleFill(x, y + 20.0f * s, 7.0f * s, 12);
    glColor3f(1.0f, 0.9f, 0.3f);
    drawCircleFill(x, y + 21.0f * s, 3.5f * s, 10);
}

static void ripple(float x, float y, const float* c)
{
    glColor4f(c[0], c[1], c[2], 0.55f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(x - 10.0f, y); glVertex2f(x - 2.0f, y + 2.0f);
        glVertex2f(x + 2.0f, y + 2.0f); glVertex2f(x + 10.0f, y);
    glEnd();
    glLineWidth(1.0f);
}

static void boat(float x, float y)
{
    glColor3f(0.25f, 0.18f, 0.12f);
    glBegin(GL_POLYGON);
        glVertex2f(x - 22.0f, y + 6.0f); glVertex2f(x + 22.0f, y + 6.0f);
        glVertex2f(x + 14.0f, y - 6.0f); glVertex2f(x - 14.0f, y - 6.0f);
    glEnd();
    glColor3f(0.95f, 0.95f, 0.9f);
    glBegin(GL_TRIANGLES);
        glVertex2f(x, y + 28.0f); glVertex2f(x - 12.0f, y + 6.0f); glVertex2f(x + 12.0f, y + 6.0f);
    glEnd();
}

static void iceberg(float x, float y)
{
    glColor3f(0.75f, 0.90f, 1.0f);
    glBegin(GL_POLYGON);
        glVertex2f(x - 22.0f, y - 8.0f); glVertex2f(x - 8.0f, y + 16.0f); glVertex2f(x + 4.0f, y + 6.0f);
        glVertex2f(x + 16.0f, y + 20.0f); glVertex2f(x + 26.0f, y - 8.0f);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_TRIANGLES);
        glVertex2f(x - 8.0f, y + 16.0f); glVertex2f(x - 14.0f, y + 2.0f); glVertex2f(x - 2.0f, y + 4.0f);
    glEnd();
}

static void cloud(float x, float y, float s, const float* c, float a)
{
    glColor4f(c[0], c[1], c[2], a);
    drawCircleFill(x - 28.0f * s, y - 3.0f * s, 19.0f * s, 18);
    drawCircleFill(x - 8.0f * s,  y + 8.0f * s, 24.0f * s, 18);
    drawCircleFill(x + 16.0f * s, y + 4.0f * s, 21.0f * s, 18);
    drawCircleFill(x + 34.0f * s, y - 4.0f * s, 16.0f * s, 18);
    glBegin(GL_QUADS);
        glVertex2f(x - 38.0f * s, y - 18.0f * s); glVertex2f(x + 44.0f * s, y - 18.0f * s);
        glVertex2f(x + 44.0f * s, y - 2.0f * s);  glVertex2f(x - 38.0f * s, y - 2.0f * s);
    glEnd();
}

// ---------------------------------------------------------------- deep space
static void planet(float x, float y, float r, float pr, float pg, float pb, bool ring)
{
    if (ring)
    {
        glColor4f(0.9f, 0.85f, 0.7f, 0.8f);
        glLineWidth(3.0f);
        glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 40; i++) { float t = i * 6.2831853f / 40.0f; glVertex2f(x + cosf(t) * r * 1.7f, y + sinf(t) * r * 0.45f); }
        glEnd();
        glLineWidth(1.0f);
    }
    glColor3f(pr * 0.55f, pg * 0.55f, pb * 0.55f);          // shadow side
    drawCircleFill(x, y, r, 32);
    glColor3f(pr, pg, pb);                                  // lit side
    drawCircleFill(x - r * 0.12f, y + r * 0.12f, r * 0.88f, 32);
    glColor3f(std::min(1.0f, pr + 0.3f), std::min(1.0f, pg + 0.3f), std::min(1.0f, pb + 0.3f));
    drawCircleFill(x - r * 0.35f, y + r * 0.35f, r * 0.3f, 16);   // highlight
    glColor4f(1.0f, 1.0f, 1.0f, 0.18f);                     // bands
    glBegin(GL_QUADS);
        glVertex2f(x - r * 0.9f, y - r * 0.15f); glVertex2f(x + r * 0.9f, y - r * 0.15f);
        glVertex2f(x + r * 0.9f, y + r * 0.0f);  glVertex2f(x - r * 0.9f, y + r * 0.0f);
    glEnd();
    if (ring)
    {
        glColor4f(0.9f, 0.85f, 0.7f, 0.9f);
        glLineWidth(3.0f);
        glBegin(GL_LINE_STRIP);
            for (int i = 20; i <= 40; i++) { float t = i * 6.2831853f / 40.0f; glVertex2f(x + cosf(t) * r * 1.7f, y + sinf(t) * r * 0.45f); }
        glEnd();
        glLineWidth(1.0f);
    }
}

static void drawSpace()
{
    glBegin(GL_QUADS);
        glColor3f(0.02f, 0.03f, 0.10f); glVertex2f(0, HEIGHT); glVertex2f(WIDTH, HEIGHT);
        glColor3f(0.10f, 0.05f, 0.22f); glVertex2f(WIDTH, 0);  glVertex2f(0, 0);
    glEnd();

    // distant nebula glow
    for (int i = 0; i < 3; i++)
    {
        float ny = fmodf(i * 260.0f - sceneScroll * 0.15f + 2000.0f, 900.0f) - 150.0f;
        glBegin(GL_TRIANGLE_FAN);
            glColor4f(0.45f, 0.2f, 0.7f, 0.22f); glVertex2f(150.0f + i * 260.0f, ny);
            glColor4f(0.45f, 0.2f, 0.7f, 0.0f);
            for (int a = 0; a <= 20; a++) glVertex2f(150.0f + i * 260.0f + cosf(a * 0.314159f) * 190.0f, ny + sinf(a * 0.314159f) * 160.0f);
        glEnd();
    }

    // two star layers (far = slow and dim, near = faster and bright)
    glEnable(GL_POINT_SMOOTH);
    for (int layer = 0; layer < 2; layer++)
    {
        float sp = layer == 0 ? 0.35f : 0.8f, cell = 100.0f;
        int c0 = (int)floorf(sceneScroll * sp / cell) - 1;
        glPointSize(layer == 0 ? 1.5f : 2.6f);
        glBegin(GL_POINTS);
        for (int c = c0; c <= c0 + 8; c++)
            for (int k = 0; k < 5; k++)
            {
                unsigned h = hashCell(c * 7 + k, 11 + layer);
                float x = (h % 800), y = c * cell + ((h >> 10) % 100) - sceneScroll * sp;
                float tw = 0.6f + 0.4f * sinf(sceneScroll * 0.05f + (float)(h % 50));
                glColor4f(0.85f, 0.92f, 1.0f, (layer == 0 ? 0.55f : 0.95f) * tw);
                glVertex2f(x, y);
            }
        glEnd();
    }

    // planets and asteroids
    const float CELL = 280.0f;
    int c0 = (int)floorf(sceneScroll * 0.8f / CELL) - 1;
    static const float pc[5][3] = {{0.95f, 0.55f, 0.25f}, {0.25f, 0.75f, 0.80f}, {0.70f, 0.35f, 0.85f}, {0.85f, 0.30f, 0.30f}, {0.40f, 0.80f, 0.40f}};
    for (int c = c0; c <= c0 + 4; c++)
    {
        unsigned h1 = hashCell(c, 21), h2 = hashCell(c, 22), h3 = hashCell(c, 23);
        float sy = (c + 0.5f) * CELL - sceneScroll * 0.8f;
        if (h1 % 10 < 7)
        {
            int ci = (int)(h2 % 5);
            planet(70.0f + (h3 % 660), sy, 22.0f + (h1 % 30), pc[ci][0], pc[ci][1], pc[ci][2], (h2 % 3) == 0);
        }
        for (int k = 0; k < 3; k++)
        {
            unsigned ha = hashCell(c * 5 + k, 31);
            rockC((float)(ha % 800), sy + 120.0f + (ha >> 12) % 100, 5.0f + (ha % 6), 0.55f, 0.52f, 0.5f);
        }
    }
}

// ---------------------------------------------------------------- main background
void drawBackgroundAndStars()
{
    int sec = currentSector % NUM_SECTORS;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (sec == 4) { drawSpace(); return; }
    const Pal& P = PAL[sec];

    // 1) ground with light/dark bands
    col3(P.ground);
    glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(WIDTH, 0); glVertex2f(WIDTH, YMAX); glVertex2f(0, YMAX);
    glEnd();
    glColor4f(P.band[0], P.band[1], P.band[2], 0.55f);
    for (float y = -fmodf(sceneScroll, 160.0f); y < YMAX; y += 160.0f)
    {
        glBegin(GL_QUADS);
            glVertex2f(0, y); glVertex2f(WIDTH, y); glVertex2f(WIDTH, y + 80.0f); glVertex2f(0, y + 80.0f);
        glEnd();
    }

    // 2) shore + water/lava
    glBegin(GL_TRIANGLE_STRIP);
    for (float y = -12.0f; y <= YMAX; y += 12.0f)
    {
        float e = seaEdge(y + sceneScroll);
        col3(P.shore); glVertex2f(e - 16.0f, y);
        col3(P.shore); glVertex2f(WIDTH + 10.0f, y);
    }
    glEnd();
    glBegin(GL_TRIANGLE_STRIP);
    for (float y = -12.0f; y <= YMAX; y += 12.0f)
    {
        float e = seaEdge(y + sceneScroll);
        col3(P.w1); glVertex2f(e, y);
        col3(P.w2); glVertex2f(WIDTH + 10.0f, y);
    }
    glEnd();

    // 3) winding path
    col3(P.path);
    glBegin(GL_TRIANGLE_STRIP);
    for (float y = -12.0f; y <= YMAX; y += 12.0f)
    {
        float c = pathCenter(y + sceneScroll);
        glVertex2f(c - 18.0f, y); glVertex2f(c + 18.0f, y);
    }
    glEnd();

    // 4) scenery, one cell per 130px of world height
    static const float huts[2][3] = {{0.85f, 0.70f, 0.45f}, {0.55f, 0.35f, 0.20f}};
    static const float redHouse[2][3] = {{0.62f, 0.38f, 0.18f}, {0.78f, 0.22f, 0.18f}};
    const float CELL = 130.0f;
    int c0 = (int)floorf(sceneScroll / CELL) - 1;
    for (int c = c0; c <= c0 + (int)(YMAX / CELL) + 2; c++)
    {
        float wy = (c + 0.5f) * CELL;
        float sy = wy - sceneScroll;
        float pc = pathCenter(wy), se = seaEdge(wy);
        unsigned h1 = hashCell(c, 1), h2 = hashCell(c, 2), h3 = hashCell(c, 3);

        float lx = 28.0f + (h1 % 1000) / 1000.0f * std::max(10.0f, pc - 90.0f);
        int kind = (int)(h2 % 10);
        float sz = 0.9f + (h3 % 40) / 100.0f;
        if (sec == 0)
        {
            if (kind < 5) tree(lx, sy, sz);
            else if (kind < 7) houseC(lx + 10.0f, sy, redHouse[0], redHouse[1]);
            else if (kind < 8) flowers(lx, sy);
            else if (kind < 9) bush(lx, sy);
            else rockC(lx, sy, 9.0f, 0.62f, 0.64f, 0.68f);
        }
        else if (sec == 1)
        {
            if (kind < 4) cactus(lx, sy, sz);
            else if (kind < 7) rockC(lx, sy, 10.0f, 0.70f, 0.52f, 0.36f);
            else if (kind < 9) houseC(lx + 10.0f, sy, huts[0], huts[1]);
            else cactus(lx, sy, 0.8f);
        }
        else if (sec == 2)
        {
            if (kind < 5) pine(lx, sy, sz);
            else if (kind < 7) igloo(lx, sy);
            else rockC(lx, sy, 10.0f, 0.88f, 0.93f, 0.98f);
        }
        else
        {
            if (kind < 4) rockC(lx, sy, 11.0f, 0.32f, 0.24f, 0.24f);
            else if (kind < 8) volcano(lx, sy, sz * 0.9f);
            else rockC(lx, sy, 8.0f, 0.45f, 0.30f, 0.26f);
        }

        // between the path and the shore
        float gap = (se - 40.0f) - (pc + 50.0f);
        if (gap > 30.0f)
        {
            float rx = pc + 50.0f + (h3 % 1000) / 1000.0f * gap;
            int k2 = (int)(h1 % 10);
            if (sec == 0) { if (k2 < 4) tree(rx, sy + 40.0f, 0.8f + (h2 % 30) / 100.0f); else if (k2 < 6) flowers(rx, sy + 40.0f); }
            else if (sec == 1) { if (k2 < 4) cactus(rx, sy + 40.0f, 0.85f); else if (k2 < 6) rockC(rx, sy + 40.0f, 8.0f, 0.70f, 0.52f, 0.36f); }
            else if (sec == 2) { if (k2 < 4) pine(rx, sy + 40.0f, 0.85f); else if (k2 < 6) rockC(rx, sy + 40.0f, 8.0f, 0.88f, 0.93f, 0.98f); }
            else { if (k2 < 5) rockC(rx, sy + 40.0f, 9.0f, 0.32f, 0.24f, 0.24f); }
        }

        // ripples + floating objects on the water / lava
        if (h3 % 2 == 0) ripple(se + 30.0f + (h2 % 1000) / 1000.0f * (WIDTH - se - 60.0f), sy - 30.0f, P.rip);
        if (h1 % 3 == 0) ripple(se + 30.0f + (h3 % 1000) / 1000.0f * (WIDTH - se - 60.0f), sy + 40.0f, P.rip);
        if (h2 % 4 == 0)
        {
            float bx = se + 50.0f + (h1 % 1000) / 1000.0f * std::max(20.0f, WIDTH - se - 100.0f);
            if (sec == 0) boat(bx, sy + 20.0f);
            else if (sec == 2) iceberg(bx, sy + 20.0f);
            else if (sec == 3) rockC(bx, sy + 20.0f, 12.0f, 0.25f, 0.18f, 0.18f);
        }
    }

    // 5) clouds / ash drifting past
    for (int i = 0; i < MAX_CLOUDS; i++)
        cloud(clouds[i].x, clouds[i].y, clouds[i].scale, P.cloud, sec == 3 ? 0.80f : 0.95f);
}
