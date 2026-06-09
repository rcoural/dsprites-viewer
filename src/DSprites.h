#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace zeta {

// One decoded sprite: RGBA8888 pixels, palette index 0 rendered as transparent.
struct DSpriteImage {
    int width  = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;  // width * height * 4
};

// Decoder for the DOS Z (1996) `DSPRITES.RSC` archive - units, buildings,
// effects, cursors, UI and bitmap fonts. Format (cracked from ZED.EXE's blitter
// at virt 0x5263d, see docs/dsprites-format.md):
//
//   0          u16        record count
//   2          u32[count] absolute record offsets (ascending)
//   2+count*4  u8 [count]  per-record width tier (== byte[1])
//   2+count*5  byte[768]   256-entry 6-bit VGA palette
//   ...        record[i] = file[off[i] : off[i+1]]
//
// Per record: byte[0]=height H, byte[1]=width/8 (T) -> width = T*8; pixel data at
// align4(2+H), length width*H, stored VGA mode-X planar (4 planes). De-interleave:
// pixel(x,y): plane = x & 3, col = x >> 2; src = plane*(H*cols) + y*cols + col.
// Index 0 is transparent. Pixels are raw palette indices (no compression).
//
// This is the same decoder that powers the Zeta engine (port of Z to C++/SDL3);
// it has no engine dependencies and can be reused standalone.
class DSpriteArchive {
public:
    [[nodiscard]] bool load(const std::filesystem::path& rsc_path);

    [[nodiscard]] bool ok()    const noexcept { return offsets_.size() > 1; }
    [[nodiscard]] int  count() const noexcept {
        return offsets_.empty() ? 0 : static_cast<int>(offsets_.size()) - 1;
    }

    [[nodiscard]] int width(int index)  const noexcept;   // pixels, 0 if invalid
    [[nodiscard]] int height(int index) const noexcept;

    // Decode sprite `index` to RGBA. nullopt if out of range or malformed.
    // `remap` (optional) is a 256-entry palette-index LUT applied before the
    // palette lookup - used for team colours (the red armour ramp [16..31] is
    // shifted to a team's ramp). Index 0 stays transparent regardless of remap.
    [[nodiscard]] std::optional<DSpriteImage>
    decode(int index, const std::uint8_t* remap = nullptr) const;

    // Decode raw row-major chunky 8bpp pixels (width*height palette indices) to
    // RGBA, with the same palette / index-0-transparent / optional remap rules
    // as decode(). Used for portrait & HUD-chrome frames, which share the game
    // palette but are stored chunky (not as DSPRITES planar records).
    [[nodiscard]] std::optional<DSpriteImage>
    decode_chunky(const std::uint8_t* pixels, int width, int height,
                  const std::uint8_t* remap = nullptr) const;

    // Raw 6-bit-scaled-to-8-bit palette entry (for building team-colour LUTs etc).
    [[nodiscard]] const std::uint8_t* palette() const noexcept { return &palette_[0][0]; }

private:
    std::vector<std::uint8_t>  data_;       // whole file
    std::vector<std::uint32_t> offsets_;    // count + 1 entries
    std::uint8_t palette_[256][3]{};        // already scaled to 8-bit (0..255)
};

}  // namespace zeta
