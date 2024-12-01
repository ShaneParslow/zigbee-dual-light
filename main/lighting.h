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

/* Matrix constants for XYZ to rgb */
// https://en.wikipedia.org/wiki/CIE_1931_color_space#Construction_of_the_CIE_XYZ_color_space_from_the_Wright%E2%80%93Guild_data
#define M11  3.2404542
#define M12 -1.5371385
#define M13 -0.4985314
#define M21 -0.9692660
#define M22  1.8760108
#define M23  0.0415560
#define M31  0.0556534
#define M32 -0.2040259
#define M33  1.0572252

void set_white_on_off(const esp_zb_zcl_set_attr_value_message_t *message);
void set_rgbw_on_off(const esp_zb_zcl_set_attr_value_message_t *message);
void set_rgbw_level(const esp_zb_zcl_set_attr_value_message_t *message);
void set_rgbw_x(const esp_zb_zcl_set_attr_value_message_t *message);
void set_rgbw_y(const esp_zb_zcl_set_attr_value_message_t *message);
int lighting_init();
