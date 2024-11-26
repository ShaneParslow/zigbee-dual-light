/* Basic cluster information */
#define ESP_MANUFACTURER_NAME "\x09""ESPRESSIF"      /* Customized manufacturer name */
#define ESP_MODEL_IDENTIFIER "\x07"CONFIG_IDF_TARGET /* Customized model identifier */

/* Arbitrary endpoint ids*/
#define WHITE_LIGHT_ENDPOINT              1
#define RGBW_LIGHT_ENDPOINT               2

/* Stuff not included in the esp zcl defs */
#define COLOR_TEMP_SUPPORTED    1 << 4
#define COLOR_MODE_MIREDS       0x02

/* White light endpoint config (id, profile, device)*/
esp_zb_endpoint_config_t white_ep_cfg =
{
    .endpoint = WHITE_LIGHT_ENDPOINT,
    // This is a legacy property? Maybe? HA should be fine....
    // https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/api-reference/zcl/esp_zigbee_zcl_common.html#_CPPv422esp_zb_af_profile_id_t
    // https://www.silabs.com/documents/public/application-notes/an1117-migrating-zigbee-profiles-to-z30.pdf
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    // Maybe also legacy? Not sure.
    // https://github.com/espressif/esp-zigbee-sdk/blob/c4c46289ffd69dd87faca9d34923201f2e93b66e/examples/esp_zigbee_host/components/include/zcl/esp_zigbee_zcl_common.h#L37
    // Ikea light uses 0x010d, which isn't listed in the sdk
    // ... But is listed in the spec. Esp sdk seems kinda Old And Shit, unfortunately.
    // https://zigbeealliance.org/wp-content/uploads/2019/11/docs-15-0014-05-0plo-Lighting-OccupancyDevice-Specification-V1.0.pdf
    // Anyways, this is a color temp light.
    // Req. Basic, Identify, Groups, Scenes, On/Off, Level Control, Color Control.
    .app_device_id = 0x010c,
};

/* White light cluster config */
// TODO: verify
esp_zb_color_dimmable_light_cfg_t white_light_cfg =
{
    .basic_cfg =
        {
            .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
            .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
        }, 
    .identify_cfg =
        {
            .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
        },
    .groups_cfg =
        {
            .groups_name_support_id = ESP_ZB_ZCL_GROUPS_NAME_SUPPORT_DEFAULT_VALUE,
        },
    .scenes_cfg =
        {
            .scenes_count = ESP_ZB_ZCL_SCENES_SCENE_COUNT_DEFAULT_VALUE,
            .current_scene = ESP_ZB_ZCL_SCENES_CURRENT_SCENE_DEFAULT_VALUE,
            .current_group = ESP_ZB_ZCL_SCENES_CURRENT_GROUP_DEFAULT_VALUE,
            .scene_valid = ESP_ZB_ZCL_SCENES_SCENE_VALID_DEFAULT_VALUE,
            .name_support = ESP_ZB_ZCL_SCENES_NAME_SUPPORT_DEFAULT_VALUE,
        },
    .on_off_cfg =
        {
            .on_off = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE,
        },
    .level_cfg =
        {
            .current_level = ESP_ZB_ZCL_LEVEL_CONTROL_CURRENT_LEVEL_DEFAULT_VALUE,
        },
    .color_cfg =
        {
            .current_x = ESP_ZB_ZCL_COLOR_CONTROL_CURRENT_X_DEF_VALUE,
            .current_y = ESP_ZB_ZCL_COLOR_CONTROL_CURRENT_Y_DEF_VALUE,
            .color_mode = COLOR_MODE_MIREDS,
            .options = ESP_ZB_ZCL_COLOR_CONTROL_OPTIONS_DEFAULT_VALUE,
            .enhanced_color_mode = COLOR_MODE_MIREDS,
            .color_capabilities = COLOR_TEMP_SUPPORTED,
        },
};