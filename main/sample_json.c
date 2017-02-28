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

#ifndef ALINK_PASSTHROUGH
static const char *TAG = "app_main";
SemaphoreHandle_t xSemWriteInfo = NULL;

/*do your job here*/
typedef struct  virtual_dev {
    char power;
    char temp_value;
    char light_value;
    char time_delay;
    char work_mode;
} dev_info_t;

static dev_info_t light_info = {
    .power       = 0x01,
    .work_mode   = 0x30,
    .temp_value  = 0x50,
    .light_value = 0,
    .time_delay  = 0x01,
};

static char *device_attr[5] = { "OnOff_Power", "Color_Temperature", "Light_Brightness",
                                "TimeDelay_PowerOff", "WorkMode_MasterLight"
                              };

const char *main_dev_params =
    "{\"OnOff_Power\": { \"value\": \"%d\" }, \"Color_Temperature\": { \"value\": \"%d\" }, \"Light_Brightness\": { \"value\": \"%d\" }, \"TimeDelay_PowerOff\": { \"value\": \"%d\"}, \"WorkMode_MasterLight\": { \"value\": \"%d\"}}";

void read_task_test(void *arg)
{
    char *down_cmd = (char *)malloc(ALINK_DATA_LEN);
    alink_err_t ret = ALINK_ERR;
    for (;;) {
        ret = esp_alink_read(down_cmd, ALINK_DATA_LEN, portMAX_DELAY);
        if (ret == ALINK_ERR) {
            ALINK_LOGW("esp_alink_read is err");
            continue;
        }

        int attrLen = 0, valueLen = 0, value = 0, i = 0;
        char *valueStr = NULL, *attrStr = NULL;
        for (i = 0; i < 5; i++) {
            attrStr = json_get_value_by_name(down_cmd, strlen(down_cmd), device_attr[i], &attrLen, 0);
            valueStr = json_get_value_by_name(attrStr, attrLen, "value", &valueLen, 0);

            if (valueStr && valueLen > 0) {
                char lastChar = *(valueStr + valueLen);
                *(valueStr + valueLen) = 0;
                value = atoi(valueStr);
                *(valueStr + valueLen) = lastChar;
                switch (i) {
                case 0:
                    light_info.power = value;
                    break;
                case 1:
                    light_info.temp_value = value;
                    break;
                case 2:
                    light_info.light_value = value;
                    break;
                case 3:
                    light_info.time_delay = value;
                    break;
                case 4:
                    light_info.work_mode = value;
                    break;
                default:
                    break;
                }
            }
        }
        ALINK_LOGD("read: power:%d, temp_value: %d, light_value: %d, time_delay: %d, work_mode: %d",
                   light_info.power, light_info.temp_value, light_info.light_value, light_info.time_delay, light_info.work_mode);
        xSemaphoreGive(xSemWriteInfo);
    }
    free(down_cmd);
    vTaskDelete(NULL);
}

void write_task_test(void *arg)
{
    alink_err_t ret = ALINK_ERR;
    char *up_cmd = (char *)malloc(ALINK_DATA_LEN);
    for (;;) {
        xSemaphoreTake(xSemWriteInfo, portMAX_DELAY);
        memset(up_cmd, 0, ALINK_DATA_LEN);
        sprintf(up_cmd, main_dev_params, light_info.power,
                light_info.temp_value, light_info.light_value,
                light_info.time_delay, light_info.work_mode);
        ret = esp_alink_write(up_cmd, ALINK_DATA_LEN, 500 / portTICK_PERIOD_MS);
        if (ret == ALINK_ERR) ALINK_LOGW("esp_alink_write is err");
        platform_msleep(500);
    }
    free(up_cmd);
    vTaskDelete(NULL);
}

alink_err_t alink_event_handler(alink_event_t event)
{
    switch (event) {
    case ALINK_EVENT_CLOUD_CONNECTED:
        ALINK_LOGD("alink cloud connected!");
        if (xSemWriteInfo == NULL)
            xSemWriteInfo = xSemaphoreCreateBinary();
        xSemaphoreGive(xSemWriteInfo);
        break;
    case ALINK_EVENT_CLOUD_DISCONNECTED:
        ALINK_LOGD("alink cloud disconnected!");
        break;
    case ALINK_EVENT_GET_DEVICE_DATA:
        ALINK_LOGD("The cloud initiates a query to the device");
        break;
    case ALINK_EVENT_SET_DEVICE_DATA:
        ALINK_LOGD("The cloud is set to send instructions");
        break;
    case ALINK_EVENT_POST_CLOUD_DATA:
        ALINK_LOGD("the device post data success!");
        break;
    default:
        break;
    }
    return ALINK_OK;
}

static void free_heap_task(void *arg)
{
    for (;;) {
        ALINK_LOGD("free heap size: %d", esp_get_free_heap_size());
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
    ALINK_LOGI("free_heap :%u\n", esp_get_free_heap_size());
    nvs_flash_init();
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(NULL, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    if (xSemWriteInfo == NULL) xSemWriteInfo = xSemaphoreCreateBinary();
    alink_product_t product_info = {
        .sn             = "12345678",
        .name           = "ALINKTEST",
        .version        = "1.0.0",
        .model          = "ALINKTEST_LIVING_LIGHT_SMARTLED",
        .key            = "ljB6vqoLzmP8fGkE6pon",
        .secret         = "YJJZjytOCXDhtQqip4EjWbhR95zTgI92RVjzjyZF",
        .key_sandbox    = "dpZZEpm9eBfqzK7yVeLq",
        .secret_sandbox = "THnfRRsU5vu6g6m9X6uFyAjUWflgZ0iyGjdEneKm",
    };
    ESP_ERROR_CHECK( esp_alink_event_init(alink_event_handler) );
    ESP_ERROR_CHECK( esp_alink_init(&product_info) );
    xTaskCreate(read_task_test, "read_task_test", 1024 * 8, NULL, 9, NULL);
    xTaskCreate(write_task_test, "write_task_test", 1024 * 8, NULL, 4, NULL);
    xTaskCreate(free_heap_task, "free_heap_task", 1024 * 8, NULL, 3, NULL);
}
#endif
