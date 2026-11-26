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

// gl.h -- the part of OpenGL the output stage uses: its types, its constants and its functions, every function asked
// of the driver by its name through the renderer's import, OpenGL 1.1's included (the loader finds those in the
// library). It needs none of the system's OpenGL headers, whose versions differ (Windows' stops at OpenGL 1.1), and
// its names differ from theirs, so that both can be included in one file. The constants are the values of the Khronos
// registry, the same in every header.
// Example: a renderer loads the functions once its context is current, then gives the table to the stage:
//     auto gl = render_output::LoadGl(ri.GL_GetProcAddress);
//     if (!gl) { /* gl.error(): an OpenGL below 3.0, or a function the driver lacks */ }
#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace render_output {

// OpenGL's types, as the Khronos registry defines them
using GLenum = std::uint32_t;
using GLbitfield = std::uint32_t;
using GLuint = std::uint32_t;
using GLint = std::int32_t;
using GLsizei = std::int32_t;
using GLfloat = float;
using GLboolean = std::uint8_t;
using GLubyte = std::uint8_t;
using GLchar = char;

// the constants the stage uses
constexpr GLenum kNoError = 0;
constexpr GLenum kVersion = 0x1F02;
constexpr GLenum kTriangles = 0x0004;
constexpr GLenum kTexture2d = 0x0DE1;
constexpr GLenum kTextureMinFilter = 0x2801;
constexpr GLenum kTextureMagFilter = 0x2800;
constexpr GLenum kTextureWrapS = 0x2802;
constexpr GLenum kTextureWrapT = 0x2803;
constexpr GLenum kTextureMaxLevel = 0x813D;
constexpr GLint kNearest = 0x2600;
constexpr GLint kClampToEdge = 0x812F;
constexpr GLenum kTexture0 = 0x84C0;
constexpr GLenum kActiveTexture = 0x84E0;
constexpr GLenum kTextureBinding2d = 0x8069;
constexpr GLenum kRgba = 0x1908;
constexpr GLenum kUnsignedByte = 0x1401;
constexpr GLenum kRgba8 = 0x8058;
constexpr GLenum kRgba16 = 0x805B;
constexpr GLenum kDepth24Stencil8 = 0x88F0;
constexpr GLenum kFramebuffer = 0x8D40;
constexpr GLenum kReadFramebuffer = 0x8CA8;
constexpr GLenum kDrawFramebuffer = 0x8CA9;
constexpr GLenum kRenderbuffer = 0x8D41;
constexpr GLenum kColourAttachment0 = 0x8CE0;
constexpr GLenum kDepthStencilAttachment = 0x821A;
constexpr GLenum kFramebufferComplete = 0x8CD5;
constexpr GLenum kMaxSamples = 0x8D57;
constexpr GLbitfield kColourBufferBit = 0x00004000;
constexpr GLbitfield kDepthBufferBit = 0x00000100;
constexpr GLbitfield kStencilBufferBit = 0x00000400;
constexpr GLbitfield kEnableBit = 0x00002000;
constexpr GLbitfield kViewportBit = 0x00000800;
constexpr GLbitfield kPolygonBit = 0x00000008;
constexpr GLbitfield kTextureBit = 0x00040000;
constexpr GLenum kBlend = 0x0BE2;
constexpr GLenum kDepthTest = 0x0B71;
constexpr GLenum kAlphaTest = 0x0BC0;
constexpr GLenum kStencilTest = 0x0B90;
constexpr GLenum kScissorTest = 0x0C11;
constexpr GLenum kCullFace = 0x0B44;
constexpr GLenum kClipPlane0 = 0x3000;
constexpr GLenum kColourLogicOp = 0x0BF2;
constexpr GLenum kPolygonOffsetFill = 0x8037;
constexpr GLenum kFrontAndBack = 0x0408;
constexpr GLenum kFill = 0x1B02;
constexpr GLenum kVertexShader = 0x8B31;
constexpr GLenum kFragmentShader = 0x8B30;
constexpr GLenum kCompileStatus = 0x8B81;
constexpr GLenum kLinkStatus = 0x8B82;
constexpr GLenum kInfoLogLength = 0x8B84;

// A function of the OpenGL library, untyped, and the renderer's import that finds one by its name.
using GlFunction = void (*)();
using GlLoader = GlFunction (*)(const char* name);

// OpenGL's version, as the driver gives it.
struct GlVersion {
    int major = 0;
    int minor = 0;

    [[nodiscard]] auto operator<=>(const GlVersion&) const = default;
};

// The version the stage needs: framebuffer objects, multisampled renderbuffers, their resolve, GLSL 1.30.
constexpr GlVersion kNeededVersion{.major = 3, .minor = 0};

