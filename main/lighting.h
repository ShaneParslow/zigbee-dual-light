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

void white_light_set_power(bool state);
void rgbw_light_set_power(bool state);
int lighting_init();
