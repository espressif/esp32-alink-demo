/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

#include "esp_info_store.h"
#include "esp_alink_log.h"

static const char *TAG = "esp_info_store";

int esp_info_erase(const char *key)
{
    ALINK_PARAM_CHECK(key);

    alink_err_t ret   = -1;
    nvs_handle handle = 0;

    ret = nvs_open(ALINK_SPACE_NAME, NVS_READWRITE, &handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "nvs_open ret:%x", ret);

    if (!strcmp(key, ALINK_SPACE_NAME)) {
        ret = nvs_erase_all(handle);
    } else {
        ret = nvs_erase_key(handle, key);
    }

    nvs_commit(handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "nvs_erase_key ret:%x", ret);
    return ALINK_OK;
}

ssize_t esp_info_save(const char *key, const void *value, size_t length)
{
    ALINK_PARAM_CHECK(key);
    ALINK_PARAM_CHECK(value);
    ALINK_PARAM_CHECK(length > 0);

    alink_err_t ret   = -1;
    nvs_handle handle = 0;

    ret = nvs_open(ALINK_SPACE_NAME, NVS_READWRITE, &handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "nvs_open ret:%x", ret);

    /**
     * Reduce the number of flash writes
     */
    char *tmp = (char *)alink_malloc(length);
    ret = nvs_get_blob(handle, key, tmp, &length);

    if ((ret == ESP_OK) && !memcmp(tmp, value, length)) {
        alink_free(tmp);
        nvs_close(handle);
        return length;
    }

    alink_free(tmp);

    ret = nvs_set_blob(handle, key, value, length);
    nvs_commit(handle);
    nvs_close(handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "nvs_set_blob ret:%x", ret);

    return length;
}

ssize_t esp_info_load(const char *key, void *value, size_t length)
{
    ALINK_PARAM_CHECK(key);
    ALINK_PARAM_CHECK(value);
    ALINK_PARAM_CHECK(length > 0);

    alink_err_t ret   = -1;
    nvs_handle handle = 0;

    ret = nvs_open(ALINK_SPACE_NAME, NVS_READWRITE, &handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "nvs_open ret:%x", ret);
    ret = nvs_get_blob(handle, key, value, &length);
    nvs_close(handle);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ALINK_LOGW("No data storage,the load data is empty");
        return ALINK_ERR;
    }

    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "nvs_get_blob ret:%x", ret);
    return length;
}
