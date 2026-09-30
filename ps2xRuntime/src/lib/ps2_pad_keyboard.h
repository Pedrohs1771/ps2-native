#pragma once

#include "ps2_host_backend.h"
#include <cstdint>

namespace ps2_pad_detail
{
    struct KeyboardState
    {
        uint16_t buttons = 0xFFFFu;
        uint8_t lx = 0x80u;
        uint8_t ly = 0x80u;
    };

    // Querying keys is supplied by the host; tests use a recorded key set.
    template <typename IsDown>
    KeyboardState readKeyboardState(IsDown isDown)
    {
        KeyboardState state;
        auto press = [&state](uint16_t mask) { state.buttons &= ~mask; };
        if (isDown(KEY_UP)) press(0x0010u);
        if (isDown(KEY_DOWN)) press(0x0040u);
        if (isDown(KEY_LEFT)) press(0x0080u);
        if (isDown(KEY_RIGHT)) press(0x0020u);
        auto axis = [](bool negative, bool positive) -> uint8_t {
            return negative == positive ? 128u : negative ? 0u : 255u;
        };
        state.lx = axis(isDown(KEY_A), isDown(KEY_D));
        state.ly = axis(isDown(KEY_W), isDown(KEY_S));
        if (isDown(KEY_X) || isDown(KEY_SPACE)) press(0x4000u);
        if (isDown(KEY_C) || isDown(KEY_ESCAPE)) press(0x2000u);
        if (isDown(KEY_Z) || isDown(KEY_KP_0)) press(0x8000u);
        if (isDown(KEY_V) || isDown(KEY_KP_1)) press(0x1000u);
        if (isDown(KEY_Q)) press(0x0400u);
        if (isDown(KEY_E)) press(0x0800u);
        if (isDown(KEY_LEFT_SHIFT)) press(0x0100u);
        if (isDown(KEY_RIGHT_SHIFT)) press(0x0200u);
        if (isDown(KEY_ENTER)) press(0x0008u);
        if (isDown(KEY_TAB)) press(0x0001u);
        return state;
    }
}
