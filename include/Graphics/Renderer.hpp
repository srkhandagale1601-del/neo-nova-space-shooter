#pragma once
#include <GL/freeglut.h>

// Text rendering helper (named renderGlText to avoid Win32 DrawText macro collision)
void renderGlText(float x, float y, const char* text, void* font = GLUT_BITMAP_HELVETICA_18);

// Cartoon-style drawing helpers
void drawCircleFill(float x, float y, float r, int segs = 24);
void drawCircleOutline(float x, float y, float r, int segs = 24);
void renderTextOutlined(float x, float y, const char* text, void* font,
                        float r, float g, float b, float a = 1.0f);
float strokeTextWidth(const char* text, float scale);
void renderStrokeTitle(float x, float y, const char* text, float scale,
                       float r, float g, float b);
