
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_types.h"
#include "esp_system.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include "platform.h"
#include "esp_system.h"
#include "lwip/err.h"

void platform_printf(_IN_ const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

int platform_config_read(_OUT_ char *buffer, _IN_ int length)
{
    if (!buffer || length <= 0)
        return -1;
    // printf("---------- platform_config_read: %02x %02x %02x length: %d -------------\n",
    //        buffer[0], buffer[1], buffer[2], length);
    esp_err_t ret     = -1;
    nvs_handle config_handle = 0;

    ret = nvs_open("alink", NVS_READWRITE, &config_handle);
    if (ret != 0) return -1;
    ret = nvs_get_blob(config_handle, "config", buffer, (size_t *)&length);
    if (ret != 0) return -1;
    nvs_close(config_handle);
    return 0;
}

int platform_config_write(_IN_ const char *buffer, _IN_ int length)
{
    // printf("---------- platform_config_write: %02x %02x %02x length: %d -------------\n",
    //        buffer[0], buffer[1], buffer[2], length);

    if (!buffer || length <= 0)
        return -1;

    esp_err_t ret     = -1;
    nvs_handle config_handle = 0;
    ret = nvs_open("alink", NVS_READWRITE, &config_handle);
    if (ret != 0) return -1;
    ret = nvs_set_blob(config_handle, "config", buffer, length);
    if (ret != 0) return -1;
    nvs_commit(config_handle);
    nvs_close(config_handle);
    return 0;
}

char *platform_get_module_name(_OUT_ char name_str[PLATFORM_MODULE_NAME_LEN])
{
    strcpy(name_str, "esp32");
    return name_str;
}

int platform_sys_net_is_ready(void)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    return 1;
}

void platform_sys_reboot(void)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    system_restart();
}
