// mp7_launcher.cpp — Cruise Navigator native launcher
// Proper SDL2 GPU-accelerated renderer, MP7 ocean theme.
// Replaces the basic software-render draft.

#ifdef _WIN32
#  define SDL_MAIN_HANDLED
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <commdlg.h>
#  include <io.h>
#  pragma comment(lib, "comdlg32.lib")
#  pragma comment(lib, "ole32.lib")
#endif

#include "mp7_launcher.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <math.h>

// ── config ───────────────────────────────────────────────────────────────────

static char s_cfg[512];
static char s_saved[512];

static void cfg_init() {
    if (s_cfg[0]) return;
#ifdef _WIN32
    char exe[512]; GetModuleFileNameA(NULL, exe, sizeof exe);
    char* p = strrchr(exe, '\\'); if (p) *p = 0;
    snprintf(s_cfg, sizeof s_cfg, "%s\\cruise_navigator.cfg", exe);
#else
    snprintf(s_cfg, sizeof s_cfg, "cruise_navigator.cfg");
#endif
}
static void cfg_load() {
    cfg_init();
    FILE* f = fopen(s_cfg, "r"); if (!f) return;
    if (fgets(s_saved, sizeof s_saved, f)) {
        size_t n = strlen(s_saved);
        while (n && (s_saved[n-1]=='\n'||s_saved[n-1]=='\r')) s_saved[--n] = 0;
    }
    fclose(f);
}
static void cfg_save(const char* path) {
    cfg_init();
    FILE* f = fopen(s_cfg, "w"); if (!f) return;
    fprintf(f, "%s\n", path); fclose(f);
}

// ── log ring ─────────────────────────────────────────────────────────────────

#define CN_LOG_MAX 1024
#define CN_LINE    256
typedef struct { char text[CN_LINE]; int level; } CnLine;
static CnLine s_log[CN_LOG_MAX];
static int s_log_head = 0, s_log_n = 0;

void cn_log(int level, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    CnLine* ll = &s_log[s_log_head % CN_LOG_MAX];
    time_t t = time(NULL); struct tm* tm_ = localtime(&t);
    char ts[24]; strftime(ts, sizeof ts, "%H:%M:%S", tm_);
    char body[CN_LINE - 14];
    vsnprintf(body, sizeof body, fmt, ap);
    snprintf(ll->text, CN_LINE, "[%s] %s", ts, body);
    ll->level = level; va_end(ap);
    s_log_head++;
    if (s_log_n < CN_LOG_MAX) s_log_n++;
    fprintf(stderr, "[CN] %s\n", ll->text);
}

void cn_log_fatal(const char* msg) {
    cn_log(2, "FATAL: %s", msg);
    cfg_init();
    char crash[512];
#ifdef _WIN32
    char exe[512]; GetModuleFileNameA(NULL, exe, sizeof exe);
    char* p = strrchr(exe, '\\'); if (p) *p = 0;
    snprintf(crash, sizeof crash, "%s\\crash_log.txt", exe);
#else
    snprintf(crash, sizeof crash, "crash_log.txt");
#endif
    FILE* f = fopen(crash, "a");
    if (f) {
        time_t t = time(NULL);
        char ts[32]; strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&t));
        fprintf(f, "[%s] FATAL: %s\n", ts, msg);
        fclose(f);
    }
}
int cn_log_count() { return s_log_n; }
const char* cn_log_line(int i)  { return s_log[(i) % CN_LOG_MAX].text; }
int         cn_log_level(int i) { return s_log[(i) % CN_LOG_MAX].level; }

// ── file dialog ───────────────────────────────────────────────────────────────

static bool open_dialog(char* out, int len) {
#ifdef _WIN32
    OPENFILENAMEA ofn = {0};
    char buf[MAX_PATH] = {0};
    ofn.lStructSize = sizeof ofn;
    ofn.lpstrFilter = "GameCube Disc Image\0*.iso;*.ciso;*.rvz;*.gcm\0All Files\0*.*\0";
    ofn.lpstrFile   = buf; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle  = "Select Mario Party 7 Disc Image";
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) return false;
    strncpy(out, buf, len-1); out[len-1] = 0; return true;
