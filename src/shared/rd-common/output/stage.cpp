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

// stage.cpp -- the output stage on OpenGL 3.0's framebuffer objects. The scene is a framebuffer of one colour image
// and one depth and stencil image (DEPTH24_STENCIL8, as a window's); multisampled, both are renderbuffers and a second,
// single-sampled framebuffer receives their resolve (glBlitFramebuffer), since OpenGL reads no multisampled
// framebuffer object. The pass is one triangle covering the window, its fragment shader reading the scene's pixel
// under it (texelFetch, no filtering: the scene has the window's size), raising it to 1 / gamma and adding the dither.
#include "stage.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <utility>

namespace render_output {

namespace {

// the pass's vertex shader: the triangle's corners come as OpenGL's own vertices (glVertex2f), in clip coordinates
constexpr const char* kVertexSource = R"(#version 130
void main()
{
    gl_Position = vec4(gl_Vertex.xy, 0.0, 1.0);
}
)";

// the pass's fragment shader. The dither is Jimenez's interleaved gradient noise ("Next Generation Post Processing in
// Call of Duty: Advanced Warfare", SIGGRAPH 2014), a value of [0, 1) for each pixel without a pattern of its own,
// moved to [-0.375, 0.375) of one of the window's levels: an exact level keeps its value after the rounding. A
// driver's conversion to 8 bits is not always the exact rounding (NVIDIA's rounded a value 15/32 of a level above
// another up), hence the margin under half a level.
constexpr const char* kFragmentSource = R"(#version 130
uniform sampler2D scene;
uniform float inverseGamma;
out vec4 colour;
void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec3 value = pow(texelFetch(scene, pixel, 0).rgb, vec3(inverseGamma));
    float noise = fract(52.9829189 * fract(dot(vec2(pixel), vec2(0.06711056, 0.00583715))));
    colour = vec4(value + (noise - 0.5) * 0.75 / 255.0, 1.0);
}
)";

// the triangle that covers the window, in clip coordinates
constexpr std::array<std::array<GLfloat, 2>, 3> kCoveringTriangle{{{-1.0F, -1.0F}, {3.0F, -1.0F}, {-1.0F, 3.0F}}};

// what the pass turns off of the renderer's state, which glPushAttrib keeps
constexpr std::array<GLenum, 9> kPassDisables{kBlend,    kDepthTest,  kAlphaTest,     kStencilTest,      kScissorTest,
                                              kCullFace, kClipPlane0, kColourLogicOp, kPolygonOffsetFill};

// The text of a shader's or a program's log, of the length the driver gave.
template <typename GetLog> std::string Log(GLint length, GetLog getLog)
{
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    GLsizei written = 0;
    getLog(static_cast<GLsizei>(log.size()), &written, log.data());
    log.resize(static_cast<std::size_t>(std::clamp(written, 0, static_cast<GLsizei>(log.size()))));
    return log;
}

// One shader compiled, or its log.
std::expected<GLuint, std::string> Compile(const Gl& gl, GLenum type, const char* source)
{
    const GLuint shader = gl.CreateShader(type);
    gl.ShaderSource(shader, 1, &source, nullptr);
    gl.CompileShader(shader);
    GLint compiled = 0;
    gl.GetShaderiv(shader, kCompileStatus, &compiled);

    if (compiled == 0) {
        GLint length = 0;
        gl.GetShaderiv(shader, kInfoLogLength, &length);
        std::string log = Log(length, [&](GLsizei size, GLsizei* written, GLchar* text) { gl.GetShaderInfoLog(shader, size, written, text); });
        gl.DeleteShader(shader);
        return std::unexpected(std::move(log));
    }

    return shader;
}

