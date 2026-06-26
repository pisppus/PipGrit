#include <PipGrit/Sim/SandSim.hpp>
#include <PipGrit/Sim/Material.hpp>

#include <cstdint>
#include <algorithm>

namespace pipgrit
{
    namespace
    {
        inline uint32_t nextRand(uint32_t &s) noexcept
        {
            uint32_t x = s;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            s = x;
            return x;
        }

        [[nodiscard]] inline bool isEmptyCell(const uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y, MapMode mode) noexcept
        {
            if (static_cast<uint16_t>(x) < static_cast<uint16_t>(w) && static_cast<uint16_t>(y) < static_cast<uint16_t>(h))
            {
                return (cells[static_cast<size_t>(y) * w + x] & 0x7F) == Empty;
            }

            int16_t tx = x;
            int16_t ty = y;
            if (ty >= h)
            {
                if (mode == MapMode::Void) return true;
                if (mode == MapMode::Wrap) ty = 0;
                else return false;
            }
            if (tx < 0 || tx >= w)
            {
                if (mode == MapMode::Void) return true;
                if (mode == MapMode::Wrap) tx = (tx + w) % w;
                else return false;
            }
            return (cells[static_cast<size_t>(ty) * w + tx] & 0x7F) == Empty;
        }

        [[nodiscard]] inline bool isDisplaceable(const uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y, Cell self, MapMode mode) noexcept
        {
            if (static_cast<uint16_t>(x) < static_cast<uint16_t>(w) && static_cast<uint16_t>(y) < static_cast<uint16_t>(h))
            {
                const uint8_t c = cells[static_cast<size_t>(y) * w + x] & 0x7F;
                if (c == Empty)
                    return true;
                if (self == Sand && (c == Water || c == Acid || c == Oil))
                    return true;
                return false;
            }

            int16_t tx = x;
            int16_t ty = y;
            if (ty >= h)
            {
                if (mode == MapMode::Void) return true;
                if (mode == MapMode::Wrap) ty = 0;
                else return false;
            }
            if (tx < 0 || tx >= w)
            {
                if (mode == MapMode::Void) return true;
                if (mode == MapMode::Wrap) tx = (tx + w) % w;
                else return false;
            }

            const uint8_t c = cells[static_cast<size_t>(ty) * w + tx] & 0x7F;
            if (c == Empty)
                return true;
            if (self == Sand && (c == Water || c == Acid || c == Oil))
                return true;
            return false;
        }

        inline void swapOrClearCells(uint8_t *cells, int16_t w, int16_t h, int16_t ax, int16_t ay, int16_t bx, int16_t by, MapMode mode) noexcept
        {
            if (by >= h)
            {
                if (mode == MapMode::Void)
                {
                    cells[static_cast<size_t>(ay) * w + ax] = Empty;
                    return;
                }
                else if (mode == MapMode::Wrap)
                {
                    by = 0;
                }
                else return;
            }
            if (bx < 0 || bx >= w)
            {
                if (mode == MapMode::Void)
                {
                    cells[static_cast<size_t>(ay) * w + ax] = Empty;
                    return;
                }
                else if (mode == MapMode::Wrap)
                {
                    bx = (bx + w) % w;
                }
                else return;
            }

            const size_t idxA = static_cast<size_t>(ay) * w + ax;
            const size_t idxB = static_cast<size_t>(by) * w + bx;

            const uint8_t valA = cells[idxA];
            const uint8_t valB = cells[idxB];

            cells[idxA] = (valB == Empty) ? Empty : (valB | 0x80);
            cells[idxB] = (valA == Empty) ? Empty : (valA | 0x80);
        }
    }

