/******************************************************************************
 * Copyright (C) 2014 -2016  Espressif System
 *
 * FileName: app_main.c
 *
 * Description:
 *
 * Modification history:
 * 2016/11/16, v0.0.1 create this file.
*******************************************************************************/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_system.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "esp_alink.h"
#include "product.h"
#include "cJSON.h"
#include "alink_json_parser.h"

#ifndef ALINK_PASSTHROUGH
static const char *TAG = "sample_json";

/*do your job here*/
typedef struct  virtual_dev {
    uint8_t errorcode;
    uint8_t hue;
    uint8_t luminance;
    uint8_t power;
    uint8_t work_mode;
} dev_info_t;

/**
 * @brief  In order to simplify the analysis of json package operations,
 * use the package alink_json_parse, you can also use the standard cJson data analysis
 */
static alink_err_t device_data_parse(const char *json_str, const char *key, uint8_t *value)
{
    alink_err_t ret = 0;
    char sub_str[64] = {0};
    char value_tmp[8] = {0};

    ret = alink_json_parse(json_str, key, sub_str);
    if (ret < 0) return ALINK_ERR;
    ret = alink_json_parse(sub_str, "value", value_tmp);
    if (ret < 0) return ALINK_ERR;

    *value = atoi(value_tmp);
    return ALINK_ERR;
}

static alink_err_t device_data_pack(const char *json_str, const char *key, int value)
{
    char sub_str[64] = {0};
    alink_err_t ret = 0;

    ret = alink_json_pack(sub_str, "value", value);
    if (ret < 0) return ALINK_ERR;
    ret = alink_json_pack(json_str, key, sub_str);
    if (ret < 0) return ALINK_ERR;
    return ALINK_OK;
}

/**
 * @brief When the service received errno a jump that is complete activation,
 *        activation of the order need to modify the specific equipment
 */
static const char *activate_data = "{\"ErrorCode\": { \"value\": \"1\" }}";
static const char *activate_data2 = "{\"ErrorCode\": { \"value\": \"0\" }}";
static alink_err_t alink_activate_device()
{
    alink_err_t ret = 0;
    ret = alink_write(activate_data, ALINK_DATA_LEN, 500);
    ret = alink_write(activate_data2, ALINK_DATA_LEN, 500);
    if (ret < 0) ALINK_LOGW("alink_write is err");
    return ALINK_OK;
}

/**
 * @brief  The alink protocol specifies that the state of the device must be
 *         proactively attached to the Ali server
 */
static alink_err_t proactive_report_data()
{
    alink_err_t ret = 0;
    char *up_cmd = (char *)calloc(1, ALINK_DATA_LEN);
    dev_info_t light_info = {
        .errorcode = 0x00,
        .hue       = 0x10,
        .luminance = 0x50,
        .power     = 0x01,
        .work_mode = 0x02,
    };
    // device_data_pack(up_cmd, "ErrorCode", light_info.errorcode);
    device_data_pack(up_cmd, "Hue", light_info.hue);
    device_data_pack(up_cmd, "Luminance", light_info.luminance);
    device_data_pack(up_cmd, "Switch", light_info.power);
    device_data_pack(up_cmd, "WorkMode", light_info.work_mode);
    ret = alink_write(up_cmd, ALINK_DATA_LEN, 500);
    free(up_cmd);
    if (ret < 0) ALINK_LOGW("alink_write is err");
    return ALINK_OK;
}

/*
 * getDeviceStatus: {"attrSet":[],"uuid":"7DD5CE4ECE654B721BE8F4F912C10B8E"}
 * postDeviceData: {"Luminance":{"value":"80"},"Switch":{"value":"1"},"attrSet":["Luminance","Switch","Hue","ErrorCode","WorkMode","onlineState"],"Hue":{"value":"16"},"ErrorCode":{"value":"0"},"uuid":"158EE04889E2B1FE4BF18AFE4BFD0F04","WorkMode":{"value":"2"},"onlineState":{"when":"1495184488","value":"on"}
 *                 {"Switch":{"value":"1"},"attrSet":["Switch"],"uuid":"158EE04889E2B1FE4BF18AFE4BFD0F04"}
 *
 * @note  read_task_test stack space is small, need to follow the specific
 *        application to increase the size of the stack
 */
