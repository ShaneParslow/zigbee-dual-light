#include "ha/esp_zigbee_ha_standard.h"
#include "driver/ledc.h"

#include "lighting.h"

static const char *TAG = "ZB_DUAL_LIGHT_LIGHTING_C";

const static gpio_num_t pin_map[NUM_LIGHTS] = {
    [MAIN_COOL] = GPIO_NUM_3,
    [MAIN_WARM] = GPIO_NUM_11,
    [AUX_W]     = GPIO_NUM_1,
    [AUX_R]     = GPIO_NUM_7,
    [AUX_G]     = GPIO_NUM_5,
    [AUX_B]     = GPIO_NUM_4,
};

/* Save the luminance/level of the lights. The color/temp needs to be scaled by this, which is a todo */
static uint8_t white_level  = 0xfe;
static uint8_t rgbw_level   = 0xfe;

/* Saved chromaticity */
static uint16_t white_temp = 0; // mireds
static uint16_t rgbw_x = ESP_ZB_ZCL_COLOR_CONTROL_CURRENT_X_DEF_VALUE;
static uint16_t rgbw_y = ESP_ZB_ZCL_COLOR_CONTROL_CURRENT_Y_DEF_VALUE;

static void timer_init()
{
    ledc_timer_config_t pwm_conf = {};
	
	pwm_conf.speed_mode = LEDC_LOW_SPEED_MODE;
	pwm_conf.timer_num = 0;
	pwm_conf.freq_hz = 300000;
	pwm_conf.duty_resolution = LEDC_TIMER_8_BIT;
	pwm_conf.clk_cfg = LEDC_USE_PLL_DIV_CLK;

    ledc_timer_config(&pwm_conf);
}

static void channel_init(enum channel chnl)
{
    ledc_channel_config_t chnl_conf = {};

	chnl_conf.gpio_num = pin_map[chnl];
	chnl_conf.channel = chnl;
	chnl_conf.timer_sel = 0;
	chnl_conf.duty = 32; // A nice dim value to show that we've started but nothing has actually set the level

	ledc_channel_config(&chnl_conf);
}

static void all_channels_init()
{
    for (enum channel chnl = 0; chnl < NUM_LIGHTS; chnl++) {
        channel_init(chnl);
    }
}

void white_set_power(uint8_t state)
{
    for (enum channel chnl = MAIN_COOL; chnl <= MAIN_WARM; chnl++) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, chnl, state);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, chnl);
    }
}

void rgbw_set_power(uint8_t state)
{
    for (enum channel chnl = AUX_W; chnl <= AUX_B; chnl++) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, chnl, state);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, chnl);
    }
}

static void update_white()
{
    // Default min/max for the esp32 stack is 0x0000 and 0xfeff
    // For this i'll just fade between those two values
    // I don't really care if this is that accurate because i'll just tweak it in the controller automations
    uint8_t warm, cool;

    double ratio;

    ratio = (double)white_temp / (double)0xfeff;

    // Simple fade between warm and cool leds
    warm = ratio < 1 ? round(ratio * (double)white_level) : 255;
    cool = 255 - warm;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, MAIN_COOL, cool);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, MAIN_WARM, warm);

    ledc_update_duty(LEDC_LOW_SPEED_MODE, MAIN_COOL);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, MAIN_WARM);
}

void set_white_on_off(const esp_zb_zcl_set_attr_value_message_t *message)
{

    if(message->attribute.data.value && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
        bool on_off = *(bool *)message->attribute.data.value;
        if(on_off == false) {
            white_set_power(0);
        }
        else {
            update_white();
        }
        ESP_LOGI(TAG, "White set to %s", on_off ? "On" : "Off");
    }
    else {
        ESP_LOGI(TAG, "Invalid OnOff data and/or type");
    }
}

void set_white_level(const esp_zb_zcl_set_attr_value_message_t *message)
{
    if(message->attribute.data.value && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
        white_level = *(uint8_t*)message->attribute.data.value;
        update_white();
    }
    else {
        ESP_LOGI(TAG, "Invalid Level data and/or type");
    }
}

void set_white_temp(const esp_zb_zcl_set_attr_value_message_t *message)
{
    if(message->attribute.data.value && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
        white_temp = *(uint16_t*)message->attribute.data.value;
        update_white();
    }
    else {
        ESP_LOGI(TAG, "Invalid Color data and/or type");
    }
}

