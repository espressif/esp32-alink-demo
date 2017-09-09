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

#include <string.h>

#include "esp_wifi.h"

#include "alink_product.h"
#include "esp_alink_log.h"

static const char *TAG = "alink_product";
static alink_product_t g_device_info;

alink_err_t product_get(_OUT_ void *product_info)
{
    ALINK_PARAM_CHECK(product_info);

    memcpy(product_info, &g_device_info, sizeof(alink_product_t));
    return ALINK_OK;
}

alink_err_t product_set(_IN_ const void *product_info)
{
    ALINK_PARAM_CHECK(product_info);

    memcpy(&g_device_info, product_info, sizeof(alink_product_t));
    return ALINK_OK;
}

char *product_get_name(char name_str[PRODUCT_NAME_LEN])
{
    return strncpy(name_str, g_device_info.name, PRODUCT_NAME_LEN);
}

char *product_get_version(char ver_str[PRODUCT_VERSION_LEN])
{
    return strncpy(ver_str, g_device_info.version, PRODUCT_VERSION_LEN);
}

char *product_get_model(char model_str[PRODUCT_MODEL_LEN])
{
    return strncpy(model_str, g_device_info.model, PRODUCT_MODEL_LEN);
}

char *product_get_key(char key_str[PRODUCT_KEY_LEN])
{
    return strncpy(key_str, g_device_info.key, PRODUCT_KEY_LEN);
}

char *product_get_secret(char secret_str[PRODUCT_SECRET_LEN])
{
    return strncpy(secret_str, g_device_info.secret, PRODUCT_SECRET_LEN);
}

char *product_get_debug_key(char key_str[PRODUCT_KEY_LEN])
{
    return strncpy(key_str, g_device_info.key_sandbox, PRODUCT_KEY_LEN);
}

char *product_get_debug_secret(char secret_str[PRODUCT_SECRET_LEN])
{
    return strncpy(secret_str, g_device_info.secret_sandbox, PRODUCT_SECRET_LEN);
}

char *product_get_device_key(char key_str[PRODUCT_KEY_LEN])
{
    return strncpy(key_str, g_device_info.key_device, PRODUCT_KEY_LEN);
}

char *product_get_device_secret(char secret_str[PRODUCT_SECRET_LEN])
{
    return strncpy(secret_str, g_device_info.secret_device, PRODUCT_SECRET_LEN);
}

char *product_get_sn(char sn_str[PRODUCT_SN_LEN])
{
    uint8_t mac[6] = {0};
    ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_STA, mac));
    snprintf(sn_str, PRODUCT_SN_LEN, "%02x%02x%02x%02x%02x%02x", MAC2STR(mac));
    return sn_str;
}
