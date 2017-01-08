#if 1
#include "platform.h"
void platform_firmware_upgrade_start(void)
{
    // esp_err_t err;
    // const esp_partition_t *esp_current_partition = esp_ota_get_boot_partition();
    // if (esp_current_partition->type != ESP_PARTITION_TYPE_APP) {
    //     ESP_LOGE(TAG, "Error： esp_current_partition->type != ESP_PARTITION_TYPE_APP");
    //     return false;
    // }

    // esp_partition_t find_partition;
    // memset(&operate_partition, 0, sizeof(esp_partition_t));
    // /*choose which OTA image should we write to*/
    // switch (esp_current_partition->subtype) {
    // case ESP_PARTITION_SUBTYPE_APP_FACTORY:
    //     find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
    //     break;
    // case  ESP_PARTITION_SUBTYPE_APP_OTA_0:
    //     find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
    //     break;
    // case ESP_PARTITION_SUBTYPE_APP_OTA_1:
    //     find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
    //     break;
    // default:
    //     break;
    // }
    // find_partition.type = ESP_PARTITION_TYPE_APP;

    // const esp_partition_t *partition = esp_partition_find_first(find_partition.type, find_partition.subtype, NULL);
    // assert(partition != NULL);
    // memset(&operate_partition, 0, sizeof(esp_partition_t));
    // err = esp_ota_begin( partition, OTA_SIZE_UNKNOWN, &out_handle);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "esp_ota_begin failed err=0x%x!", err);
    //     return false;
    // } else {
    //     memcpy(&operate_partition, partition, sizeof(esp_partition_t));
    //     ESP_LOGI(TAG, "esp_ota_begin init OK");
    //     return true;
    // }
}
int platform_firmware_upgrade_write(_IN_ char *buffer, _IN_ uint32_t length)
{
    // err = esp_ota_write( out_handle, (const void *)ota_write_data, buff_len);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "Error: esp_ota_write failed! err=%x", err);
    //     close(socket_id);
    //     return task_fatal_error();
    // }
    // unsigned int written_len = 0;
    // written_len = fwrite(buffer, 1, length, fp);

    // if (written_len != length)
    //     return -1;
    return 0;
}

int platform_firmware_upgrade_finish(void)
{
    // if (esp_ota_end(out_handle) != ESP_OK) {
    //     ESP_LOGE(TAG, "esp_ota_end failed!");
    //     return task_fatal_error();
    // }
    // err = esp_ota_set_boot_partition(&operate_partition);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
    //     return task_fatal_error();
    // }
    return 0;
}

#endif
