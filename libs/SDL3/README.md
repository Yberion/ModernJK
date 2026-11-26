# SDL 3 (vendored source tree)

SDL 3 gives the client its window and OpenGL context, the keyboard, the mouse, the gamepads, the clipboard and the
error's message box. It is built as a static library by its own `CMakeLists.txt`, from `Dependencies.cmake` (target
`vendored::sdl`), for the engines that open a window; the dedicated server does not link it.

- Source: the release 3.4.20 of 3 November 2026 (`REVISION.txt`: `release-3.4.20-0-gf00dc919c`), the archive
  `SDL3-3.4.20.tar.gz` of the project's own releases page, https://github.com/libsdl-org/SDL/releases/tag/release-3.4.20,
  15,656,494 bytes, SHA-256 `f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f`. Its GPG signature
  (`SDL3-3.4.20.tar.gz.sig`, made with the keys `1528635D8053A57F77D1E08630A59377A7763BE6` and
  `0900104363B4C9D4223DE149D913FE7D4B61D39B`) was not checked: the checksum alone stands for the archive.
- Kept unchanged: `src/`, `include/`, `cmake/` but `cmake/test/`, `wayland-protocols/` (the protocols' descriptions,
  from which the build generates the Wayland driver's code), `CMakeLists.txt`, `LICENSE.txt`, `CREDITS.md`,
  `WhatsNew.txt` and `REVISION.txt` (the build reads the revision from it). Left out: `test/`, `examples/`, `docs/`,
  `build-scripts/`, `cmake/test/`, the projects of other build systems and platforms (`VisualC/`, `VisualC-GDK/`,
  `Xcode/`, `android-project/`, `Android.mk`), its `README.md` (this file in its place), `INSTALL.md`, `BUGS.txt`,
  `AGENTS.md`, `CLAUDE.md`, `.clang-format`, `.clang-tidy`, `.editorconfig`, `.git-hash`, `.wikiheaders-options`, and
  the seven generated `.hlsl` files of `src/render/gpu/shaders/`, which SDL's own `.gitignore` of that folder
  excludes. Two of its folders have names the repository's `.gitignore` ignores (`include/build_config/`,
  `src/hidapi/dist/`): the repository's `.gitignore` takes them back in.
- Build (`Dependencies.cmake`): a static library, so that nothing ships beside an engine (about 2 MB more in each
  executable); a player can still put another SDL 3 in its place with the environment variable `SDL3_DYNAMIC_API`
  (SDL's `README-dynapi`). Off: the subsystems the client does not use (the audio, which OpenAL Soft plays; the GPU
  and render APIs, the renderer drawing with OpenGL; the camera, the sensors, the haptic devices, the file dialogs, the
  tray, the power state), OpenGL ES, its test library, tests, examples and install. On Linux, off as well: IBus,
  XTest, the X screen saver and shape extensions, FriBidi, libthai, the KMS/DRM video driver, liburing, libusb,
  PipeWire, and the Raspberry Pi's and Rockchip's video drivers; X11 (with Xcursor, Xdbe, Xfixes, XInput 2, XRandR
  and Xsync), Wayland (with libdecor), EGL, udev and D-Bus are required, their libraries opened at run time, so that a
  player needs only the system's own (the development packages: the repository's `README.md`). SDL leaves out without
  an error what the build machine lacks, so `Dependencies.cmake` checks each required piece after SDL's configuration
  and stops when one is missing, rather than giving a binary without it.
  The one warning its own code gives with its own flags is switched off on its target, with its reason
  (`Dependencies.cmake`): with clang, `-Wunused-but-set-global` (`src/events/SDL_events.c`). With MSVC it is built
  without link-time optimization: SDL compiles `src/stdlib/SDL_mslibc.c` with `/GL-`, which the Release presets' `/GL`
  contradicts (D9025, C4652). On a system whose `wayland-scanner` is older than the protocols (Ubuntu 22.04's 1.20),
  the scanner prints "XML failed validation against built-in DTD" for four of them (the attribute
  `deprecated-since`, which its DTD does not know) and generates their code all the same.
- Patches: none.
- Licence: the zlib licence (`LICENSE.txt`), which the GNU GPL version 2 accepts. Parts under their own: HIDAPI under
  its original or BSD licence (`src/hidapi/LICENSE.txt`), the math functions of `src/libm` under Sun's notice, the
  Khronos headers (`src/video/khronos`) under the MIT and Apache 2.0 licences, yuv2rgb under the BSD licence
  (`src/video/yuv2rgb/LICENSE`), and stb_image and miniz (public domain or MIT) in `src/video`.
- To update it: replace the kept files with those of a newer release's archive (its checksum compared with the
  release page's), remove the files left out again, read `WhatsNew.txt`, build, and run `ctest` (`modernjk.platform`
  creates a window on SDL's dummy video driver and reads an event back).