#else
    fprintf(stdout, "Disc image path (.iso/.ciso/.rvz): "); fflush(stdout);
    if (!fgets(out, len, stdin)) return false;
    size_t n = strlen(out);
    while (n && (out[n-1]=='\n'||out[n-1]=='\r')) out[--n] = 0;
    return out[0] != 0;
#endif
}

static bool readable(const char* p) {
    FILE* f = fopen(p, "rb"); if (!f) return false; fclose(f); return true;
}

// ── SDL2 renderer helpers ─────────────────────────────────────────────────────

struct Col { Uint8 r,g,b,a; };
static const Col C_BG     = {10,  22,  40,  255};
static const Col C_PANEL  = {16,  32,  60,  220};
static const Col C_BORDER = {50,  80,  120, 255};
static const Col C_GOLD   = {245, 200, 66,  255};
static const Col C_WHITE  = {230, 244, 255, 255};
static const Col C_DIM    = {100, 145, 180, 255};
static const Col C_GREEN  = {78,  214, 124, 255};
static const Col C_RED    = {232, 48,  48,  255};
static const Col C_TEAL   = {26,  158, 143, 255};
static const Col C_DARK   = {5,   12,  25,  255};
static const Col C_WARN   = {245, 185, 50,  255};

static void set_col(SDL_Renderer* r, Col c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}
static void fill_rect(SDL_Renderer* r, int x, int y, int w, int h, Col c) {
    set_col(r, c); SDL_Rect rc={x,y,w,h}; SDL_RenderFillRect(r, &rc);
}
static void draw_rect(SDL_Renderer* r, int x, int y, int w, int h, Col c) {
    set_col(r, c); SDL_Rect rc={x,y,w,h}; SDL_RenderDrawRect(r, &rc);
}
// Rounded rect (approximated with corner cuts)
static void fill_rounded(SDL_Renderer* rend, int x, int y, int w, int h, int rad, Col c) {
    set_col(rend, c);
    SDL_Rect rects[3] = {
        {x+rad, y,     w-rad*2, h},
        {x,     y+rad, rad,     h-rad*2},
        {x+w-rad, y+rad, rad,   h-rad*2}
    };
    for (int i = 0; i < 3; i++) SDL_RenderFillRect(rend, &rects[i]);
    // Fill corners with circles (8-step approximation)
    for (int qx = 0; qx < rad; qx++) {
        int qy = (int)sqrtf((float)(rad*rad - qx*qx));
        SDL_RenderDrawLine(rend, x+rad-qx,       y+rad-qy,   x+rad-qx,       y+rad);
        SDL_RenderDrawLine(rend, x+w-rad+qx,     y+rad-qy,   x+w-rad+qx,     y+rad);
        SDL_RenderDrawLine(rend, x+rad-qx,       y+h-rad,    x+rad-qx,       y+h-rad+qy);
        SDL_RenderDrawLine(rend, x+w-rad+qx,     y+h-rad,    x+w-rad+qx,     y+h-rad+qy);
    }
}

// ── Font atlas (5×7 bitmap, rendered to texture) ──────────────────────────────

