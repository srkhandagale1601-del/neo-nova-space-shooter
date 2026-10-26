#include "Graphics/CGLab.hpp"
#include "Core/Config.hpp"
#include <GL/freeglut.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>

static const float PI = 3.14159265f;

bool cgLabPanel  = false;
bool cgWireframe = false;
bool cgCRT       = false;
bool cgDebug     = false;
bool cgRaster    = false;

int   cgWinW = WIDTH, cgWinH = HEIGHT;
float cgFlash = 0.0f;
float cgFps   = 60.0f;

// ======================================================================
// 3D PIPELINE
//   1. Model transform  : rotation matrices Rx, Ry, Rz applied by hand
//   2. Back-face culling : sign of (normal . view vector)
//   3. Painter's algorithm: faces sorted far -> near
//   4. Lighting          : Lambert diffuse + Blinn-Phong specular (flat shading)
//   5. Perspective       : x' = x * D / (z + D)
// ======================================================================

Mesh makeBipyramid(int sides, float radius, float halfHeight)
{
    Mesh m;
    for (int i = 0; i < sides; i++)
    {
        float a = i * 2.0f * PI / sides;
        m.v.push_back(Vec3{cosf(a) * radius, sinf(a) * radius, 0.0f});
    }
    int top = (int)m.v.size();
    m.v.push_back(Vec3{0.0f, 0.0f, halfHeight});
    int bot = (int)m.v.size();
    m.v.push_back(Vec3{0.0f, 0.0f, -halfHeight});

    for (int i = 0; i < sides; i++)
    {
        int j = (i + 1) % sides;
        m.f.push_back(i); m.f.push_back(j); m.f.push_back(top);
        m.f.push_back(j); m.f.push_back(i); m.f.push_back(bot);
    }
    return m;
}

static Vec3 rotateXYZ(Vec3 p, float rx, float ry, float rz)
{
    float c = cosf(rx), s = sinf(rx);
    float y = p.y * c - p.z * s, z = p.y * s + p.z * c;
    p.y = y; p.z = z;

    c = cosf(ry); s = sinf(ry);
    float x = p.x * c + p.z * s;
    z = -p.x * s + p.z * c;
    p.x = x; p.z = z;

    c = cosf(rz); s = sinf(rz);
    x = p.x * c - p.y * s;
    y = p.x * s + p.y * c;
    p.x = x; p.y = y;
    return p;
}

