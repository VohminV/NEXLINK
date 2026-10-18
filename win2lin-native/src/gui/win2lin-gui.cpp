// win2lin-gui - SDL2 front-end for win2lin-relinker.
// Picks a Windows .exe, a Linux target dir, runs the relinker (PE -> ELF)
// and launches the resulting ELF with LD_LIBRARY_PATH pointing at the shims.
// No SPIR-V / Vulkan path here (we have no RDNA 2); graphics stays on the CPU
// side for now - just the relinker + shim .so loading.

#include <SDL2/SDL.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static const int WINDOW_W = 900;
static const int WINDOW_H = 560;

// ---------------------------------------------------------------------------
// 5x7 bitmap font (rows top->bottom, each row is 5 bits, LSB = leftmost px)
// ---------------------------------------------------------------------------
static const uint8_t font5x7[95][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 0x20 space
    {0x00,0x00,0x2f,0x00,0x00,0x00,0x00}, // 0x21 !
    {0x00,0x00,0x07,0x00,0x07,0x00,0x00}, // 0x22 "
    {0x00,0x14,0x3e,0x14,0x3e,0x14,0x00}, // 0x23 #
    {0x00,0x24,0x2a,0x7f,0x2a,0x12,0x00}, // 0x24 $
    {0x00,0x62,0x64,0x08,0x10,0x26,0x00}, // 0x25 %
    {0x00,0x32,0x4a,0x4c,0x18,0x00,0x00}, // 0x26 &
    {0x00,0x00,0x05,0x00,0x00,0x00,0x00}, // 0x27 '
    {0x00,0x10,0x20,0x40,0x20,0x10,0x00}, // 0x28 (
    {0x00,0x40,0x20,0x10,0x20,0x40,0x00}, // 0x29 )
    {0x00,0x00,0x00,0x6c,0x00,0x00,0x00}, // 0x2a *
    {0x00,0x08,0x08,0x3e,0x08,0x08,0x00}, // 0x2b +
    {0x00,0x00,0x00,0x00,0x30,0x30,0x60}, // 0x2c ,
    {0x00,0x08,0x08,0x08,0x08,0x08,0x00}, // 0x2d -
    {0x00,0x00,0x60,0x00,0x00,0x00,0x00}, // 0x2e .
    {0x00,0x20,0x10,0x08,0x04,0x02,0x00}, // 0x2f /
    {0x00,0x3e,0x41,0x5d,0x4d,0x43,0x3e}, // 0x30 0
    {0x00,0x40,0x60,0x40,0x40,0x40,0x7f}, // 0x31 1
    {0x00,0x3e,0x41,0x01,0x3e,0x40,0x7f}, // 0x32 2
    {0x00,0x3e,0x41,0x01,0x7e,0x41,0x3e}, // 0x33 3
    {0x00,0x0c,0x0e,0x1e,0x7e,0x60,0x40}, // 0x34 4
    {0x00,0x7f,0x40,0x7c,0x01,0x41,0x3e}, // 0x35 5
    {0x00,0x3e,0x40,0x7c,0x41,0x41,0x3e}, // 0x36 6
    {0x00,0x7f,0x01,0x01,0x01,0x01,0x01}, // 0x37 7
    {0x00,0x3e,0x41,0x3e,0x41,0x41,0x3e}, // 0x38 8
    {0x00,0x3e,0x41,0x3e,0x01,0x41,0x3e}, // 0x39 9
    {0x00,0x00,0x36,0x36,0x00,0x00,0x00}, // 0x3a :
    {0x00,0x00,0x36,0x36,0x00,0x36,0x00}, // 0x3b ;
    {0x00,0x08,0x14,0x22,0x44,0x00,0x00}, // 0x3c <
    {0x00,0x00,0x7f,0x00,0x7f,0x00,0x00}, // 0x3d =
    {0x00,0x44,0x22,0x14,0x08,0x00,0x00}, // 0x3e >
    {0x00,0x3e,0x41,0x01,0x01,0x41,0x3e}, // 0x3f ?
    {0x00,0x3e,0x41,0x4d,0x55,0x55,0x7e}, // 0x40 @
    {0x00,0x3e,0x41,0x41,0x7f,0x41,0x41}, // 0x41 A
    {0x00,0x7f,0x41,0x41,0x7e,0x01,0x7e}, // 0x42 B
    {0x00,0x3e,0x41,0x40,0x40,0x41,0x3e}, // 0x43 C
    {0x00,0x7f,0x41,0x41,0x41,0x41,0x3e}, // 0x44 D
    {0x00,0x7f,0x40,0x7c,0x40,0x40,0x7f}, // 0x45 E
    {0x00,0x7f,0x40,0x7c,0x40,0x40,0x40}, // 0x46 F
    {0x00,0x3e,0x41,0x40,0x41,0x41,0x3e}, // 0x47 G
    {0x00,0x41,0x41,0x7f,0x41,0x41,0x41}, // 0x48 H
    {0x00,0x7f,0x10,0x10,0x10,0x10,0x7f}, // 0x49 I
    {0x00,0x7f,0x01,0x01,0x01,0x41,0x3e}, // 0x4a J
    {0x00,0x41,0x41,0x3e,0x41,0x41,0x7f}, // 0x4b K
    {0x00,0x40,0x40,0x40,0x40,0x40,0x7f}, // 0x4c L
    {0x00,0x41,0x63,0x55,0x55,0x49,0x41}, // 0x4d M
    {0x00,0x41,0x63,0x73,0x59,0x4d,0x41}, // 0x4e N
    {0x00,0x3e,0x41,0x41,0x41,0x41,0x3e}, // 0x4f O
    {0x00,0x7f,0x41,0x41,0x7e,0x40,0x40}, // 0x50 P
    {0x00,0x3e,0x41,0x41,0x51,0x4a,0x34}, // 0x51 Q
    {0x00,0x7f,0x41,0x41,0x7e,0x44,0x38}, // 0x52 R
    {0x00,0x7e,0x41,0x41,0x01,0x41,0x3e}, // 0x53 S
    {0x00,0x7f,0x10,0x08,0x08,0x08,0x08}, // 0x54 T
    {0x00,0x41,0x41,0x41,0x41,0x41,0x3e}, // 0x55 U
    {0x00,0x41,0x41,0x41,0x41,0x22,0x1c}, // 0x56 V
    {0x00,0x41,0x41,0x41,0x55,0x55,0x66}, // 0x57 W
    {0x00,0x41,0x22,0x1c,0x22,0x41,0x41}, // 0x58 X
    {0x00,0x41,0x41,0x22,0x1c,0x22,0x41}, // 0x59 Y
    {0x00,0x7f,0x02,0x04,0x08,0x10,0x7f}, // 0x5a Z
    {0x00,0x7f,0x40,0x40,0x40,0x40,0x40}, // 0x5b [
    {0x00,0x02,0x04,0x08,0x10,0x20,0x00}, // 0x5c backslash
    {0x00,0x40,0x40,0x40,0x40,0x40,0x7f}, // 0x5d ]
    {0x00,0x20,0x40,0x20,0x00,0x00,0x00}, // 0x5e ^
    {0x00,0x40,0x40,0x40,0x40,0x40,0x40}, // 0x5f _
    {0x00,0x00,0x01,0x00,0x00,0x00,0x00}, // 0x60 `
    {0x00,0x00,0x36,0x49,0x49,0x49,0x36}, // 0x61 a
    {0x00,0x40,0x40,0x7e,0x41,0x41,0x7e}, // 0x62 b
    {0x00,0x00,0x36,0x40,0x40,0x40,0x36}, // 0x63 c
    {0x00,0x02,0x02,0x7e,0x41,0x41,0x7e}, // 0x64 d
    {0x00,0x00,0x36,0x49,0x49,0x49,0x49}, // 0x65 e
    {0x00,0x10,0x20,0x7c,0x20,0x20,0x40}, // 0x66 f
    {0x00,0x00,0x49,0x49,0x49,0x49,0x7e}, // 0x67 g
    {0x00,0x40,0x40,0x7e,0x41,0x41,0x41}, // 0x68 h
    {0x00,0x00,0x00,0x41,0x00,0x41,0x7e}, // 0x69 i
    {0x00,0x00,0x01,0x01,0x41,0x41,0x7e}, // 0x6a j
    {0x00,0x40,0x40,0x7e,0x10,0x20,0x40}, // 0x6b k
    {0x00,0x00,0x00,0x41,0x41,0x41,0x7f}, // 0x6c l
    {0x00,0x00,0x36,0x49,0x49,0x49,0x49}, // 0x6d m
    {0x00,0x00,0x7e,0x41,0x41,0x41,0x41}, // 0x6e n
    {0x00,0x00,0x36,0x49,0x49,0x49,0x36}, // 0x6f o
    {0x00,0x40,0x40,0x7e,0x41,0x40,0x40}, // 0x70 p
    {0x00,0x02,0x02,0x7e,0x41,0x41,0x41}, // 0x71 q
    {0x00,0x00,0x7e,0x40,0x40,0x40,0x40}, // 0x72 r
    {0x00,0x00,0x3e,0x40,0x3e,0x01,0x7e}, // 0x73 s
    {0x00,0x00,0x41,0x7f,0x01,0x01,0x01}, // 0x74 t
    {0x00,0x00,0x41,0x41,0x41,0x41,0x7e}, // 0x75 u
    {0x00,0x00,0x41,0x41,0x41,0x22,0x1c}, // 0x76 v
    {0x00,0x00,0x41,0x41,0x55,0x55,0x66}, // 0x77 w
    {0x00,0x00,0x41,0x22,0x1c,0x22,0x41}, // 0x78 x
    {0x00,0x00,0x41,0x41,0x41,0x22,0x1c}, // 0x79 y
    {0x00,0x00,0x7f,0x02,0x04,0x10,0x7f}, // 0x7a z
    {0x00,0x10,0x20,0x40,0x20,0x10,0x00}, // 0x7b {
    {0x00,0x10,0x10,0x10,0x10,0x10,0x10}, // 0x7c |
    {0x00,0x40,0x20,0x10,0x20,0x40,0x00}, // 0x7d }
    {0x00,0x06,0x09,0x09,0x09,0x06,0x00}, // 0x7e ~
};

