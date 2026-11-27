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

// engine.h -- the output stage as a renderer runs it, compiled by each renderer against its own headers (engine.cpp):
// OpenGL's functions loaded through the renderer's import, the stage made at the window's size with the settings
// r_ext_multisample, r_outputPrecision and r_outputDither, the player's brightness r_gamma applied by the pass.
// Example: the renderer makes the stage once it knows the window (Output_Init), draws each image into it
// (Output_DrawScene), makes the picture readable before it copies or reads from it (Output_MakeReadable, then
// Output_DrawScene), and presents it before the swap and before a capture (Output_Present).
#pragma once

#include <shared/rd-common/output/stage.h>

// Loads OpenGL's functions, checks its version and makes the stage at glConfig's size, then sets glConfig's bits of
// colour, depth and stencil to the scene's. A fatal error, with what failed, when the driver cannot.
void Output_Init();

// Deletes the stage's images and its pass, with the context current, when the renderer stops.
void Output_Shutdown();

// Binds the scene for drawing: called where an image starts. The settings changed since are applied first.
void Output_DrawScene();

// Makes the scene's images again at glConfig's size, between two images, once the window's size changed.
void Output_Resize();

// Makes a region of the picture drawn so far readable (glReadPixels, glCopyTexSubImage2D), then Output_DrawScene
// after the read.
void Output_MakeReadable(render_output::Buffers buffers, int x, int y, int width, int height);

// Draws the scene into the window's back buffer through the pass, with r_gamma: before the swap, and before a capture
// reads the window's back buffer (then Output_DrawScene).
void Output_Present();

// Prints the stage's line of gfxinfo: the scene's precision, its samples, the dither.
void Output_PrintInfo();
