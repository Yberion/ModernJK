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

// gl.cpp -- the loading of the stage's OpenGL functions by their names. The version is read first, through the one
// function needed for it: under X11 the loader gives a pointer for any name, also for a function the driver lacks,
// so only a version of 3.0 or more says that OpenGL 3.0's functions are there.
#include "gl.h"

#include <fmt/format.h>

#include <charconv>
#include <system_error>

namespace render_output {

namespace {

// Asks the loader for one function and keeps it under its own type; its name joins `missing` when there is none.
template <typename Function> void Load(GlLoader loader, const char* name, Function& function, std::string& missing)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the library gives every function untyped, and each is called through its own type
    function = reinterpret_cast<Function>(loader(name));

    if (function == nullptr) {
        missing += missing.empty() ? name : fmt::format(", {}", name);
    }
}

// Reads one number at the start of text, and moves text past it.
std::optional<int> ReadNumber(std::string_view& text)
{
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

    if (error != std::errc{}) {
        return std::nullopt;
    }

    text.remove_prefix(static_cast<std::size_t>(end - text.data()));
    return value;
}

} // namespace

std::optional<GlVersion> ParseVersion(std::string_view text)
{
    const std::optional<int> major = ReadNumber(text);

    if (!major || !text.starts_with('.')) {
        return std::nullopt;
    }

    text.remove_prefix(1);
    const std::optional<int> minor = ReadNumber(text);

    if (!minor) {
        return std::nullopt;
    }

    return GlVersion{.major = *major, .minor = *minor};
}

std::expected<Gl, std::string> LoadGl(GlLoader loader)
{
    Gl gl;
    std::string missing;
    Load(loader, "glGetString", gl.GetString, missing);

    if (gl.GetString == nullptr) {
        return std::unexpected("OpenGL's glGetString could not be found");
    }

    // OpenGL's strings are of unsigned bytes holding ASCII
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): GLubyte and char are both bytes
    const auto* versionText = reinterpret_cast<const char*>(gl.GetString(kVersion));
    const std::string_view versionString = versionText != nullptr ? std::string_view(versionText) : std::string_view();
    const std::optional<GlVersion> version = ParseVersion(versionString);

    if (!version || *version < kNeededVersion) {
        return std::unexpected(
            fmt::format("OpenGL {}.{} or later is needed; the driver gives \"{}\"", kNeededVersion.major, kNeededVersion.minor, versionString));
    }

    Load(loader, "glGetError", gl.GetError, missing);
    Load(loader, "glGetIntegerv", gl.GetIntegerv, missing);
    Load(loader, "glEnable", gl.Enable, missing);
    Load(loader, "glDisable", gl.Disable, missing);
    Load(loader, "glPushAttrib", gl.PushAttrib, missing);
    Load(loader, "glPopAttrib", gl.PopAttrib, missing);
    Load(loader, "glViewport", gl.Viewport, missing);
    Load(loader, "glColorMask", gl.ColorMask, missing);
    Load(loader, "glPolygonMode", gl.PolygonMode, missing);
    Load(loader, "glGenTextures", gl.GenTextures, missing);
    Load(loader, "glDeleteTextures", gl.DeleteTextures, missing);
    Load(loader, "glBindTexture", gl.BindTexture, missing);
    Load(loader, "glTexParameteri", gl.TexParameteri, missing);
    Load(loader, "glTexImage2D", gl.TexImage2D, missing);
    Load(loader, "glBegin", gl.Begin, missing);
    Load(loader, "glEnd", gl.End, missing);
    Load(loader, "glVertex2f", gl.Vertex2f, missing);
    Load(loader, "glActiveTexture", gl.ActiveTexture, missing);
    Load(loader, "glCreateShader", gl.CreateShader, missing);
    Load(loader, "glShaderSource", gl.ShaderSource, missing);
    Load(loader, "glCompileShader", gl.CompileShader, missing);
    Load(loader, "glGetShaderiv", gl.GetShaderiv, missing);
    Load(loader, "glGetShaderInfoLog", gl.GetShaderInfoLog, missing);
    Load(loader, "glDeleteShader", gl.DeleteShader, missing);
    Load(loader, "glCreateProgram", gl.CreateProgram, missing);
    Load(loader, "glAttachShader", gl.AttachShader, missing);
    Load(loader, "glLinkProgram", gl.LinkProgram, missing);
    Load(loader, "glGetProgramiv", gl.GetProgramiv, missing);
    Load(loader, "glGetProgramInfoLog", gl.GetProgramInfoLog, missing);
    Load(loader, "glDeleteProgram", gl.DeleteProgram, missing);
    Load(loader, "glUseProgram", gl.UseProgram, missing);
    Load(loader, "glGetUniformLocation", gl.GetUniformLocation, missing);
    Load(loader, "glUniform1i", gl.Uniform1i, missing);
    Load(loader, "glUniform1f", gl.Uniform1f, missing);
    Load(loader, "glBindFragDataLocation", gl.BindFragDataLocation, missing);
    Load(loader, "glGenFramebuffers", gl.GenFramebuffers, missing);
    Load(loader, "glDeleteFramebuffers", gl.DeleteFramebuffers, missing);
    Load(loader, "glBindFramebuffer", gl.BindFramebuffer, missing);
    Load(loader, "glFramebufferTexture2D", gl.FramebufferTexture2D, missing);
    Load(loader, "glFramebufferRenderbuffer", gl.FramebufferRenderbuffer, missing);
    Load(loader, "glCheckFramebufferStatus", gl.CheckFramebufferStatus, missing);
    Load(loader, "glGenRenderbuffers", gl.GenRenderbuffers, missing);
    Load(loader, "glDeleteRenderbuffers", gl.DeleteRenderbuffers, missing);
    Load(loader, "glBindRenderbuffer", gl.BindRenderbuffer, missing);
    Load(loader, "glRenderbufferStorageMultisample", gl.RenderbufferStorageMultisample, missing);
    Load(loader, "glBlitFramebuffer", gl.BlitFramebuffer, missing);

    if (!missing.empty()) {
        return std::unexpected(fmt::format("the driver's OpenGL ({}) lacks {}", versionString, missing));
    }

    return gl;
}

} // namespace render_output