static void read_task_test(void *arg)
{
    char *down_cmd = (char *)malloc(ALINK_DATA_LEN);
    alink_err_t ret = ALINK_ERR;
    dev_info_t light_info;

    for (;;) {
        ret = alink_read(down_cmd, ALINK_DATA_LEN, portMAX_DELAY);
        if (ret < 0) {
            ALINK_LOGW("alink_read is err");
            continue;
        }

        char method_str[32] = {0};
        ret = alink_json_parse(down_cmd, "method", method_str);
        if (ret < 0) {
            ALINK_LOGW("alink_json_parse, ret: %d", ret);
            continue;
        }

        if (!strcmp(method_str, "getDeviceStatus")) {
            proactive_report_data();
            continue;
        } else if (!strcmp(method_str, "setDeviceStatus")) {
            // ALINK_LOGD("setDeviceStatus: %s", down_cmd);
            device_data_parse(down_cmd, "ErrorCode", &(light_info.errorcode));
            device_data_parse(down_cmd, "Hue", &(light_info.hue));
            device_data_parse(down_cmd, "Luminance", &(light_info.luminance));
            device_data_parse(down_cmd, "Switch", &(light_info.power));
            device_data_parse(down_cmd, "WorkMode", &(light_info.work_mode));
            ALINK_LOGI("read: errorcode:%d, hue: %d, luminance: %d, Switch: %d, work_mode: %d",
                       light_info.errorcode, light_info.hue, light_info.luminance, light_info.power, light_info.work_mode);

            /* write data is not necessary */
            ret = alink_write(down_cmd, ALINK_DATA_LEN, 0);
            if (ret < 0) ALINK_LOGW("alink_write is err");
        }
    }
    free(down_cmd);
    vTaskDelete(NULL);
}

static int count = 0; /*!< Count the number of packets received */
static alink_err_t alink_event_handler(alink_event_t event)
{
    switch (event) {
    case ALINK_EVENT_CLOUD_CONNECTED:
        ALINK_LOGD("Alink cloud connected!");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        proactive_report_data();
        break;
    case ALINK_EVENT_CLOUD_DISCONNECTED:
        ALINK_LOGD("Alink cloud disconnected!");
        break;
    case ALINK_EVENT_GET_DEVICE_DATA:
        ALINK_LOGD("The cloud initiates a query to the device");
        break;
    case ALINK_EVENT_SET_DEVICE_DATA:
        count++;
        ALINK_LOGD("The cloud is set to send instructions");
        break;
    case ALINK_EVENT_POST_CLOUD_DATA:
        ALINK_LOGD("The device post data success!");
        break;
    case ALINK_EVENT_STA_DISCONNECTED:
        ALINK_LOGD("Wifi disconnected");
        break;
    case ALINK_EVENT_CONFIG_NETWORK:
        ALINK_LOGD("Enter the network configuration mode");
        break;
    case ALINK_EVENT_UPDATE_ROUTER:
        ALINK_LOGD("Requests update router");
        alink_update_router();
        break;
    case ALINK_EVENT_FACTORY_RESET:
        ALINK_LOGD("Requests factory reset");
        alink_factory_setting();
        break;
    case ALINK_EVENT_ACTIVATE_DEVICE:
        ALINK_LOGD("Requests activate device");
        alink_activate_device();
        break;

    default:
        break;
    }
    return ALINK_OK;
}

/**
 * @brief This function is only for detecting memory leaks
 */
static void free_heap_task(void *arg)
{
    for (;;) {
        ALINK_LOGI("free heap size: %d, count: %d", esp_get_free_heap_size(), count);
        vTaskDelay(5000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}


/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main()
{
    ALINK_LOGI("alink embed v1.0.0 free_heap :%u\n", esp_get_free_heap_size());
    nvs_flash_init();
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(NULL, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    /**
     * @brief You can use other trigger mode, to trigger the distribution network, activation and other operations
     */
    xTaskCreate(alink_key_trigger, "alink_key_trigger", 1024 * 2, NULL, 10, NULL);

    alink_product_t product_info = {
        .name           = "alink_product",
        .version        = "1.0.1",
        .model          = "ALINKTEST_LIVING_LIGHT_ALINK_TEST",
        .key            = "5gPFl8G4GyFZ1fPWk20m",
        .secret         = "ngthgTlZ65bX5LpViKIWNsDPhOf2As9ChnoL9gQb",
        .key_sandbox    = "dpZZEpm9eBfqzK7yVeLq",
        .secret_sandbox = "THnfRRsU5vu6g6m9X6uFyAjUWflgZ0iyGjdEneKm",
    };

    ESP_ERROR_CHECK( alink_init(&product_info, alink_event_handler) );
    xTaskCreate(read_task_test, "read_task_test", 1024 * 2, NULL, 9, NULL);
    xTaskCreate(free_heap_task, "free_heap_task", 1024 * 2, NULL, 3, NULL);
}
#endif
