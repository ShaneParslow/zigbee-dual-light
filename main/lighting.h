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

void white_set_power(unsigned char state);
void rgbw_set_power(unsigned char state);
void set_rgbw_on_off(const esp_zb_zcl_set_attr_value_message_t *message);
int lighting_init();
