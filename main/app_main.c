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
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "esp_alink.h"
#include "alink_product.h"
#include "cJSON.h"
#include "esp_json_parser.h"
#include "alink_export.h"

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

static dev_info_t g_light_info = {
    .errorcode = 0x00,
    .hue       = 0x10,
    .luminance = 0x50,
    .power     = 0x01,
    .work_mode = 0x02,
};
/**
 * @brief  In order to simplify the analysis of json package operations,
 * use the package alink_json_parse, you can also use the standard cJson data analysis
 */
static alink_err_t device_data_parse(const char *json_str, const char *key, uint8_t *value)
{
    char sub_str[64] = {0};
    char value_tmp[8] = {0};

    if (esp_json_parse(json_str, key, sub_str) < 0) {
        return ALINK_ERR;
    }

    if (esp_json_parse(sub_str, "value", value_tmp) < 0) {
        return ALINK_ERR;
    }

    *value = atoi(value_tmp);
    return ALINK_ERR;
}

static alink_err_t device_data_pack(const char *json_str, const char *key, int value)
{
    char sub_str[64] = {0};

    if (esp_json_pack(sub_str, "value", value) < 0) {
        return ALINK_ERR;
    }

    if (esp_json_pack(json_str, key, sub_str) < 0) {
        return ALINK_ERR;
    }

    return ALINK_OK;
}

/**
 * @brief When the service received errno a jump that is complete activation,
 *        activation of the order need to modify the specific equipment
 */
static alink_err_t alink_activate_device()
{
    alink_err_t ret = 0;
    const char *activate_data = NULL;

    activate_data = "{\"ErrorCode\": { \"value\": \"1\" }}";
    ret = alink_write(activate_data, strlen(activate_data) + 1, 200);
    ALINK_ERROR_CHECK(ret < 0, ALINK_ERR, "alink_write, ret: %d", ret);

    activate_data = "{\"ErrorCode\": { \"value\": \"0\" }}";
    ret = alink_write(activate_data, strlen(activate_data) + 1, 200);
    ALINK_ERROR_CHECK(ret < 0, ALINK_ERR, "alink_write, ret: %d", ret);

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

    device_data_pack(up_cmd, "Hue", g_light_info.hue);
    device_data_pack(up_cmd, "Luminance", g_light_info.luminance);
    device_data_pack(up_cmd, "Switch", g_light_info.power);
    device_data_pack(up_cmd, "WorkMode", g_light_info.work_mode);
    ret = alink_write(up_cmd, strlen(up_cmd) + 1, 500);
    free(up_cmd);

    if (ret < 0) {
        ALINK_LOGW("alink_write is err");
    }

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

    for (;;) {
        if (alink_read(down_cmd, ALINK_DATA_LEN, portMAX_DELAY) < 0) {
            ALINK_LOGW("alink_read is err");
            continue;
        }

        char method_str[32] = {0};
        if (esp_json_parse(down_cmd, "method", method_str) < 0) {
            ALINK_LOGW("alink_json_parse, is err");
            continue;
        }

        if (!strcmp(method_str, "getDeviceStatus")) {
            proactive_report_data();
            continue;
        } else if (!strcmp(method_str, "setDeviceStatus")) {
            ALINK_LOGV("setDeviceStatus: %s", down_cmd);
            device_data_parse(down_cmd, "ErrorCode", &(g_light_info.errorcode));
            device_data_parse(down_cmd, "Hue", &(g_light_info.hue));
            device_data_parse(down_cmd, "Luminance", &(g_light_info.luminance));
            device_data_parse(down_cmd, "Switch", &(g_light_info.power));
            device_data_parse(down_cmd, "WorkMode", &(g_light_info.work_mode));
            ALINK_LOGI("read: errorcode:%d, hue: %d, luminance: %d, Switch: %d, work_mode: %d",
                       g_light_info.errorcode, g_light_info.hue, g_light_info.luminance,
                       g_light_info.power, g_light_info.work_mode);

            /* write data is not necessary */
            if (alink_write(down_cmd, strlen(down_cmd) + 1, 0) < 0) {
                ALINK_LOGW("alink_write is err");
            }
        }
    }

    free(down_cmd);
    vTaskDelete(NULL);
}

