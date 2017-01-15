#if 1
/*
 * Copyright (c) 2014-2015 Alibaba Group. All rights reserved.
 *
 * Alibaba Group retains all right, title and interest (including all
 * intellectual property rights) in and to this computer program, which is
 * protected by applicable intellectual property laws.  Unless you have
 * obtained a separate written license from Alibaba Group., you are not
 * authorized to utilize all or a part of this computer program for any
 * purpose (including reproduction, distribution, modification, and
 * compilation into object code), and you must immediately destroy or
 * return to Alibaba Group all copies of this computer program.  If you
 * are licensed by Alibaba Group, your rights to utilize this computer
 * program are limited by the terms of that license.  To obtain a license,
 * please contact Alibaba Group.
 *
 * This computer program contains trade secrets owned by Alibaba Group.
 * and, unless unauthorized by Alibaba Group in writing, you agree to
 * maintain the confidentiality of this computer program and related
 * information and to not disclose this computer program and related
 * information to any other person or entity.
 *
 * THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
 * Alibaba Group EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, TITLE, AND NONINFRINGEMENT.
 */
#include "platform/platform.h"
#include "product/product.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "alink_export.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "alink_export_rawdata.h"
#include "alink_user_config.h"
static const char *TAG = "alink_passthroug";

static int sample_running = ALINK_TRUE;

void *alink_sample_mutex;
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

char *device_attr[5] = { "OnOff_Power", "Color_Temperature", "Light_Brightness",
                         "TimeDelay_PowerOff", "WorkMode_MasterLight"
                       };

const char *main_dev_params =
    "{\"OnOff_Power\": { \"value\": \"%d\" }, \"Color_Temperature\": { \"value\": \"%d\" }, \"Light_Brightness\": { \"value\": \"%d\" }, \"TimeDelay_PowerOff\": { \"value\": \"%d\"}, \"WorkMode_MasterLight\": { \"value\": \"%d\"}}";
static char device_status_change = 1;


static int get_device_state()
{
    int ret = 0;
    platform_mutex_lock(alink_sample_mutex);
    ret = device_status_change;
    platform_mutex_unlock(alink_sample_mutex);
    return ret;
}

static int set_device_state(int state)
{
    platform_mutex_lock(alink_sample_mutex);
    device_status_change = state;
    platform_mutex_unlock(alink_sample_mutex);
    return state;
}


/* this sample save cmd value to virtual_device*/
static int execute_cmd(_IN_ const char *rawdata, int len)
{
    int ret = 0, i = 0;
    if (len < 1)
        ret = -1;
    for (i = 0; i < len; i++) {
        ALINK_LOGI("%2x ", (unsigned char) rawdata[i]);
        switch (i) {
        case 2:
            if (virtual_device.power != rawdata[i]) {
                virtual_device.power = rawdata[i];
            }
            break;
        case 3:
            if (virtual_device.work_mode != rawdata[i]) {
                virtual_device.work_mode = rawdata[i];
            }
            break;
        case 4:
            if (virtual_device.temp_value != rawdata[i]) {
                virtual_device.temp_value = rawdata[i];
            }
            break;
        case 5:
            if (virtual_device.light_value != rawdata[i]) {
                virtual_device.light_value = rawdata[i];
            }
            break;
        case 6:
            if (virtual_device.time_delay != rawdata[i]) {
                virtual_device.time_delay = rawdata[i];
            }
            break;
        default:
            break;
        }
    }
    return ret;
}

static int get_device_status(_OUT_ char *rawdata, int len)
{
    /* do your job here */
    int ret = 0;
    if (len > 7) {
        rawdata[0] = 0xaa;
        rawdata[1] = 0x07;
        rawdata[2] = virtual_device.power;
        rawdata[3] = virtual_device.work_mode;
        rawdata[4] = virtual_device.temp_value;
        rawdata[5] = virtual_device.light_value;
        rawdata[6] = virtual_device.time_delay;
        rawdata[7] = 0x55;
    } else {
        ret = -1;
    }
    /* do your job end */
    return ret;
}
static int alink_device_post_raw_data(void)
{
    /* do your job here */
    int len = 8, ret = 0;
    char rawdata[8] = { 0 };
    if (get_device_state()) {
        get_device_status(rawdata, len);
        ret = alink_post_device_rawdata(rawdata, len);
        set_device_state(0);
        if (ret) {
            ALINK_LOGI("post failed!\n");
        } else {
            ALINK_LOGI("dev post raw data success!\n");
        }
    }
    /* do your job end */
    return ret;
}
static int rawdata_get_callback(const char *in_rawdata, int in_len, char *out_rawdata, int *out_len)
{
    int ret = 0;
    set_device_state(1);
    /*do your job end! */
    return ret;
}

