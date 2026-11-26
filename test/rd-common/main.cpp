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

// The tests of src/shared/rd-common, its output stage (output/) without OpenGL: the version read at the start of the driver's string,
// the samples allowed against the driver's maximum, and the loading of the functions through a loader that knows
// some of them (a version below 3.0 refused, a missing function named, a complete table).
// Usage: modernjk_test_rd_common; prints each failed check, exits with their count. With the argument "real", by hand,
// it runs instead the stage on the system's own OpenGL driver, in a window (real.cpp).
#include <check.h>

#include <shared/rd-common/output/gl.h>
#include <shared/rd-common/output/stage.h>

#include <fmt/base.h>

#include <cstring>
#include <string_view>

int RunRealTests();

namespace {

// what the fake driver says its version is
const char* fakeVersion = "4.6.0 NVIDIA 590.00";
// the one function the fake driver lacks, or none
const char* fakeMissing = "";

const render_output::GLubyte* FakeGetString(render_output::GLenum /*name*/)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): OpenGL's strings are unsigned bytes
    return reinterpret_cast<const render_output::GLubyte*>(fakeVersion);
}

void FakeFunction() {}

// the fake driver's loader: glGetString, and every other name but fakeMissing
render_output::GlFunction FakeLoader(const char* name)
{
    if (std::strcmp(name, "glGetString") == 0) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): a function of the library comes untyped
        return reinterpret_cast<render_output::GlFunction>(&FakeGetString);
    }

    return std::strcmp(name, fakeMissing) == 0 ? nullptr : &FakeFunction;
}

render_output::GlFunction EmptyLoader(const char* /*name*/)
{
    return nullptr;
}

void CheckVersion()
{
    using render_output::GlVersion;
    using render_output::ParseVersion;
    CHECK(ParseVersion("4.6.0 NVIDIA 590.00") == (GlVersion{.major = 4, .minor = 6}));
    CHECK(ParseVersion("4.5 (Compatibility Profile) Mesa 23.2.1") == (GlVersion{.major = 4, .minor = 5}));
    CHECK(ParseVersion("3.0") == (GlVersion{.major = 3, .minor = 0}));
    CHECK(!ParseVersion("").has_value());
    CHECK(!ParseVersion("OpenGL ES 3.2").has_value());
    CHECK(!ParseVersion("4").has_value());
    CHECK(!ParseVersion("4.").has_value());
    CHECK((GlVersion{.major = 2, .minor = 1}) < render_output::kNeededVersion);
    CHECK((GlVersion{.major = 3, .minor = 0}) >= render_output::kNeededVersion);
}

void CheckSamples()
{
    using render_output::AllowedSamples;
    CHECK_EQUAL(AllowedSamples(0, 8), 0);
    CHECK_EQUAL(AllowedSamples(1, 8), 0);
    CHECK_EQUAL(AllowedSamples(-1, 8), 0);
    CHECK_EQUAL(AllowedSamples(2, 8), 2);
    CHECK_EQUAL(AllowedSamples(3, 8), 2);
    CHECK_EQUAL(AllowedSamples(4, 8), 4);
    CHECK_EQUAL(AllowedSamples(8, 8), 8);
    CHECK_EQUAL(AllowedSamples(16, 32), 8);
    CHECK_EQUAL(AllowedSamples(8, 4), 4);
    CHECK_EQUAL(AllowedSamples(4, 0), 0);

    // the menu's row: 0, then the counts the driver allows, rising, round
    using render_output::NextSamples;
    CHECK_EQUAL(NextSamples(0, 8, false), 2);
    CHECK_EQUAL(NextSamples(2, 8, false), 4);
    CHECK_EQUAL(NextSamples(4, 8, false), 8);
    CHECK_EQUAL(NextSamples(8, 8, false), 0);
    CHECK_EQUAL(NextSamples(0, 8, true), 8);
    CHECK_EQUAL(NextSamples(2, 8, true), 0);
    CHECK_EQUAL(NextSamples(3, 8, false), 4);
    CHECK_EQUAL(NextSamples(4, 4, false), 0);
    CHECK_EQUAL(NextSamples(8, 4, false), 0);
    CHECK_EQUAL(NextSamples(0, 4, true), 4);
    CHECK_EQUAL(NextSamples(0, 0, false), 0);
    CHECK_EQUAL(NextSamples(0, 0, true), 0);
    static_assert(NextSamples(0, 32, false) == 2 && NextSamples(2, 2, false) == 0);
}

void CheckLoading()
{
    fakeVersion = "2.1 Mesa 8.0";
    fakeMissing = "";
    const auto old = render_output::LoadGl(&FakeLoader);
    CHECK(!old.has_value());

    if (!old) {
        CHECK(old.error().contains("3.0 or later"));
        CHECK(old.error().contains("2.1 Mesa 8.0"));
    }

    fakeVersion = "4.6.0 NVIDIA 590.00";
    fakeMissing = "glBlitFramebuffer";
    const auto lacking = render_output::LoadGl(&FakeLoader);
    CHECK(!lacking.has_value());

    if (!lacking) {
        CHECK(lacking.error().contains("glBlitFramebuffer"));
    }

    fakeMissing = "";
    const auto complete = render_output::LoadGl(&FakeLoader);
    CHECK(complete.has_value());

    if (complete) {
        CHECK(complete->BlitFramebuffer != nullptr);
        CHECK(complete->Vertex2f != nullptr);
    }

    CHECK(!render_output::LoadGl(&EmptyLoader).has_value());
}

} // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && std::string_view(argv[1]) == "real") {
        return RunRealTests();
    }

    CheckVersion();
    CheckSamples();
    CheckLoading();

    if (test::failures != 0) {
        fmt::println("{} check(s) failed", test::failures);
    }

    return test::failures;
}