// The version at the start of OpenGL's version string ("4.6.0 NVIDIA 590.00", "4.5 (Compatibility Profile) Mesa"),
// or nothing when it does not start with one.
[[nodiscard]] std::optional<GlVersion> ParseVersion(std::string_view text);

// The functions of the stage, each a pointer to the driver's. The calling convention is the platform's default one:
// OpenGL's (APIENTRY) differs from it only on 32-bit Windows, which the project does not build.
struct Gl {
    // OpenGL 1.1
    GLenum (*GetError)() = nullptr;
    const GLubyte* (*GetString)(GLenum name) = nullptr;
    void (*GetIntegerv)(GLenum name, GLint* data) = nullptr;
    void (*Enable)(GLenum capability) = nullptr;
    void (*Disable)(GLenum capability) = nullptr;
    void (*PushAttrib)(GLbitfield mask) = nullptr;
    void (*PopAttrib)() = nullptr;
    void (*Viewport)(GLint x, GLint y, GLsizei width, GLsizei height) = nullptr;
    void (*ColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) = nullptr;
    void (*PolygonMode)(GLenum face, GLenum mode) = nullptr;
    void (*GenTextures)(GLsizei count, GLuint* textures) = nullptr;
    void (*DeleteTextures)(GLsizei count, const GLuint* textures) = nullptr;
    void (*BindTexture)(GLenum target, GLuint texture) = nullptr;
    void (*TexParameteri)(GLenum target, GLenum name, GLint value) = nullptr;
    void (*TexImage2D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
                       const void* pixels) = nullptr;
    void (*Begin)(GLenum mode) = nullptr;
    void (*End)() = nullptr;
    void (*Vertex2f)(GLfloat x, GLfloat y) = nullptr;
    // OpenGL 1.3
    void (*ActiveTexture)(GLenum unit) = nullptr;
    // OpenGL 2.0
    GLuint (*CreateShader)(GLenum type) = nullptr;
    void (*ShaderSource)(GLuint shader, GLsizei count, const GLchar* const* sources, const GLint* lengths) = nullptr;
    void (*CompileShader)(GLuint shader) = nullptr;
    void (*GetShaderiv)(GLuint shader, GLenum name, GLint* value) = nullptr;
    void (*GetShaderInfoLog)(GLuint shader, GLsizei size, GLsizei* length, GLchar* log) = nullptr;
    void (*DeleteShader)(GLuint shader) = nullptr;
    GLuint (*CreateProgram)() = nullptr;
    void (*AttachShader)(GLuint program, GLuint shader) = nullptr;
    void (*LinkProgram)(GLuint program) = nullptr;
    void (*GetProgramiv)(GLuint program, GLenum name, GLint* value) = nullptr;
    void (*GetProgramInfoLog)(GLuint program, GLsizei size, GLsizei* length, GLchar* log) = nullptr;
    void (*DeleteProgram)(GLuint program) = nullptr;
    void (*UseProgram)(GLuint program) = nullptr;
    GLint (*GetUniformLocation)(GLuint program, const GLchar* name) = nullptr;
    void (*Uniform1i)(GLint location, GLint value) = nullptr;
    void (*Uniform1f)(GLint location, GLfloat value) = nullptr;
    // OpenGL 3.0
    void (*BindFragDataLocation)(GLuint program, GLuint colour, const GLchar* name) = nullptr;
    void (*GenFramebuffers)(GLsizei count, GLuint* framebuffers) = nullptr;
    void (*DeleteFramebuffers)(GLsizei count, const GLuint* framebuffers) = nullptr;
    void (*BindFramebuffer)(GLenum target, GLuint framebuffer) = nullptr;
    void (*FramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textureTarget, GLuint texture, GLint level) = nullptr;
    void (*FramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbufferTarget, GLuint renderbuffer) = nullptr;
    GLenum (*CheckFramebufferStatus)(GLenum target) = nullptr;
    void (*GenRenderbuffers)(GLsizei count, GLuint* renderbuffers) = nullptr;
    void (*DeleteRenderbuffers)(GLsizei count, const GLuint* renderbuffers) = nullptr;
    void (*BindRenderbuffer)(GLenum target, GLuint renderbuffer) = nullptr;
    void (*RenderbufferStorageMultisample)(GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height) = nullptr;
    void (*BlitFramebuffer)(GLint sourceX0, GLint sourceY0, GLint sourceX1, GLint sourceY1, GLint destinationX0, GLint destinationY0, GLint destinationX1,
                            GLint destinationY1, GLbitfield mask, GLenum filter) = nullptr;
};

// Loads the table through the renderer's import, with the context current: first the version, refused below
// kNeededVersion, then every function. The error's text names the version or the functions the driver lacks.
[[nodiscard]] std::expected<Gl, std::string> LoadGl(GlLoader loader);

} // namespace render_output
