// Standalone grid browser for the DOS Z (1996) DSPRITES.RSC sprite archive.
// Decodes every record natively (src/DSprites.*) and renders the whole bank as a
// scrollable, zoomable grid. No assets are bundled - point it at your own
// DSPRITES.RSC from an original Z install.
//
//   dsprites-view <path/to/DSPRITES.RSC> [options]
//
//     --scale N        initial pixel zoom (default 4)
//     --jump  N        select sprite N and scroll to it on start
//     --size  W H      initial window size (default 1100 760)
//     --shot  OUT.bmp  render one frame to a BMP and exit (headless export)
//
//   In the window:
//     wheel / PageUp·Down / Up·Down / Home·End   scroll
//     Ctrl/Shift + wheel  ·  +/-                  zoom
//     left click                                  select (title shows #idx WxH)
//     type digits + Enter                         jump to sprite index
//     b   cycle background (dark / checker / magenta)   g   toggle index labels
//     Esc / window close                          quit

#include <SDL3/SDL.h>

#include "DSprites.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

// 3x5 bitmap digits; 5 rows, low 3 bits = columns (bit2 = leftmost).
constexpr std::uint8_t kDigits[10][5] = {
    {0b111, 0b101, 0b101, 0b101, 0b111},  // 0
    {0b010, 0b110, 0b010, 0b010, 0b111},  // 1
    {0b111, 0b001, 0b111, 0b100, 0b111},  // 2
    {0b111, 0b001, 0b111, 0b001, 0b111},  // 3
    {0b101, 0b101, 0b111, 0b001, 0b001},  // 4
    {0b111, 0b100, 0b111, 0b001, 0b111},  // 5
    {0b111, 0b100, 0b111, 0b101, 0b111},  // 6
    {0b111, 0b001, 0b010, 0b010, 0b010},  // 7
    {0b111, 0b101, 0b111, 0b101, 0b111},  // 8
    {0b111, 0b101, 0b111, 0b001, 0b111},  // 9
};

void draw_number(SDL_Renderer* r, int value, float x, float y, float px) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d", value);
    for (const char* p = buf; *p; ++p) {
        const int d = *p - '0';
        if (d < 0 || d > 9) continue;
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 3; ++col)
                if (kDigits[d][row] & (1 << (2 - col))) {
                    SDL_FRect rc{x + col * px, y + row * px, px, px};
                    SDL_RenderFillRect(r, &rc);
                }
        x += 4 * px;
    }
}

struct TexCache {
    const zeta::DSpriteArchive* arch = nullptr;
    SDL_Renderer* ren = nullptr;
    std::vector<SDL_Texture*> tex;
    std::vector<char> tried;

    void init(const zeta::DSpriteArchive* a, SDL_Renderer* r) {
        arch = a;
        ren = r;
        tex.assign(a->count(), nullptr);
        tried.assign(a->count(), 0);
    }
    SDL_Texture* get(int i) {
        if (i < 0 || i >= static_cast<int>(tex.size())) return nullptr;
        if (!tried[i]) {
            tried[i] = 1;
            if (auto img = arch->decode(i); img && img->width > 0 && img->height > 0) {
                SDL_Texture* t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888,
                                                   SDL_TEXTUREACCESS_STATIC,
                                                   img->width, img->height);
                if (t) {
                    SDL_UpdateTexture(t, nullptr, img->rgba.data(), img->width * 4);
                    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
                    tex[i] = t;
                }
            }
        }
        return tex[i];
    }
};

// Integer upscale to fill `box` for sprites <= box; float downscale to fit for
// oversized ones (wide banners / font sheets). Keeps small art crisp.
float fit_scale(int w, int h, float box) {
    const int maxdim = std::max(w, h);
    if (maxdim <= 0) return 1.0f;
    if (maxdim * 1.0f <= box) return std::max(1.0f, std::floor(box / maxdim));
    return box / static_cast<float>(maxdim);
}

}  // namespace