static Vec3 sub(const Vec3& a, const Vec3& b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
static float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec3 cross(const Vec3& a, const Vec3& b)
{
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static Vec3 normalize(const Vec3& a)
{
    float l = sqrtf(dot(a, a));
    if (l < 1e-6f) return a;
    return Vec3{a.x / l, a.y / l, a.z / l};
}

struct DrawFace { float depth; Vec2 p[3]; float shade; float spec; };

void drawMesh3D(const Mesh& m, float cx, float cy, float scale,
                float rx, float ry, float rz,
                float r, float g, float b, float alpha)
{
    const float D = 5.0f;                       // camera distance
    const Vec3 L = normalize(Vec3{-0.5f, 0.65f, -0.6f});   // direction TO the light
    const Vec3 V = Vec3{0.0f, 0.0f, -1.0f};     // direction TO the eye
    const Vec3 H = normalize(Vec3{L.x + V.x, L.y + V.y, L.z + V.z});

    std::vector<Vec3> rv(m.v.size());
    for (size_t i = 0; i < m.v.size(); i++)
        rv[i] = rotateXYZ(m.v[i], rx, ry, rz);

    std::vector<DrawFace> faces;
    for (size_t i = 0; i + 2 < m.f.size(); i += 3)
    {
        Vec3 a = rv[m.f[i]], bb = rv[m.f[i + 1]], c = rv[m.f[i + 2]];
        Vec3 n = cross(sub(bb, a), sub(c, a));
        Vec3 ctr = Vec3{(a.x + bb.x + c.x) / 3, (a.y + bb.y + c.y) / 3, (a.z + bb.z + c.z) / 3};
        if (dot(n, ctr) < 0.0f) n = Vec3{-n.x, -n.y, -n.z};   // orient outward (convex mesh)
        n = normalize(n);

        // Back-face culling: skip faces turned away from the camera at (0,0,-D)
        Vec3 toFace = Vec3{ctr.x, ctr.y, ctr.z + D};
        if (dot(n, toFace) >= 0.0f) continue;

        DrawFace df;
        df.depth = ctr.z;
        Vec3 pts[3] = {a, bb, c};
        for (int k = 0; k < 3; k++)
        {
            float persp = D / (pts[k].z + D);       // perspective divide
            df.p[k].x = cx + pts[k].x * persp * scale;
            df.p[k].y = cy + pts[k].y * persp * scale;
        }
        float lam = std::max(0.0f, dot(n, L));
        df.shade = 0.22f + 0.78f * lam;                         // ambient + diffuse
        df.spec = powf(std::max(0.0f, dot(n, H)), 28.0f) * 0.9f; // Blinn specular
        faces.push_back(df);
    }

    // Painter's algorithm: draw far faces first
    std::sort(faces.begin(), faces.end(),
              [](const DrawFace& a, const DrawFace& b2) { return a.depth > b2.depth; });

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (size_t i = 0; i < faces.size(); i++)
    {
        const DrawFace& f = faces[i];
        float sr = std::min(1.0f, r * f.shade + f.spec);
        float sg = std::min(1.0f, g * f.shade + f.spec);
        float sb = std::min(1.0f, b * f.shade + f.spec);
        glColor4f(sr, sg, sb, alpha);
        glBegin(GL_TRIANGLES);
            glVertex2f(f.p[0].x, f.p[0].y);
            glVertex2f(f.p[1].x, f.p[1].y);
            glVertex2f(f.p[2].x, f.p[2].y);
        glEnd();

        glColor4f(std::min(1.0f, r + 0.5f), std::min(1.0f, g + 0.5f), std::min(1.0f, b + 0.5f), 0.7f * alpha);
        glBegin(GL_LINE_LOOP);
            glVertex2f(f.p[0].x, f.p[0].y);
            glVertex2f(f.p[1].x, f.p[1].y);
            glVertex2f(f.p[2].x, f.p[2].y);
        glEnd();
    }
}

// ======================================================================
// BEZIER CURVES - de Casteljau's repeated linear interpolation
// ======================================================================

static float lerp(float a, float b, float t) { return a + (b - a) * t; }

float bezier1D(float a, float b, float c, float d, float t)
{
    float ab = lerp(a, b, t), bc = lerp(b, c, t), cd = lerp(c, d, t);
    float abc = lerp(ab, bc, t), bcd = lerp(bc, cd, t);
    return lerp(abc, bcd, t);
}

Vec2 bezier(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, float t)
{
    return Vec2{bezier1D(p0.x, p1.x, p2.x, p3.x, t), bezier1D(p0.y, p1.y, p2.y, p3.y, t)};
}

// ======================================================================
// SCAN CONVERSION - integer-only line and circle algorithms
// ======================================================================

void bresenhamLine(int x0, int y0, int x1, int y1)
{
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;                       // decision variable
    glBegin(GL_POINTS);
    for (;;)
    {
        glVertex2i(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    glEnd();
}

void midpointCircle(int cx, int cy, int r)
{
    int x = 0, y = r, d = 1 - r;             // midpoint decision parameter
    glBegin(GL_POINTS);
    while (x <= y)
    {
        glVertex2i(cx + x, cy + y); glVertex2i(cx - x, cy + y);
        glVertex2i(cx + x, cy - y); glVertex2i(cx - x, cy - y);
        glVertex2i(cx + y, cy + x); glVertex2i(cx - y, cy + x);
        glVertex2i(cx + y, cy - x); glVertex2i(cx - y, cy - x);
        if (d < 0) d += 2 * x + 3;
        else { d += 2 * (x - y) + 5; y--; }
        x++;
    }
    glEnd();
}

// ======================================================================
// COHEN-SUTHERLAND LINE CLIPPING
// ======================================================================

static int outCode(float x, float y, float xmin, float ymin, float xmax, float ymax)
{
    int c = 0;
    if (x < xmin) c |= 1;       // LEFT
    else if (x > xmax) c |= 2;  // RIGHT
    if (y < ymin) c |= 4;       // BOTTOM
    else if (y > ymax) c |= 8;  // TOP
    return c;
}

bool clipLineCohenSutherland(float& x0, float& y0, float& x1, float& y1,
                             float xmin, float ymin, float xmax, float ymax,
                             int* oc0, int* oc1)
{
    int c0 = outCode(x0, y0, xmin, ymin, xmax, ymax);
    int c1 = outCode(x1, y1, xmin, ymin, xmax, ymax);
    if (oc0) *oc0 = c0;
    if (oc1) *oc1 = c1;

    for (int guard = 0; guard < 8; guard++)
    {
        if (!(c0 | c1)) return true;     // trivially inside
        if (c0 & c1) return false;       // trivially outside
        int co = c0 ? c0 : c1;
        float x = 0, y = 0;
        if (co & 8)      { x = x0 + (x1 - x0) * (ymax - y0) / (y1 - y0); y = ymax; }
        else if (co & 4) { x = x0 + (x1 - x0) * (ymin - y0) / (y1 - y0); y = ymin; }
        else if (co & 2) { y = y0 + (y1 - y0) * (xmax - x0) / (x1 - x0); x = xmax; }
        else             { y = y0 + (y1 - y0) * (xmin - x0) / (x1 - x0); x = xmin; }
        if (co == c0) { x0 = x; y0 = y; c0 = outCode(x0, y0, xmin, ymin, xmax, ymax); }
        else          { x1 = x; y1 = y; c1 = outCode(x1, y1, xmin, ymin, xmax, ymax); }
    }
    return false;
}

// ======================================================================
// LIGHTING & BLENDING
//   drawGlow: triangle fan whose centre vertex is bright and rim vertices are
//   transparent. OpenGL's Gouraud (smooth) shading interpolates the colour
//   across the fan = a radial light falloff. Additive blending (GL_ONE) adds
//   light to whatever is behind it.
// ======================================================================

void drawGlow(float x, float y, float radius, float r, float g, float b, float a)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_TRIANGLE_FAN);
        glColor4f(r, g, b, a);
        glVertex2f(x, y);
        glColor4f(r, g, b, 0.0f);
        for (int i = 0; i <= 14; i++)
        {
            float t = i * 2.0f * PI / 14.0f;
            glVertex2f(x + cosf(t) * radius, y + sinf(t) * radius);
        }
    glEnd();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// Short-lived point lights spawned by explosions
struct LightFlash { float x, y, radius, r, g, b; int life, maxLife; bool active; };
static const int MAX_LIGHTS = 24;
static LightFlash lights[MAX_LIGHTS];

void addLightFlash(float x, float y, float radius, float r, float g, float b)
{
    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (!lights[i].active)
        {
            lights[i] = LightFlash{x, y, radius, r, g, b, 14, 14, true};
            return;
        }
    }
}

void updateLightFlashes()
{
    for (int i = 0; i < MAX_LIGHTS; i++)
        if (lights[i].active && --lights[i].life <= 0) lights[i].active = false;
}

void drawLightFlashes()
{
    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (!lights[i].active) continue;
        float t = (float)lights[i].life / lights[i].maxLife;
        drawGlow(lights[i].x, lights[i].y, lights[i].radius * (1.6f - 0.6f * t),
                 lights[i].r, lights[i].g, lights[i].b, 0.55f * t);
    }
}

// ======================================================================
// TEXTURE MAPPING - procedural fBm value-noise texture wrapped on a sphere
//   * texture generated at startup (4 octaves of smooth value noise)
//   * sphere mapping: (u,v) from longitude / latitude of each vertex
//   * lit per-vertex with Lambert N.L and modulated with the texture
// ======================================================================

static unsigned int planetTex = 0;

static float hash2(int x, int y)
{
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}

static float valueNoise(float u, float v, int px, int py)
{
    float x = u * px, y = v * py;
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    int x0 = ((xi % px) + px) % px, x1 = (x0 + 1) % px;
    int y0 = ((yi % py) + py) % py, y1 = (y0 + 1) % py;
    float a = lerp(hash2(x0, y0), hash2(x1, y0), fx);
    float b = lerp(hash2(x0, y1), hash2(x1, y1), fx);
    return lerp(a, b, fy);
}

void initPlanet()
{
    const int TW = 256, TH = 128;
    static unsigned char data[TW * TH];
    for (int j = 0; j < TH; j++)
    {
        for (int i = 0; i < TW; i++)
        {
            float u = (float)i / TW, v = (float)j / TH;
            float amp = 0.55f, sum = 0.0f, norm = 0.0f;
            for (int o = 0; o < 4; o++)
            {
                sum += amp * valueNoise(u, v, 4 << o, 2 << o);
                norm += amp;
                amp *= 0.5f;
            }
            float n = sum / norm;
            n = std::min(1.0f, std::max(0.0f, (n - 0.25f) * 1.9f));
            data[j * TW + i] = (unsigned char)(70 + 185 * n);
        }
    }
    glGenTextures(1, &planetTex);
    glBindTexture(GL_TEXTURE_2D, planetTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, TW, TH, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
}

void drawPlanet(float cx, float cy, float R, float tr, float tg, float tb, float rotation)
{
    if (!planetTex) return;
    const int RINGS = 12, SEG = 44;
    const Vec3 L = normalize(Vec3{-0.55f, 0.45f, 0.70f});

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, planetTex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    for (int k = 0; k < RINGS; k++)
    {
        glBegin(GL_TRIANGLE_STRIP);
        for (int j = 0; j <= SEG; j++)
        {
            float ang = j * 2.0f * PI / SEG;
            for (int s = 0; s < 2; s++)
            {
                float rr = (float)(k + s) / RINGS;
                float px = rr * cosf(ang), py = rr * sinf(ang);
                float pz = sqrtf(std::max(0.0f, 1.0f - rr * rr));
                float u = atan2f(px, pz) / (2.0f * PI) + rotation;   // longitude
                float v = asinf(std::max(-1.0f, std::min(1.0f, py))) / PI + 0.5f;  // latitude
                float diff = std::max(0.0f, px * L.x + py * L.y + pz * L.z);
                float lit = 0.10f + 0.95f * diff;
                glColor4f(tr * lit, tg * lit, tb * lit, 1.0f);
                glTexCoord2f(u, v);
                glVertex2f(cx + px * R, cy + py * R);
            }
        }
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);

    // Atmosphere: additive rim glow
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBegin(GL_TRIANGLE_STRIP);
    for (int j = 0; j <= SEG; j++)
    {
        float ang = j * 2.0f * PI / SEG;
        glColor4f(tr, tg, tb, 0.35f);
        glVertex2f(cx + cosf(ang) * R * 0.97f, cy + sinf(ang) * R * 0.97f);
        glColor4f(tr, tg, tb, 0.0f);
        glVertex2f(cx + cosf(ang) * R * 1.28f, cy + sinf(ang) * R * 1.28f);
    }
    glEnd();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// ======================================================================
// PERSPECTIVE PROJECTION - warp starfield + synthwave floor grid
//   Stars live in 3D (x,y,z). Each frame z shrinks (camera flies forward)
//   and the point is projected with  sx = vpx + x / z .
//   A streak is drawn from the projection at z to the projection at z+dz.
// ======================================================================

struct WarpStar { float x, y, z; };
static const int WARP_N = 190;
static const float WARP_ZMAX = 30.0f;
static WarpStar warp[WARP_N];
static const float VPX = WIDTH / 2.0f, VPY = HEIGHT * 0.62f;

static void respawnWarp(WarpStar& s, bool anyDepth)
{
    s.x = (float)(rand() % 2400 - 1200);
    s.y = (float)(rand() % 2000 - 1000);
    s.z = anyDepth ? 1.0f + (rand() % 1000) / 1000.0f * (WARP_ZMAX - 1.0f) : WARP_ZMAX;
}

void initWarp()
{
    for (int i = 0; i < WARP_N; i++) respawnWarp(warp[i], true);
}

void updateWarp(float speed)
{
    for (int i = 0; i < WARP_N; i++)
    {
        warp[i].z -= 0.16f * speed * (1.0f + (WARP_ZMAX - warp[i].z) * 0.04f);
        if (warp[i].z < 0.7f) respawnWarp(warp[i], false);
    }
}

void drawWarp(float r, float g, float b)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (int i = 0; i < WARP_N; i++)
    {
        const WarpStar& s = warp[i];
        float near01 = 1.0f - s.z / WARP_ZMAX;
        float trail = 0.25f + near01 * 1.4f;
        float x0 = VPX + s.x / s.z * 0.9f, y0 = VPY + s.y / s.z * 0.9f;
        float x1 = VPX + s.x / (s.z + trail) * 0.9f, y1 = VPY + s.y / (s.z + trail) * 0.9f;
        if (x0 < -50 || x0 > WIDTH + 50 || y0 < -50 || y0 > HEIGHT + 50) continue;
        glLineWidth(1.0f + near01 * 2.2f);
        glBegin(GL_LINES);
            glColor4f(r, g, b, 0.0f);
            glVertex2f(x1, y1);
            glColor4f(r, g, b, 0.25f + 0.75f * near01);
            glVertex2f(x0, y0);
        glEnd();
    }
    glLineWidth(1.0f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void drawPerspectiveGrid(float r, float g, float b, float scroll)
{
    const float horizon = HEIGHT * 0.34f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(1.0f);

    // Lines of constant depth: spacing shrinks toward the horizon (1/z law)
    float frac = scroll - floorf(scroll);
    glBegin(GL_LINES);
    for (int i = 0; i < 14; i++)
    {
        float d = i + frac;
        float s = 1.0f / (1.0f + d * 0.55f);
        float y = horizon * (1.0f - s);   // d=0 -> bottom, d->inf -> horizon
        float a = 0.28f * s;
        glColor4f(r, g, b, a);
        glVertex2f(0.0f, y);
        glVertex2f((float)WIDTH, y);
    }
    // Lines of constant x converge at the vanishing point
    for (int j = -10; j <= 10; j++)
    {
        glColor4f(r, g, b, 0.28f);
        glVertex2f(VPX + j * 90.0f, 0.0f);
        glColor4f(r, g, b, 0.0f);
        glVertex2f(VPX + j * 6.0f, horizon);
    }
    glEnd();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// ======================================================================
// POST-PROCESSING (2D overlays)
// ======================================================================

void drawVignette()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const int N = 48;
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= N; i++)
    {
        float a = i * 2.0f * PI / N;
        float c = cosf(a), s = sinf(a);
        glColor4f(0, 0, 0, 0.0f);
        glVertex2f(WIDTH / 2.0f + c * 330.0f, HEIGHT / 2.0f + s * 260.0f);
        glColor4f(0, 0, 0, 0.0f);
        glVertex2f(WIDTH / 2.0f + c * 600.0f, HEIGHT / 2.0f + s * 480.0f);
    }
    glEnd();
}

void drawScanlines()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.16f);
    glBegin(GL_LINES);
    for (int y = 0; y < HEIGHT; y += 3)
    {
        glVertex2f(0.0f, (float)y);
        glVertex2f((float)WIDTH, (float)y);
    }
    glEnd();
}

void drawScreenFlash()
{
    if (cgFlash < 0.01f) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(1.0f, 0.75f, 0.45f, std::min(0.55f, cgFlash) * 0.7f);
    glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(WIDTH, 0); glVertex2f(WIDTH, HEIGHT); glVertex2f(0, HEIGHT);
    glEnd();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void updateFps()
{
    static int last = 0, frames = 0;
    frames++;
    int now = glutGet(GLUT_ELAPSED_TIME);
    if (now - last >= 500)
    {
        cgFps = frames * 1000.0f / (now - last);
        frames = 0;
        last = now;
    }
}
