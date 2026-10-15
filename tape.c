#include "tape.h"

#define RGB(r,g,b) ((uint32_t)(r) | ((uint32_t)(g)<<8) | ((uint32_t)(b)<<16) | 0xff000000u)
#define W 480
#define H 272

/* Rows written by the current render: the whole screen, or one band. */
static int row0, row1 = H;

static uint32_t color(const TapeSurface *s, uint32_t c)
{
    unsigned r = c & 255, g = (c >> 8) & 255, b = (c >> 16) & 255;
    switch (s->format) {
    case 0: return (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11);
    case 1: return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | 0x8000;
    case 2: return (r >> 4) | ((g >> 4) << 4) | ((b >> 4) << 8) | 0xf000;
    default: return c;
    }
}

#define CAMERA 900
#define HALF_DEPTH 16
/* Widest projected coordinate: the outline's |x| plus the face depth. */
#define OUTLINE_MAX_X 252
/* project() multiplies a Q10 rotated coordinate by CAMERA*8 in 32-bit int. */
_Static_assert((long long)(OUTLINE_MAX_X + HALF_DEPTH) * 1024 * CAMERA * 8 <= 0x7fffffff,
               "project() would overflow: enlarge depth or use 64-bit maths");
#define SHELL RGB(29,30,27)

typedef struct { int x, y; } Point;
typedef struct { int x, y, z; } Point3;
typedef struct {
    const TapeSurface *surface;
    const TapePose *pose;
    int dy, brightness, face_visible;
    int clip_x0, clip_y0, clip_x1, clip_y1;
} Canvas;

/* One contour drives both the solid shell and its edge-to-edge artwork. */
static const Point outline[20] = {
    {-230,-148}, {230,-148}, {238,-146}, {246,-140}, {251,-133},
    {252,-126}, {252,124}, {250,134}, {244,142}, {236,147},
    {228,148}, {-228,148}, {-236,147}, {-244,142}, {-250,134},
    {-252,124}, {-252,-126}, {-251,-133}, {-246,-140}, {-238,-146}
};

static Point inset(Point p)
{
    p.x += p.x > 0 ? -4 : 4;
    p.y += p.y > 0 ? -4 : 4;
    return p;
}

static void fill_rect(const TapeSurface *s, int x, int y, int x1, int y1, uint32_t c)
{
    int i, j;
    uint32_t v = color(s, c);
    if (x < 0) x = 0;
    if (y < row0) y = row0;
    if (x1 > W) x1 = W;
    if (y1 > row1) y1 = row1;
    for (j = y; j < y1; ++j) {
        if (s->format == 3) {
            uint32_t *p = (uint32_t *)s->pixels + j * s->stride;
            for (i = x; i < x1; ++i) p[i] = v;
        } else {
            uint16_t *p = (uint16_t *)s->pixels + j * s->stride;
            for (i = x; i < x1; ++i) p[i] = (uint16_t)v;
        }
    }
}

static int ceil256(int v)
{
    return v >= 0 ? (v+255)/256 : -((-v)/256);
}

static int ceil8(int v)
{
    return v >= 0 ? (v+7)/8 : -((-v)/8);
}

/* Projected points retain eighth-pixel precision. Rounding each tiny artwork
 * rectangle to whole pixels first would leave stripes in the angled hubs. */
static void fill_polygon(const TapeSurface *s, const Point *points, int count, uint32_t c)
{
    int i, y, top = H*8, bottom = 0;
    for (i = 0; i < count; ++i) {
        if (points[i].y < top) top = points[i].y;
        if (points[i].y > bottom) bottom = points[i].y;
    }
    top = ceil8(top-4);
    bottom = ceil8(bottom-4);
    if (top < 0) top = 0;
    if (bottom > H) bottom = H;
    for (y = top; y < bottom; ++y) {
        int left = 0, right = 0, hits = 0;
        for (i = 0; i < count; ++i) {
            Point a = points[i], b = points[(i+1)%count];
            int scan = 8*y+4, x;
            if (a.y > b.y) { Point swap = a; a = b; b = swap; }
            if (scan < a.y || scan >= b.y) continue;
            x = a.x*32 + (scan-a.y)*(b.x-a.x)*32/(b.y-a.y);
            if (!hits || x < left) left = x;
            if (!hits || x > right) right = x;
            ++hits;
        }
        if (hits >= 2)
            fill_rect(s, ceil256(left-128), y, ceil256(right-128), y+1, c);
    }
}