static int rawdata_set_callback(_IN_ char *rawdata, int len)
{
    /* TODO: */
    /*get cmd from server, do your job here! */
    int ret = 0;
    ret = execute_cmd(rawdata, len);
    /* do your job end! */
    set_device_state(1);
    return ret;
}


static int alink_handler_systemstates_callback(_IN_ void *dev_mac, _IN_ void *sys_state)
{
    ALINK_PARAM_CHECK(dev_mac == NULL);
    ALINK_PARAM_CHECK(sys_state == NULL);

    char uuid[33] = { 0 };
    char *mac = (char *)dev_mac;
    enum ALINK_STATUS *state = (enum ALINK_STATUS *)sys_state;
    switch (*state) {
    case ALINK_STATUS_INITED:
        break;
    case ALINK_STATUS_REGISTERED:
        sprintf(uuid, "%s", alink_get_uuid(NULL));
        ALINK_LOGI("ALINK_STATUS_REGISTERED, mac %s uuid %s \n", mac,
                 uuid);
        break;
    case ALINK_STATUS_LOGGED:
        sprintf(uuid, "%s", alink_get_uuid(NULL));
        ALINK_LOGI("ALINK_STATUS_LOGGED, mac %s uuid %s\n", mac, uuid);
        break;
    default:
        break;
    }
    return 0;
}



static void alink_fill_deviceinfo(_OUT_ struct device_info *deviceinfo)
{
    ALINK_PARAM_CHECK(deviceinfo == NULL);
    /*fill main device info here */
    product_get_name(deviceinfo->name);
    product_get_sn(deviceinfo->sn);  // 产品注册方式 如果是sn, 那么需要保障sn唯一
    product_get_key(deviceinfo->key);
    product_get_model(deviceinfo->model);
    product_get_secret(deviceinfo->secret);
    product_get_type(deviceinfo->type);
    product_get_version(deviceinfo->version);
    product_get_category(deviceinfo->category);
    product_get_manufacturer(deviceinfo->manufacturer);
    product_get_debug_key(deviceinfo->key_sandbox);
    product_get_debug_secret(deviceinfo->secret_sandbox);
    platform_wifi_get_mac(deviceinfo->mac);//产品注册mac唯一 or sn唯一  统一大写
    product_get_cid(deviceinfo->cid); // 使用接口获取唯一chipid,防伪造设备

    ALINK_LOGI("DEV_MODEL:%s", deviceinfo->model);
}

// extern  TCB_t * pxCurrentTCB[ portNUM_PROCESSORS ];

void alink_passthroug(void *arg)
{
    struct device_info *main_dev;
    main_dev = platform_malloc(sizeof(struct device_info));
    alink_sample_mutex = platform_mutex_init();
    memset(main_dev, 0, sizeof(struct device_info));
    alink_fill_deviceinfo(main_dev);
    alink_set_loglevel(ALINK_LL_DEBUG | ALINK_LL_INFO | ALINK_LL_WARN |
                       ALINK_LL_ERROR | ALINK_LL_DUMP);
    main_dev->sys_callback[ALINK_FUNC_SERVER_STATUS] = alink_handler_systemstates_callback;

    /*start alink-sdk */
    alink_start_rawdata(main_dev, rawdata_get_callback, rawdata_set_callback);
    platform_free(main_dev);

    ALINK_LOGI("wait main device login");
    /*wait main device login, -1 means wait forever */
    alink_wait_connect(NULL, ALINK_WAIT_FOREVER);

    while (sample_running) {
        alink_device_post_raw_data();
        vTaskDelay(500 / portTICK_RATE_MS);
    }
    alink_end();
    platform_mutex_destroy(alink_sample_mutex);
    vTaskDelete(NULL);
}

#endif
