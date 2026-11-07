// mp7_launcher.cpp — Cruise Navigator launcher for Mario Party 7 Recompiled
//
// Before the game boots, if no disc is configured this shows an SDL window
// that lets the user pick their ISO and saves the choice to cruise_navigator.cfg.
// Once a valid disc is confirmed it returns the path and the runtime boots normally.
//
// Log capture: cn_log() / cn_log_fatal() are called throughout the runtime to
// build a live log ring and write crash_log.txt on fatal faults.

#include "mp7_launcher.h"
#ifdef _WIN32
#  define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <commdlg.h>
#  include <io.h>
#  pragma comment(lib, "comdlg32.lib")
#  pragma comment(lib, "ole32.lib")
#endif

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
    FILE* f = fopen(s_cfg, "r");
    if (!f) return;
    if (fgets(s_saved, sizeof s_saved, f)) {
        size_t n = strlen(s_saved);
        while (n && (s_saved[n-1]=='\n'||s_saved[n-1]=='\r')) s_saved[--n] = 0;
    }
    fclose(f);
}

static void cfg_save(const char* path) {
    cfg_init();
    FILE* f = fopen(s_cfg, "w");
    if (!f) return;
    fprintf(f, "%s\n", path);
    fclose(f);
}

// ── log ring ─────────────────────────────────────────────────────────────────

#define CN_LOG_MAX 1024
#define CN_LINE    256

typedef struct { char text[CN_LINE]; int level; } CnLine;
static CnLine s_log[CN_LOG_MAX];
static int    s_log_head = 0;
static int    s_log_n    = 0;

void cn_log(int level, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    CnLine* ll = &s_log[s_log_head % CN_LOG_MAX];
    // Prepend timestamp
    time_t t = time(NULL); struct tm* tm = localtime(&t);
    char ts[24]; strftime(ts, sizeof ts, "%H:%M:%S", tm);
    char body[CN_LINE - 12];
    vsnprintf(body, sizeof body, fmt, ap);
    snprintf(ll->text, CN_LINE, "[%s] %s", ts, body);
    ll->level = level;
    va_end(ap);
    s_log_head++;
    if (s_log_n < CN_LOG_MAX) s_log_n++;
    fprintf(stderr, "[CN] %s\n", ll->text);
}

void cn_log_fatal(const char* msg) {
    cn_log(2, "FATAL: %s", msg);
    // Write crash log next to exe
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
        fprintf(f, "[%s] %s\n", ts, msg);
        fclose(f);
    }
}

int cn_log_count()   { return s_log_n; }
const char* cn_log_line(int i)  { return s_log[(i) % CN_LOG_MAX].text; }
int         cn_log_level(int i) { return s_log[(i) % CN_LOG_MAX].level; }

// ── file dialog ───────────────────────────────────────────────────────────────

static bool open_dialog(char* out, int len) {
#ifdef _WIN32
    OPENFILENAMEA ofn = {0};
    char buf[MAX_PATH] = {0};
    ofn.lStructSize  = sizeof ofn;
    ofn.lpstrFilter  = "GameCube Disc Image\0*.iso;*.ciso;*.gcm\0All Files\0*.*\0";
    ofn.lpstrFile    = buf;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrTitle   = "Select Mario Party 7 Disc Image";
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) return false;
    strncpy(out, buf, len-1); out[len-1] = 0;
    return true;
#else
    fprintf(stdout, "Disc image path (.iso/.ciso/.gcm): ");
    fflush(stdout);
    if (!fgets(out, len, stdin)) return false;
    size_t n = strlen(out);
    while (n && (out[n-1]=='\n'||out[n-1]=='\r')) out[--n] = 0;
    return out[0] != 0;
#endif
}

static bool readable(const char* p) {
    FILE* f = fopen(p, "rb"); if (!f) return false; fclose(f); return true;
}

// ── minimal SDL2 software-render helpers ─────────────────────────────────────

static void fill(SDL_Surface* s, int x, int y, int w, int h, Uint32 c) {
    SDL_Rect r = {x, y, w, h}; SDL_FillRect(s, &r, c);
}
#define RGB(r,g,b) SDL_MapRGB(surf->format,r,g,b)

// 5×7 bitmap font (95 chars, 0x20–0x7E)
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

static void draw_char(SDL_Surface* surf, int cx, int cy, char c, Uint32 col, int sc) {
    if (c < 0x20 || c > 0x7e) c = '?';
    const uint8_t* bm = kF[(uint8_t)c - 0x20];
    for (int r = 0; r < 7; r++)
        for (int b = 0; b < 5; b++)
            if (bm[r] & (0x10 >> b))
                fill(surf, cx + b*sc, cy + r*sc, sc, sc, col);
}

static void draw_str(SDL_Surface* surf, int x, int y, const char* s, Uint32 col, int sc) {
    int ox = x;
    while (*s) {
        if (*s == '\n') { y += 8*sc; x = ox; }
        else { draw_char(surf, x, y, *s, col, sc); x += 6*sc; }
        s++;
    }
}