static const uint8_t kF[95][7] = {
{0,0,0,0,0,0,0},{4,4,4,4,0,0,4},{10,10,0,0,0,0,0},{10,31,10,31,10,0,0},
{14,21,20,14,5,21,14},{24,25,2,4,8,19,3},{12,18,20,8,20,18,13},{4,4,0,0,0,0,0},
{2,4,8,8,8,4,2},{8,4,2,2,2,4,8},{0,21,14,31,14,21,0},{0,4,4,31,4,4,0},
{0,0,0,0,6,4,8},{0,0,0,31,0,0,0},{0,0,0,0,0,6,6},{1,2,4,8,16,0,0},
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,6,8,16,31},{31,1,2,6,1,17,14},
{2,6,10,18,31,2,2},{31,16,30,1,1,17,14},{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},
{14,17,17,14,17,17,14},{14,17,17,15,1,2,12},{0,6,6,0,6,6,0},{0,6,6,0,6,4,8},
{2,4,8,16,8,4,2},{0,0,31,0,31,0,0},{8,4,2,1,2,4,8},{14,17,1,6,4,0,4},
{14,17,1,13,21,21,14},{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,14},
{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,2,18,12},{17,18,20,24,20,18,17},
{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,25,21,19,19,17},{14,17,17,17,17,17,14},
{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},
{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},
{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},{14,8,8,8,8,8,14},
{16,8,4,2,1,0,0},{14,2,2,2,2,2,14},{4,10,17,0,0,0,0},{0,0,0,0,0,0,31},
{8,4,0,0,0,0,0},{0,0,14,1,15,17,15},{16,16,30,17,17,17,30},{0,0,14,16,16,17,14},
{1,1,15,17,17,17,15},{0,0,14,17,31,16,14},{6,8,8,28,8,8,8},{0,15,17,17,15,1,14},
{16,16,30,17,17,17,17},{4,0,12,4,4,4,14},{2,0,6,2,2,18,12},{16,16,18,20,24,20,18},
{12,4,4,4,4,4,14},{0,0,26,21,21,17,17},{0,0,30,17,17,17,17},{0,0,14,17,17,17,14},
{0,0,30,17,17,30,16},{0,0,15,17,17,15,1},{0,0,22,24,16,16,16},{0,0,14,16,14,1,30},
{8,8,28,8,8,9,6},{0,0,17,17,17,17,15},{0,0,17,17,17,10,4},{0,0,17,17,21,21,10},
{0,0,17,10,4,10,17},{0,0,17,17,15,1,14},{0,0,31,2,4,8,31},{6,8,8,16,8,8,6},
{4,4,4,4,4,4,4},{12,2,2,1,2,2,12},{0,8,21,2,0,0,0}
};

// Build an SDL_Texture atlas: 95 chars × 5px wide × 7px tall at 1× scale.
// At draw time we blit scaled copies.
static SDL_Texture* g_font_tex = NULL;
static void font_init(SDL_Renderer* rend) {
    if (g_font_tex) return;
    // 95 chars in a row: width=95*6, height=7
    const int AW = 95*6, AH = 7;
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, AW, AH, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_FillRect(s, NULL, 0x00000000);
    for (int ch = 0; ch < 95; ch++) {
        for (int row = 0; row < 7; row++) {
            uint8_t bits = kF[ch][row];
            for (int col = 0; col < 5; col++) {
                if (bits & (0x10 >> col)) {
                    SDL_Rect px = {ch*6 + col, row, 1, 1};
                    SDL_FillRect(s, &px, 0xFFFFFFFF);
                }
            }
        }
    }
    g_font_tex = SDL_CreateTextureFromSurface(rend, s);
    SDL_FreeSurface(s);
    SDL_SetTextureBlendMode(g_font_tex, SDL_BLENDMODE_BLEND);
}

static void draw_char(SDL_Renderer* rend, int x, int y, char c, Col col, int sc) {
    if (c < 0x20 || c > 0x7e) c = '?';
    int idx = (uint8_t)c - 0x20;
    SDL_SetTextureColorMod(g_font_tex, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(g_font_tex, col.a);
    SDL_Rect src = {idx*6, 0, 5, 7};
    SDL_Rect dst = {x, y, 5*sc, 7*sc};
    SDL_RenderCopy(rend, g_font_tex, &src, &dst);
}

static int draw_str(SDL_Renderer* rend, int x, int y, const char* s, Col col, int sc) {
    int ox = x;
    while (*s) {
        if (*s == '\n') { y += 8*sc; x = ox; }
        else { draw_char(rend, x, y, *s, col, sc); x += 6*sc; }
        s++;
    }
    return x;
}

static int str_w(const char* s, int sc) {
    int w = 0;
    while (*s) { if (*s != '\n') w += 6*sc; s++; }
    return w;
}

// ── Animated ocean background ─────────────────────────────────────────────────

static void draw_bg(SDL_Renderer* rend, int W, int H, float t) {
    // Deep gradient (approximated with horizontal bands)
    for (int y = 0; y < H; y++) {
        float v = (float)y / H;
        Uint8 r = (Uint8)(10  + v*8);
        Uint8 g = (Uint8)(22  + v*12);
        Uint8 b = (Uint8)(40  + v*20);
        SDL_SetRenderDrawColor(rend, r, g, b, 255);
        SDL_RenderDrawLine(rend, 0, y, W, y);
    }
    // Animated wave bands
    for (int i = 0; i < 4; i++) {
        float phase = t * 0.4f + i * 1.1f;
        int wy = H - 60 - i*22 + (int)(sinf(phase)*8);
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 29, 100, 140, 18 + i*8);
        for (int x = 0; x < W; x++) {
            int dy = (int)(sinf(x * 0.018f + phase)*6);
            SDL_Rect wr = {x, wy+dy, 1, 14-i*2};
            SDL_RenderFillRect(rend, &wr);
        }
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
    }
    // Stars
    srand(42);
    for (int i = 0; i < 40; i++) {
        int sx = rand() % W, sy = rand() % (H/3);
        float blink = 0.5f + 0.5f*sinf(t*1.2f + i*0.7f);
        Uint8 a = (Uint8)(80 + 170*blink);
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 255, 240, 180, a);
        SDL_RenderDrawPoint(rend, sx, sy);
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
    }
}

