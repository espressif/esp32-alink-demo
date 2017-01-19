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

void esp_alink_init(_IN_ struct device_info *product);

#define ALINK_DATA_LEN 512
alink_up_cmd_ptr alink_up_cmd_malloc();
alink_err_t alink_up_cmd_free(_IN_ alink_up_cmd_ptr up_cmd);
alink_err_t alink_up_cmd_memcpy(_IN_ alink_up_cmd_ptr dest, _OUT_ alink_up_cmd_ptr src);

alink_down_cmd_ptr alink_down_cmd_malloc();
alink_err_t alink_down_cmd_free(_IN_ alink_down_cmd_ptr down_cmd);
alink_err_t alink_up_cmd_memcpy(_IN_ alink_up_cmd_ptr dest, _OUT_ alink_up_cmd_ptr src);

alink_err_t alink_write(alink_up_cmd_ptr up_cmd, TickType_t ticks_to_wait);
alink_err_t alink_read(alink_down_cmd_ptr down_cmd, TickType_t ticks_to_wait);

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


static alink_err_t read_flag = ALINK_FALSE;
void read_task_test(void *arg)
{
    alink_down_cmd_ptr down_cmd = alink_down_cmd_malloc();
    for (;;) {
        alink_read(down_cmd, portMAX_DELAY);

        int attrLen = 0, valueLen = 0, value = 0, i = 0;
        char *valueStr = NULL, *attrStr = NULL;
        for (i = 0; i < 5; i++) {
            attrStr = alink_JsonGetValueByName(down_cmd->param, strlen(down_cmd->param), device_attr[i], &attrLen, 0);
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
        read_flag = ALINK_TRUE;
    }
    free(down_cmd);
    vTaskDelete(NULL);
}


void write_task_test(void *arg)
{
    alink_up_cmd_ptr up_cmd = alink_up_cmd_malloc();
    for (;;) {
        if (read_flag == ALINK_FALSE) {
            platform_msleep(500);
            continue;
        }
        memset(up_cmd->param, 0, ALINK_DATA_LEN);
        sprintf(up_cmd->param, main_dev_params, virtual_device.power,
                virtual_device.temp_value, virtual_device.light_value,
                virtual_device.time_delay, virtual_device.work_mode);
        alink_write(up_cmd, 100 / portTICK_PERIOD_MS);
        read_flag = ALINK_FALSE;
    }
    alink_up_cmd_free(up_cmd);
    vTaskDelete(NULL);
}


/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
static void free_heap_task(void *arg)
{
    while (1) {
        ets_printf("free heap size: %d\n", esp_get_free_heap_size());
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void app_main()
{
    ets_printf("==== esp32_alink sdk version: 1.0.5  ====\n");
    ets_printf("free_heap :%u\n", esp_get_free_heap_size());
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
    xTaskCreate(read_task_test, "read_task_test", 1024 * 8, NULL, 9, NULL);
    xTaskCreate(write_task_test, "write_task_test", 1024 * 8, NULL, 9, NULL);
    // if(product) free(product);
    // xTaskCreate(free_heap_task, "free_heap_task", 1024, NULL, 3, NULL);
    printf("free_heap3:%u\n", esp_get_free_heap_size());
}

