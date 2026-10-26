#include "Graphics/Renderer.hpp"
#include <cmath>
#include <cstring>

void renderGlText(float x, float y, const char* text, void* font)
{
    glRasterPos2f(x, y);
    for (int i = 0; text[i] != '\0'; i++)
    {
        glutBitmapCharacter(font, text[i]);
    }
}

void drawCircleFill(float x, float y, float r, int segs)
{
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y);
        for (int i = 0; i <= segs; i++)
        {
            float t = i * 2.0f * 3.14159265f / segs;
            glVertex2f(x + cosf(t) * r, y + sinf(t) * r);
        }
    glEnd();
}

void drawCircleOutline(float x, float y, float r, int segs)
{
    glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segs; i++)
        {
            float t = i * 2.0f * 3.14159265f / segs;
            glVertex2f(x + cosf(t) * r, y + sinf(t) * r);
        }
    glEnd();
}

// Dark 1px ring first, then the coloured text: readable on any background.
void renderTextOutlined(float x, float y, const char* text, void* font, float r, float g, float b, float a)
{
    glColor4f(0.02f, 0.05f, 0.12f, a);
    renderGlText(x - 1.0f, y, text, font);
    renderGlText(x + 1.0f, y, text, font);
    renderGlText(x, y - 1.0f, text, font);
    renderGlText(x, y + 1.0f, text, font);
    glColor4f(r, g, b, a);
    renderGlText(x, y, text, font);
}

float strokeTextWidth(const char* text, float scale)
{
    return glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)text) * scale;
}

// Large title: thick dark outline pass, then a bright fill pass (stroke font, scaled).
void renderStrokeTitle(float x, float y, const char* text, float scale,
                       float r, float g, float b)
{
    for (int pass = 0; pass < 2; pass++)
    {
        glPushMatrix();
        glTranslatef(x + (pass == 0 ? 2.5f : 0.0f), y - (pass == 0 ? 2.5f : 0.0f), 0.0f);
        glScalef(scale, scale, 1.0f);
        if (pass == 0) { glColor3f(0.05f, 0.1f, 0.25f); glLineWidth(9.0f); }
        else           { glColor3f(r, g, b);            glLineWidth(4.0f); }
        glutStrokeString(GLUT_STROKE_ROMAN, (const unsigned char*)text);
        glPopMatrix();
    }
    glLineWidth(1.0f);
}
