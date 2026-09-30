#pragma once

#include <filesystem>

class PS2Runtime;
struct R5900Context;

// Development diagnostics. Call only on the guest executor at a safe point.
bool ps2xCaptureSceneState(PS2Runtime &runtime, const R5900Context &context,
                          const std::filesystem::path &directory) noexcept;
void ps2xPollSceneCapture(PS2Runtime &runtime, const R5900Context &context) noexcept;
