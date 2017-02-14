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
static xQueueHandle xQueueDevInfo = NULL;
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
        ret = esp_read(&down_cmd, sizeof(dev_info_t), portMAX_DELAY);
        if (ret == ALINK_ERR) {
            ALINK_LOGW("esp_read is err");
            continue;
        }

        if (ret == sizeof(dev_info_t) && (down_cmd.header == LIGHT_METADATA_HEADER) && (down_cmd.end == LIGHT_METADATA_END)) {
            memcpy(&light_info, &down_cmd, sizeof(dev_info_t));
            ALINK_LOGI("power:%d, temp_value: %d, light_value: %d, time_delay: %d, work_mode: %d",
                       light_info.power, light_info.temp_value, light_info.light_value, light_info.time_delay, light_info.work_mode);
        }

        if (xQueueSend(xQueueDevInfo, &light_info, 0) == pdFALSE) {
            ALINK_LOGD("xQueueSend: xQueueDevInfo is full");
        }
    }
    vTaskDelete(NULL);
}

void write_task_test(void *arg)
{
    alink_err_t ret = ALINK_ERR;
    dev_info_t up_cmd;
    for (;;) {
        if (xQueueReceive(xQueueDevInfo, &up_cmd, portMAX_DELAY) == pdFALSE) {
            ALINK_LOGE("xQueueReceive:xQueueDevInfo is empty");
            break;
        }
        ret = esp_write(&up_cmd, sizeof(dev_info_t), 500 / portTICK_PERIOD_MS);
        if (ret == ALINK_ERR) ALINK_LOGW("esp_write is err");

        // platform_msleep(500);
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

    struct device_info product_info = {
        .sn             = "12345678",
        .name           = "ALINKTEST",
        .type           = "LIGHT",
        .category       = "LIVING",
        .manufacturer   = "ALINKTEST",
        .version        = "1.0.0",
        .model          = "ALINKTEST_LIVING_LIGHT_SMARTLED_LUA",
        .cid            = "2D0044000F47333139373038",
        .key            = "bIjq3G1NcgjSfF9uSeK2",
        .secret         = "W6tXrtzgQHGZqksvJLMdCPArmkecBAdcr2F5tjuF",
        .key_sandbox    = "dpZZEpm9eBfqzK7yVeLq",
        .secret_sandbox = "THnfRRsU5vu6g6m9X6uFyAjUWflgZ0iyGjdEneKm",
    };

    ALINK_LOGI("*********************************");
    ALINK_LOGI("*         PRODUCT INFO          *");
    ALINK_LOGI("*********************************");
    ALINK_LOGI("name   : %s", product_info.name);
    ALINK_LOGI("type   : %s", product_info.type);
    ALINK_LOGI("version: %s", product_info.version);
    ALINK_LOGI("model  : %s", product_info.model);
    esp_alink_init(&product_info);

    if (xQueueDevInfo == NULL)
        xQueueDevInfo = xQueueCreate(DEV_INFO_QUEUE_NUM, sizeof(dev_info_t));

    /* After the device is powered on, it automatically reports all attribute status data */
    if (xQueueSend(xQueueDevInfo, &light_info, 0) == pdFALSE) {
        ALINK_LOGD("xQueueSend: xQueueDevInfo is full");
    }

    xTaskCreate(read_task_test, "read_task_test", 1024 * 8, NULL, 9, NULL);
    xTaskCreate(write_task_test, "write_task_test", 1024 * 8, NULL, 4, NULL);
    printf("free_heap3:%u\n", esp_get_free_heap_size());
}

#endif