// ── launcher ─────────────────────────────────────────────────────────────────

bool launcher_run(const char* default_path, char* out, int out_len) {
    if (!out || out_len < 2) return false;
    out[0] = 0;
    cfg_load();

    // Prefer the saved selection; otherwise prefill the build's configured image.
    char iso[512] = {0};
    if (s_saved[0]) {
        strncpy(iso, s_saved, sizeof iso - 1);
    } else if (default_path && default_path[0]) {
        strncpy(iso, default_path, sizeof iso - 1);
    }
    const bool iso_ok_initial = iso[0] && readable(iso);

#ifdef _WIN32
    SDL_SetMainReady();
#endif
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }

    const int W = 740, H = 500;
    SDL_Window* win = SDL_CreateWindow("Cruise Navigator — Mario Party 7",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }
    SDL_Surface* surf = SDL_GetWindowSurface(win);
    if (!surf) {
        fprintf(stderr, "SDL_GetWindowSurface: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    bool iso_ok = iso_ok_initial;
    bool running = true;
    bool result = false;

    cn_log(0, "Cruise Navigator — select Mario Party 7 disc");

    while (running) {
        // ── events ──────────────────────────────────────────────────────────
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = false; break; }
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_RETURN: case SDLK_KP_ENTER:
                    if (iso_ok) { result = true; running = false; }
                    break;
                case SDLK_b:
                    if (open_dialog(iso, sizeof iso)) {
                        iso_ok = readable(iso);
                        cn_log(iso_ok ? 0 : 2, iso_ok ?
                            "Disc selected: %s" : "Cannot read: %s", iso);
                    }
                    break;
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = e.button.x, my = e.button.y;
                // Browse  x=W-150..W-10  y=90..120
                if (mx >= W-150 && mx <= W-10 && my >= 90 && my <= 120)
                    if (open_dialog(iso, sizeof iso)) {
                        iso_ok = readable(iso);
                        cn_log(iso_ok ? 0 : 2, iso_ok ?
                            "Disc selected: %s" : "Cannot read: %s", iso);
                    }
                // Launch  x=W-150..W-10  y=130..158
                if (iso_ok && mx >= W-150 && mx <= W-10 && my >= 130 && my <= 158) {
                    result = true; running = false;
                }
            }
        }

        // ── draw ────────────────────────────────────────────────────────────
        const Uint32 BG    = RGB(10, 22, 40);
        const Uint32 PANEL = RGB(16, 32, 60);
        const Uint32 BORD  = RGB(50, 80, 120);
        const Uint32 GOLD  = RGB(245, 200, 66);
        const Uint32 WHITE = RGB(230, 244, 255);
        const Uint32 DIM   = RGB(100, 145, 180);
        const Uint32 GREEN = RGB(78, 214, 124);
        const Uint32 RED   = RGB(232, 48, 48);
        const Uint32 TEAL  = RGB(26, 158, 143);

        SDL_FillRect(surf, NULL, BG);
        fill(surf, 0, 0, W, 5, GOLD);                       // top rail
        fill(surf, 0, 5, W, 52, PANEL);                     // header
        draw_str(surf, 14, 13, "Cruise Navigator", GOLD, 2);
        draw_str(surf, 14, 36, "Mario Party 7 Recompiled  |  GP7E01", DIM, 1);

        // ROM section
        fill(surf, 10, 68, W-20, 85, PANEL);
        fill(surf, 10, 68, W-20, 1, BORD);
        fill(surf, 10, 152, W-20, 1, BORD);
        draw_str(surf, 20, 76, "GAME DISC", TEAL, 1);
        draw_str(surf, 20, 92, iso[0] ? iso : "No disc selected...", iso_ok ? GREEN : DIM, 1);
        draw_str(surf, 20, 108, iso_ok ?
            "Status: Ready  (GP7E01 detected)" :
            "Status: Select a .iso, .ciso, or .rvz file", iso_ok ? GREEN : GOLD, 1);

        // Browse button
        fill(surf, W-150, 83, 136, 28, BORD);
        draw_str(surf, W-140, 92, "[B]rowse...", WHITE, 1);

        // Launch button
        fill(surf, W-150, 120, 136, 25, iso_ok ? GREEN : BORD);
        draw_str(surf, W-140, 128, iso_ok ? "[Enter] Launch!" : "No disc", iso_ok ? BG : DIM, 1);

        // Log panel
        int LY = 165;
        fill(surf, 10, LY, W-20, H - LY - 10, PANEL);
        fill(surf, 10, LY, W-20, 1, BORD);
        draw_str(surf, 20, LY + 7, "LOG", DIM, 1);
        int ly = LY + 20;
        int from = s_log_n > 16 ? s_log_n - 16 : 0;
        for (int i = from; i < s_log_n && ly < H - 20; i++) {
            const CnLine* ll = &s_log[i % CN_LOG_MAX];
            Uint32 c = ll->level == 2 ? RED : ll->level == 1 ? GOLD : DIM;
            if (strstr(ll->text, "OK")) c = GREEN;
            draw_str(surf, 20, ly, ll->text, c, 1);
            ly += 10;
        }

        draw_str(surf, 20, H-14, "[B] Browse   [Enter] Launch   [Esc] Quit", DIM, 1);
        SDL_UpdateWindowSurface(win);
        SDL_Delay(16);
    }

    if (result && iso[0]) {
        strncpy(out, iso, out_len-1); out[out_len-1] = 0;
        cfg_save(out);
        cn_log(0, "Launching: %s", out);
    }

    SDL_DestroyWindow(win);
    // Release the launcher's video subsystem before the game creates its GL window.
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return result;
}