int main(int argc, char** argv) {
    const char* path = nullptr;
    const char* shot = nullptr;
    int zoom = 4, win_w = 1100, win_h = 760, jump_to = -1;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--scale" && i + 1 < argc) zoom = std::atoi(argv[++i]);
        else if (a == "--jump" && i + 1 < argc) jump_to = std::atoi(argv[++i]);
        else if (a == "--shot" && i + 1 < argc) shot = argv[++i];
        else if (a == "--size" && i + 2 < argc) { win_w = std::atoi(argv[++i]); win_h = std::atoi(argv[++i]); }
        else if (!a.empty() && a[0] != '-') path = argv[i];
        else { std::fprintf(stderr, "unknown option: %s\n", a.c_str()); }
    }
    if (!path) {
        std::fprintf(stderr,
            "usage: dsprites-view <DSPRITES.RSC> [--scale N] [--jump N] [--shot out.bmp]\n");
        return 2;
    }

    zeta::DSpriteArchive arch;
    if (!arch.load(path)) {
        std::fprintf(stderr, "failed to load %s\n", path);
        return 1;
    }
    const int count = arch.count();
    zoom = std::clamp(zoom, 1, 24);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow("DSPRITES.RSC viewer", win_w, win_h, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* ren = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
    if (!win || !ren) {
        std::fprintf(stderr, "SDL window/renderer: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(ren, 1);

    TexCache cache;
    cache.init(&arch, ren);

    constexpr float BOX_SRC = 16.0f;   // nominal source px the cell content box holds at zoom 1
    constexpr float PADX = 12.0f, PADY = 10.0f, LABEL_H = 16.0f, MARGIN = 10.0f, LABEL_PX = 2.0f;

    float scroll_y = 0.0f;
    int hovered = -1, selected = jump_to >= 0 && jump_to < count ? jump_to : -1;
    float mx = 0, my = 0;
    int bg = 0;            // 0 dark, 1 checker, 2 magenta
    bool labels = true;
    std::string jump_buf;
    bool want_center_selected = selected >= 0;

    auto layout = [&](int& cols, int& rows, float& cellW, float& cellH, int& W, int& H) {
        SDL_GetRenderOutputSize(ren, &W, &H);
        const float box = BOX_SRC * zoom;
        cellW = box + PADX;
        cellH = box + LABEL_H + PADY;
        cols = std::max(1, static_cast<int>((W - 2 * MARGIN) / cellW));
        rows = (count + cols - 1) / cols;
    };

    auto frame = [&]() {
        int cols, rows, W, H;
        float cellW, cellH;
        layout(cols, rows, cellW, cellH, W, H);
        const float box = BOX_SRC * zoom;
        const float content_h = rows * cellH + 2 * PADY;
        const float max_scroll = std::max(0.0f, content_h - H);

        if (want_center_selected && selected >= 0) {
            const int row = selected / cols;
            scroll_y = std::clamp(row * cellH + PADY - (H - cellH) * 0.5f, 0.0f, max_scroll);
            want_center_selected = false;
        }
        scroll_y = std::clamp(scroll_y, 0.0f, max_scroll);

        // hovered index from mouse
        hovered = -1;
        {
            const float gx = mx - MARGIN;
            const float gy = my + scroll_y - PADY;
            if (gx >= 0 && gy >= 0) {
                const int c = static_cast<int>(gx / cellW);
                const int r = static_cast<int>(gy / cellH);
                if (c >= 0 && c < cols) {
                    const int idx = r * cols + c;
                    if (idx >= 0 && idx < count) hovered = idx;
                }
            }
        }

        if (bg == 2) SDL_SetRenderDrawColor(ren, 200, 0, 200, 255);
        else         SDL_SetRenderDrawColor(ren, 24, 24, 30, 255);
        SDL_RenderClear(ren);

        const int first_row = std::max(0, static_cast<int>((scroll_y - PADY) / cellH));
        const int last_row  = std::min(rows - 1, static_cast<int>((scroll_y + H - PADY) / cellH) + 1);

        for (int r = first_row; r <= last_row; ++r) {
            for (int c = 0; c < cols; ++c) {
                const int idx = r * cols + c;
                if (idx >= count) break;
                const float cx = MARGIN + c * cellW;
                const float cy = PADY + r * cellH - scroll_y;
                const SDL_FRect content{cx + PADX * 0.5f, cy + PADY * 0.5f, box, box};

                // content panel / checker (to read transparency)
                if (bg == 1) {
                    const float q = std::max(4.0f, box / 8.0f);
                    for (float yy = 0; yy < box; yy += q)
                        for (float xx = 0; xx < box; xx += q) {
                            const bool dark = (static_cast<int>(xx / q) + static_cast<int>(yy / q)) & 1;
                            SDL_SetRenderDrawColor(ren, dark ? 40 : 60, dark ? 40 : 60, dark ? 46 : 66, 255);
                            SDL_FRect cell{content.x + xx, content.y + yy,
                                           std::min(q, box - xx), std::min(q, box - yy)};
                            SDL_RenderFillRect(ren, &cell);
                        }
                } else {
                    SDL_SetRenderDrawColor(ren, 16, 16, 20, 255);
                    SDL_RenderFillRect(ren, &content);
                }

                if (SDL_Texture* t = cache.get(idx)) {
                    const int sw = arch.width(idx), sh = arch.height(idx);
                    const float s = fit_scale(sw, sh, box);
                    const float dw = sw * s, dh = sh * s;
                    SDL_FRect dst{content.x + (box - dw) * 0.5f, content.y + (box - dh) * 0.5f, dw, dh};
                    SDL_RenderTexture(ren, t, nullptr, &dst);
                }

                if (idx == selected) {
                    SDL_SetRenderDrawColor(ren, 80, 200, 255, 255);
                    SDL_FRect b{content.x - 2, content.y - 2, box + 4, box + 4};
                    SDL_RenderRect(ren, &b);
                } else if (idx == hovered) {
                    SDL_SetRenderDrawColor(ren, 120, 120, 140, 255);
                    SDL_FRect b{content.x - 1, content.y - 1, box + 2, box + 2};
                    SDL_RenderRect(ren, &b);
                }

                if (labels) {
                    SDL_SetRenderDrawColor(ren, 150, 150, 165, 255);
                    draw_number(ren, idx, cx + PADX * 0.5f, cy + box + PADY * 0.5f + 2, LABEL_PX);
                }
            }
        }

        // title bar = live info
        char title[256];
        const int info = hovered >= 0 ? hovered : selected;
        if (!jump_buf.empty()) {
            std::snprintf(title, sizeof title, "DSPRITES.RSC - jump to #%s (Enter)", jump_buf.c_str());
        } else if (info >= 0) {
            std::snprintf(title, sizeof title, "DSPRITES.RSC - #%d  %dx%d   (%d sprites, zoom %dx)",
                          info, arch.width(info), arch.height(info), count, zoom);
        } else {
            std::snprintf(title, sizeof title, "DSPRITES.RSC viewer - %d sprites, zoom %dx", count, zoom);
        }
        SDL_SetWindowTitle(win, title);
    };

    // headless screenshot: warm up (let the window map / textures upload),
    // render, save, quit.
    if (shot) {
        constexpr Uint64 kWarmupMs = 250;
        const Uint64 start = SDL_GetTicks();
        while (SDL_GetTicks() - start < kWarmupMs) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {}
            frame();
            SDL_RenderPresent(ren);
        }
        frame();
        SDL_RenderPresent(ren);
        if (SDL_Surface* surf = SDL_RenderReadPixels(ren, nullptr)) {
            if (!SDL_SaveBMP(surf, shot))
                std::fprintf(stderr, "SDL_SaveBMP: %s\n", SDL_GetError());
            else
                std::fprintf(stderr, "wrote %s\n", shot);
            SDL_DestroySurface(surf);
        }
        SDL_Quit();
        return 0;
    }

    auto bump_zoom = [&](int dir) {
        const int z = std::clamp(zoom + dir, 1, 24);
        if (z != zoom) { zoom = z; want_center_selected = selected >= 0; }
    };

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT: running = false; break;
                case SDL_EVENT_MOUSE_MOTION: mx = e.motion.x; my = e.motion.y; break;
                case SDL_EVENT_MOUSE_WHEEL: {
                    const SDL_Keymod mod = SDL_GetModState();
                    if (mod & (SDL_KMOD_CTRL | SDL_KMOD_SHIFT)) bump_zoom(e.wheel.y > 0 ? 1 : -1);
                    else scroll_y -= e.wheel.y * (BOX_SRC * zoom) * 0.9f;
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (e.button.button == SDL_BUTTON_LEFT && hovered >= 0) selected = hovered;
                    break;
                case SDL_EVENT_KEY_DOWN: {
                    const SDL_Keycode k = e.key.key;
                    int cols, rows, W, H; float cellW, cellH;
                    layout(cols, rows, cellW, cellH, W, H);
                    if (k >= SDLK_0 && k <= SDLK_9) jump_buf += static_cast<char>('0' + (k - SDLK_0));
                    else if (k == SDLK_BACKSPACE) { if (!jump_buf.empty()) jump_buf.pop_back(); }
                    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                        if (!jump_buf.empty()) {
                            const int idx = std::clamp(std::atoi(jump_buf.c_str()), 0, count - 1);
                            selected = idx; want_center_selected = true; jump_buf.clear();
                        }
                    } else if (k == SDLK_ESCAPE) { if (!jump_buf.empty()) jump_buf.clear(); else running = false; }
                    else if (k == SDLK_PAGEUP)   scroll_y -= H * 0.9f;
                    else if (k == SDLK_PAGEDOWN) scroll_y += H * 0.9f;
                    else if (k == SDLK_UP)       scroll_y -= cellH;
                    else if (k == SDLK_DOWN)     scroll_y += cellH;
                    else if (k == SDLK_HOME)     scroll_y = 0;
                    else if (k == SDLK_END)      scroll_y = 1e9f;
                    else if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS)  bump_zoom(1);
                    else if (k == SDLK_MINUS || k == SDLK_KP_MINUS)                    bump_zoom(-1);
                    else if (k == SDLK_B)        bg = (bg + 1) % 3;
                    else if (k == SDLK_G)        labels = !labels;
                    break;
                }
                default: break;
            }
        }
        frame();
        SDL_RenderPresent(ren);
    }

    SDL_Quit();
    return 0;
}
