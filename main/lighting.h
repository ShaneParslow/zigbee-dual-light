#pragma once

enum channel {
    MAIN_COOL = 0,
    MAIN_WARM,
    AUX_W,
    AUX_R,
    AUX_G,
    AUX_B,
    NUM_LIGHTS,
};

int lighting_init();
