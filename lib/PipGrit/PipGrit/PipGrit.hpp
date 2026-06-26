#pragma once

#include <PipGrit/Core/Config/Features.hpp>
#include <PipGrit/Sim/Material.hpp>
#include <PipGrit/Sim/Grid.hpp>
#include <PipGrit/Sim/SandSim.hpp>
#include <PipGrit/Render/Renderer.hpp>
#include <PipGrit/Hud/BitmapFont.hpp>
#include <PipGrit/Hud/Hud.hpp>
#include <PipGrit/App/Engine.hpp>

namespace pipgrit
{
    inline constexpr uint8_t kVersionMajor = PIPGRIT_VERSION_MAJOR;
    inline constexpr uint8_t kVersionMinor = PIPGRIT_VERSION_MINOR;
    inline constexpr uint8_t kVersionPatch = PIPGRIT_VERSION_PATCH;
    inline constexpr const char *kVersion = PIPGRIT_VERSION;
}