// ── Button helper ─────────────────────────────────────────────────────────────

struct Btn { int x,y,w,h; bool hovered; bool active; };

static bool btn_hit(const Btn& b, int mx, int my) {
    return mx >= b.x && mx < b.x+b.w && my >= b.y && my < b.y+b.h;
}

static void draw_btn(SDL_Renderer* rend, const Btn& b, const char* label,
                     Col bg, Col fg, int sc=2) {
    Col border = b.hovered ? C_GOLD : C_BORDER;
    // shadow
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 0,0,0, 60);
    for (int i = 1; i <= 3; i++) {
        SDL_Rect s={b.x+i, b.y+i, b.w, b.h};
        SDL_RenderFillRect(rend, &s);
    }
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
    // fill
    Col fill_c = b.hovered ? Col{(Uint8)(bg.r+20),(Uint8)(bg.g+20),(Uint8)(bg.b+20),bg.a} : bg;
    if (b.active) fill_c = {(Uint8)(bg.r-20),(Uint8)(bg.g+30),(Uint8)(bg.b-20),bg.a};
    fill_rounded(rend, b.x, b.y, b.w, b.h, 8, fill_c);
    draw_rect(rend, b.x, b.y, b.w, b.h, border);
    // centred label
    int tw = str_w(label, sc);
    draw_str(rend, b.x + (b.w-tw)/2, b.y + (b.h - 7*sc)/2, label, fg, sc);
}

// ── Status dot ───────────────────────────────────────────────────────────────

static void draw_dot(SDL_Renderer* rend, int cx, int cy, int r, Col c) {
    set_col(rend, c);
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x*x + y*y <= r*r)
                SDL_RenderDrawPoint(rend, cx+x, cy+y);
}

// ── Launcher UI ───────────────────────────────────────────────────────────────

