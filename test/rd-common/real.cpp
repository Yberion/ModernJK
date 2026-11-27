/*
===========================================================================
Copyright (C) 2026, ModernJK contributors

This file is part of the ModernJK source code.

ModernJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// The stage on the system's own OpenGL driver, run by hand (modernjk_test_rd_common real): a window of the platform
// layer, the 256 levels of grey drawn into the scene one column each, then read back in the window after the pass:
// at r_gamma 1 each level as it was drawn, the dither included, with and without multisampling; at
// other values the curve's level, 255 * (level / 255)^(1 / gamma) rounded, within one level (the driver's pow);
// and a multisampled scene made readable gives its levels back as drawn.
#include <check.h>

#include <shared/platform/platform.h>
#include <shared/platform/window/window.h>
#include <shared/rd-common/output/gl.h>
#include <shared/rd-common/output/stage.h>

#include <SDL3/SDL.h>

#include <fmt/base.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace {

using namespace render_output;

constexpr int kLevels = 256;
constexpr GLenum kRgb = 0x1907;

// the functions the test draws and reads with, beside the stage's
struct Extra {
    void (*ClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) = nullptr;
    void (*Clear)(GLbitfield mask) = nullptr;
    void (*Scissor)(GLint x, GLint y, GLsizei width, GLsizei height) = nullptr;
    void (*ReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) = nullptr;
};

GlFunction Loader(const char* name)
{
    return SDL_GL_GetProcAddress(name);
}

template <typename Function> void LoadExtra(const char* name, Function& function)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): a function of the library comes untyped
    function = reinterpret_cast<Function>(Loader(name));
}

// the 256 levels of grey into the scene, one column each
void DrawLevels(const Gl& gl, const Extra& extra, Size size)
{
    gl.Enable(kScissorTest);

    for (int level = 0; level < kLevels; ++level) {
        const float value = static_cast<float>(level) / 255.0F;
        extra.Scissor(level, 0, 1, size.height);
        extra.ClearColor(value, value, value, 1.0F);
        extra.Clear(kColourBufferBit);
    }

    gl.Disable(kScissorTest);
}

// the red of the first row's 256 pixels of the framebuffer bound for reading
std::array<int, kLevels> ReadLevels(const Extra& extra)
{
    std::array<std::uint8_t, kLevels * 3> pixels{};
    extra.ReadPixels(0, 0, kLevels, 1, kRgb, kUnsignedByte, pixels.data());
    std::array<int, kLevels> levels{};

    for (int level = 0; level < kLevels; ++level) {
        levels[static_cast<std::size_t>(level)] = pixels[static_cast<std::size_t>(level) * 3];
    }

    return levels;
}

// the level the curve gives, as the screen's ramp computed it in 8 bits
int CurveLevel(int level, float gamma)
{
    return static_cast<int>(std::lround(255.0 * std::pow(level / 255.0, 1.0 / gamma)));
}

// one case of the pass: the levels read in the window against the curve's, the largest difference allowed
void CheckPass(const Gl& gl, const Extra& extra, const Settings& settings, float gamma, int allowed)
{
    auto stage = Stage::Create(gl, settings);
    CHECK(stage.has_value());

    if (!stage) {
        fmt::println("{}", stage.error());
        return;
    }

    stage->DrawScene();
    DrawLevels(gl, extra, settings.size);
    stage->Present(gamma);
    const std::array<int, kLevels> levels = ReadLevels(extra);
    int worst = 0;

    for (int level = 0; level < kLevels; ++level) {
        worst = std::max(worst, std::abs(levels[static_cast<std::size_t>(level)] - CurveLevel(level, gamma)));
    }

    fmt::println("{} samples, gamma {}: largest difference {} level(s)", stage->Current().samples, gamma, worst);
    CHECK(worst <= allowed);
}

// a multisampled scene made readable gives back the levels drawn
void CheckReadable(const Gl& gl, const Extra& extra, Size size)
{
    auto stage = Stage::Create(gl, {.size = size, .samples = 4});
    CHECK(stage.has_value());

    if (!stage) {
        return;
    }

    stage->DrawScene();
    DrawLevels(gl, extra, size);
    stage->MakeReadable(Buffers::Colour, {.x = 0, .y = 0, .width = size.width, .height = size.height});
    const std::array<int, kLevels> levels = ReadLevels(extra);
    stage->DrawScene();
    int wrong = 0;

    for (int level = 0; level < kLevels; ++level) {
        wrong += levels[static_cast<std::size_t>(level)] != level ? 1 : 0;
    }

    fmt::println("readable, {} samples: {} level(s) wrong", stage->Current().samples, wrong);
    CHECK_EQUAL(wrong, 0);
}

} // namespace

// runs the stage's checks in a window on the system's own driver
int RunRealTests()
{
    if (const auto started = platform::Start("modernjk_test_rd_common", "test"); !started) {
        fmt::println("{}", started.error());
        return 1;
    }

    {
        platform::WindowSettings settings{.state = platform::DisplayState::Window, .width = kLevels, .height = 64};
        auto window = platform::Window::Open("modernjk_test_rd_common", settings, platform::Context::OpenGl);
        CHECK(window.has_value());

        if (!window) {
            fmt::println("{}", window.error());
        }
        else if (auto gl = LoadGl(&Loader); !gl) {
            fmt::println("{}", gl.error());
            CHECK(gl.has_value());
        }
        else {
            Extra extra;
            LoadExtra("glClearColor", extra.ClearColor);
            LoadExtra("glClear", extra.Clear);
            LoadExtra("glScissor", extra.Scissor);
            LoadExtra("glReadPixels", extra.ReadPixels);
            const platform::PixelSize pixels = window->Size();
            const Size size{.width = pixels.width, .height = pixels.height};
            CHECK(size.width >= kLevels);
            fmt::println("{}", platform::Description());

            for (const int samples : {0, 4}) {
                const Settings stageSettings{.size = size, .samples = samples};
                CheckPass(*gl, extra, stageSettings, 1.0F, 0);
                CheckPass(*gl, extra, stageSettings, 1.5F, 1);
                CheckPass(*gl, extra, stageSettings, 2.5F, 1);
            }

            CheckReadable(*gl, extra, size);
            CHECK(window->Present());
        }
    }

    platform::Stop();

    if (test::failures != 0) {
        fmt::println("{} check(s) failed", test::failures);
    }

    return test::failures;
}
