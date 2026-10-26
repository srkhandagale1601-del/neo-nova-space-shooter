#pragma once
// ======================================================================
// CGLab - the Computer Graphics toolbox of VECTOR STORM.
// Every function here implements (by hand) a classic CG algorithm, so the
// graphics pipeline is visible in the game instead of hidden in a library.
// ======================================================================
#include <vector>

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

// Runtime toggles (F1..F5)
extern bool cgLabPanel;   // F1  live CG Lab panel
extern bool cgWireframe;  // F2  glPolygonMode(GL_LINE)
extern bool cgCRT;        // F3  scanlines post effect
extern bool cgDebug;      // F4  Bezier control polygons + AABB hitboxes
extern bool cgRaster;     // F5  software rasterisation (Bresenham / midpoint)

extern int   cgWinW, cgWinH;  // window size in pixels (for the viewport transform)
extern float cgFlash;         // full-screen impact flash (0..1)
extern float cgFps;

// ---------- 3D pipeline: mesh -> rotate -> perspective -> cull -> sort -> light
struct Mesh
{
    std::vector<Vec3> v;   // vertices
    std::vector<int>  f;   // triangle indices (3 per face)
};

Mesh makeBipyramid(int sides, float radius, float halfHeight);
void drawMesh3D(const Mesh& m, float cx, float cy, float scale,
                float rx, float ry, float rz,
                float r, float g, float b, float alpha = 1.0f);

// ---------- Curves
Vec2  bezier(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, float t);
float bezier1D(float a, float b, float c, float d, float t);

// ---------- Scan conversion
void bresenhamLine(int x0, int y0, int x1, int y1);
void midpointCircle(int cx, int cy, int r);

// ---------- Clipping (returns false when the line is fully outside)
bool clipLineCohenSutherland(float& x0, float& y0, float& x1, float& y1,
                             float xmin, float ymin, float xmax, float ymax,
                             int* outcode0 = 0, int* outcode1 = 0);

// ---------- Lighting / blending
void drawGlow(float x, float y, float radius, float r, float g, float b, float a);
void addLightFlash(float x, float y, float radius, float r, float g, float b);
void updateLightFlashes();
void drawLightFlashes();

// ---------- Texture mapping
void initPlanet();
void drawPlanet(float cx, float cy, float radius, float tr, float tg, float tb, float rotation);

// ---------- Perspective projection
void initWarp();
void updateWarp(float speed);
void drawWarp(float r, float g, float b);
void drawPerspectiveGrid(float r, float g, float b, float scroll);

// ---------- Post-processing
void drawVignette();
void drawScanlines();
void drawScreenFlash();
void updateFps();
