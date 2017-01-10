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

#define EXAMPLE_FILENAME "alink.bin"
#define BUFFSIZE 1024
#define TEXT_BUFFSIZE 1024

static const char *TAG = "alink_ota";
/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };
/*an packet receive buffer*/
static char text[BUFFSIZE + 1] = { 0 };
/* an image total length*/
static int binary_file_length = 0;

/* operate handle : uninitialized value is zero ,every ota begin would exponential growth*/
static esp_ota_handle_t out_handle = 0;
static esp_partition_t operate_partition;

void task_fatal_error()
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    // close(socket_id);
    vTaskDelete(NULL);
}

/*read buffer by byte still delim ,return read bytes counts*/
int read_until(char *buffer, char delim, int len)
{
//  /*TODO: delim check,buffer check,further: do an buffer length limited*/
    int i = 0;
    while (buffer[i] != delim && i < len) {
        ++i;
    }
    return i + 1;
}

/* resolve a packet from http socket
 * return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
 * otherwise return false
 * */
static bool resolve_pkg(char text[], int total_len, esp_ota_handle_t out_handle)
{
    /* i means current position */
    int i = 0, i_read_len = 0;
    while (text[i] != 0 && i < total_len) {
        i_read_len = read_until(&text[i], '\n', total_len);
        // if we resolve \r\n line,we think packet header is finished
        if (i_read_len == 2) {
            int i_write_len = total_len - (i + 2);
            memset(ota_write_data, 0, BUFFSIZE);
            /*copy first http packet body to write buffer*/
            memcpy(ota_write_data, &(text[i + 2]), i_write_len);
            /*check write packet header first byte:0xE9 second byte:0x09 */
            if (ota_write_data[0] == 0xE9 && i_write_len >= 2 && ota_write_data[1] == 0x09) {
                ESP_LOGI(TAG, "OTA Write Header format Check OK. first byte is %02x ,second byte is %02x", ota_write_data[0], ota_write_data[1]);
            } else {
                ESP_LOGE(TAG, "OTA Write Header format Check Failed! first byte is %02x ,second byte is %02x", ota_write_data[0], ota_write_data[1]);
                return false;
            }

            esp_err_t err = esp_ota_write( out_handle, (const void *)ota_write_data, i_write_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error: esp_ota_write failed! err=%x", err);
                return false;
            } else {
                ESP_LOGI(TAG, "esp_ota_write header OK");
                binary_file_length += i_write_len;
            }
            return true;
        }
        i += i_read_len;
    }
    return false;
}


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
    esp_err_t err;
    static bool pkg_body_start = false, flag = true;

    if (length < 0) { /*receive error*/
        ESP_LOGE(TAG, "Error: receive data error! errno=%d", errno);
        return -1;
    }

    if (length > 0 && !pkg_body_start) { /*deal with packet header*/
        memcpy(buffer, text, length);
        pkg_body_start = resolve_pkg(text, length, out_handle);
    } else if (length > 0 && pkg_body_start) { /*deal with packet body*/
        memcpy(buffer, text, length);
        err = esp_ota_write( out_handle, (const void *)buffer, length);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error: esp_ota_write failed! err=%x", err);
            return -1;
        }
        binary_file_length += length;
        ESP_LOGI(TAG, "Have written image length %d", binary_file_length);
    } else if (length == 0) {  /*packet over*/
        flag = false;
        ESP_LOGI(TAG, "Connection closed, all packets received");
    } else {
        ESP_LOGE(TAG, "Unexpected recv result");
        return -1;
    }
    return 0;
}

int platform_firmware_upgrade_finish(void)
{
    esp_err_t err;
    if (esp_ota_end(out_handle) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed!");
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

