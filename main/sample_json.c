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
static const char *TAG = "sample_json";
SemaphoreHandle_t xSemWriteInfo = NULL;

/*do your job here*/
typedef struct  virtual_dev {
    char errorcode;
    char hue;
    char luminance;
    char power;
    char work_mode;
} dev_info_t;

static dev_info_t light_info = {
    .errorcode = 0x00,
    .hue       = 0x10,
    .luminance = 0x50,
    .power     = 0x01,
    .work_mode = 0x02,
};

const char *device_attr[] = {
    "ErrorCode",
    "Hue",
    "Luminance",
    "Switch",
    "WorkMode",
    NULL
};

const char *main_dev_params =
    "{\"ErrorCode\":{\"value\":\"%d\"},\"Hue\":{\"value\":\"%d\"},\"Luminance\":{\"value\":\"%d\"},\"Switch\":{\"value\":\"%d\"},\"WorkMode\":{\"value\":\"%d\"}}";

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
                    light_info.errorcode = value;
                    break;
                case 1:
                    light_info.hue = value;
                    break;
                case 2:
                    light_info.luminance = value;
                    break;
                case 3:
                    light_info.power = value;
                    break;
                case 4:
                    light_info.work_mode = value;
                    break;
                default:
                    break;
                }
            }
        }
        ALINK_LOGI("read: errorcode:%d, hue: %d, luminance: %d, Switch: %d, work_mode: %d",
                   light_info.errorcode, light_info.hue, light_info.luminance, light_info.power, light_info.work_mode);
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
        sprintf(up_cmd, main_dev_params, light_info.errorcode,
                light_info.hue, light_info.luminance,
                light_info.power, light_info.work_mode);
        ret = esp_alink_write(up_cmd, ALINK_DATA_LEN, 500 / portTICK_PERIOD_MS);
        if (ret == ALINK_ERR) ALINK_LOGW("esp_alink_write is err");
        platform_msleep(500);
    }
    free(up_cmd);
    vTaskDelete(NULL);
}

int count = 0;
alink_err_t alink_event_handler(alink_event_t event)
{
    switch (event) {
    case ALINK_EVENT_CLOUD_CONNECTED:
        ALINK_LOGD("Alink cloud connected!");
        if (xSemWriteInfo == NULL)
            xSemWriteInfo = xSemaphoreCreateBinary();
        // xSemaphoreGive(xSemWriteInfo);
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
        esp_alink_factory_reset();
        break;
    case ALINK_EVENT_ACTIVATE_DEVICE:
        ALINK_LOGD("Requests activate device");
        alink_activate_device();
        xSemaphoreGive(xSemWriteInfo);
        break;

    default:
        break;
    }
    return ALINK_OK;
}

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

void alink_key_event(void* arg);
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
        .name           = "alink_product",
        .version        = "1.0.0",
        .model          = "ALINKTEST_LIVING_LIGHT_ALINK_TEST",
        .key            = "5gPFl8G4GyFZ1fPWk20m",
        .secret         = "ngthgTlZ65bX5LpViKIWNsDPhOf2As9ChnoL9gQb",
        .key_sandbox    = "dpZZEpm9eBfqzK7yVeLq",
        .secret_sandbox = "THnfRRsU5vu6g6m9X6uFyAjUWflgZ0iyGjdEneKm",
    };

    ESP_ERROR_CHECK( esp_alink_event_init(alink_event_handler) );
    xTaskCreate(alink_key_event, "alink_key_event", 1024 * 4, NULL, 10, NULL);
    ESP_ERROR_CHECK( esp_alink_init(&product_info) );

    xTaskCreate(read_task_test, "read_task_test", 1024 * 2, NULL, 9, NULL);
    xTaskCreate(write_task_test, "write_task_test", 1024 * 2, NULL, 4, NULL);
    xTaskCreate(free_heap_task, "free_heap_task", 1024 * 2, NULL, 3, NULL);
}
#endif