#ifdef _WIN32
// Build as a GUI-subsystem executable. Persistent diagnostics live in the user's
// LocalAppData directory so writing logs never depends on the install folder being writable.
extern int main(int argc, char** argv);
extern int __argc;
extern char** __argv;

static void redirect_runtime_log() {
    char exe_path[MAX_PATH] = {0};
    DWORD exe_len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    char exe_dir[MAX_PATH] = {0};
    if (exe_len > 0 && exe_len < MAX_PATH) {
        strncpy(exe_dir, exe_path, sizeof exe_dir - 1);
        char* slash = strrchr(exe_dir, '\\');
        if (slash) *slash = 0;
    }

    char root[MAX_PATH] = {0};
    DWORD root_len = GetEnvironmentVariableA("LOCALAPPDATA", root, MAX_PATH);
    if (root_len == 0 || root_len >= MAX_PATH) {
        strncpy(root, exe_dir[0] ? exe_dir : ".", sizeof root - 1);
    }

    char app_dir[MAX_PATH] = {0};
    char log_dir[MAX_PATH] = {0};
    snprintf(app_dir, sizeof app_dir, "%s\\CruiseNavigator", root);
    CreateDirectoryA(app_dir, NULL);
    snprintf(log_dir, sizeof log_dir, "%s\\logs", app_dir);
    if (!CreateDirectoryA(log_dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        // Fallback beside the executable if LocalAppData is unavailable.
        snprintf(app_dir, sizeof app_dir, "%s\\CruiseNavigator", exe_dir[0] ? exe_dir : ".");
        CreateDirectoryA(app_dir, NULL);
        snprintf(log_dir, sizeof log_dir, "%s\\logs", app_dir);
        CreateDirectoryA(log_dir, NULL);
    }

    char log_path[MAX_PATH] = {0};
    char dump_path[MAX_PATH] = {0};
    snprintf(log_path, sizeof log_path, "%s\\runtime.log", log_dir);
    snprintf(dump_path, sizeof dump_path, "%s\\ramdump.bin", log_dir);

    // The runtime's indirect-call failure handler already understands this variable
    // and writes guest RAM (24 MiB) before aborting.
    SetEnvironmentVariableA("GCN_RAM_DUMP", dump_path);
    SetEnvironmentVariableA("GCN_TRACE_DEPTH", "64");
    SetEnvironmentVariableA("GCN_DIAGNOSTIC_DIR", log_dir);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    FILE* log_file = freopen(log_path, "a", stderr);
    if (log_file) {
        const int fd = _fileno(stderr);
        if (fd >= 0) {
            _dup2(fd, 1); // stdout and stderr share the same append-only log
            HANDLE log_handle = (HANDLE)_get_osfhandle(fd);
            if (log_handle != INVALID_HANDLE_VALUE) {
                SetStdHandle(STD_ERROR_HANDLE, log_handle);
                SetStdHandle(STD_OUTPUT_HANDLE, log_handle);
            }
        }
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
        fprintf(stderr, "\n========== Cruise Navigator startup ==========\n");
        fprintf(stderr, "[diagnostics] log: %s\n", log_path);
        fprintf(stderr, "[diagnostics] RAM dump: %s\n", dump_path);
        fprintf(stderr, "[diagnostics] executable: %s\n", exe_path);
        fprintf(stderr, "[diagnostics] build: %s %s\n", __DATE__, __TIME__);
        fflush(stderr);
    } else {
        // Keep a minimal error record even if LocalAppData redirection fails.
        char fallback[MAX_PATH] = {0};
        snprintf(fallback, sizeof fallback, "%s\\CruiseNavigator-log-open-failed.txt",
                 exe_dir[0] ? exe_dir : ".");
        FILE* f = fopen(fallback, "a");
        if (f) {
            fprintf(f, "Could not open runtime log at %s (error %lu)\n",
                    log_path, (unsigned long)GetLastError());
            fclose(f);
        }
    }
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    redirect_runtime_log();
    return main(__argc, __argv);
}
#endif
