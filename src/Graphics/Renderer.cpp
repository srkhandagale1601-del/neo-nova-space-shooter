#include "Graphics/Renderer.hpp"

void renderGlText(float x, float y, const char* text, void* font)
{
    glRasterPos2f(x, y);
    for (int i = 0; text[i] != '\0'; i++)
    {
        glutBitmapCharacter(font, text[i]);
    }
}