// The pass's program linked from its two shaders, or the log of what failed.
std::expected<GLuint, std::string> Link(const Gl& gl)
{
    const std::expected<GLuint, std::string> vertex = Compile(gl, kVertexShader, kVertexSource);

    if (!vertex) {
        return std::unexpected(fmt::format("the pass's vertex shader was refused: {}", vertex.error()));
    }

    const std::expected<GLuint, std::string> fragment = Compile(gl, kFragmentShader, kFragmentSource);

    if (!fragment) {
        gl.DeleteShader(*vertex);
        return std::unexpected(fmt::format("the pass's fragment shader was refused: {}", fragment.error()));
    }

    const GLuint program = gl.CreateProgram();
    gl.AttachShader(program, *vertex);
    gl.AttachShader(program, *fragment);
    gl.BindFragDataLocation(program, 0, "colour");
    gl.LinkProgram(program);
    // the program keeps them as long as it lives
    gl.DeleteShader(*vertex);
    gl.DeleteShader(*fragment);
    GLint linked = 0;
    gl.GetProgramiv(program, kLinkStatus, &linked);

    if (linked == 0) {
        GLint length = 0;
        gl.GetProgramiv(program, kInfoLogLength, &length);
        std::string log = Log(length, [&](GLsizei size, GLsizei* written, GLchar* text) { gl.GetProgramInfoLog(program, size, written, text); });
        gl.DeleteProgram(program);
        return std::unexpected(fmt::format("the pass's program was refused: {}", log));
    }

    return program;
}

// The texture of a single-sampled colour image: one level, read pixel by pixel.
GLuint MakeTexture(const Gl& gl, GLenum format, Size size)
{
    // the renderer keeps which texture each unit has: the one bound before is bound again
    GLint previous = 0;
    gl.GetIntegerv(kTextureBinding2d, &previous);
    GLuint texture = 0;
    gl.GenTextures(1, &texture);
    gl.BindTexture(kTexture2d, texture);
    gl.TexImage2D(kTexture2d, 0, static_cast<GLint>(format), size.width, size.height, 0, kRgba, kUnsignedByte, nullptr);
    gl.TexParameteri(kTexture2d, kTextureMinFilter, kNearest);
    gl.TexParameteri(kTexture2d, kTextureMagFilter, kNearest);
    gl.TexParameteri(kTexture2d, kTextureWrapS, kClampToEdge);
    gl.TexParameteri(kTexture2d, kTextureWrapT, kClampToEdge);
    gl.TexParameteri(kTexture2d, kTextureMaxLevel, 0);
    gl.BindTexture(kTexture2d, static_cast<GLuint>(previous));
    return texture;
}

// A renderbuffer of the format and samples given, 0 samples for a single-sampled one.
GLuint MakeRenderbuffer(const Gl& gl, GLenum format, int samples, Size size)
{
    GLuint renderbuffer = 0;
    gl.GenRenderbuffers(1, &renderbuffer);
    gl.BindRenderbuffer(kRenderbuffer, renderbuffer);
    gl.RenderbufferStorageMultisample(kRenderbuffer, samples, format, size.width, size.height);
    gl.BindRenderbuffer(kRenderbuffer, 0);
    return renderbuffer;
}

// Binds a framebuffer and attaches a colour image (a texture, or else a renderbuffer) and a depth and stencil
// renderbuffer to it; false when the driver does not take them together.
bool Attach(const Gl& gl, GLuint framebuffer, GLuint colourTexture, GLuint colourRenderbuffer, GLuint depth)
{
    gl.BindFramebuffer(kFramebuffer, framebuffer);

    if (colourTexture != 0) {
        gl.FramebufferTexture2D(kFramebuffer, kColourAttachment0, kTexture2d, colourTexture, 0);
    }
    else {
        gl.FramebufferRenderbuffer(kFramebuffer, kColourAttachment0, kRenderbuffer, colourRenderbuffer);
    }

    gl.FramebufferRenderbuffer(kFramebuffer, kDepthStencilAttachment, kRenderbuffer, depth);
    return gl.CheckFramebufferStatus(kFramebuffer) == kFramebufferComplete;
}

