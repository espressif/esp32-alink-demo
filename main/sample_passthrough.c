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

static const char *TAG = "app_main";
static SemaphoreHandle_t xSemWrite = NULL;

/*do your job here*/
struct virtual_dev {
    char power;
    char work_mode;
    char temp_value;
    char light_value;
    char time_delay;
} virtual_device = {
    0x01, 0x30, 0x50, 0, 0x01
};

void read_task_test(void *arg)
{
    char *down_cmd = (char *)malloc(8);
    for (;;) {
        esp_read(down_cmd, ALINK_DATA_LEN, portMAX_DELAY);
        ALINK_LOGD("read_data: %02x %02x %d %d %d %d %d %02x",
                   down_cmd[0], down_cmd[1], down_cmd[2], down_cmd[3],
                   down_cmd[4], down_cmd[5], down_cmd[6], down_cmd[7]);
        memcpy(&virtual_device, down_cmd + 2, 5);
        ALINK_LOGI("\npower:%d, temp_value: %d, light_value: %d, time_delay: %d, work_mode: %d",
                   virtual_device.power, virtual_device.temp_value, virtual_device.light_value, virtual_device.time_delay, virtual_device.work_mode);
        xSemaphoreGive(xSemWrite);
    }
    free(down_cmd);
    vTaskDelete(NULL);
}

void write_task_test(void *arg)
{
    char *up_cmd = (char *)malloc(8);
    up_cmd[0] = 0xaa;
    up_cmd[1] = 0x07;
    up_cmd[7] = 0x55;
    for (;;) {
        xSemaphoreTake(xSemWrite, portMAX_DELAY);
        memcpy(up_cmd + 2, &virtual_device, 5);
        esp_write(up_cmd, 8, 100 / portTICK_PERIOD_MS);
        ALINK_LOGD("\npower:%d, temp_value: %d, light_value: %d, time_delay: %d, work_mode: %d",
                   virtual_device.power, virtual_device.temp_value, virtual_device.light_value, virtual_device.time_delay, virtual_device.work_mode);
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
    ALINK_LOGI("mode: passthrough, free_heap :%u\n", esp_get_free_heap_size());
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
    esp_alink_init(&product_info);
    ALINK_LOGI("esp32 alink version: %s", product_info.version);

    xSemWrite = xSemaphoreCreateBinary();
    xSemaphoreGive(xSemWrite);
    xTaskCreate(read_task_test, "read_task_test", 1024 * 8, NULL, 9, NULL);
    xTaskCreate(write_task_test, "write_task_test", 1024 * 8, NULL, 9, NULL);
    printf("free_heap3:%u\n", esp_get_free_heap_size());
}

#endif