static alink_err_t alink_event_handler(alink_event_t event)
{
    switch (event) {
    case ALINK_EVENT_CLOUD_CONNECTED:
        ALINK_LOGD("Alink cloud connected!");
        proactive_report_data();
        break;

    case ALINK_EVENT_CLOUD_DISCONNECTED:
        ALINK_LOGD("Alink cloud disconnected!");
        break;

    case ALINK_EVENT_GET_DEVICE_DATA:
        ALINK_LOGD("The cloud initiates a query to the device");
        break;

    case ALINK_EVENT_SET_DEVICE_DATA:
        ALINK_LOGD("The cloud is set to send instructions");
        break;

    case ALINK_EVENT_POST_CLOUD_DATA:
        ALINK_LOGD("The device post data success!");
        break;

    case ALINK_EVENT_WIFI_DISCONNECTED:
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
        ALINK_LOGI("free heap size: %d", esp_get_free_heap_size());
        vTaskDelay(5000 / portTICK_RATE_MS);
    }

    vTaskDelete(NULL);
}

/**
 * @brief Too much serial print information will not be able to pass high-frequency
 *        send and receive data test
 *
 * @Note When GPIO2 is connected to 3V3 and restarts the device, the log level will be modified
 */
void reduce_serial_print()
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,  // interrupt of rising edge
        .pin_bit_mask = 1 << GPIO_NUM_2,  // bit mask of the pins, use GPIO2 here
        .mode = GPIO_MODE_INPUT,  // set as input mode
        .pull_up_en = GPIO_PULLUP_ENABLE,  // enable pull-up mode
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    if (!gpio_get_level(GPIO_NUM_2)) {
        ALINK_LOGI("*********************************");
        ALINK_LOGI("*       SET LOGLEVEL INFO       *");
        ALINK_LOGI("*********************************");
        alink_set_loglevel(ALINK_LL_INFO);
    }
}


/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main()
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ALINK_LOGI("================= SYSTEM INFO ================");
    ALINK_LOGI("compile time     : %s %s", __DATE__, __TIME__);
    ALINK_LOGI("free heap        : %dB", esp_get_free_heap_size());
    ALINK_LOGI("idf version      : %s", esp_get_idf_version());
    ALINK_LOGI("CPU cores        : %d", chip_info.cores);
    ALINK_LOGI("chip name        : %s", ALINK_CHIPID);
    ALINK_LOGI("modle name       : %s", ALINK_MODULE_NAME);
    ALINK_LOGI("function         : WiFi%s%s",
               (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
               (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    ALINK_LOGI("silicon revision : %d", chip_info.revision);
    ALINK_LOGI("flash            : %dMB %s", spi_flash_get_chip_size() / (1024 * 1024),
               (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

#ifdef CONFIG_ALINK_VERSION_SDS
    ALINK_LOGI("alink version    : esp32-alink_sds\n");
#else
    ALINK_LOGI("alink version    : esp32-alink_embed\n");
#endif

    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(NULL, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    /**
     * @brief You can use other trigger mode, to trigger the distribution network, activation and other operations
     */
    extern void alink_key_trigger(void *arg);
    xTaskCreate(alink_key_trigger, "alink_key_trigger", 1024 * 2, NULL, 10, NULL);

#ifdef CONFIG_ALINK_VERSION_SDS
    const alink_product_t product_info = {
        .name           = "alink_product",
        /*!< Product version number, ota upgrade need to be modified */
        .version        = "1.0.0",
        .model          = "OPENALINK_LIVING_LIGHT_SDS_TEST",
        /*!< The Key-value pair used in the product */
        .key            = "1L6ueddLqnRORAQ2sGOL",
        .secret         = "qfxCLoc1yXEk9aLdx5F74tl1pdxl0W0q7eYOvvuo",
        /*!< The Key-value pair used in the sandbox environment */
        .key_sandbox    = "",
        .secret_sandbox = "",
    };
#else
    const alink_product_t product_info = {
        .name           = "alink_product",
        /*!< Product version number, ota upgrade need to be modified */
        .version        = "1.0.0",
        .model          = "ALINKTEST_LIVING_LIGHT_ALINK_TEST",
        /*!< The Key-value pair used in the product */
        .key            = "5gPFl8G4GyFZ1fPWk20m",
        .secret         = "ngthgTlZ65bX5LpViKIWNsDPhOf2As9ChnoL9gQb",
        /*!< The Key-value pair used in the sandbox environment */
        .key_sandbox    = "dpZZEpm9eBfqzK7yVeLq",
        .secret_sandbox = "THnfRRsU5vu6g6m9X6uFyAjUWflgZ0iyGjdEneKm",
    };
#endif

    ESP_ERROR_CHECK(alink_init(&product_info, alink_event_handler));
    reduce_serial_print();
    xTaskCreate(read_task_test, "read_task_test", 1024 * 4, NULL, 9, NULL);
    xTaskCreate(free_heap_task, "free_heap_task", 1024 * 2, NULL, 3, NULL);
}
#endif