bool launcher_run(const char* default_path, char* out, int out_len) {
    if (!out || out_len < 2) return false;
    out[0] = 0;
    cfg_load();

    char iso[512] = {0};
    if (s_saved[0])         strncpy(iso, s_saved,       sizeof iso - 1);
    else if (default_path)  strncpy(iso, default_path,  sizeof iso - 1);
    bool iso_ok = iso[0] && readable(iso);

#ifdef _WIN32
    SDL_SetMainReady();
#endif
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return false;
    }

    const int W = 820, H = 540;
    SDL_Window* win = SDL_CreateWindow(
        "Cruise Navigator — Mario Party 7",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W, H, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) { SDL_QuitSubSystem(SDL_INIT_VIDEO); return false; }

    SDL_Renderer* rend = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!rend) {
        // Fallback to software
        rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!rend) { SDL_DestroyWindow(win); SDL_QuitSubSystem(SDL_INIT_VIDEO); return false; }
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
    font_init(rend);

    // Button layout
    const int BTN_W = 160, BTN_H = 42;
    Btn browse_btn = {W - 180, 118, BTN_W, BTN_H, false, false};
    Btn launch_btn = {W - 180, 170, BTN_W, BTN_H, false, false};

    bool running = true, result = false;
    float anim = 0.0f;
    Uint32 last_tick = SDL_GetTicks();

    cn_log(0, "Cruise Navigator ready");
    if (iso_ok)  cn_log(0, "Last disc: %s", iso);
    else         cn_log(0, "Select your Mario Party 7 disc image to begin");

    while (running) {
        Uint32 now = SDL_GetTicks();
        anim += (now - last_tick) * 0.001f;
        last_tick = now;

        int mx, my;
        SDL_GetMouseState(&mx, &my);
        browse_btn.hovered = btn_hit(browse_btn, mx, my);
        launch_btn.hovered = iso_ok && btn_hit(launch_btn, mx, my);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = false; break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_ESCAPE) { running = false; }
                else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER)
                    { if (iso_ok) { result = true; running = false; } }
                else if (e.key.keysym.sym == SDLK_b || e.key.keysym.sym == SDLK_o)
                    goto do_browse;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    if (btn_hit(browse_btn, e.button.x, e.button.y)) {
                        do_browse:
                        if (open_dialog(iso, sizeof iso)) {
                            iso_ok = readable(iso);
                            cn_log(iso_ok?0:2, iso_ok?"Disc selected: %s":"Cannot read: %s", iso);
                        }
                    } else if (iso_ok && btn_hit(launch_btn, e.button.x, e.button.y)) {
                        result = true; running = false;
                    }
                }
                break;
            }
        }

        // ── Draw ────────────────────────────────────────────────────────────
        draw_bg(rend, W, H, anim);

        // Gold top rail
        fill_rect(rend, 0, 0, W, 5, C_GOLD);

        // ── Header panel ──
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, C_PANEL.r, C_PANEL.g, C_PANEL.b, 220);
        SDL_Rect hdr_r = {0, 5, W, 70};
        SDL_RenderFillRect(rend, &hdr_r);
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);

        // Animated star
        float pulse = 0.85f + 0.15f*sinf(anim*2.2f);
        Col star_col = {(Uint8)(245*pulse), (Uint8)(200*pulse), (Uint8)(66*pulse), 255};
        draw_str(rend, 16, 14, "*", star_col, 4);

        // Title - large
        draw_str(rend, 72, 14, "Cruise Navigator", C_GOLD, 3);
        draw_str(rend, 72, 46, "Mario Party 7 Recompiled  |  GP7E01  |  v0.1-alpha", C_DIM, 1);

        // ── ROM Panel ──
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 16, 32, 60, 210);
        SDL_Rect rom_r = {12, 88, W-24, 136};
        SDL_RenderFillRect(rend, &rom_r);
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
        draw_rect(rend, 12, 88, W-24, 136, C_BORDER);
        draw_str(rend, 24, 97, "GAME DISC", C_TEAL, 1);

        // Path display
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 5, 12, 25, 180);
        SDL_Rect path_r = {24, 112, W - 24 - BTN_W - 32, 28};
        SDL_RenderFillRect(rend, &path_r);
        draw_rect(rend, 24, 112, W - 24 - BTN_W - 32, 28,
                  iso_ok ? Col{40,120,70,255} : C_BORDER);
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);

        // Truncate path display to fit
        char disp[128] = {0};
        if (iso[0]) {
            const char* base = iso;
            // Show filename only if path is long
            const char* sl = strrchr(iso, '\\');
            const char* sl2 = strrchr(iso, '/');
            if (sl2 > sl) sl = sl2;
            if (sl) base = sl + 1;
            snprintf(disp, sizeof disp, "%s", base);
        } else {
            snprintf(disp, sizeof disp, "No disc image selected...");
        }
        draw_str(rend, 32, 119, disp, iso_ok ? C_GREEN : C_DIM, 1);

        // Status indicator
        draw_dot(rend, 36, 162, 5, iso_ok ? C_GREEN : C_RED);
        draw_str(rend, 46, 157, iso_ok
            ? "Ready  -  GP7E01 (Mario Party 7, USA)"
            : "Select a .iso  .ciso  .rvz  or  .gcm  file",
            iso_ok ? C_GREEN : C_WARN, 1);

        // Buttons
        draw_btn(rend, browse_btn, "Browse...", C_PANEL, C_WHITE, 2);
        Col lc = iso_ok ? C_GREEN : C_BORDER;
        Col lf = iso_ok ? C_DARK  : C_DIM;
        draw_btn(rend, launch_btn, iso_ok ? "Launch!" : "No disc", lc, lf, 2);

        // ── Log panel ──
        const int LY = 236;
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 5, 12, 25, 200);
        SDL_Rect log_r = {12, LY, W-24, H - LY - 42};
        SDL_RenderFillRect(rend, &log_r);
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
        draw_rect(rend, 12, LY, W-24, H - LY - 42, C_BORDER);
        draw_str(rend, 24, LY+8, "LOG", C_DIM, 1);

        const int LPAD = 24, LLINE = 12;
        int ly_start = LY + 24;
        int max_lines = (H - LY - 42 - 24) / LLINE;
        int from = s_log_n > max_lines ? s_log_n - max_lines : 0;
        for (int i = from; i < s_log_n; i++) {
            const CnLine* ll = &s_log[i % CN_LOG_MAX];
            Col lc2 = C_DIM;
            if (ll->level == 2)             lc2 = C_RED;
            else if (ll->level == 1)        lc2 = C_WARN;
            else if (strstr(ll->text,"OK")) lc2 = C_GREEN;
            else if (strstr(ll->text,"[OS]")) lc2 = {176,122,252,255};
            int lx = ly_start + (i - from) * LLINE;
            if (lx + 7 > H - 42) break;
            draw_str(rend, LPAD, lx, ll->text, lc2, 1);
        }

        // ── Bottom hint bar ──
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 8, 18, 36, 230);
        SDL_Rect bot_r = {0, H-36, W, 36};
        SDL_RenderFillRect(rend, &bot_r);
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
        draw_str(rend, 16, H-26, "[B] Browse   [Enter] Launch   [Esc] Quit", C_DIM, 1);
        // Version right-aligned
        const char* ver = "Cruise Navigator v0.1";
        draw_str(rend, W - str_w(ver, 1) - 16, H-26, ver, C_BORDER, 1);

        SDL_RenderPresent(rend);
    }

    if (g_font_tex) { SDL_DestroyTexture(g_font_tex); g_font_tex = NULL; }
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    if (result && iso[0]) {
        strncpy(out, iso, out_len-1); out[out_len-1] = 0;
        cfg_save(out);
        cn_log(0, "Launching: %s", out);
    }
    return result;
}

