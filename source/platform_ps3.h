// PS3-only pieces next to the shared platform interface: video output and system events.
#pragma once

#include <cstdint>

namespace ps3 {

/// Sets up the RSX and two display buffers at the console's current video mode.
///
/// Args:
///     width: Receives the output width in pixels.
///     height: Receives the output height in pixels.
///
/// Returns:
///     False when the video mode is unknown or a buffer could not be allocated.
bool videoInit(int& width, int& height);

/// Copies a finished frame (width * height 0xAARRGGBB words) to the back buffer and flips.
void videoPresent(const std::uint32_t* pixels);

/// Waits for the last flip and releases the RSX.
void videoShutdown();

/// Runs the system callbacks. Call once per frame; the system quits apps that stop calling it.
void pumpEvents();

/// True once the console asked the app to quit (PS button, "Quit Game").
bool quitRequested();

/// True once after the system drew over the frame (in-game XMB menu closed): redraw it.
bool takeRedraw();

}  // namespace ps3
