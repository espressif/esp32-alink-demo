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

#ifndef ALINK_PASSTHROUGH
static const char *TAG = "app_main";
SemaphoreHandle_t xSemWrite = NULL;

/*do your job here*/
struct virtual_dev {
    char power;
    char temp_value;
    char light_value;
    char time_delay;
    char work_mode;
} virtual_device = {
    0x01, 0x30, 0x50, 0, 0x01
};

static char *device_attr[5] = { "OnOff_Power", "Color_Temperature", "Light_Brightness",
                                "TimeDelay_PowerOff", "WorkMode_MasterLight"
                              };

const char *main_dev_params =
    "{\"OnOff_Power\": { \"value\": \"%d\" }, \"Color_Temperature\": { \"value\": \"%d\" }, \"Light_Brightness\": { \"value\": \"%d\" }, \"TimeDelay_PowerOff\": { \"value\": \"%d\"}, \"WorkMode_MasterLight\": { \"value\": \"%d\"}}";

static xQueueHandle xQueueDownCmd = NULL;
void read_task_test(void *arg)
{
    char *down_cmd = (char *)malloc(ALINK_DATA_LEN);
    for (;;) {
        esp_read(down_cmd, ALINK_DATA_LEN, portMAX_DELAY);

        int attrLen = 0, valueLen = 0, value = 0, i = 0;
        char *valueStr = NULL, *attrStr = NULL;
        for (i = 0; i < 5; i++) {
            attrStr = alink_JsonGetValueByName(down_cmd, strlen(down_cmd), device_attr[i], &attrLen, 0);
            valueStr = alink_JsonGetValueByName(attrStr, attrLen, "value", &valueLen, 0);

            if (valueStr && valueLen > 0) {
                char lastChar = *(valueStr + valueLen);
                *(valueStr + valueLen) = 0;
                value = atoi(valueStr);
                *(valueStr + valueLen) = lastChar;
                switch (i) {
                case 0:
                    virtual_device.power = value;
                    break;
                case 1:
                    virtual_device.temp_value = value;
                    break;
                case 2:
                    virtual_device.light_value = value;
                    break;
                case 3:
                    virtual_device.time_delay = value;
                    break;
                case 4:
                    virtual_device.work_mode = value;
                    break;
                default:
                    break;
                }
            }
        }
        xSemaphoreGive(xSemWrite);
    }
    free(down_cmd);
    vTaskDelete(NULL);
}

void write_task_test(void *arg)
{
    char *up_cmd = (char *)malloc(ALINK_DATA_LEN);
    for (;;) {
        xSemaphoreTake(xSemWrite, portMAX_DELAY);
        memset(up_cmd, 0, ALINK_DATA_LEN);
        sprintf(up_cmd, main_dev_params, virtual_device.power,
                virtual_device.temp_value, virtual_device.light_value,
                virtual_device.time_delay, virtual_device.work_mode);
        esp_write(up_cmd, ALINK_DATA_LEN, 500 / portTICK_PERIOD_MS);
        // platform_msleep(500);
    }
    free(up_cmd);
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
    ALINK_LOGI("mode: json, free_heap: %u\n", esp_get_free_heap_size());
    nvs_flash_init();
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(NULL, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    struct device_info product_info = {
        .sn             = "12345678",
        .name           = "ALINKTEST",
        .type           = "LIGHT",
        .category       = "LIVING",
        .manufacturer   = "ALINKTEST",
        .version        = "1.0.0",
        .model          = "ALINKTEST_LIVING_LIGHT_SMARTLED",
        .cid            = "2D0044000F47333139373038",
        .key            = "ljB6vqoLzmP8fGkE6pon",
        .secret         = "YJJZjytOCXDhtQqip4EjWbhR95zTgI92RVjzjyZF",
        .key_sandbox    = "dpZZEpm9eBfqzK7yVeLq",
        .secret_sandbox = "THnfRRsU5vu6g6m9X6uFyAjUWflgZ0iyGjdEneKm",
    };

    esp_alink_init(&product_info);
    ALINK_LOGI("esp32 alink version: %s", product_info.version);
    if(xSemWrite == NULL) xSemWrite = xSemaphoreCreateBinary();
    xTaskCreate(read_task_test, "read_task_test", 1024 * 8, NULL, 9, NULL);
    xTaskCreate(write_task_test, "write_task_test", 1024 * 8, NULL, 4, NULL);
    ALINK_LOGI("free_heap3:%u\n", esp_get_free_heap_size());
}
#endif