static void set_color(SDL_Renderer* r, uint32_t color) {
    SDL_SetRenderDrawColor(r, (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, 0xff);
}

static void draw_char(SDL_Renderer* r, int x, int y, char ch, uint32_t color) {
    if (ch < 0x20 || ch > 0x7e) ch = '?';
    const uint8_t* glyph = font5x7[ch - 0x20];
    set_color(r, color);
    for (int row = 0; row < 7; ++row) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 5; ++col) {
            if (bits & (1u << (4 - col))) {
                SDL_RenderDrawPoint(r, x + col, y + row);
            }
        }
    }
}

static int text_w(const char* s) { return (int)strlen(s) * 6; }

static void draw_text(SDL_Renderer* r, int x, int y, const char* s, uint32_t color) {
    int cx = x;
    for (const char* p = s; *p; ++p) {
        if (*p == '\n') { cx = x; y += 8; continue; }
        draw_char(r, cx, y, *p, color);
        cx += 6;
    }
}

// ---------------------------------------------------------------------------
// UI widgets
// ---------------------------------------------------------------------------
enum FieldId { FIELD_EXE = 0, FIELD_DIR, FIELD_COUNT };

struct App {
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    bool running = true;

    char exe_path[4096] = {0};
    char target_dir[4096] = {0};
    int focused = -1;          // -1 = none, FIELD_EXE / FIELD_DIR
    bool blink = true;
    Uint32 blink_tick = 0;

