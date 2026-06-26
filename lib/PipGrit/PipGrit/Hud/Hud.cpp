#include <PipGrit/Hud/Hud.hpp>
#include <PipGrit/Sim/Material.hpp>
#include <PipGrit/Render/Renderer.hpp>

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace pipgrit
{
    namespace
    {
        constexpr uint16_t kColorText = pipcore::Sprite::swap16(pipcore::Sprite::color565(0xFF, 0xFF, 0xFF));
        constexpr uint16_t kColorShadow = pipcore::Sprite::swap16(pipcore::Sprite::color565(0x00, 0x00, 0x00));

        const uint16_t bgSwapped = pipcore::Sprite::swap16(pipcore::Sprite::color565(12, 12, 18));
        const uint16_t selectBorderSwapped = pipcore::Sprite::swap16(pipcore::Sprite::color565(0xFF, 0x00, 0x00));

        inline uint16_t blendSwapped565(uint16_t bgSwapped, uint16_t fgSwapped, uint8_t alpha) noexcept
        {
            uint16_t bg = __builtin_bswap16(bgSwapped);
            uint16_t fg = __builtin_bswap16(fgSwapped);
            uint16_t res = pipcore::Sprite::blend565(bg, fg, alpha);
            return __builtin_bswap16(res);
        }

        void drawRoundedRectAA(uint16_t *buf, int16_t stride, int16_t bufH, int16_t rx, int16_t ry, int16_t rw, int16_t rh, int16_t radius, uint16_t color, uint16_t bgColor)
        {
            const int16_t x0 = std::max<int16_t>(rx - 1, 0);
            const int16_t x1 = std::min<int16_t>(static_cast<int16_t>(rx + rw + 1), stride);
            const int16_t y0 = std::max<int16_t>(ry - 1, 0);
            const int16_t y1 = std::min<int16_t>(static_cast<int16_t>(ry + rh + 1), bufH);

            const float cx = rx + rw * 0.5f;
            const float cy = ry + rh * 0.5f;
            const float half_w = rw * 0.5f;
            const float half_h = rh * 0.5f;
            const float r_val = static_cast<float>(radius);

            for (int16_t y = y0; y < y1; ++y)
            {
                uint16_t *row = buf + static_cast<size_t>(y) * stride;
                const float py = y + 0.5f;
                const float dy = std::abs(py - cy) - (half_h - r_val);

                for (int16_t x = x0; x < x1; ++x)
                {
                    const float px = x + 0.5f;
                    const float dx = std::abs(px - cx) - (half_w - r_val);

                    float dist;
                    if (dx > 0.0f && dy > 0.0f)
                    {
                        dist = std::sqrt(dx * dx + dy * dy) - r_val;
                    }
                    else
                    {
                        dist = std::max(dx, dy) - r_val;
                    }

                    if (dist <= -0.5f)
                    {
                        row[x] = color;
                    }
                    else if (dist < 0.5f)
                    {
                        float alpha = 0.5f - dist;
                        uint8_t a = static_cast<uint8_t>(alpha * 255.0f);
                        row[x] = blendSwapped565(bgColor, color, a);
                    }
                }
            }
        }
    }

    void Hud::bind(pipcore::Platform *platform, Renderer *renderer) noexcept
    {
        _platform = platform;
        _renderer = renderer;
        _perf.setPlatform(platform);

        const int16_t screenW = platform->display() ? platform->display()->width() : 480;

        int16_t cx = screenW - 16;
        for (int c = 3; c >= 0; --c)
        {
            _catW[c] = BitmapFont::textWidth(kCategories[c].name) + 12;
            cx -= _catW[c];
            _catX[c] = cx;
            cx -= 10;
        }

        selectCategory(0);
    }

    void Hud::selectCategory(uint8_t catIdx) noexcept
    {
        if (catIdx >= 4) return;
        _selectedCategory = catIdx;

        int16_t startX = 16;
        _activeItemCount = 0;

        for (size_t i = 0; i < kSelectorItemCount; ++i)
        {
            if (kSelectorItems[i].category == catIdx)
            {
                _activeItemIndices[_activeItemCount] = static_cast<uint8_t>(i);
                _activeItemX[_activeItemCount] = startX;
                _activeItemW[_activeItemCount] = BitmapFont::textWidth(kSelectorItems[i].name) + 24; 
                startX += _activeItemW[_activeItemCount] + 10;
                _activeItemCount++;
            }
        }
        _totalWidth = startX + 8;
        _maxScrollX = std::max<int16_t>(0, _totalWidth - (_platform && _platform->display() ? _platform->display()->width() : 480));
        _scrollX = 0;
        
        if (_activeItemCount > 0)
        {
            _selectedActiveIdx = 0;
            _selectedIdx = _activeItemIndices[0];
        }
    }

    void Hud::composite(uint16_t *bandBuf, int16_t bandY0, int16_t bandRows, int16_t stride, uint32_t cellCount, uint8_t touches, int16_t touchX, int16_t touchY) noexcept
    {
        if (!_enabled || !bandBuf || !_platform)
            return;

        constexpr int16_t screenMarginX = 5;
        constexpr int16_t screenMarginY = 7;
        
        char line[48];
        const int len = std::snprintf(line, sizeof(line),
                                      "%4.0ffps %2ums  c:%lu  t:%u",
                                      _perf.avgFps() > 0 ? _perf.avgFps() : _perf.fps(),
                                      _perf.frameTimeMs(),
                                      static_cast<unsigned long>(cellCount),
                                      static_cast<unsigned>(touches));
        if (len <= 0)
            return;

        const int16_t textW = BitmapFont::textWidth(line);
        const int16_t boxH = BitmapFont::GlyphH + 2;
        const int16_t boxW = textW + 6;

        const int16_t hudY0 = screenMarginY;
        const int16_t hudY1 = screenMarginY + boxH;

        const int16_t intersectY0 = std::max(bandY0, hudY0);
        const int16_t intersectY1 = std::min<int16_t>(static_cast<int16_t>(bandY0 + bandRows), hudY1);

        const uint16_t bg = kColorLut[Empty];

        if (intersectY0 < intersectY1)
        {
            constexpr size_t kScratchW = 220;
            constexpr size_t kScratchH = 12;
            if (static_cast<size_t>(boxW) > kScratchW || static_cast<size_t>(boxH) > kScratchH)
                return;

            static uint16_t box[kScratchW * kScratchH];
            const size_t totalPixels = static_cast<size_t>(boxW) * boxH;
            for (size_t i = 0; i < totalPixels; ++i)
                box[i] = bg;

            BitmapFont::drawString(box, boxW, boxH, 4, 2, line, kColorShadow, 0, false);
            BitmapFont::drawString(box, boxW, boxH, 3, 1, line, kColorText, 0, false);

            for (int16_t y = intersectY0; y < intersectY1; ++y)
            {
                const int16_t boxRow = y - hudY0;
                const int16_t bandRow = y - bandY0;

                uint16_t *dst = bandBuf + static_cast<size_t>(bandRow) * stride + screenMarginX;
                const uint16_t *src = box + static_cast<size_t>(boxRow) * boxW;

                std::memcpy(dst, src, static_cast<size_t>(boxW) * sizeof(uint16_t));
            }
        }

        if (touchX >= 0 && touchY >= 0 && _renderer && _renderer->grid())
        {
            const Grid *grid = _renderer->grid();
            if (grid->inBounds(touchX, touchY))
            {
                uint8_t t_val = grid->temp(touchX, touchY);
                uint8_t p_val = grid->localPressure(touchX, touchY);

                char r_line[32];
                const int r_len = std::snprintf(r_line, sizeof(r_line), "T:%uC P:%u%%", t_val, p_val);
                if (r_len > 0)
                {
                    const int16_t r_textW = BitmapFont::textWidth(r_line);
                    const int16_t r_boxW = r_textW + 6;
                    const int16_t r_boxX0 = stride - r_boxW - screenMarginX;

                    if (intersectY0 < intersectY1)
                    {
                        constexpr size_t kRScratchW = 120;
                        constexpr size_t kScratchH = 12;
                        if (static_cast<size_t>(r_boxW) > kRScratchW || static_cast<size_t>(boxH) > kScratchH)
                            return;

                        static uint16_t r_box[kRScratchW * kScratchH];
                        const size_t r_totalPixels = static_cast<size_t>(r_boxW) * boxH;
                        for (size_t i = 0; i < r_totalPixels; ++i)
                            r_box[i] = bg;

                        BitmapFont::drawString(r_box, r_boxW, boxH, 4, 2, r_line, kColorShadow, 0, false);
                        BitmapFont::drawString(r_box, r_boxW, boxH, 3, 1, r_line, kColorText, 0, false);

                        for (int16_t y = intersectY0; y < intersectY1; ++y)
                        {
                            const int16_t boxRow = y - hudY0;
                            const int16_t bandRow = y - bandY0;

                            uint16_t *dst = bandBuf + static_cast<size_t>(bandRow) * stride + r_boxX0;
                            const uint16_t *src = r_box + static_cast<size_t>(boxRow) * r_boxW;

                            std::memcpy(dst, src, static_cast<size_t>(r_boxW) * sizeof(uint16_t));
                        }
                    }
                }
            }
        }
    }

    void Hud::drawSelectorBar(uint16_t *dst, int16_t screenW, int16_t rows) noexcept
    {
        if (!dst || screenW <= 0 || rows <= 0)
            return;

        const size_t totalPixels = static_cast<size_t>(screenW) * rows;
        for (size_t i = 0; i < totalPixels; ++i)
            dst[i] = bgSwapped;

        const int16_t catY = 4;
        for (size_t c = 0; c < 4; ++c)
        {
            const int16_t cx = _catX[c];
            
            if (c == _selectedCategory)
            {
                char tabStr[32];
                std::snprintf(tabStr, sizeof(tabStr), "[%s]", kCategories[c].name);
                BitmapFont::drawString(dst, screenW, rows, cx, catY, tabStr, kCategories[c].color, 0, false);
            }
            else
            {
                BitmapFont::drawString(dst, screenW, rows, static_cast<int16_t>(cx + 6), catY, kCategories[c].name, kColorText, 0, false);
            }
        }

        const int16_t barH = 16;
        const int16_t ry = 15;
        const int16_t radius = 5;

        for (size_t i = 0; i < _activeItemCount; ++i)
        {
            const int16_t rx = _activeItemX[i] - _scrollX;
            const int16_t rw = _activeItemW[i];

            if (rx + rw < 0 || rx >= screenW)
                continue;

            const SelectorItem &item = kSelectorItems[_activeItemIndices[i]];
            const uint16_t itemColorSwapped = pipcore::Sprite::swap16(item.color);
            const uint16_t textColorSwapped = pipcore::Sprite::swap16(item.textColor);

            if (i == _selectedActiveIdx)
            {
                drawRoundedRectAA(dst, screenW, rows, rx, ry, rw, barH, radius, selectBorderSwapped, bgSwapped);
                int16_t innerRadius = std::max<int16_t>(1, radius - 2);
                drawRoundedRectAA(dst, screenW, rows, static_cast<int16_t>(rx + 2), static_cast<int16_t>(ry + 2), static_cast<int16_t>(rw - 4), static_cast<int16_t>(barH - 4), innerRadius, itemColorSwapped, selectBorderSwapped);
            }
            else
            {
                drawRoundedRectAA(dst, screenW, rows, rx, ry, rw, barH, radius, itemColorSwapped, bgSwapped);
            }

            const int16_t textW = BitmapFont::textWidth(item.name);
            const int16_t tx = rx + (rw - textW) / 2;
            const int16_t ty = ry + (barH - BitmapFont::GlyphH) / 2;

            BitmapFont::drawString(dst, screenW, rows, static_cast<int16_t>(tx + 1), static_cast<int16_t>(ty + 1), item.name, kColorShadow, 0, false);
            BitmapFont::drawString(dst, screenW, rows, tx, ty, item.name, textColorSwapped, 0, false);
        }

        for (int16_t y = 0; y < rows; ++y)
        {
            uint16_t *row = dst + static_cast<size_t>(y) * screenW;
            
            for (int16_t x = 0; x < 32; ++x)
            {
                uint8_t alpha = static_cast<uint8_t>((x * 255) / 32);
                row[x] = blendSwapped565(bgSwapped, row[x], alpha);
            }
            
            for (int16_t x = screenW - 32; x < screenW; ++x)
            {
                uint8_t alpha = static_cast<uint8_t>(((screenW - 1 - x) * 255) / 32);
                row[x] = blendSwapped565(bgSwapped, row[x], alpha);
            }
        }
    }
}