static Point project(const Canvas *c, Point3 p)
{
    const TapePose *pose = c->pose;
    int rx = p.x*pose->yaw_cos + p.z*pose->yaw_sin;
    int rz = -p.x*pose->yaw_sin + p.z*pose->yaw_cos;
    int ry = p.y*pose->pitch_cos - rz*pose->pitch_sin/1024;
    int depth;
    Point out;
    rz = p.y*pose->pitch_sin + rz*pose->pitch_cos/1024;
    depth = (CAMERA + HALF_DEPTH + pose->retreat)*1024 + rz;
    /* The bounded scene stays well in front of the near plane. */
    out.x = (W/2)*8 + rx*CAMERA*8/depth;
    out.y = (H/2 + c->dy)*8 + ry*CAMERA*8/depth;
    return out;
}

static void world_quad(const Canvas *c, const Point3 world[4], uint32_t color_value)
{
    Point screen[4];
    int i;
    for (i = 0; i < 4; ++i) screen[i] = project(c, world[i]);
    fill_polygon(c->surface, screen, 4, color_value);
}

static uint32_t shaded(uint32_t c, unsigned light)
{
    return RGB((c & 255)*light/256, ((c>>8) & 255)*light/256,
               ((c>>16) & 255)*light/256);
}

static void rect(const Canvas *canvas, int x, int y, int w, int h, uint32_t c)
{
    int x1 = x+w, y1 = y+h;
    if (!canvas->face_visible) return;
    /* Window masking happens before projection, so tape remains behind glass. */
    if (x < canvas->clip_x0) x = canvas->clip_x0;
    if (y < canvas->clip_y0) y = canvas->clip_y0;
    if (x1 > canvas->clip_x1) x1 = canvas->clip_x1;
    if (y1 > canvas->clip_y1) y1 = canvas->clip_y1;
    if (x >= x1 || y >= y1) return;
    if (canvas->pose && canvas->pose->flipping) {
        int sign = canvas->pose->yaw_cos >= 0 ? 1 : -1;
        Point3 world[4] = {
            {sign*(x-W/2), y-H/2, -sign*HALF_DEPTH},
            {sign*(x1-W/2), y-H/2, -sign*HALF_DEPTH},
            {sign*(x1-W/2), y1-H/2, -sign*HALF_DEPTH},
            {sign*(x-W/2), y1-H/2, -sign*HALF_DEPTH}
        };
        world_quad(canvas, world, shaded(c, canvas->brightness));
    } else {
        fill_rect(canvas->surface, x, y+canvas->dy, x1, y1+canvas->dy, c);
    }
}

static void face_quad(const Canvas *c, const Point points[4], uint32_t value)
{
    int i, sign;
    Point3 world[4];
    if (!c->face_visible || !c->pose || !c->pose->flipping) return;
    sign = c->pose->yaw_cos >= 0 ? 1 : -1;
    for (i = 0; i < 4; ++i) {
        world[i].x = sign*(points[i].x-W/2);
        world[i].y = points[i].y-H/2;
        world[i].z = -sign*HALF_DEPTH;
    }
    world_quad(c, world, shaded(value, c->brightness));
}

/* Clip a convex face to a horizontal half-plane. Each pass adds at most one
 * vertex: the 20-point contour needs at most 22 after both cuts (24 reserved). */
