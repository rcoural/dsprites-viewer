#include "DSprites.h"

#include <cstdio>

namespace zeta {

namespace {
constexpr std::uint16_t rd16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
constexpr std::uint32_t rd32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
constexpr int align4(int v) noexcept { return (v + 3) & ~3; }
}  // namespace

bool DSpriteArchive::load(const std::filesystem::path& rsc_path) {
    offsets_.clear();
    std::FILE* f = std::fopen(rsc_path.string().c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "DSprites: cannot open %s\n", rsc_path.string().c_str());
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz < 6) { std::fclose(f); return false; }
    data_.resize(static_cast<std::size_t>(sz));
    const bool read_ok = std::fread(data_.data(), 1, data_.size(), f) == data_.size();
    std::fclose(f);
    if (!read_ok) { data_.clear(); return false; }

    const int cnt = rd16(data_.data());
    const std::size_t tab_end  = 2u + static_cast<std::size_t>(cnt) * 4u;
    const std::size_t pal_off  = 2u + static_cast<std::size_t>(cnt) * 5u;   // +type table
    if (pal_off + 768u > data_.size() || cnt == 0) {
        std::fprintf(stderr, "DSprites: %s not a valid archive (count=%d)\n",
                     rsc_path.string().c_str(), cnt);
        data_.clear();
        return false;
    }

    offsets_.reserve(static_cast<std::size_t>(cnt) + 1);
    for (int i = 0; i < cnt; ++i) offsets_.push_back(rd32(&data_[2u + i * 4u]));
    offsets_.push_back(static_cast<std::uint32_t>(data_.size()));
    // ascending + in-bounds sanity (first record must start right after palette).
    if (offsets_[0] != pal_off + 768u) {
        std::fprintf(stderr, "DSprites: unexpected first-record offset 0x%x (want 0x%x)\n",
                     static_cast<unsigned>(offsets_[0]),
                     static_cast<unsigned>(pal_off + 768u));
    }

    for (int i = 0; i < 256; ++i) {
        const std::uint8_t* c = &data_[pal_off + i * 3u];
        // 6-bit VGA (0..63) -> 8-bit.
        palette_[i][0] = static_cast<std::uint8_t>(c[0] << 2);
        palette_[i][1] = static_cast<std::uint8_t>(c[1] << 2);
        palette_[i][2] = static_cast<std::uint8_t>(c[2] << 2);
    }
    (void)tab_end;
    std::fprintf(stderr, "DSprites: loaded %s (%d sprites)\n", rsc_path.string().c_str(), cnt);
    return true;
}

int DSpriteArchive::width(int index) const noexcept {
    if (index < 0 || index >= count()) return 0;
    return data_[offsets_[index] + 1] * 8;   // byte[1] * 8
}

int DSpriteArchive::height(int index) const noexcept {
    if (index < 0 || index >= count()) return 0;
    return data_[offsets_[index]];           // byte[0]
}

std::optional<DSpriteImage> DSpriteArchive::decode(int index, const std::uint8_t* remap) const {
    if (index < 0 || index >= count()) return std::nullopt;
    const std::uint32_t off = offsets_[index];
    const std::uint32_t end = offsets_[index + 1];
    if (end <= off + 2) return std::nullopt;

    const int H = data_[off];
    const int W = data_[off + 1] * 8;
    if (H == 0 || W == 0) return std::nullopt;

    const std::uint32_t pix = off + static_cast<std::uint32_t>(align4(2 + H));
    const std::size_t need = static_cast<std::size_t>(W) * H;
    if (pix + need > end) return std::nullopt;

    DSpriteImage img;
    img.width  = W;
    img.height = H;
    img.rgba.assign(need * 4u, 0);  // transparent by default

    const int cols = W / 4;
    std::size_t s = pix;
    for (int plane = 0; plane < 4; ++plane) {
        for (int y = 0; y < H; ++y) {
            for (int c = 0; c < cols; ++c) {
                const std::uint8_t v = data_[s++];
                if (v == 0) continue;  // transparent (remap never touches index 0)
                const std::uint8_t pv = remap ? remap[v] : v;
                const int x = plane + c * 4;
                std::uint8_t* px = &img.rgba[(static_cast<std::size_t>(y) * W + x) * 4u];
                px[0] = palette_[pv][0];
                px[1] = palette_[pv][1];
                px[2] = palette_[pv][2];
                px[3] = 255;
            }
        }
    }
    return img;
}

std::optional<DSpriteImage> DSpriteArchive::decode_chunky(
        const std::uint8_t* pixels, int W, int H, const std::uint8_t* remap) const {
    if (!pixels || W <= 0 || H <= 0) return std::nullopt;
    DSpriteImage img;
    img.width  = W;
    img.height = H;
    const std::size_t need = static_cast<std::size_t>(W) * H;
    img.rgba.assign(need * 4u, 0);  // transparent by default
    for (std::size_t i = 0; i < need; ++i) {
        const std::uint8_t v = pixels[i];
        if (v == 0) continue;  // index 0 = transparent
        const std::uint8_t pv = remap ? remap[v] : v;
        std::uint8_t* px = &img.rgba[i * 4u];
        px[0] = palette_[pv][0];
        px[1] = palette_[pv][1];
        px[2] = palette_[pv][2];
        px[3] = 255;
    }
    return img;
}

}  // namespace zeta