// The bits a blit copies for what a read needs.
GLbitfield Mask(Buffers buffers)
{
    return buffers == Buffers::Colour ? kColourBufferBit : kDepthBufferBit | kStencilBufferBit;
}

} // namespace

Stage::Stage(const Gl& gl, Pass pass, Images images, const Settings& settings) noexcept : gl_(&gl), pass_(pass), images_(images), settings_(settings) {}

Stage::Stage(Stage&& other) noexcept
    : gl_(std::exchange(other.gl_, nullptr)), pass_(std::exchange(other.pass_, {})), images_(std::exchange(other.images_, {})), settings_(other.settings_)
{
}

Stage& Stage::operator=(Stage&& other) noexcept
{
    if (this != &other) {
        Release();
        gl_ = std::exchange(other.gl_, nullptr);
        pass_ = std::exchange(other.pass_, {});
        images_ = std::exchange(other.images_, {});
        settings_ = other.settings_;
    }

    return *this;
}

Stage::~Stage()
{
    Release();
}

void Stage::Release() noexcept
{
    if (gl_ == nullptr) {
        return;
    }

    DeleteImages(*gl_, images_);

    if (pass_.program != 0) {
        gl_->DeleteProgram(pass_.program);
        pass_ = {};
    }
}

std::expected<Stage, std::string> Stage::Create(const Gl& gl, const Settings& settings)
{
    const std::expected<GLuint, std::string> program = Link(gl);

    if (!program) {
        return std::unexpected(program.error());
    }

    const Pass pass{.program = *program, .inverseGamma = gl.GetUniformLocation(*program, "inverseGamma")};
    // the scene is the texture of unit 0, set once for the program's life
    gl.UseProgram(pass.program);
    gl.Uniform1i(gl.GetUniformLocation(pass.program, "scene"), 0);
    gl.UseProgram(0);

    GLint driverMaximum = 0;
    gl.GetIntegerv(kMaxSamples, &driverMaximum);
    Settings allowed = settings;
    allowed.samples = AllowedSamples(settings.samples, driverMaximum);
    std::expected<Images, std::string> images = MakeImages(gl, allowed);

    if (!images) {
        gl.DeleteProgram(pass.program);
        return std::unexpected(images.error());
    }

    return Stage(gl, pass, *images, allowed);
}

bool Stage::Apply(const Settings& settings)
{
    assert(gl_ != nullptr);
    GLint driverMaximum = 0;
    gl_->GetIntegerv(kMaxSamples, &driverMaximum);
    Settings allowed = settings;
    allowed.samples = AllowedSamples(settings.samples, driverMaximum);

    if (allowed == settings_) {
        return true;
    }

    std::expected<Images, std::string> images = MakeImages(*gl_, allowed);

    if (!images) {
        DrawScene();
        return false;
    }

    DeleteImages(*gl_, images_);
    images_ = *images;
    settings_ = allowed;
    DrawScene();
    return true;
}

const Settings& Stage::Current() const noexcept
{
    return settings_;
}

std::expected<Stage::Images, std::string> Stage::MakeImages(const Gl& gl, const Settings& settings)
{
    Images images;
    // 16 bits a channel, unsigned normalized: the blends clamped to [0, 1], as a window's colour clamps them
    const GLenum format = kRgba16;
    const bool multisampled = settings.samples > 0;
    gl.GenFramebuffers(1, &images.sceneFramebuffer);

    if (multisampled) {
        images.sceneColour = MakeRenderbuffer(gl, format, settings.samples, settings.size);
    }
    else {
        images.sceneTexture = MakeTexture(gl, format, settings.size);
    }

    images.sceneDepth = MakeRenderbuffer(gl, kDepth24Stencil8, settings.samples, settings.size);
    bool complete = Attach(gl, images.sceneFramebuffer, images.sceneTexture, images.sceneColour, images.sceneDepth);

    if (complete && multisampled) {
        gl.GenFramebuffers(1, &images.resolveFramebuffer);
        images.resolveTexture = MakeTexture(gl, format, settings.size);
        images.resolveDepth = MakeRenderbuffer(gl, kDepth24Stencil8, 0, settings.size);
        complete = Attach(gl, images.resolveFramebuffer, images.resolveTexture, 0, images.resolveDepth);
    }

    gl.BindFramebuffer(kFramebuffer, images.sceneFramebuffer);

    if (!complete) {
        gl.BindFramebuffer(kFramebuffer, 0);
        DeleteImages(gl, images);
        return std::unexpected(
            fmt::format("the driver refused the scene's images (RGBA16, {} samples, {} by {})", settings.samples, settings.size.width, settings.size.height));
    }

    return images;
}