// ── Windows entry / diagnostic redirect ──────────────────────────────────────

#ifdef _WIN32
static void redirect_runtime_log() {
    char exe[MAX_PATH]={0}, dir[MAX_PATH]={0};
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    strncpy(dir, exe, MAX_PATH-1);
    char* p = strrchr(dir,'\\'); if (!p) p = strrchr(dir,'/');
    if (p) *p = 0;

    char crash_dir[MAX_PATH]; snprintf(crash_dir, MAX_PATH, "%s\\crash", dir);
    CreateDirectoryA(crash_dir, NULL);  // ok if already exists

    char log_path[MAX_PATH]; snprintf(log_path, MAX_PATH, "%s\\runtime.log", crash_dir);
    char dump_path[MAX_PATH]; snprintf(dump_path, MAX_PATH, "%s\\ramdump.bin", crash_dir);

    SetEnvironmentVariableA("GCN_RAM_DUMP", dump_path);
    SetEnvironmentVariableA("GCN_TRACE_DEPTH", "64");
    SetEnvironmentVariableA("GCN_DIAGNOSTIC_DIR", crash_dir);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    if (FILE* lf = freopen(log_path, "a", stderr)) {
        int fd = _fileno(stderr);
        if (fd >= 0) {
            _dup2(fd, 1);
            HANDLE h = (HANDLE)_get_osfhandle(fd);
            if (h != INVALID_HANDLE_VALUE) {
                SetStdHandle(STD_ERROR_HANDLE, h);
                SetStdHandle(STD_OUTPUT_HANDLE, h);
            }
        }
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
        fprintf(stderr, "\n=== Cruise Navigator %s %s ===\n", __DATE__, __TIME__);
        fprintf(stderr, "[diag] dir: %s\n[diag] log: %s\n[diag] dump: %s\n",
                dir, log_path, dump_path);
        fflush(stderr);
    }
}

extern int main(int argc, char** argv);
extern int __argc; extern char** __argv;
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int) {
    redirect_runtime_log();
    return main(__argc, __argv);
}
#endif
