# The output stage

The last step of each image of the two OpenGL renderers (the multiplayer's, and the single player's that draws Jedi Academy and Jedi Outcast): one static library, `modernjk_render_output`, independent of the renderers, and its part the renderers compile (`engine/`). The renderer draws its whole picture into an image of its own, the scene; at the end of the image one pass draws the scene into the window and applies the player's brightness there.

## In the game

The brightness slider of the setup menus (`r_gamma`) changes the picture at once, the menus and the HUD included, the same in a window, in borderless and in exclusive fullscreen, with or without the focus, and in the screenshots: the screen's own brightness table, which the game used to change for the whole desktop and gave back at each alt-tab, is never touched. The multisampling (`r_ext_multisample`, the Anti-aliasing row of the advanced video page) belongs to the scene, so it changes at once, without a restart of the video, and the window itself is a plain one: its colour only, no depth, no stencil, no samples.

## Files

| file | what it does |
|---|---|
| `gl.h`, `gl.cpp` | the part of OpenGL the stage uses: its types and constants, under names of their own, and its functions, every one asked of the driver by name through the renderer's import (`GL_GetProcAddress`), OpenGL 1.1's included; the version read first and refused below 3.0 |
| `samples.h` | the samples a pixel the multisampling can have (none, 2, 4, 8, at most what the driver allows), and the value the video menus' row goes to at a click: one definition for the stage and for the menus of the three games, the standard library only |
| `stage.h`, `stage.cpp` | the stage: the scene's images (a framebuffer object of one colour image and one `DEPTH24_STENCIL8` image; multisampled, renderbuffers and a single-sampled copy for the resolve), the pass (GLSL 1.30), what makes the picture readable before the renderer reads it |
| `engine/` | the stage as a renderer runs it, compiled by each renderer against its own headers: [engine/README.md](engine/README.md) |

## Rules

- OpenGL 3.0 or later, read in the driver's version string before any other function is asked: under X11 the loader gives a pointer for any name, also for a function the driver lacks. No fallback below it.
- The scene has the window's size, read pixel by pixel by the pass (`texelFetch`, no filtering).
- The pass's curve is `value^(1 / gamma)` for each channel, `gamma` in r_gamma's range [0.5, 3]: the formula of the screen's ramp, whose table held 8 bits, computed in floating point.
- The scene's precision is 8 or 16 bits a channel, unsigned normalized (`Precision`): both clamp the blends to [0, 1], as a window's colour does, so the game's multi-pass shaders give the picture they always gave. A floating-point image does not clamp them ("If the color buffer is floating-point, no clamping occurs", ARB_color_buffer_float), and one with 2 bits of alpha (`RGB10_A2`) would change the shaders that blend with the destination's alpha (Jedi Academy's main menu, a cloud layer, Jedi Outcast's menu).
- The dither (`Dither::Ordered`) is a fixed pattern of 4 by 4 pixels (Bayer's matrix) within 0.375 of one of the window's levels: an exact level keeps its value, so a picture without levels between two gives the same capture with or without it. The margin under half a level is there because a driver's conversion to 8 bits is not always the exact rounding (NVIDIA's rounded a value 15/32 of a level above another up).
- The samples are 2, 4 or 8, at most what the driver allows (`AllowedSamples`); fewer than 2 is none. The menus' row offers none and the counts the driver allows, in rising order (`NextSamples`).
- OpenGL reads no multisampled framebuffer object (`glReadPixels`, `glCopyTexSubImage2D`): a read of the picture is preceded by `MakeReadable`, which resolves the region read into the single-sampled copy and binds that copy for reading, and followed by `DrawScene`.
- `Present` leaves the renderer's state of OpenGL as it was (`glPushAttrib` of what it changes, the active texture unit read and set back), and the window's framebuffer bound, for the swap and for a capture to read.
- `Create` and `Apply` bind the scene; a refusal of the driver is a result the caller reads (`[[nodiscard]]`), the stage keeping its images as they were. No exception of its own.

## Example

```cpp
auto gl = render_output::LoadGl(ri.GL_GetProcAddress);
auto stage = render_output::Stage::Create(*gl, {.size = {.width = 1920, .height = 1080}, .samples = 4});

stage->DrawScene();                       // the image: the world, the HUD, the menus
stage->MakeReadable(render_output::Buffers::Colour, {.x = 0, .y = 0, .width = 1920, .height = 1080});
// ... glCopyTexSubImage2D from the scene, as the dynamic glow does ...
stage->DrawScene();
stage->Present(r_gamma->value, render_output::Dither::None);   // then the window's swap
```

## Tests

`test/rd-common` (`modernjk.rd-common` in `ctest`), without OpenGL: the version read in the driver's string, the samples allowed and the row's next value, the table loaded through a fake driver (a version below 3.0 refused with its text, a missing function named, a complete table). By hand, `modernjk_test_rd_common real` runs the stage on the system's own driver in a window of the platform layer: the 256 levels of grey drawn into the scene and read back in the window, at r_gamma 1 each level as drawn with and without the dither, at both precisions, with and without 4 samples; at r_gamma 1.5 and 2.5 the curve's level within one (the driver's `pow`); a multisampled scene made readable gives back its levels.
