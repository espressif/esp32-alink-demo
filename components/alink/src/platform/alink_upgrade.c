#if 1

/* OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#define BUFFSIZE 1024

static const char *TAG = "alink_upgrade";
/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };

/* operate handle : uninitialized value is zero ,every ota begin would exponential growth*/
static esp_ota_handle_t out_handle = 0;
static esp_partition_t operate_partition;

#define require_action_exit(con, msg, ...) if(con) {ESP_LOGE(TAG, msg, ##__VA_ARGS__); esp_restart();}

void platform_firmware_upgrade_start(void)
{
    esp_err_t err;
    const esp_partition_t *esp_current_partition = esp_ota_get_boot_partition();
    if (esp_current_partition->type != ESP_PARTITION_TYPE_APP) {
        ESP_LOGE(TAG, "Error： esp_current_partition->type != ESP_PARTITION_TYPE_APP");
        return;
    }

    esp_partition_t find_partition;
    memset(&operate_partition, 0, sizeof(esp_partition_t));
    /*choose which OTA image should we write to*/
    switch (esp_current_partition->subtype) {
    case ESP_PARTITION_SUBTYPE_APP_FACTORY:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        break;
    case  ESP_PARTITION_SUBTYPE_APP_OTA_0:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
        break;
    case ESP_PARTITION_SUBTYPE_APP_OTA_1:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        break;
    default:
        break;
    }
    find_partition.type = ESP_PARTITION_TYPE_APP;

    const esp_partition_t *partition = esp_partition_find_first(find_partition.type, find_partition.subtype, NULL);
    assert(partition != NULL);
    memset(&operate_partition, 0, sizeof(esp_partition_t));
    err = esp_ota_begin( partition, OTA_SIZE_UNKNOWN, &out_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed err=0x%x!", err);
    } else {
        memcpy(&operate_partition, partition, sizeof(esp_partition_t));
        ESP_LOGI(TAG, "esp_ota_begin init OK");
    }
}

int platform_firmware_upgrade_write(char *buffer, uint32_t length)
{
    require_action_exit(length <= 0, "[%s, %d]:Parameter error length <= 0", __func__, __LINE__);
    require_action_exit(buffer == NULL, "[%s, %d]:Parameter error buffer == NULL", __func__, __LINE__);

    esp_err_t err;
    static int binary_file_length = 0;
    memset(ota_write_data, 0, BUFFSIZE);
    memcpy(ota_write_data, buffer, length);
    err = esp_ota_write( out_handle, (const void *)ota_write_data, length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error: esp_ota_write failed! err=%x", err);
        return -1;
    }
    binary_file_length += length;
    ESP_LOGI(TAG, "Have written image length %d", binary_file_length);

    return 0;
}

int platform_firmware_upgrade_finish(void)
{
    esp_err_t err;
    err = esp_ota_end(out_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed! err: %d", err);
        return -1;
    }
    err = esp_ota_set_boot_partition(&operate_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
        return -1;
    }
    return 0;
}
#endif

