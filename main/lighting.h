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
// The actual values listed there would overflow, so just chop off the last 3 digits
// (longs are 32 bits, the largest possible number would be a ~65k int16 * ~8M matrix constant = ~512B)
#define M11  8042
#define M12 -3049
#define M13 -1592
#define M21 -1752
#define M22  4851
#define M23  302
#define M31  18
#define M32 -49
#define M33  3432
#define MDIV 3401

void set_white_on_off(const esp_zb_zcl_set_attr_value_message_t *message);
void set_rgbw_on_off(const esp_zb_zcl_set_attr_value_message_t *message);
void set_rgbw_level(const esp_zb_zcl_set_attr_value_message_t *message);
void set_rgbw_x(const esp_zb_zcl_set_attr_value_message_t *message);
void set_rgbw_y(const esp_zb_zcl_set_attr_value_message_t *message);
int lighting_init();
