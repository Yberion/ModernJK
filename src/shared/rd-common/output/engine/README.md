# The output stage as a renderer runs it

`engine.h` and `engine.cpp`, compiled by each OpenGL renderer against its own headers (`<rd-vanilla/tr_local.h>` of its tree): the multiplayer's `rd-vanilla`, the single player's `rdsp-vanilla` and `rdjosp-vanilla`. The renderer's boundary keeps the renderer's shape: free functions named `Output_`, the renderer's settings (`cvar_t`), its `glConfig`, its import `ri`.

## In the game

What happens to every image of the three games between the renderer's last drawing and the swap: the scene drawn into the window with the player's brightness. A screenshot, Jedi Outcast's saved picture and a level's shot are read after that pass, so they show what the screen showed.

## Functions

| function | called | what it does |
|---|---|---|
| `Output_Init` | where the renderer starts on a new window (`InitOpenGL`) | loads OpenGL's functions, checks its version, sets `r_ext_multisample_avail` to the most samples the driver allows (the video menus' row offers up to it), makes the stage at `glConfig`'s size with `r_ext_multisample` and `r_outputPrecision`, sets `glConfig`'s bits of colour, depth and stencil to the scene's; a fatal error with the driver's text when it cannot |
| `Output_Shutdown` | where the renderer stops for a restart of the video (`RE_Shutdown` with `destroyWindow`) | deletes the stage's images and pass |
| `Output_DrawScene` | at the start of each image (`RB_DrawBuffer`), after a read, after the swap | binds the scene for drawing; a changed `r_ext_multisample` or `r_outputPrecision` is applied first, the scene's images made again |
| `Output_Resize` | when the window's size changed, between two images (the renderer's export `DisplayChanged`, once `glConfig` has the new size) | makes the scene's images again at `glConfig`'s size, the pass kept; a warning when the driver refuses them, the images then as they were |
| `Output_MakeReadable` | before the renderer reads its picture: the dynamic glow's copies, the distortion's capture, the post-render entities' copy, a flare's pixel of depth, the overdraw's stencil, the single player's dissolve | makes the region readable (resolved when multisampled) |
| `Output_Present` | before the swap (`RB_SwapBuffers`), the splash, and before a capture reads the window (`RB_ReadPixels`) | draws the scene into the window's back buffer with `r_gamma` and `r_outputDither` |
| `Output_PrintInfo` | in `gfxinfo` | the line `OUTPUT: RGBA8, 4 samples, dither none` |

## Rules

- The table of OpenGL's functions is on the heap, the stage pointing to it, both in one object reached through one function (`Running`), from `Output_Init` to `Output_Shutdown`.
- The settings are the renderer's own, registered in each tree's `R_Register`: `r_ext_multisample`, `r_outputPrecision` and `r_outputDither` (applied at once), and `r_gamma`, read at each pass. `r_ext_multisample_avail` is the stage's answer, for the menus.
- A function called outside `Output_Init` and `Output_Shutdown` does nothing.
