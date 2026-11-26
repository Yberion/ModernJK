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

// stage.h -- the renderers' output stage. The renderer draws its whole picture (the world, the effects, the HUD, the
// menus) into an image of its own, the scene, with its depth, its stencil and its samples; at the end of the image one
// pass draws the scene into the window and applies the player's brightness there, r_gamma's curve computed in floating
// point, the same at every display state and with or without the focus, since the screen's own ramp is never touched.
// What the renderer reads back of its picture (a copy into a texture, a pixel of depth, the stencil) is made readable
// first: a multisampled image is resolved into a single-sampled one, which OpenGL asks before any read.
// Example: a renderer makes the stage once its context is current, draws each image into it, then presents it:
//     auto stage = render_output::Stage::Create(gl, {.size = {.width = 1920, .height = 1080}, .samples = 4});
//     stage->DrawScene();   // the world, the HUD, the menus
//     stage->MakeReadable(render_output::Buffers::Colour, {.x = 0, .y = 0, .width = 1920, .height = 1080});
//     // ... glCopyTexSubImage2D from the scene ...
//     stage->DrawScene();
//     stage->Present(1.2F, render_output::Dither::None);   // then the window's swap
#pragma once

#include "gl.h"
#include "samples.h"

#include <expected>
#include <string>

namespace render_output {

// The precision of the scene's colour; both clamp the blends to [0, 1], as a window's colour does.
enum class Precision {
    Rgba8,  // 8 bits a channel, as the window's
    Rgba16, // 16 bits a channel, unsigned normalized
};

// What the pass adds to the colour before the window's 8 bits a channel round it.
enum class Dither {
    None,    // nothing: the colour rounded to the nearest of the window's levels
    Ordered, // a fixed pattern of 4 by 4 pixels, within 0.375 of a level: an exact level stays as it is
};

// A size in pixels.
struct Size {
    int width = 0;
    int height = 0;

    [[nodiscard]] bool operator==(const Size&) const = default;
};

// A rectangle of the picture in pixels, from its bottom left corner, as OpenGL counts.
struct Region {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// What the stage's images are made of.
struct Settings {
    // the window's size, which the scene has
    Size size;
    // the samples a pixel asked; 0 or 1 for none, else 2, 4 or 8 at most what the driver allows (AllowedSamples)
    int samples = 0;
    Precision precision = Precision::Rgba8;

    [[nodiscard]] bool operator==(const Settings&) const = default;
};

// What a read of the picture needs.
enum class Buffers {
    Colour,       // a copy into a texture, a capture
    DepthStencil, // a pixel of depth (the flares), the stencil (the overdraw's count)
};

// r_gamma's range, which the pass keeps the gamma in. The pass's curve is value^(1 / gamma) for each channel in
// [0, 1], the formula the screen's ramp had, computed in floating point where the ramp's table held 8 bits.
constexpr float kMinimumGamma = 0.5F;
constexpr float kMaximumGamma = 3.0F;

class Stage {
public:
    // Makes the pass and the images for the settings, with the context current; the error's text when the driver
    // refuses one. gl must outlive the stage.
    [[nodiscard]] static std::expected<Stage, std::string> Create(const Gl& gl, const Settings& settings);

    // Makes the images again for other settings, the pass kept. False when the driver refuses them: the stage keeps
    // its images as they were.
    [[nodiscard]] bool Apply(const Settings& settings);

    // The settings in effect, the samples as the driver allows them.
    [[nodiscard]] const Settings& Current() const noexcept;

    // Binds the scene for drawing and for reading: what the renderer draws goes there.
    void DrawScene();

    // Makes a region of the picture drawn so far readable by glReadPixels and glCopyTexSubImage2D: resolved into the
    // single-sampled image, which is bound for reading, when the scene is multisampled; the scene stays bound for
    // drawing. DrawScene binds the scene for reading again.
    void MakeReadable(Buffers buffers, Region region);

    // Draws the scene into the window's back buffer through the pass, with r_gamma's curve and the dither, and
    // leaves the window's framebuffer bound, for the swap and for a capture to read. The state of OpenGL the
    // renderer keeps is as it was.
    void Present(float gamma, Dither dither);

    Stage(const Stage&) = delete;
    Stage& operator=(const Stage&) = delete;
    Stage(Stage&& other) noexcept;
    Stage& operator=(Stage&& other) noexcept;
    ~Stage();

private:
    // the images, each 0 when it does not exist
    struct Images {
        GLuint sceneFramebuffer = 0;
        // the scene's colour: a texture without multisampling, a renderbuffer with
        GLuint sceneTexture = 0;
        GLuint sceneColour = 0;
        GLuint sceneDepth = 0;
        // the single-sampled copy of a multisampled scene
        GLuint resolveFramebuffer = 0;
        GLuint resolveTexture = 0;
        GLuint resolveDepth = 0;
    };

    // the pass's program and its settings' places
    struct Pass {
        GLuint program = 0;
        GLint inverseGamma = -1;
        GLint dither = -1;
    };

    Stage(const Gl& gl, Pass pass, Images images, const Settings& settings) noexcept;

    [[nodiscard]] static std::expected<Images, std::string> MakeImages(const Gl& gl, const Settings& settings);
    static void DeleteImages(const Gl& gl, Images& images) noexcept;
    void Release() noexcept;

    const Gl* gl_;
    Pass pass_;
    Images images_;
    Settings settings_;
};

} // namespace render_output