static int clip_y(const Point *in, int count, Point *out, int y, int below)
{
    int i, n = 0;
    Point a = in[count-1];
    int a_inside = below ? a.y >= y : a.y <= y;
    for (i = 0; i < count; ++i) {
        Point b = in[i];
        int b_inside = below ? b.y >= y : b.y <= y;
        if (a_inside != b_inside) {
            out[n].x = a.x + (y-a.y)*(b.x-a.x)/(b.y-a.y);
            out[n++].y = y;
        }
        if (b_inside) out[n++] = b;
        a = b;
        a_inside = b_inside;
    }
    return n;
}

static void face_band(const Canvas *c, int y0, int y1, uint32_t value)
{
    Point face[24], clipped[24], screen[24];
    int i, n, sign;
    if (!c->face_visible) return;
    if (!c->pose || !c->pose->flipping) {
        rect(c, 0, y0, W, y1-y0, value);
        return;
    }
    for (i = 0; i < 20; ++i) {
        Point p = inset(outline[i]);
        face[i].x = p.x + W/2;
        face[i].y = p.y + H/2;
    }
    n = clip_y(face, 20, clipped, y0, 1);
    if (!n) return;
    n = clip_y(clipped, n, face, y1, 0);
    if (n < 3) return;
    sign = c->pose->yaw_cos >= 0 ? 1 : -1;
    for (i = 0; i < n; ++i) {
        Point3 p = {sign*(face[i].x-W/2), face[i].y-H/2, -sign*HALF_DEPTH};
        screen[i] = project(c, p);
    }
    fill_polygon(c->surface, screen, n, shaded(value, c->brightness));
}

static uint32_t mix(uint32_t a, uint32_t b, unsigned amount)
{
    unsigned back = 1024-amount;
    return RGB(((a & 255)*back + (b & 255)*amount)/1024,
               (((a>>8) & 255)*back + ((b>>8) & 255)*amount)/1024,
               (((a>>16) & 255)*back + ((b>>16) & 255)*amount)/1024);
}

static void disc(const Canvas *s, int cx, int cy, int r, uint32_t c)
{
    int y, x = r;
    for (y = 0; y <= r; ++y) {
        while (x*x + y*y > r*r) --x;
        rect(s, cx-x, cy+y, x*2+1, 1, c);
        if (y) rect(s, cx-x, cy-y, x*2+1, 1, c);
    }
}

static void roundrect(const Canvas *s, int x, int y, int w, int h,
                      int r, uint32_t c)
{
    rect(s, x+r, y, w-2*r, h, c);
    rect(s, x, y+r, w, h-2*r, c);
    disc(s, x+r, y+r, r, c);
    disc(s, x+w-r-1, y+r, r, c);
    disc(s, x+r, y+h-r-1, r, c);
    disc(s, x+w-r-1, y+h-r-1, r, c);
}