    uint32_t SandSim::stepPowder(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept
    {
        const Cell self = static_cast<Cell>(cells[static_cast<size_t>(y) * w + x] & 0x7F);

        int16_t fall_dist = 0;
        const int16_t max_fall = static_cast<int16_t>((nextRand(_rng) % 3) + 2);
        while (fall_dist < max_fall)
        {
            int16_t next_y = y + fall_dist + 1;
            if (next_y >= h)
            {
                if (_mapMode == MapMode::Void)
                {
                    fall_dist++;
                    break;
                }
                else if (_mapMode == MapMode::Wrap)
                {
                    int16_t wrapped_y = next_y % h;
                    if (isDisplaceable(cells, w, h, x, wrapped_y, self, _mapMode))
                        fall_dist++;
                    else
                        break;
                }
                else
                {
                    break;
                }
            }
            else
            {
                if (isDisplaceable(cells, w, h, x, next_y, self, _mapMode))
                    fall_dist++;
                else
                    break;
            }
        }

        if (fall_dist > 0)
        {
            swapOrClearCells(cells, w, h, x, y, x, static_cast<int16_t>(y + fall_dist), _mapMode);
            return 1;
        }

        const bool leftFirst = (nextRand(_rng) & 1u) != 0u;
        const int16_t d1 = leftFirst ? -1 : 1;
        const int16_t d2 = -d1;

        if (isDisplaceable(cells, w, h, static_cast<int16_t>(x + d1), static_cast<int16_t>(y + 1), self, _mapMode))
        {
            swapOrClearCells(cells, w, h, x, y, static_cast<int16_t>(x + d1), static_cast<int16_t>(y + 1), _mapMode);
            return 1;
        }
        if (isDisplaceable(cells, w, h, static_cast<int16_t>(x + d2), static_cast<int16_t>(y + 1), self, _mapMode))
        {
            swapOrClearCells(cells, w, h, x, y, static_cast<int16_t>(x + d2), static_cast<int16_t>(y + 1), _mapMode);
            return 1;
        }
        return 0;
    }

    uint32_t SandSim::stepLiquid(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept
    {
        const Cell self = static_cast<Cell>(cells[static_cast<size_t>(y) * w + x] & 0x7F);

        if (self == Acid)
        {
            const int16_t dx[] = {0, 0, -1, 1};
            const int16_t dy[] = {-1, 1, 0, 0};
            for (int i = 0; i < 4; ++i)
            {
                const int16_t nx = x + dx[i];
                const int16_t ny = y + dy[i];
                if (nx >= 0 && nx < w && ny >= 0 && ny < h)
                {
                    const size_t idx = static_cast<size_t>(ny) * w + nx;
                    const uint8_t neighbor = cells[idx] & 0x7F;
                    if (neighbor != Empty && neighbor != Acid && neighbor != Wall)
                    {
                        if ((nextRand(_rng) % 100) < 18)
                        {
                            cells[idx] = Empty;
                            cells[static_cast<size_t>(y) * w + x] = Empty;
                            return 1;
                        }
                    }
                }
            }
        }

        if (self == Water)
        {
            uint8_t t = _grid->temp(x, y);
            if (t >= 100)
            {
                cells[static_cast<size_t>(y) * w + x] = Steam | 0x80;
                _grid->setTemp(x, y, t - 20);
                return 1;
            }
            else if (t <= 5)
            {
                cells[static_cast<size_t>(y) * w + x] = Ice | 0x80;
                return 1;
            }
        }

        int16_t fall_dist = 0;
        const int16_t max_fall = static_cast<int16_t>((nextRand(_rng) % 3) + 2);
        while (fall_dist < max_fall)
        {
            int16_t next_y = y + fall_dist + 1;
            if (next_y >= h)
            {
                if (_mapMode == MapMode::Void)
                {
                    fall_dist++;
                    break;
                }
                else if (_mapMode == MapMode::Wrap)
                {
                    int16_t wrapped_y = next_y % h;
                    if (isEmptyCell(cells, w, h, x, wrapped_y, _mapMode))
                        fall_dist++;
                    else
                        break;
                }
                else
                {
                    break;
                }
            }
            else
            {
                if (isEmptyCell(cells, w, h, x, next_y, _mapMode))
                    fall_dist++;
                else
                    break;
            }
        }

        if (fall_dist > 0)
        {
            swapOrClearCells(cells, w, h, x, y, x, static_cast<int16_t>(y + fall_dist), _mapMode);
            return 1;
        }

        const bool leftFirst = (nextRand(_rng) & 1u) != 0u;
        const int16_t d1 = leftFirst ? -1 : 1;
        const int16_t d2 = -d1;

        if (isEmptyCell(cells, w, h, static_cast<int16_t>(x + d1), static_cast<int16_t>(y + 1), _mapMode))
        {
            swapOrClearCells(cells, w, h, x, y, static_cast<int16_t>(x + d1), static_cast<int16_t>(y + 1), _mapMode);
            return 1;
        }
        if (isEmptyCell(cells, w, h, static_cast<int16_t>(x + d2), static_cast<int16_t>(y + 1), _mapMode))
        {
            swapOrClearCells(cells, w, h, x, y, static_cast<int16_t>(x + d2), static_cast<int16_t>(y + 1), _mapMode);
            return 1;
        }

        int16_t flow_dist1 = 0;
        const int16_t max_flow = static_cast<int16_t>((nextRand(_rng) % 3) + 2);
        while (flow_dist1 < max_flow)
        {
            int16_t next_x = x + d1 * (flow_dist1 + 1);
            if (isEmptyCell(cells, w, h, next_x, y, _mapMode))
                flow_dist1++;
            else
                break;
        }
        if (flow_dist1 > 0)
        {
            swapOrClearCells(cells, w, h, x, y, static_cast<int16_t>(x + d1 * flow_dist1), y, _mapMode);
            return 1;
        }

        int16_t flow_dist2 = 0;
        while (flow_dist2 < max_flow)
        {
            int16_t next_x = x + d2 * (flow_dist2 + 1);
            if (isEmptyCell(cells, w, h, next_x, y, _mapMode))
                flow_dist2++;
            else
                break;
        }
        if (flow_dist2 > 0)
        {
            swapOrClearCells(cells, w, h, x, y, static_cast<int16_t>(x + d2 * flow_dist2), y, _mapMode);
            return 1;
        }

        return 0;
    }

    uint32_t SandSim::stepFire(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept
    {
        _grid->setTemp(x, y, 255);

        if ((nextRand(_rng) % 100) < 15)
        {
            cells[static_cast<size_t>(y) * w + x] = ((nextRand(_rng) % 100) < 10) ? (Ash | 0x80) : Empty;
            return 1;
        }

        const int16_t dx[] = {0, 0, -1, 1};
        const int16_t dy[] = {-1, 1, 0, 0};
        for (int i = 0; i < 4; ++i)
        {
            const int16_t nx = x + dx[i];
            const int16_t ny = y + dy[i];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h)
            {
                const size_t idx = static_cast<size_t>(ny) * w + nx;
                const uint8_t neighbor = cells[idx] & 0x7F;

                uint16_t nt = _grid->temp(nx, ny) + 45;
                if (nt > 255) nt = 255;
                _grid->setTemp(nx, ny, static_cast<uint8_t>(nt));

                if (neighbor == Water)
                {
                    cells[idx] = Steam | 0x80;
                    cells[static_cast<size_t>(y) * w + x] = Steam | 0x80;
                    _grid->setTemp(nx, ny, 100);
                    _grid->setTemp(x, y, 100);
                    return 1;
                }
                else if (neighbor == Wood)
                {
                    if (_grid->temp(nx, ny) >= 120)
                        cells[idx] = Fire | 0x80;
                }
                else if (neighbor == Oil)
                {
                    bool exposedToAir = false;
                    const int16_t ox[] = {0, 0, -1, 1};
                    const int16_t oy[] = {-1, 1, 0, 0};
                    for (int j = 0; j < 4; ++j)
                    {
                        int16_t sx = nx + ox[j];
                        int16_t sy = ny + oy[j];
                        if (sx >= 0 && sx < w && sy >= 0 && sy < h)
                        {
                            uint8_t n_neigh = cells[static_cast<size_t>(sy) * w + sx] & 0x7F;
                            if (n_neigh == Empty || n_neigh == Steam)
                            {
                                exposedToAir = true;
                                break;
                            }
                        }
                    }

                    if (exposedToAir)
                    {
                        if ((nextRand(_rng) % 100) < 20)
                        {
                            cells[idx] = Fire | 0x80;
                        }
                    }
                }
                else if (neighbor == Ice)
                {
                    cells[idx] = Water | 0x80;
                    _grid->setTemp(nx, ny, 40);
                }
            }
        }

        const int16_t float_dir_y = -1;
        const int16_t float_dir_x = static_cast<int16_t>((nextRand(_rng) % 3) - 1);

        const int16_t target_y = y + float_dir_y;
        const int16_t target_x = x + float_dir_x;

        if (target_y >= 0 && target_x >= 0 && target_x < w)
        {
            const size_t target_idx = static_cast<size_t>(target_y) * w + target_x;
            if ((cells[target_idx] & 0x7F) == Empty)
            {
                cells[static_cast<size_t>(y) * w + x] = Empty;
                cells[target_idx] = Fire | 0x80;
                _grid->setTemp(target_x, target_y, 255);
                _grid->setTemp(x, y, 100);
                return 1;
            }
        }
        return 0;
    }

    uint32_t SandSim::stepSteam(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept
    {
        uint8_t t = _grid->temp(x, y);

        if (t < 85)
        {
            if ((nextRand(_rng) % 100) < 4)
            {
                cells[static_cast<size_t>(y) * w + x] = Water | 0x80;
                return 1;
            }
        }

        const int16_t float_dir_y = -1;
        const int16_t float_dir_x = static_cast<int16_t>((nextRand(_rng) % 3) - 1);

        const int16_t target_y = y + float_dir_y;
        const int16_t target_x = x + float_dir_x;

        if (target_y >= 0 && target_x >= 0 && target_x < w)
        {
            const size_t target_idx = static_cast<size_t>(target_y) * w + target_x;
            if ((cells[target_idx] & 0x7F) == Empty)
            {
                cells[static_cast<size_t>(y) * w + x] = Empty;
                cells[target_idx] = Steam | 0x80;
                _grid->setTemp(target_x, target_y, t);
                _grid->setTemp(x, y, 20);
                return 1;
            }
        }
        return 0;
    }

    uint32_t SandSim::stepAsh(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept
    {
        const uint8_t t = _grid->temp(x, y);

        if (t > 80)
        {
            if ((nextRand(_rng) % 100) < 5)
            {
                cells[static_cast<size_t>(y) * w + x] = Empty;
                return 1;
            }

            const int16_t float_dir_y = -1;
            const int16_t float_dir_x = static_cast<int16_t>((nextRand(_rng) % 3) - 1);

            const int16_t target_y = y + float_dir_y;
            const int16_t target_x = x + float_dir_x;

            if (target_y >= 0 && target_x >= 0 && target_x < w)
            {
                const size_t target_idx = static_cast<size_t>(target_y) * w + target_x;
                if ((cells[target_idx] & 0x7F) == Empty)
                {
                    cells[static_cast<size_t>(y) * w + x] = Empty;
                    cells[target_idx] = Ash | 0x80;
                    _grid->setTemp(target_x, target_y, t);
                    _grid->setTemp(x, y, 20);
                    return 1;
                }
            }
            return 0;
        }
        else
        {
            return stepPowder(cells, w, h, x, y);
        }
    }

    uint32_t SandSim::stepLava(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept
    {
        _grid->setTemp(x, y, 255);

        const int16_t dx[] = {0, 0, -1, 1};
        const int16_t dy[] = {-1, 1, 0, 0};
        for (int i = 0; i < 4; ++i)
        {
            const int16_t nx = x + dx[i];
            const int16_t ny = y + dy[i];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h)
            {
                const size_t idx = static_cast<size_t>(ny) * w + nx;
                const uint8_t neighbor = cells[idx] & 0x7F;

                _grid->setTemp(nx, ny, 255);

                if (neighbor == Water)
                {
                    cells[idx] = Steam | 0x80;
                    cells[static_cast<size_t>(y) * w + x] = Stone | 0x80;
                    _grid->setTemp(x, y, 150);
                    return 1;
                }
                else if (neighbor == Wood || neighbor == Oil || neighbor == Gunpowder)
                {
                    cells[idx] = Fire | 0x80;
                }
                else if (neighbor == Ice)
                {
                    cells[idx] = Water | 0x80;
                }
                else if (neighbor == Acid)
                {
                    cells[idx] = Steam | 0x80;
                }
            }
        }

        if ((nextRand(_rng) % 100) < 40)
        {
            return stepLiquid(cells, w, h, x, y);
        }
        return 0;
    }

    uint32_t SandSim::stepGunpowder(uint8_t *cells, int16_t w, int16_t h, int16_t x, int16_t y) noexcept
    {
        const uint8_t t = _grid->temp(x, y);
        bool explode = (t >= 110);

        if (!explode)
        {
            const int16_t dx[] = {0, 0, -1, 1};
            const int16_t dy[] = {-1, 1, 0, 0};
            for (int i = 0; i < 4; ++i)
            {
                const int16_t nx = x + dx[i];
                const int16_t ny = y + dy[i];
                if (nx >= 0 && nx < w && ny >= 0 && ny < h)
                {
                    const uint8_t neighbor = cells[static_cast<size_t>(ny) * w + nx] & 0x7F;
                    if (neighbor == Fire || neighbor == Lava)
                    {
                        explode = true;
                        break;
                    }
                }
            }
        }

        if (explode)
        {
            cells[static_cast<size_t>(y) * w + x] = Fire | 0x80;
            _grid->setTemp(x, y, 255);

            const int16_t ex[] = {0, 0, -1, 1, -1, 1, -1, 1};
            const int16_t ey[] = {-1, 1, 0, 0, -1, -1, 1, 1};
            for (int i = 0; i < 8; ++i)
            {
                const int16_t nx = x + ex[i];
                const int16_t ny = y + ey[i];
                if (nx >= 0 && nx < w && ny >= 0 && ny < h)
                {
                    const size_t idx = static_cast<size_t>(ny) * w + nx;
                    const uint8_t neighbor = cells[idx] & 0x7F;
                    if (neighbor == Gunpowder || neighbor == Wood || neighbor == Oil)
                    {
                        cells[idx] = Fire | 0x80;
                        _grid->setTemp(nx, ny, 255);
                    }
                    else if (neighbor == Empty)
                    {
                        if ((nextRand(_rng) % 100) < 50)
                        {
                            cells[idx] = Fire | 0x80;
                            _grid->setTemp(nx, ny, 255);
                        }
                    }
                }
            }
            return 1;
        }

        return stepPowder(cells, w, h, x, y);
    }

    uint32_t SandSim::step() noexcept
    {
        if (!_grid)
            return 0;

        _grid->diffuseHeat();

        const int16_t w = _grid->width();
        const int16_t h = _grid->height();
        uint8_t *const cells = _grid->cells();

        const bool ltr = (nextRand(_rng) & 1u) == 0u;

        uint32_t moved = 0;
        for (int16_t y = h - 1; y >= 0; --y)
        {
            const size_t yOff = static_cast<size_t>(y) * w;
            const int16_t xStart = ltr ? 0 : static_cast<int16_t>(w - 1);
            const int16_t xEnd = ltr ? static_cast<int16_t>(w) : -1;
            const int16_t xStep = ltr ? 1 : -1;

            const uint8_t *temp_row = _grid->temps() + (static_cast<size_t>(y >> 2) * (w >> 2));

            for (int16_t x = xStart; x != xEnd; x += xStep)
            {
                const uint8_t rawCell = cells[yOff + x];
                if (rawCell == Empty || (rawCell & 0x80))
                    continue;

                const Cell c = static_cast<Cell>(rawCell);
                const uint8_t t = temp_row[x >> 2];

                if (c == Ice)
                {
                    if (t > 25)
                    {
                        cells[yOff + x] = Water | 0x80;
                        _grid->setTemp(x, y, t > 5 ? t - 5 : 0);
                        moved++;
                        continue;
                    }
                }
                else if (c == Wood)
                {
                    if (t >= 120)
                    {
                        cells[yOff + x] = Fire | 0x80;
                        moved++;
                        continue;
                    }
                }

                if (c == Fire)
                {
                    moved += stepFire(cells, w, h, x, y);
                }
                else if (c == Steam)
                {
                    moved += stepSteam(cells, w, h, x, y);
                }
                else if (c == Ash)
                {
                    moved += stepAsh(cells, w, h, x, y);
                }
                else if (c == Lava)
                {
                    moved += stepLava(cells, w, h, x, y);
                }
                else if (c == Gunpowder)
                {
                    moved += stepGunpowder(cells, w, h, x, y);
                }
                else
                {
                    switch (matterOf(c))
                    {
                    case Matter::Powder:
                        moved += stepPowder(cells, w, h, x, y);
                        break;
                    case Matter::Liquid:
                        moved += stepLiquid(cells, w, h, x, y);
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        const size_t totalCells = static_cast<size_t>(w) * h;
        const size_t words = totalCells >> 2;
        auto *cells32 = reinterpret_cast<uint32_t *>(cells);
        for (size_t i = 0; i < words; ++i)
        {
            cells32[i] &= 0x7F7F7F7FU;
        }

        ++_stepCount;
        return moved;
    }
}