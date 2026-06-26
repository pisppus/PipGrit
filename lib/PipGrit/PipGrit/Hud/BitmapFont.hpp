#pragma once

#include <cstdint>
#include <cstddef>

namespace pipgrit
{
    class BitmapFont
    {
    public:
        static inline constexpr int16_t GlyphW = 5;
        static inline constexpr int16_t GlyphH = 7;
        static inline constexpr int16_t Advance = GlyphW + 1;
        static inline constexpr uint8_t FirstChar = 0x20;
        static inline constexpr uint8_t LastChar = 0x7E;
        static inline constexpr uint8_t GlyphCount = LastChar - FirstChar + 1;

        static void drawChar(uint16_t *buf, int16_t stride, int16_t bufH, int16_t x, int16_t y,
                             char ch, uint16_t fg, uint16_t bg, bool opaque) noexcept;

        static int16_t drawString(uint16_t *buf, int16_t bufW, int16_t bufH,
                                  int16_t x, int16_t y, const char *text,
                                  uint16_t fg, uint16_t bg, bool opaque) noexcept;

        [[nodiscard]] static int16_t textWidth(const char *text) noexcept;

    private:
        static const uint8_t kGlyphs[GlyphCount][GlyphH];
    };
}