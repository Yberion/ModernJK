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

// engine.cpp -- the output stage as a renderer runs it, compiled by each renderer against its own headers
// (<rd-vanilla/tr_local.h> of its tree): the table of OpenGL's functions and the stage live from Output_Init to
// Output_Shutdown, in one object reached through one function (Running), the stage keeping a pointer to the table
// beside it. The settings are the renderer's own (r_ext_multisample, r_outputPrecision, r_outputDither, r_gamma),
// registered in its tree's R_Register.
#include "engine.h"

#include <rd-vanilla/tr_local.h>

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {

// What the stage needs from Output_Init to Output_Shutdown. The table is the stage's, which points to it: on the
// heap, so that it stays where it is when the object moves.
struct Output {
    std::unique_ptr<render_output::Gl> gl;
    render_output::Stage stage;
};

// The one output of the renderer, empty outside Output_Init and Output_Shutdown.
std::optional<Output>& Running()
{
    static std::optional<Output> output;
    return output;
}

// The scene's precision r_outputPrecision names: 1 for 16 bits a channel, anything else 8.
render_output::Precision Precision(const cvar_t& setting)
{
    return setting.integer == 1 ? render_output::Precision::Rgba16 : render_output::Precision::Rgba8;
}

// The stage's settings: the window's size, the renderer's settings.
render_output::Settings ReadSettings()
{
    return {
        .size = {.width = glConfig.vidWidth, .height = glConfig.vidHeight}, .samples = r_ext_multisample->integer, .precision = Precision(*r_outputPrecision)};
}

} // namespace

void Output_Init()
{
    std::expected<render_output::Gl, std::string> loaded = render_output::LoadGl(ri.GL_GetProcAddress);

    if (!loaded) {
        Q_Error(errorParm_t::ERR_FATAL, "{}", loaded.error());
    }

    auto gl = std::make_unique<render_output::Gl>(*loaded);
    // the most samples the driver allows, which the video menus' row offers up to
    render_output::GLint driverMaximum = 0;
    gl->GetIntegerv(render_output::kMaxSamples, &driverMaximum);
    ri.Cvar_SetValue("r_ext_multisample_avail", static_cast<float>(render_output::AllowedSamples(render_output::kSampleCounts.front(), driverMaximum)));
    std::expected<render_output::Stage, std::string> stage = render_output::Stage::Create(*gl, ReadSettings());

    if (!stage) {
        Q_Error(errorParm_t::ERR_FATAL, "{}", stage.error());
    }

    Output& output = Running().emplace(Output{.gl = std::move(gl), .stage = std::move(*stage)});
    output.stage.DrawScene();
    r_outputPrecision->modified = false;
    r_ext_multisample->modified = false;
    // what the renderer draws into: the scene's colour, depth and stencil
    glConfig.colorBits = 32;
    glConfig.depthBits = 24;
    glConfig.stencilBits = 8;
}

void Output_Shutdown()
{
    Running().reset();
}

void Output_DrawScene()
{
    std::optional<Output>& running = Running();

    if (!running) {
        return;
    }

    render_output::Stage& stage = running->stage;

    if (r_ext_multisample->modified || r_outputPrecision->modified) {
        r_ext_multisample->modified = false;
        r_outputPrecision->modified = false;

        if (!stage.Apply(ReadSettings())) {
            Q_PrintlnWarning("the scene's images could not be made for r_ext_multisample {} and r_outputPrecision {}", r_ext_multisample->integer,
                             r_outputPrecision->integer);
        }
    }

    stage.DrawScene();
}

void Output_MakeReadable(render_output::Buffers buffers, int x, int y, int width, int height)
{
    if (std::optional<Output>& running = Running()) {
        running->stage.MakeReadable(buffers, {.x = x, .y = y, .width = width, .height = height});
    }
}

void Output_Present()
{
    if (std::optional<Output>& running = Running()) {
        running->stage.Present(r_gamma->value, r_outputDither->integer != 0 ? render_output::Dither::Ordered : render_output::Dither::None);
    }
}

void Output_PrintInfo()
{
    const std::optional<Output>& running = Running();

    if (!running) {
        return;
    }

    const render_output::Settings& current = running->stage.Current();
    Q_Println("OUTPUT: {}, {} samples, dither {}", current.precision == render_output::Precision::Rgba16 ? "RGBA16" : "RGBA8", current.samples,
              r_outputDither->integer != 0 ? "ordered" : "none");
}
