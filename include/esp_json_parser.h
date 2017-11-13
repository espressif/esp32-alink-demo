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

#ifndef __ESP_JOSN_PARSER_H__
#define __ESP_JOSN_PARSER_H__

#include "esp_system.h"
#include "esp_alink_log.h"

#ifdef __cplusplus
extern "C" {
#endif /*!< _cplusplus */

/**
 * @brief This api is used only for alink json internal use
 */
alink_err_t __esp_json_parse(const char *json_str, const char *key, void *value, int value_type);
alink_err_t __esp_json_pack(char *json_str, const char *key, int value, int value_type);

/**
 * @brief  alink_err_t esp_json_parse(const char *json_str, const char *key, void *value)
 *         Parse the json formatted string
 *
 * @param  json_str The string pointer to be parsed
 * @param  key      Nuild value pairs
 * @param  value    You must ensure that the incoming type is consistent with the
 *                  post-resolution type
 *
 * @note   Does not support the analysis of array types
 *
 * @return
 *     - ALINK_OK  Success
 *     - ALINK_ERR Parameter error
 */
#define esp_json_parse(json_str, key, value) \
    __esp_json_parse((const char *)(json_str), (const char *)(key), value, \
                     __builtin_types_compatible_p(typeof(value), short *) * 1\
                     + __builtin_types_compatible_p(typeof(value), int *) * 1\
                     + __builtin_types_compatible_p(typeof(value), uint16_t *) * 1\
                     + __builtin_types_compatible_p(typeof(value), uint32_t *) * 1\
                     + __builtin_types_compatible_p(typeof(value), float *) * 2\
                     + __builtin_types_compatible_p(typeof(value), double *) * 3)

/**
 * @brief  esp_json_pack(char *json_str, const char *key, int/double/char value);
 *         Create a json string
 *
 * @param  json_str Save the generated json string
 * @param  key      Build value pairs
 * @param  value    This is a generic, support int/double/char/char *
 *
 * @note   If the value is double or float type only retains the integer part,
 *         requires complete data calling  esp_json_pack_double()
 *
 * @return
 *     - generates the length of the json string  Success
 *     - ALINK_ERR                                 Parameter error
 */
#define esp_json_pack(json_str, key, value) \
    __esp_json_pack((char *)(json_str), (const char *)(key), (int)(value), \
                    __builtin_types_compatible_p(typeof(value), char) * 1\
                    + __builtin_types_compatible_p(typeof(value), short) * 1\
                    + __builtin_types_compatible_p(typeof(value), int) * 1\
                    + __builtin_types_compatible_p(typeof(value), uint8_t) * 1\
                    + __builtin_types_compatible_p(typeof(value), uint16_t) * 1\
                    + __builtin_types_compatible_p(typeof(value), uint32_t) * 1\
                    + __builtin_types_compatible_p(typeof(value), float) * 2\
                    + __builtin_types_compatible_p(typeof(value), double) * 2\
                    + __builtin_types_compatible_p(typeof(value), char *) * 3\
                    + __builtin_types_compatible_p(typeof(value), const char *) * 3\
                    + __builtin_types_compatible_p(typeof(value), unsigned char *) * 3\
                    + __builtin_types_compatible_p(typeof(value), const unsigned char *) * 3)

/**
 * @brief  Create a double type json string, Make up for the lack of esp_json_pack()
 *
 * @param  json_str Save the generated json string
 * @param  key      Build value pairs
 * @param  value    The value to be stored
 *
 * @return
 *     - generates the length of the json string  Success
 *     - ALINK_ERR                                 Parameter error
 */
alink_err_t esp_json_pack_double(char *json_str, const char *key, double value);

/****************************** example ******************************/
/*
static void esp_json_test()
{
    int ret = 0;
    char *json_root = (char *)alink_calloc(1, 512);
    char *json_sub = (char *)alink_calloc(1, 64);
    int valueint = 0;
    char valuestring[20];
    double valuedouble = 0;

    // {
    //     "key0": 0,
    //     "key1": "tmp1",
    //     "key4": {
    //         "key2": {
    //             "name": "json test"
    //         },
    //         "key3": 1
    //     },
    //     "key5": 99.00000,
    //     "key6": 99.23456
    // }
    ret = esp_json_pack(json_root, "key0", valueint++);
    ret = esp_json_pack(json_root, "key1", "tmp1");
    ret = esp_json_pack(json_sub, "key2", "{\"name\":\"json test\"}");
    ret = esp_json_pack(json_sub, "key3", valueint++);
    ret = esp_json_pack(json_root, "key4", json_sub);
    ret = esp_json_pack(json_root, "key5", 99.23456);
    ret = esp_json_pack_double(json_root, "key6", 99.23456);
    printf("json_len: %d, json: %s\n", ret, json_root);

    printf("------------- json parse -----------\n");

    esp_json_parse(json_root, "key0", &valueint);
    printf("key0: %d\n", valueint);
    esp_json_parse(json_root, "key1", valuestring);
    printf("key1: %s\n", valuestring);
    esp_json_parse(json_root, "key4", json_sub);
    printf("key4: %s\n", json_sub);

    esp_json_parse(json_sub, "key2", valuestring);
    printf("key2: %s\n", valuestring);
    esp_json_parse(json_sub, "key3", &valueint);
    printf("key3: %d\n", valueint);
    esp_json_parse(json_root, "key5", &valueint);
    printf("key5: %d\n", valueint);
    // CJson floating point type handling problems, later will be fixed
    esp_json_parse(json_root, "key6", &valuedouble);

    // printf can not output double type
    char double_char[16] = {0};
    sprintf(double_char, "%lf", valuedouble);
    printf("key6: %s\n", double_char);

    alink_free(json_root);
    alink_free(json_sub);
}
 */

#ifdef __cplusplus
}
#endif /*!< _cplusplus */

#endif /*!< __ESP_JOSN_PARSER_H__ */
