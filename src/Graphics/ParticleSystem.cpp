#include "Graphics/ParticleSystem.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/CGLab.hpp"
#include <GL/freeglut.h>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <algorithm>

static Particle particles[MAX_PARTICLES];
static FloatingText floatingTexts[MAX_FLOATING_TEXTS];

void initParticles()
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        particles[i].active = false;
    }
}

void spawnExplosion(float x, float y, float r, float g, float b, int count, float speedMultiplier)
{
    // Every explosion is also a dynamic point light
    if (count >= 12) addLightFlash(x, y, 28.0f + count * 1.8f * speedMultiplier, r, g, b);
    int spawned = 0;
    for (int i = 0; i < MAX_PARTICLES && spawned < count; i++)
    {
        if (!particles[i].active)
        {
            particles[i].active = true;
            particles[i].x = x;
            particles[i].y = y;

            float angle = (float)(rand() % 360) * 3.14159265f / 180.0f;
            float speed = (1.2f + (rand() % 55) / 10.0f) * speedMultiplier;

            particles[i].vx = cosf(angle) * speed;
            particles[i].vy = sinf(angle) * speed;

            particles[i].maxLife = 22 + rand() % 28;
            particles[i].life = particles[i].maxLife;
            particles[i].size = 2.5f + (rand() % 35) / 10.0f;

            particles[i].r = std::min(1.0f, r + (rand() % 20 - 10) / 100.0f);
            particles[i].g = std::min(1.0f, g + (rand() % 20 - 10) / 100.0f);
            particles[i].b = std::min(1.0f, b + (rand() % 20 - 10) / 100.0f);

            spawned++;
        }
    }
}

void spawnEngineTrail(float x, float y, float r, float g, float b, float vyOffset)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (!particles[i].active)
        {
            particles[i].active = true;
            particles[i].x = x + (rand() % 7 - 3);
            particles[i].y = y;
            particles[i].vx = (rand() % 11 - 5) / 10.0f;
            particles[i].vy = vyOffset - (rand() % 20) / 10.0f;
            particles[i].maxLife = 12 + rand() % 10;
            particles[i].life = particles[i].maxLife;
            particles[i].size = 3.0f + (rand() % 20) / 10.0f;
            particles[i].r = r;
            particles[i].g = g;
            particles[i].b = b;
            break;
        }
    }
}

void updateParticles()
{
    updateLightFlashes();
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (particles[i].active)
        {
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;

            particles[i].vx *= 0.95f;
            particles[i].vy *= 0.95f;

            particles[i].life--;
            if (particles[i].life <= 0)
            {
                particles[i].active = false;
            }
        }
    }
}

void drawParticles()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (particles[i].active)
        {
            float alpha = (float)particles[i].life / (float)particles[i].maxLife;
            glColor4f(particles[i].r, particles[i].g, particles[i].b, alpha);
            glPointSize(particles[i].size * alpha);

            glBegin(GL_POINTS);
                glVertex2f(particles[i].x, particles[i].y);
            glEnd();
        }
    }

    drawLightFlashes();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void initFloatingTexts()
{
    for (int i = 0; i < MAX_FLOATING_TEXTS; i++)
    {
        floatingTexts[i].active = false;
    }
}

void addFloatingText(float x, float y, const char* str, float r, float g, float b)
{
    for (int i = 0; i < MAX_FLOATING_TEXTS; i++)
    {
        if (!floatingTexts[i].active)
        {
            floatingTexts[i].active = true;
            floatingTexts[i].x = x;
            floatingTexts[i].y = y;
            strncpy(floatingTexts[i].text, str, 39);
            floatingTexts[i].text[39] = '\0';
            floatingTexts[i].r = r;
            floatingTexts[i].g = g;
            floatingTexts[i].b = b;
            floatingTexts[i].maxLife = 45;
            floatingTexts[i].life = 45;
            break;
        }
    }
}

void updateFloatingTexts()
{
    for (int i = 0; i < MAX_FLOATING_TEXTS; i++)
    {
        if (floatingTexts[i].active)
        {
            floatingTexts[i].y += 0.85f;
            floatingTexts[i].life--;
            if (floatingTexts[i].life <= 0)
            {
                floatingTexts[i].active = false;
            }
        }
    }
}

void drawFloatingTexts()
{
    for (int i = 0; i < MAX_FLOATING_TEXTS; i++)
    {
        if (floatingTexts[i].active)
        {
            float alpha = (float)floatingTexts[i].life / (float)floatingTexts[i].maxLife;
            glColor4f(floatingTexts[i].r, floatingTexts[i].g, floatingTexts[i].b, alpha);
            renderTextOutlined(floatingTexts[i].x, floatingTexts[i].y, floatingTexts[i].text, GLUT_BITMAP_HELVETICA_12, floatingTexts[i].r, floatingTexts[i].g, floatingTexts[i].b, alpha);
        }
    }
}