void Stage::DeleteImages(const Gl& gl, Images& images) noexcept
{
    // OpenGL skips the names 0
    gl.DeleteFramebuffers(1, &images.sceneFramebuffer);
    gl.DeleteFramebuffers(1, &images.resolveFramebuffer);
    gl.DeleteTextures(1, &images.sceneTexture);
    gl.DeleteTextures(1, &images.resolveTexture);
    gl.DeleteRenderbuffers(1, &images.sceneColour);
    gl.DeleteRenderbuffers(1, &images.sceneDepth);
    gl.DeleteRenderbuffers(1, &images.resolveDepth);
    images = {};
}

void Stage::DrawScene()
{
    gl_->BindFramebuffer(kFramebuffer, images_.sceneFramebuffer);
}

void Stage::MakeReadable(Buffers buffers, Region region)
{
    if (settings_.samples == 0) {
        gl_->BindFramebuffer(kReadFramebuffer, images_.sceneFramebuffer);
        return;
    }

    const GLint right = region.x + region.width;
    const GLint top = region.y + region.height;
    gl_->BindFramebuffer(kReadFramebuffer, images_.sceneFramebuffer);
    gl_->BindFramebuffer(kDrawFramebuffer, images_.resolveFramebuffer);
    gl_->BlitFramebuffer(region.x, region.y, right, top, region.x, region.y, right, top, Mask(buffers), static_cast<GLenum>(kNearest));
    gl_->BindFramebuffer(kDrawFramebuffer, images_.sceneFramebuffer);
    gl_->BindFramebuffer(kReadFramebuffer, images_.resolveFramebuffer);
}

void Stage::Present(float gamma)
{
    const Size size = settings_.size;
    GLuint source = images_.sceneTexture;

    if (settings_.samples > 0) {
        MakeReadable(Buffers::Colour, {.x = 0, .y = 0, .width = size.width, .height = size.height});
        source = images_.resolveTexture;
    }

    // the window's framebuffer first: glPopAttrib sets its draw buffer back on the framebuffer bound then
    gl_->BindFramebuffer(kFramebuffer, 0);
    GLint unit = 0;
    gl_->GetIntegerv(kActiveTexture, &unit);
    gl_->PushAttrib(kEnableBit | kColourBufferBit | kViewportBit | kPolygonBit | kTextureBit);

    for (const GLenum capability : kPassDisables) {
        gl_->Disable(capability);
    }

    gl_->ColorMask(1, 1, 1, 1);
    gl_->PolygonMode(kFrontAndBack, kFill);
    gl_->Viewport(0, 0, size.width, size.height);
    gl_->ActiveTexture(kTexture0);
    gl_->BindTexture(kTexture2d, source);
    gl_->UseProgram(pass_.program);
    gl_->Uniform1f(pass_.inverseGamma, 1.0F / std::clamp(gamma, kMinimumGamma, kMaximumGamma));
    gl_->Begin(kTriangles);

    for (const auto& [x, y] : kCoveringTriangle) {
        gl_->Vertex2f(x, y);
    }

    gl_->End();
    gl_->UseProgram(0);
    gl_->PopAttrib();
    gl_->ActiveTexture(static_cast<GLenum>(unit));
}

} // namespace render_output
