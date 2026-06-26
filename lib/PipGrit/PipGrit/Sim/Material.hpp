#pragma once

#include <PipGrit/Core/Config/Features.hpp>
#include <PipCore/Graphics/Sprite.hpp>
#include <cstdint>

namespace pipgrit
{
    enum Cell : uint8_t
    {
        Empty = 0,
        Sand = 1,
        Water = 2,
        Stone = 3,
        Wood = 4,
        Wall = 5,
        Fire = 6,
        Ash = 7,
        Acid = 8,
        Oil = 9,
        Ice = 10,
        Steam = 11,
        Lava = 12,
        Gunpowder = 13,
        CellCount
    };

    enum class Matter : uint8_t
    {
        Empty,
        Powder,
        Liquid,
        Solid,
    };

    inline constexpr Matter kMatterTable[CellCount] = {
        Matter::Empty,  // Empty
        Matter::Powder, // Sand
        Matter::Liquid, // Water
        Matter::Solid,  // Stone
        Matter::Solid,  // Wood
        Matter::Solid,  // Wall
        Matter::Empty,  // Fire
        Matter::Powder, // Ash
        Matter::Liquid, // Acid
        Matter::Liquid, // Oil
        Matter::Solid,  // Ice
        Matter::Empty,  // Steam
        Matter::Liquid, // Lava
        Matter::Powder, // Gunpowder
    };

    [[nodiscard]] inline constexpr Matter matterOf(Cell c) noexcept
    {
        return (c < CellCount) ? kMatterTable[c] : Matter::Empty;
    }

    [[nodiscard]] inline constexpr bool isEmpty(Cell c) noexcept { return c == Empty; }
    [[nodiscard]] inline constexpr bool isFluid(Cell c) noexcept { return c == Water || c == Acid || c == Oil || c == Lava; }

    namespace palette
    {
        constexpr uint16_t kEmpty = pipcore::Sprite::color565(0x08, 0x08, 0x0E);
        constexpr uint16_t kSand = pipcore::Sprite::color565(0xD6, 0xB8, 0x4E);
        constexpr uint16_t kWater = pipcore::Sprite::color565(0x32, 0x6A, 0xC8);
        constexpr uint16_t kStone = pipcore::Sprite::color565(0x6E, 0x6E, 0x72);
        constexpr uint16_t kWood = pipcore::Sprite::color565(0x86, 0x55, 0x2E);
        constexpr uint16_t kWall = pipcore::Sprite::color565(0x32, 0x32, 0x38);
        constexpr uint16_t kFire = pipcore::Sprite::color565(0xE6, 0x4A, 0x19);
        constexpr uint16_t kAsh = pipcore::Sprite::color565(0x60, 0x60, 0x65);
        constexpr uint16_t kAcid = pipcore::Sprite::color565(0x39, 0xFF, 0x14);
        constexpr uint16_t kOil = pipcore::Sprite::color565(0x7D, 0x50, 0x47);
        constexpr uint16_t kIce = pipcore::Sprite::color565(0x90, 0xC0, 0xFF);
        constexpr uint16_t kSteam = pipcore::Sprite::color565(0xE0, 0xE0, 0xE5);
        constexpr uint16_t kLava = pipcore::Sprite::color565(0xFF, 0x55, 0x00);
        constexpr uint16_t kGunpowder = pipcore::Sprite::color565(0x55, 0x55, 0x5B);
    }

    constexpr uint16_t makeLutColor(size_t i) noexcept {
        if (i == Sand) return pipcore::Sprite::swap16(palette::kSand);
        if (i == Water) return pipcore::Sprite::swap16(palette::kWater);
        if (i == Stone) return pipcore::Sprite::swap16(palette::kStone);
        if (i == Wood) return pipcore::Sprite::swap16(palette::kWood);
        if (i == Wall) return pipcore::Sprite::swap16(palette::kWall);
        if (i == Fire) return pipcore::Sprite::swap16(palette::kFire);
        if (i == Ash) return pipcore::Sprite::swap16(palette::kAsh);
        if (i == Acid) return pipcore::Sprite::swap16(palette::kAcid);
        if (i == Oil) return pipcore::Sprite::swap16(palette::kOil);
        if (i == Ice) return pipcore::Sprite::swap16(palette::kIce);
        if (i == Steam) return pipcore::Sprite::swap16(palette::kSteam);
        if (i == Lava) return pipcore::Sprite::swap16(palette::kLava);
        if (i == Gunpowder) return pipcore::Sprite::swap16(palette::kGunpowder);
        return pipcore::Sprite::swap16(palette::kEmpty);
    }

    template <size_t N>
    struct ColorLut {
        uint16_t data[N];
        constexpr ColorLut() : data{} {
            for (size_t i = 0; i < N; ++i) {
                data[i] = makeLutColor(i);
            }
        }
    };

    inline constexpr auto kColorLutWrapper = ColorLut<128>();
    inline constexpr const uint16_t *kColorLut = kColorLutWrapper.data;

    [[nodiscard]] inline constexpr uint16_t colorOf(Cell c) noexcept
    {
        return (c < 128) ? kColorLut[c] : kColorLut[Empty];
    }

    [[nodiscard]] inline constexpr const char *cellName(Cell c) noexcept
    {
        switch (c)
        {
        case Empty: return "Empty";
        case Sand: return "Sand";
        case Water: return "Water";
        case Stone: return "Stone";
        case Wood: return "Wood";
        case Wall: return "Wall";
        case Fire: return "Fire";
        case Ash: return "Ash";
        case Acid: return "Acid";
        case Oil: return "Oil";
        case Ice: return "Ice";
        case Steam: return "Steam";
        case Lava: return "Lava";
        case Gunpowder: return "Gunpowder";
        default: return "?";
        }
    }
}