/* Set rgbw from saved values of x, y, and level. */
// current issue is that scaling of the level is a bit rough. it is wayyyy too bright at like 20% and only starts to really drop off below that
static void update_rgbw()
{
    // Floats are actually required because the xyY to XYZ has some terms that get quite close to 1 and I don't want to add in magic coefficients for fixed point arithmetic
    // Also the equation for xyY to XYZ is for normalized variables, but I'm pretty sure I could get past that by switching out the 1 with a UINT16_MAX.
    double x = (double)rgbw_x / (double)UINT16_MAX;
    double y = (double)rgbw_y / (double)UINT16_MAX;
    double Y = (double)rgbw_level / (double)UINT8_MAX;
    double X, Z;
    double r, g, b;
    uint8_t r_final, g_final, b_final, w_final;

    /* Conversion from xyY to XYZ. Mind the capital vs lowercase y's */
    /* Capital is luminance in both xyY and XYZ. Lower case is part of chromaticity in xyY */
    // https://en.wikipedia.org/wiki/CIE_1931_color_space#CIE_xy_chromaticity_diagram_and_the_CIE_xyY_color_space
    X = (x * Y) / y;
    Z = (Y / y) * (1.0 - x - y);

    /* XYZ to srgb matrix transform */
    // https://en.wikipedia.org/wiki/CIE_1931_color_space#Construction_of_the_CIE_XYZ_color_space_from_the_Wright%E2%80%93Guild_data
    // https://www.oceanopticsbook.info/view/photometry-and-visibility/from-xyz-to-rgb
    r = ((X * M11) + (Y * M12) + (Z * M13));
    g = ((X * M21) + (Y * M22) + (Z * M23));
    b = ((X * M31) + (Y * M32) + (Z * M33));

    ESP_LOGI(TAG, "xy: %f %f  XYZ: %f %f %f", x, y, X, Y, Z);

    r_final = r < 1 ? round(r * 255.0) : 255;
    g_final = g < 1 ? round(g * 255.0) : 255;
    b_final = b < 1 ? round(b * 255.0) : 255;

    /* Get the smallest of r/g/b, then subtract it from all components. */
    /* This might be an okay way to determine the value of w? */
    w_final = r_final;
    w_final = g_final < w_final ? g_final : w_final;
    w_final = b_final < w_final ? b_final : w_final;

    r_final -= w_final;
    g_final -= w_final;
    b_final -= w_final;
    
    ESP_LOGI(TAG, "RGBW: %hhu %hhu %hhu %hhu", r_final, g_final, b_final, w_final);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, AUX_W, w_final);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, AUX_R, r_final);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, AUX_G, g_final);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, AUX_B, b_final);

    ledc_update_duty(LEDC_LOW_SPEED_MODE, AUX_W);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, AUX_R);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, AUX_G);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, AUX_B);
}

void set_rgbw_on_off(const esp_zb_zcl_set_attr_value_message_t *message)
{

    if(message->attribute.data.value && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
        bool on_off = *(bool *)message->attribute.data.value;
        if(on_off == false) {
            rgbw_set_power(0);
        }
        else {
            update_rgbw();
        }
        ESP_LOGI(TAG, "RGBW set to %s", on_off ? "On" : "Off");
    }
    else {
        ESP_LOGI(TAG, "Invalid OnOff data and/or type");
    }
}

void set_rgbw_level(const esp_zb_zcl_set_attr_value_message_t *message)
{
    if(message->attribute.data.value && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
        rgbw_level = *(uint8_t*)message->attribute.data.value;
        update_rgbw();
    }
    else {
        ESP_LOGI(TAG, "Invalid Level data and/or type");
    }
}

void set_rgbw_x(const esp_zb_zcl_set_attr_value_message_t *message)
{
    if(message->attribute.data.value && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
        rgbw_x = *(uint16_t*)message->attribute.data.value;
        update_rgbw();
    }
    else {
        ESP_LOGI(TAG, "Invalid Color data and/or type");
    }
}

void set_rgbw_y(const esp_zb_zcl_set_attr_value_message_t *message)
{
    if(message->attribute.data.value && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
        rgbw_y = *(uint16_t*)message->attribute.data.value;
        update_rgbw();
    }
    else {
        ESP_LOGI(TAG, "Invalid Color data and/or type");
    }
}

int lighting_init()
{
    timer_init();
    all_channels_init();
    return 0;
}
