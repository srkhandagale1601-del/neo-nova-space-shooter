#include "Entities/Powerup.hpp"
#include "Graphics/Renderer.hpp"
#include <GL/freeglut.h>
#include <cstdlib>
#include <cmath>

PowerUp powerUps[MAX_POWERUPS];

void initPowerUps()
{
    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        powerUps[i].active = false;
    }
}

void spawnPowerUp(float x, float y)
{
    // 32% drop chance on enemy destruction
    if ((rand() % 100) >= 32)
        return;

    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        if (!powerUps[i].active)
        {
            powerUps[i].active = true;
            powerUps[i].x = x;
            powerUps[i].y = y;
            powerUps[i].vy = -1.6f;
            powerUps[i].type = (PowerUpType)(rand() % PU_COUNT);
            powerUps[i].pulse = 0.0f;
            break;
        }
    }
}

void updatePowerUps()
{
    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        if (powerUps[i].active)
        {
            powerUps[i].y += powerUps[i].vy;
            powerUps[i].pulse += 0.08f;
            if (powerUps[i].y < -POWERUP_SIZE)
            {
                powerUps[i].active = false;
            }
        }
    }
}

void drawPowerUps()
{
    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        if (!powerUps[i].active)
            continue;

        float glow = 0.75f + 0.25f * sinf(powerUps[i].pulse);
        float hs = POWERUP_SIZE / 2.0f;

        float r = 1.0f, g = 1.0f, b = 1.0f;
        const char* icon = "?";

        switch (powerUps[i].type)
        {
            case PU_SHIELD:
                r = 0.1f; g = 0.85f; b = 1.0f;
                icon = "S";
                break;
            case PU_TRIPLE_SHOT:
                r = 1.0f; g = 0.85f; b = 0.1f;
                icon = "3";
                break;
            case PU_PENTA_SHOT:
                r = 0.95f; g = 0.2f; b = 1.0f;
                icon = "5";
                break;
            case PU_RAPID_FIRE:
                r = 1.0f; g = 0.25f; b = 0.2f;
                icon = "R";
                break;
            case PU_SPEED_BOOST:
                r = 0.0f; g = 1.0f; b = 0.85f;
                icon = "W";
                break;
            case PU_EMP_BOMB:
                r = 0.4f; g = 0.5f; b = 1.0f;
                icon = "B";
                break;
            case PU_TIME_SLOW:
                r = 0.2f; g = 0.7f; b = 1.0f;
                icon = "T";
                break;
            case PU_EXTRA_LIFE:
                r = 0.2f; g = 1.0f; b = 0.3f;
                icon = "+";
                break;
            default:
                break;
        }

        // Outer glowing polygon
        glColor4f(r * glow, g * glow, b * glow, 0.9f);
        glBegin(GL_POLYGON);
            glVertex2f(powerUps[i].x, powerUps[i].y + hs);
            glVertex2f(powerUps[i].x + hs, powerUps[i].y);
            glVertex2f(powerUps[i].x, powerUps[i].y - hs);
            glVertex2f(powerUps[i].x - hs, powerUps[i].y);
        glEnd();

        // Inner white highlight rim
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(powerUps[i].x, powerUps[i].y + hs);
            glVertex2f(powerUps[i].x + hs, powerUps[i].y);
            glVertex2f(powerUps[i].x, powerUps[i].y - hs);
            glVertex2f(powerUps[i].x - hs, powerUps[i].y);
        glEnd();

        // Icon symbol
        glColor3f(0.05f, 0.05f, 0.12f);
        renderGlText(powerUps[i].x - 4.5f, powerUps[i].y - 5.0f, icon, GLUT_BITMAP_HELVETICA_12);
    }
}
