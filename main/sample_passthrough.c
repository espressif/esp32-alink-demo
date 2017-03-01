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

#include "product.h"
#include "esp_alink.h"
#ifdef ALINK_PASSTHROUGH

static const char *TAG = "sample_passthrough";
SemaphoreHandle_t xSemWriteInfo = NULL;
#define DEV_INFO_QUEUE_NUM        2
#define LIGHT_METADATA_HEADER   0xaa
#define LIGHT_METADATA_END      0x55
#define LIGHT_METADATA_LEN      0x07
typedef struct light_metadata {
    char header;
    char cmd_len;
    char power;
    char work_mode;
    char temp_value;
    char light_value;
    char time_delay;
    char end;
} dev_info_t;

static dev_info_t light_info = {
    .header      = LIGHT_METADATA_HEADER,
    .cmd_len     = LIGHT_METADATA_LEN,
    .power       = 0x01,
    .work_mode   = 0x30,
    .temp_value  = 0x50,
    .light_value = 0,
    .time_delay  = 0x01,
    .end         = LIGHT_METADATA_END
};

void read_task_test(void *arg)
{
    alink_err_t ret = ALINK_ERR;
    dev_info_t down_cmd;
    for (;;) {
        ret = esp_alink_read(&down_cmd, sizeof(dev_info_t), portMAX_DELAY);
        if (ret == ALINK_ERR) {
            ALINK_LOGW("esp_read is err");
            continue;
        }

        if (ret == sizeof(dev_info_t) && (down_cmd.header == LIGHT_METADATA_HEADER) && (down_cmd.end == LIGHT_METADATA_END)) {
            memcpy(&light_info, &down_cmd, sizeof(dev_info_t));
            ALINK_LOGI("read: power:%d, temp_value: %d, light_value: %d, time_delay: %d, work_mode: %d",
                       light_info.power, light_info.temp_value, light_info.light_value, light_info.time_delay, light_info.work_mode);
        }
        xSemaphoreGive(xSemWriteInfo);
    }
    vTaskDelete(NULL);
}

void write_task_test(void *arg)
{
    alink_err_t ret = ALINK_ERR;
    for (;;) {
        if (xSemaphoreTake(xSemWriteInfo, portMAX_DELAY) == pdFALSE) {
            ALINK_LOGE("xSemaphoreTake:xQueueDevInfo is empty");
            break;
        }
        ALINK_LOGI("write: power:%d, temp_value: %d, light_value: %d, time_delay: %d, work_mode: %d",
                   light_info.power, light_info.temp_value, light_info.light_value, light_info.time_delay, light_info.work_mode);
        ret = esp_alink_write(&light_info, sizeof(dev_info_t), 500);
        if (ret == ALINK_ERR) ALINK_LOGW("esp_alink_write is err");
        vTaskDelay(500 / portTICK_RATE_MS);
    }
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
        .model          = "ALINKTEST_LIVING_LIGHT_SMARTLED_LUA",
        .key            = "bIjq3G1NcgjSfF9uSeK2",
        .secret         = "W6tXrtzgQHGZqksvJLMdCPArmkecBAdcr2F5tjuF",
        .key_sandbox    = "dpZZEpm9eBfqzK7yVeLq",
        .secret_sandbox = "THnfRRsU5vu6g6m9X6uFyAjUWflgZ0iyGjdEneKm",
        /*You do not need to set the following parameters in alink v2.0 */
        .type           = "LIGHT",
        .category       = "LIVING",
        .manufacturer   = "ALINKTEST",
        .cid            = "2D0044000F47333139373038",
    };

    ESP_ERROR_CHECK( esp_alink_event_init(alink_event_handler) );
    ESP_ERROR_CHECK( esp_alink_init(&product_info) );
    xTaskCreate(read_task_test, "read_task_test", 1024 * 8, NULL, 9, NULL);
    xTaskCreate(write_task_test, "write_task_test", 1024 * 8, NULL, 4, NULL);
    xTaskCreate(free_heap_task, "free_heap_task", 1024 * 8, NULL, 3, NULL);
}

#endif
