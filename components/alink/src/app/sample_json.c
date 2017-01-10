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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include "platform/platform.h"
#include "product/product.h"
#include "alink_export.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

static char *device_attr[5] = { "OnOff_Power", "Color_Temperature", "Light_Brightness",
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


#define buffer_size 512
static int alink_device_post_data(alink_down_cmd_ptr down_cmd)
{
    alink_up_cmd up_cmd;
    int ret = ALINK_ERR;
    char *buffer = NULL;
    if (get_device_state()) {
        set_device_state(0);
        buffer = (char *)platform_malloc(buffer_size);
        if (buffer == NULL)
            return -1;
        memset(buffer, 0, buffer_size);
        sprintf(buffer, main_dev_params, virtual_device.power,
                virtual_device.temp_value, virtual_device.light_value,
                virtual_device.time_delay, virtual_device.work_mode);
        up_cmd.param = buffer;
        if (down_cmd != NULL) {
            up_cmd.target = down_cmd->account;
            up_cmd.resp_id = down_cmd->id;
        } else {
            up_cmd.target = NULL;
            up_cmd.resp_id = -1;
        }
        ret = alink_post_device_data(&up_cmd);
        if (ret == ALINK_ERR) {
            printf("post failed!\n");
            platform_msleep(2000);
        } else {
            printf("dev post data success!\n");
            device_status_change = 0;
        }
        if (buffer)
            free(buffer);
    }
    return ret;

}

/* do your job end */
static int sample_running = ALINK_TRUE;
static int main_dev_set_device_status_callback(alink_down_cmd_ptr down_cmd)
{
    int attrLen = 0, valueLen = 0, value = 0, i = 0;
    char *valueStr = NULL, *attrStr = NULL;

    /* do your job here */
    printf("%s %d \n", __FUNCTION__, __LINE__);
    printf("%s %d\n%s\n", down_cmd->uuid, down_cmd->method, down_cmd->param);
    set_device_state(1);

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
    // 不能阻塞
    return 0;
    /* do your job end! */
}

static int main_dev_get_device_status_callback(alink_down_cmd_ptr down_cmd)
{
    /* do your job here */
    printf("%s %d \n", __FUNCTION__, __LINE__);
    printf("%s %d\n%s\n", down_cmd->uuid, down_cmd->method, down_cmd->param);
    set_device_state(1);
    // app 是等返回还是直接获取服务端数据?
    // 异步
    return 0;
    /*do your job end */
}


static int alink_handler_systemstates_callback(void *dev_mac, void *sys_state)
{
    char uuid[33] = { 0 };
    char *mac = (char *)dev_mac;
    enum ALINK_STATUS *state = (enum ALINK_STATUS *)sys_state;
    switch (*state) {
    case ALINK_STATUS_INITED:
        break;
    case ALINK_STATUS_REGISTERED:
        sprintf(uuid, "%s", alink_get_uuid(NULL));
        printf("ALINK_STATUS_REGISTERED, mac %s uuid %s \n", mac,
               uuid);
        break;
    case ALINK_STATUS_LOGGED:
        sprintf(uuid, "%s", alink_get_uuid(NULL));
        printf("ALINK_STATUS_LOGGED, mac %s uuid %s\n", mac, uuid);
        break;
    default:
        break;
    }
    return 0;
}



static void alink_fill_deviceinfo(struct device_info *deviceinfo)
{   /*fill main device info here */
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
    printf("DEV_MODEL:%s \n", deviceinfo->model);
}

void alink_json(void *arg)
{
    struct device_info *main_dev;
    main_dev = platform_malloc(sizeof(struct device_info));
    alink_sample_mutex = platform_mutex_init();
    memset(main_dev, 0, sizeof(struct device_info));
    alink_fill_deviceinfo(main_dev);
    // alink_set_loglevel(ALINK_LL_DEBUG | ALINK_LL_INFO | ALINK_LL_WARN |
    //                    ALINK_LL_ERROR | ALINK_LL_DUMP);
    alink_set_loglevel(ALINK_LL_ERROR | ALINK_LL_ERROR | ALINK_LL_DUMP);
    main_dev->sys_callback[ALINK_FUNC_SERVER_STATUS] = alink_handler_systemstates_callback;


    /*start alink-sdk */

    main_dev->dev_callback[ACB_GET_DEVICE_STATUS] = main_dev_get_device_status_callback;
    main_dev->dev_callback[ACB_SET_DEVICE_STATUS] = main_dev_set_device_status_callback;
    alink_start(main_dev);  /*register main device here */

    platform_free(main_dev);

    printf("wait main device login");
    /*wait main device login, -1 means wait forever */
    alink_wait_connect(NULL, ALINK_WAIT_FOREVER);

    while (sample_running) {
        alink_device_post_data(NULL);
        platform_msleep(500);
    }

    alink_end();
    platform_mutex_destroy(alink_sample_mutex);
    vTaskDelete(NULL);
}