    char status[1024] = "Ready. Pick a Windows .exe and a Linux target dir.";
    uint32_t status_color = 0x00ff00;

    bool relinking = false;
    bool elf_ready = false;
    fs::path elf_path;

    // Button rects
    SDL_Rect btn_browse_exe{20, 140, 140, 36};
    SDL_Rect btn_browse_dir{20, 200, 140, 36};
    SDL_Rect btn_relink{20, 280, 160, 44};
    SDL_Rect btn_run{200, 280, 120, 44};
    SDL_Rect btn_quit{740, 500, 140, 40};

    // Field rects
    SDL_Rect field_exe{180, 140, 700, 36};
    SDL_Rect field_dir{180, 200, 700, 36};

    App() = default;
    ~App() { cleanup(); }

    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            snprintf(status, sizeof status, "SDL init failed: %s", SDL_GetError());
            status_color = 0xff0000;
            return false;
        }
        win = SDL_CreateWindow("win2lin-gui - PE to ELF relinker",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
        if (!win) {
            snprintf(status, sizeof status, "Window failed: %s", SDL_GetError());
            status_color = 0xff0000;
            return false;
        }
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) {
            snprintf(status, sizeof status, "Renderer failed: %s", SDL_GetError());
            status_color = 0xff0000;
            return false;
        }
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        return true;
    }

    void cleanup() {
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        SDL_Quit();
    }

    static std::string run_cmd(const std::string& cmd) {
        std::array<char, 4096> buf;
        std::string out;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) return "(popen failed)";
        while (fgets(buf.data(), (int)buf.size(), pipe.get())) out += buf.data();
        return out;
    }

    static std::string select_file_dialog(const char* title, const char* filter) {
        // Prefer zenity, fall back to kdialog.
        std::string cmd;
        if (std::system("command -v zenity >/dev/null 2>&1") == 0) {
            cmd = "zenity --file-selection --title=\"" + std::string(title) +
                  "\" --file-filter=\"" + std::string(filter) + "\" 2>/dev/null";
        } else if (std::system("command -v kdialog >/dev/null 2>&1") == 0) {
            cmd = "kdialog --getopenfilename . \"" + std::string(filter) + "\" 2>/dev/null";
        } else {
            return "";
        }
        std::string out = run_cmd(cmd);
        // Trim trailing newline / whitespace.
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
        return out;
    }

    static std::string select_dir_dialog(const char* title) {
        if (std::system("command -v zenity >/dev/null 2>&1") == 0) {
            std::string cmd = "zenity --file-selection --directory --title=\"" + std::string(title) + "\" 2>/dev/null";
            std::string out = run_cmd(cmd);
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
            return out;
        } else if (std::system("command -v kdialog >/dev/null 2>&1") == 0) {
            std::string cmd = "kdialog --getexistingdirectory . 2>/dev/null";
            std::string out = run_cmd(cmd);
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
            return out;
        }
        return "";
    }

    void do_relink() {
        if (relinking) return;
        std::string exe = exe_path;
        std::string dir = target_dir;
        if (exe.empty()) { set_status("No Windows .exe selected.", 0xff0000); return; }
        if (dir.empty()) { set_status("No Linux target dir selected.", 0xff0000); return; }

        fs::path pe(exe);
        if (!fs::exists(pe)) { set_status("Selected .exe does not exist.", 0xff0000); return; }

        fs::path out_dir(dir);
        try { fs::create_directories(out_dir); }
        catch (const std::exception& e) { set_status(std::string("Cannot create dir: ") + e.what(), 0xff0000); return; }

        fs::path out_elf = out_dir / (pe.stem().string() + ".elf");

        // Find the relinker binary next to this executable ($ORIGIN/bin/win2lin-relinker).
        fs::path self = fs::read_symlink("/proc/self/exe");
        fs::path relinker = self.parent_path() / "win2lin-relinker";
        if (!fs::exists(relinker)) {
            // Fall back to PATH lookup.
            relinker = "win2lin-relinker";
        }

        std::ostringstream cmd;
        cmd << "\"" << relinker.string() << "\" \"" << pe.string() << "\" \"" << out_elf.string() << "\"";

        relinking = true;
        elf_ready = false;
        set_status("Relinking... please wait.", 0xffff00);
        render();

        int rc = std::system(cmd.str().c_str());
        relinking = false;

        if (rc != 0) {
            set_status("Relinker failed (exit " + std::to_string(rc) + ").", 0xff0000);
            return;
        }

        if (!fs::exists(out_elf)) {
            set_status("Relinker did not produce output ELF.", 0xff0000);
            return;
        }

        elf_path = out_elf;
        elf_ready = true;
        set_status("ELF ready: " + out_elf.string(), 0x00ff00);
    }

    void do_run() {
        if (!elf_ready) { set_status("No ELF to run yet.", 0xff0000); return; }
        fs::path self = fs::read_symlink("/proc/self/exe");
        fs::path shim_dir = self.parent_path(); // shims live next to the gui bin
        std::string ld_lib = "LD_LIBRARY_PATH=" + shim_dir.string();

        std::ostringstream cmd;
        cmd << ld_lib << " \"" << elf_path.string() << "\"";
        set_status("Launching: " + elf_path.filename().string(), 0x00ff00);
        int rc = std::system(cmd.str().c_str());
        std::ostringstream s;
        s << "Process exited with code " << rc;
        set_status(s.str(), 0x00ff00);
    }

    void set_status(const char* msg, uint32_t color) {
        snprintf(status, sizeof status, "%s", msg);
        status_color = color;
    }

    void set_status(std::string msg, uint32_t color) {
        snprintf(status, sizeof status, "%s", msg.c_str());
        status_color = color;
    }

    bool field_contains(SDL_Rect f, int mx, int my) const {
        return mx >= f.x && mx < f.x + f.w && my >= f.y && my < f.y + f.h;
    }

    bool btn_contains(SDL_Rect b, int mx, int my) const {
        return mx >= b.x && mx < b.x + b.w && my >= b.y && my < b.y + b.h;
    }

    void handle_text_input(const char* txt) {
        if (focused < 0 || focused >= FIELD_COUNT) return;
        char* buf = (focused == FIELD_EXE) ? exe_path : target_dir;
        size_t len = strlen(buf);
        size_t add = strlen(txt);
        if (len + add < sizeof(buf) - 1) {
            memcpy(buf + len, txt, add);
            buf[len + add] = '\0';
        }
    }

    void handle_backspace() {
        if (focused < 0 || focused >= FIELD_COUNT) return;
        char* buf = (focused == FIELD_EXE) ? exe_path : target_dir;
        size_t len = strlen(buf);
        if (len > 0) buf[len - 1] = '\0';
    }

    void render() {
        SDL_SetRenderDrawColor(ren, 0x1e, 0x1e, 0x24, 0xff);
        SDL_RenderClear(ren);

        // Title
        draw_text(ren, 20, 16, "win2lin-gui  -  Windows .exe  ->  Linux ELF", 0xffffff);
        draw_text(ren, 20, 28, "Select a Windows game .exe, a Linux target dir, then relink & run.", 0xaaaaaa);

        // Labels
        draw_text(ren, 20, 150, "Windows .exe:", 0xcccccc);
        draw_text(ren, 20, 210, "Linux target dir:", 0xcccccc);

        // Fields
        draw_field(field_exe, exe_path, focused == FIELD_EXE);
        draw_field(field_dir, target_dir, focused == FIELD_DIR);

        // Buttons
        draw_btn(btn_browse_exe, "Browse file", 0x4a4a5a);
        draw_btn(btn_browse_dir, "Browse dir", 0x4a4a5a);
        draw_btn(btn_relink, relinking ? "Working..." : "Relink -> ELF", relinking ? 0x444444 : 0x2a6a2a);
        draw_btn(btn_run, elf_ready ? "Run ELF" : "Run ELF", elf_ready ? 0x2a6a2a : 0x444444);
        draw_btn(btn_quit, "Quit", 0x6a2a2a);

        // Status
        draw_text(ren, 20, 360, status, status_color);

        // Hint
        draw_text(ren, 20, 380, "Shim libs (libwin_*.so) are loaded from the folder next to this GUI binary.", 0x777777);
        draw_text(ren, 20, 392, "No SPIR-V / Vulkan path in this build (no RDNA 2 GPU required).", 0x777777);

        SDL_RenderPresent(ren);
    }

    void draw_field(SDL_Rect f, const char* text, bool active) {
        SDL_SetRenderDrawColor(ren, 0x2a, 0x2a, 0x32, 0xff);
        SDL_RenderFillRect(ren, &f);
        set_color(ren, active ? 0x4a90d9 : 0x444444);
        SDL_RenderDrawRect(ren, &f);

        int ty = f.y + (f.h - 7) / 2;
        if (active && blink) {
            draw_text(ren, f.x + 6, ty, text, 0xffffff);
            set_color(ren, 0xffffff);
            SDL_RenderDrawPoint(ren, f.x + 6 + text_w(text), ty);
        } else {
            draw_text(ren, f.x + 6, ty, text, 0xffffff);
        }
    }

    void draw_btn(SDL_Rect b, const char* label, uint32_t base) {
        set_color(ren, base);
        SDL_RenderFillRect(ren, &b);
        set_color(ren, 0x888888);
        SDL_RenderDrawRect(ren, &b);
        int tw = text_w(label);
        int tx = b.x + (b.w - tw) / 2;
        int ty = b.y + (b.h - 7) / 2;
        draw_text(ren, tx, ty, label, 0xffffff);
    }

    void run() {
        Uint32 last = SDL_GetTicks();
        while (running) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                handle_event(ev);
            }
            Uint32 now = SDL_GetTicks();
            if (now - last > 400) { blink = !blink; last = now; }
            render();
            SDL_Delay(16);
        }
    }

    void handle_event(SDL_Event& ev) {
        switch (ev.type) {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_MOUSEBUTTONDOWN: {
            int mx = ev.button.x, my = ev.button.y;
            if (btn_contains(btn_quit, mx, my)) { running = false; break; }
            if (btn_contains(btn_browse_exe, mx, my)) {
                std::string sel = select_file_dialog("Select Windows .exe", "*.exe");
                if (!sel.empty()) { strncpy(exe_path, sel.c_str(), sizeof exe_path - 1); exe_path[sizeof exe_path - 1] = 0; focused = FIELD_EXE; }
                break;
            }
            if (btn_contains(btn_browse_dir, mx, my)) {
                std::string sel = select_dir_dialog("Select Linux target directory");
                if (!sel.empty()) { strncpy(target_dir, sel.c_str(), sizeof target_dir - 1); target_dir[sizeof target_dir - 1] = 0; focused = FIELD_DIR; }
                break;
            }
            if (btn_contains(btn_relink, mx, my)) { do_relink(); break; }
            if (btn_contains(btn_run, mx, my)) { do_run(); break; }
            if (field_contains(field_exe, mx, my)) focused = FIELD_EXE;
            else if (field_contains(field_dir, mx, my)) focused = FIELD_DIR;
            else focused = -1;
            break;
        }
        case SDL_TEXTINPUT:
            handle_text_input(ev.text.text);
            break;
        case SDL_KEYDOWN: {
            if (ev.key.keysym.sym == SDLK_ESCAPE) running = false;
            else if (ev.key.keysym.sym == SDLK_BACKSPACE) handle_backspace();
            else if (ev.key.keysym.sym == SDLK_TAB) focused = (focused == FIELD_EXE) ? FIELD_DIR : FIELD_EXE;
            break;
        }
        default: break;
        }
    }
};

int main(int /*argc*/, char* /*argv*/[]) {
    App app;
    if (!app.init()) return 1;
    app.run();
    return 0;
}