/* Original 5x7 lettering; columns, least-significant bit at the top. */
static const unsigned char letters[26][5] = {
    {126,9,9,9,126},{127,73,73,73,54},{62,65,65,65,34},
    {127,65,65,34,28},{127,73,73,73,65},{127,9,9,9,1},
    {62,65,73,73,58},{127,8,8,8,127},{65,65,127,65,65},
    {32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
    {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},
    {127,9,9,9,6},{62,65,81,33,94},{127,9,25,41,70},
    {38,73,73,73,50},{1,1,127,1,1},{63,64,64,64,63},
    {31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
    {3,4,120,4,3},{97,81,73,69,67}
};

static const unsigned char digits[10][5] = {
    {62,81,73,69,62},{0,66,127,64,0},{98,81,73,73,70},
    {34,65,73,73,54},{24,20,18,127,16},{39,69,69,69,57},
    {62,73,73,73,50},{1,113,9,5,3},{54,73,73,73,54},
    {38,73,73,73,62}
};

static void text(const Canvas *s, int x, int y, const char *str,
                 int scale, uint32_t c)
{
    while (*str) {
        int a, b;
        unsigned char ch = (unsigned char)*str++;
        const unsigned char *glyph = ch >= 'A' && ch <= 'Z' ? letters[ch-'A'] :
                                     ch >= '0' && ch <= '9' ? digits[ch-'0'] : 0;
        if (glyph) {
            for (a = 0; a < 5; ++a)
                for (b = 0; b < 7; ++b)
                    if (glyph[a] & (1u << b))
                        rect(s, x+a*scale, y+b*scale, scale, scale, c);
        } else if (ch == '-') {
            rect(s, x, y+3*scale, 5*scale, scale, c);
        }
        x += 6*scale;
    }
}

/* Integer unit-circle table keeps the scene maths integer-only. */
static const int sin32[32] = {
    0,50,98,142,181,213,237,251,256,251,237,213,181,142,98,50,
    0,-50,-98,-142,-181,-213,-237,-251,-256,-251,-237,-213,-181,-142,-98,-50
};

/* Restrained colors and broad shapes keep the native command list small. */
#define INK RGB(13,15,15)
#define PAPER RGB(239,233,210)
#define RED RGB(218,49,25)
#define BLUE RGB(35,105,205)
/* Sine in 1/192 turns, interpolated from sin32 (Q8). Six teeth are exactly
 * 32 steps apart, so they stay evenly spaced at every reel phase. */
static int sin192(int a)
{
    int i, f;
    a %= 192;
    if (a < 0) a += 192;
    i = a / 6; f = a % 6;
    return sin32[i] + (sin32[(i+1) & 31] - sin32[i]) * f / 6;
}

/* Hub geometry in sixteenth pixels: the paper ring spans radii 17.5-23.5 and
 * each square tooth reaches from radius 12 into the ring, 5 pixels wide. */
#define RING_IN 280
#define RING_OUT 376
#define TOOTH_IN 192
#define TOOTH_OUT 288
#define TOOTH_HALF 40

/* Coverage (0-16) of a one-pixel box by the far side of an edge, from the
 * signed distance of the pixel centre past that edge, in sixteenths. */
static int ramp(int distance)
{
    distance += 8;
    return distance < 0 ? 0 : distance > 16 ? 16 : distance;
}

static int round_div(int v, int d)
{
    return v >= 0 ? (v + d/2) / d : -((-v + d/2) / d);
}

static void hub(const Canvas *s, int x, int y, int phase)
{
    /* Pixel-centre radius for every offset within the hub, in sixteenths. */
    static uint16_t root[25*25+1];
    const TapeSurface *surface = s->surface;
    uint32_t shade[17];
    int i, px, py, ux[6], uy[6];
    disc(s, x, y, 27, RGB(68,66,56));
    disc(s, x, y, 25, INK);
    disc(s, x, y, 23, PAPER);
    disc(s, x, y, 17, INK);
    for (i = 0; i < 6; ++i) {
        ux[i] = sin192(phase*6 + i*32 + 48);
        uy[i] = sin192(phase*6 + i*32);
    }
    if (s->pose && s->pose->flipping && s->pose->yaw_sin) {
        /* Brief flips keep the projected path: plain square teeth. */
        for (i = 0; i < 6; ++i) {
            static const int corner[4][2] = {
                {TOOTH_IN,-TOOTH_HALF}, {TOOTH_OUT,-TOOTH_HALF},
                {TOOTH_OUT,TOOTH_HALF}, {TOOTH_IN,TOOTH_HALF}
            };
            Point quad[4];
            int k;
            for (k = 0; k < 4; ++k) {
                quad[k].x = x + round_div(corner[k][0]*ux[i] - corner[k][1]*uy[i], 4096);
                quad[k].y = y + round_div(corner[k][0]*uy[i] + corner[k][1]*ux[i], 4096);
            }
            face_quad(s, quad, PAPER);
        }
        return;
    }
    if (!s->face_visible) return;
    for (i = 0; i <= 16; ++i) shade[i] = color(surface, mix(INK, PAPER, (unsigned)i * 64));
    if (!root[1]) {
        unsigned n, r = 0;
        for (n = 0; n <= 25*25; ++n) {
            while ((r+1)*(r+1) <= n*256) ++r;
            root[n] = (uint16_t)r;
        }
    }
    /* Resting view: area coverage softens the teeth and ring edges. Pixels well
     * inside one band are already exact from the discs above and are skipped. */
    for (py = -25; py <= 25; ++py) {
        int row = y + py + s->dy;
        if (row < row0 || row >= row1 || x - 25 < 0 || x + 25 >= W) continue;
        for (px = -25; px <= 25; ++px) {
            int d2 = px*px + py*py, d, paper;
            if (d2 > 25*25 || d2 < 11*11 || (d2 > 19*19 && d2 < 22*22)) continue;
            d = root[d2];
            paper = ramp(d - RING_IN);
            if (ramp(RING_OUT - d) < paper) paper = ramp(RING_OUT - d);
            if (d < RING_IN + 8)
                for (i = 0; i < 6; ++i) {
                    /* Tooth-frame coordinates, in 1/256 pixel. */
                    int along = px*ux[i] + py*uy[i];
                    int across = py*ux[i] - px*uy[i];
                    int tooth;
                    if (across < 0) across = -across;
                    if (along < (TOOTH_IN-8)*16 || across > (TOOTH_HALF+8)*16) continue;
                    tooth = ramp(along/16 - TOOTH_IN) * ramp(TOOTH_HALF - across/16) / 16;
                    if (tooth > paper) paper = tooth;
                }
            if (surface->format == 3)
                ((uint32_t *)surface->pixels)[row*surface->stride + x+px] = shade[paper];
            else
                ((uint16_t *)surface->pixels)[row*surface->stride + x+px] = (uint16_t)shade[paper];
        }
    }
}

static void tape_pack(const Canvas *window, int x, int radius)
{
    /* Only the inner edge of each large winding pack is exposed by the glass. */
    disc(window, x, 148, radius, RGB(88,77,57));
    disc(window, x, 148, radius-1, RGB(30,24,20));
    disc(window, x, 148, radius-4, RGB(48,37,28));
    disc(window, x, 148, radius-5, RGB(24,21,19));
}

static void head_recess(const Canvas *c, const TapePose *pose)
{
    const Point lip[4] = {{117,219}, {363,219}, {383,280}, {97,280}};
    const Point recess[4] = {{121,222}, {359,222}, {377,280}, {103,280}};
    uint32_t base = SHELL;
    unsigned amount = pose->yaw_sin;
    if (!pose->flipping || !amount) return;
    face_quad(c, lip, mix(base, RGB(71,73,64), amount));
    face_quad(c, recess, mix(base, RGB(20,23,21), amount));
    disc(c, 143, 254, 8, mix(base, RGB(61,65,58), amount));
    disc(c, 337, 254, 8, mix(base, RGB(61,65,58), amount));
    disc(c, 143, 254, 6, mix(base, RGB(6,9,8), amount));
    disc(c, 337, 254, 6, mix(base, RGB(6,9,8), amount));
    rect(c, 216, 252, 48, 16, mix(base, RGB(5,8,7), amount));
    rect(c, 219, 253, 42, 2, mix(base, RGB(107,110,98), amount));
    disc(c, 187, 262, 3, mix(base, RGB(5,8,7), amount));
    disc(c, 293, 262, 3, mix(base, RGB(5,8,7), amount));
}

static void cassette(const Canvas *c, const TapePose *pose)
{
    Canvas window = *c;
    uint32_t accent = pose->side ? BLUE : RED;
    int x, i;

    /* Full-bleed shell. Sony draws its thumbnail, title and metadata afterward.
     * A blank dark paper label gives that white text contrast at its native
     * positions, without duplicate titles or rules through the metadata. */
    /* Cover the entire rounded face, including the area outside the resting
     * viewport. Clipping to the shell replaces the old rectangular skin. */
    face_band(c, -12, 94, RGB(45,46,40));
    face_band(c, 94, 96, RGB(126,117,94));
    face_band(c, 96, 195, accent);
    face_band(c, 195, 216, PAPER);
    face_band(c, 216, 218, RGB(99,92,74));
    face_band(c, 218, 284, SHELL);

    text(c, 24, 134, pose->side ? "B" : "A", 4, INK);
    text(c, 23, 168, "SIDE", 1, INK);
    /* Tight pair: the digits span x 430-446, the same edges as MIN below. */
    text(c, 428, 136, "1", 2, INK);
    text(c, 437, 136, "5", 2, INK);
    text(c, 430, 157, "MIN", 1, INK);
    text(c, 70, 202, "TYPE I", 1, INK);
    /* Mirrors TYPE I: x 375-409 ends as far from the right as TYPE I starts. */
    text(c, 375, 202, "STEREO", 1, INK);
    /* Printed direction arrow, below the window and clear of player controls. */
    rect(c, 231, 204, 16, 3, accent);
    for (i = 0; i < 5; ++i)
        rect(c, 244+i, 200+i, 1, 11-2*i, accent);

    roundrect(c, 65, 109, 350, 78, 5, pose->side ? RGB(22,53,104) : RGB(121,42,25));
    roundrect(c, 67, 111, 346, 74, 4, RGB(7,9,8));

    /* White hubs sit in separate openings in opaque black plastic. Their
     * diameter stays fixed while the much larger tape packs wind behind it. */
    hub(c, 143, 148, pose->phase);
    hub(c, 337, 148, pose->phase);

    /* A clear window BETWEEN the hubs exposes just the packs' curved edges.
     * Draw the tape underneath the fixed glass reflections and printed scale. */
    rect(c, 182, 117, 116, 62, RGB(79,79,67));
    rect(c, 184, 119, 112, 58, RGB(126,128,110));
    rect(c, 185, 120, 110, 56, RGB(165,169,148));
    window.clip_x0 = 185; window.clip_y0 = 120;
    window.clip_x1 = 295; window.clip_y1 = 176;
    tape_pack(&window, 143, pose->left_radius);
    tape_pack(&window, 337, pose->right_radius);
    rect(c, 185, 120, 110, 1, RGB(187,187,162));
    rect(c, 185, 175, 110, 1, RGB(104,104,86));
    rect(c, 189, 148, 102, 1, RGB(139,134,113));
    for (i = 0; i < 5; ++i) {
        x = 200 + i*20;
        rect(c, x, i == 2 ? 140 : 144, 1, i == 2 ? 17 : 9,
             RGB(139,134,113));
    }
    rect(c, 73, 112, 334, 1, RGB(70,60,47));
    rect(c, 73, 183, 334, 1, pose->side ? RGB(92,157,239) : RGB(238,96,59));

    /* Recessed tape-head opening and guide holes appear as the shell lifts. */
    head_recess(c, pose);
    /* Dark molded base: native elapsed time, transport and progress stay clear. */
    for (i = 0; i < 4; ++i)
        rect(c, 185, 223+i*3, 110, 1, RGB(45,46,40));
    disc(c, 17, 231, 5, RGB(68,66,56));
    disc(c, 463, 231, 5, RGB(68,66,56));
    disc(c, 17, 231, 3, INK);
    disc(c, 463, 231, 3, INK);
    rect(c, 15, 231, 5, 1, RGB(104,99,80));
    rect(c, 461, 231, 5, 1, RGB(104,99,80));
}

static void outward_quad(const Canvas *c, const Point3 world[4], uint32_t value)
{
    Point screen[4];
    int i, area = 0;
    for (i = 0; i < 4; ++i) screen[i] = project(c, world[i]);
    for (i = 0; i < 4; ++i) {
        Point a = screen[i], b = screen[(i+1)&3];
        area += a.x*b.y-b.x*a.y;
    }
    if (area < 0) fill_polygon(c->surface, screen, 4, value);
}

static void shell_body(const Canvas *c)
{
    int i;
    for (i = 0; i < 20; ++i) {
        Point a = outline[i], b = outline[(i+1)%20];
        Point ia = inset(a), ib = inset(b);
        int dx = b.x-a.x, dy = b.y-a.y;
        int sum = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        unsigned light = 186 + (dx > 0 ? dx*60/sum : 0);
        const Point3 wall[4] = {
            {a.x,a.y,-12}, {b.x,b.y,-12}, {b.x,b.y,12}, {a.x,a.y,12}
        };
        const Point3 seam[4] = {
            {a.x,a.y,-1}, {b.x,b.y,-1}, {b.x,b.y,1}, {a.x,a.y,1}
        };
        const Point3 front_bevel[4] = {
            {b.x,b.y,-12}, {a.x,a.y,-12}, {ia.x,ia.y,-HALF_DEPTH}, {ib.x,ib.y,-HALF_DEPTH}
        };
        const Point3 back_bevel[4] = {
            {a.x,a.y,12}, {b.x,b.y,12}, {ib.x,ib.y,HALF_DEPTH}, {ia.x,ia.y,HALF_DEPTH}
        };
        outward_quad(c, wall, shaded(RGB(77,83,76), light));
        outward_quad(c, seam, RGB(19,23,21));
        outward_quad(c, front_bevel, shaded(RGB(112,115,101), light));
        outward_quad(c, back_bevel, shaded(RGB(100,110,104), light));
    }
}

static int render_scene(const TapeSurface *s, const TapeState *state);

int tape_render(const TapeSurface *s, const TapeState *state)
{
    return tape_render_rows(s, state, 0, H);
}

int tape_render_rows(const TapeSurface *s, const TapeState *state, int y0, int y1)
{
    int result;
    if (y0 < 0 || y1 > H || y0 >= y1) return -1;
    row0 = y0; row1 = y1;
    result = render_scene(s, state);
    row0 = 0; row1 = H;
    return result;
}

static int render_scene(const TapeSurface *s, const TapeState *state)
{
    TapePose pose;
    Canvas canvas = { .surface = s, .brightness = 256, .face_visible = 1,
                      .clip_x1 = W, .clip_y1 = H };
    size_t bpp;
    if (!s || !state || !s->pixels || s->format < 0 || s->format > 3 ||
        s->stride < W || s->stride > 2048) return -1;
    bpp = s->format == 3 ? 4 : 2;
    if ((uintptr_t)s->pixels % bpp || s->bytes < (size_t)s->stride*H*bpp)
        return -1;
    tape_pose(state, &pose);
    rect(&canvas, 0, 0, W, H, RGB(12,15,17));
    if (pose.flipping && pose.yaw_sin) {
        int width = 100 + 330*(1024-pose.yaw_sin)/1024;
        roundrect(&canvas, (W-width)/2-4, 242, width+8, 18, 8, RGB(8,10,11));
        roundrect(&canvas, (W-width)/2, 245, width, 12, 5, RGB(3,5,6));
    }
    canvas.pose = &pose;
    canvas.dy = pose.y_offset;
    if (pose.flipping) {
        int facing = pose.yaw_cos < 0 ? -pose.yaw_cos : pose.yaw_cos;
        canvas.brightness = 176 + facing*80/1024;
        canvas.face_visible = facing*pose.pitch_cos*(CAMERA+HALF_DEPTH+pose.retreat)
                              > HALF_DEPTH*1024*1024;
        shell_body(&canvas);
    }
    cassette(&canvas, &pose);
    /* Stationary deck lip: the cassette slides behind it and settles in place. */
    canvas.pose = 0;
    canvas.dy = 0;
    canvas.face_visible = 1;
    if (pose.loading) rect(&canvas, 0, 270, W, 2, RGB(5,7,7));
    if (pose.loading && state->entrance_ms >= 600 && state->entrance_ms < 850) {
        rect(&canvas, 16, 260, 12, 2, PAPER);
        rect(&canvas, 452, 260, 12, 2, PAPER);
    }
    return 0;
}
