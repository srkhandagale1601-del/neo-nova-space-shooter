#pragma once
#include <GL/freeglut.h>

// Text rendering helper (named renderGlText to avoid Win32 DrawText macro collision)
void renderGlText(float x, float y, const char* text, void* font = GLUT_BITMAP_HELVETICA_18);
