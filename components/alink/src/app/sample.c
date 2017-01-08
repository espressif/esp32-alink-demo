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


int get_device_state()
{
	int ret = 0;
	platform_mutex_lock(alink_sample_mutex);
	ret = device_status_change;
	platform_mutex_unlock(alink_sample_mutex);
	return ret;
}

int set_device_state(int state)
{
	platform_mutex_lock(alink_sample_mutex);
	device_status_change = state;
	platform_mutex_unlock(alink_sample_mutex);
	return state;
}



int sample_running = ALINK_TRUE;



/* this sample save cmd value to virtual_device*/
static int execute_cmd(const char *rawdata, int len)
{
	int ret = 0, i = 0;
	if (len < 1)
		ret = -1;
	for (i = 0; i < len; i++) {
		printf("%2x ", (unsigned char) rawdata[i]);
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

static int get_device_status(char *rawdata, int len)
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
int alink_device_post_raw_data(void)
{
	/* do your job here */
	int len = 8, ret = 0;
	char rawdata[8] = { 0 };
	if (get_device_state()) {
		printf("%s %d \n", __FUNCTION__, __LINE__);
		get_device_status(rawdata, len);
		printf("%s %d \n", __FUNCTION__, __LINE__);
		ret = alink_post_device_rawdata(rawdata, len);
		printf("%s %d \n", __FUNCTION__, __LINE__);
		set_device_state(0);
		if (ret) {
			printf("post failed!\n");
		} else {
			printf("dev post raw data success!\n");
		}
		printf("%s %d \n", __FUNCTION__, __LINE__);
	}
	/* do your job end */
	return ret;
}
int rawdata_get_callback(const char *in_rawdata, int in_len, char *out_rawdata, int *out_len)
{
	int ret = 0;
	printf("%s %d \n", __FUNCTION__, __LINE__);
	set_device_state(1);
	/*do your job end! */
	return ret;
}

int rawdata_set_callback(char *rawdata, int len)
{
	/* TODO: */
	/*get cmd from server, do your job here! */
	int ret = 0;
	printf("%s %d \n", __FUNCTION__, __LINE__);
	ret = execute_cmd(rawdata, len);
	/* do your job end! */
	set_device_state(1);
	return ret;
}



int alink_handler_systemstates_callback(void *dev_mac, void *sys_state)
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



void alink_fill_deviceinfo(struct device_info *deviceinfo)
{	/*fill main device info here */
	product_get_name(deviceinfo->name);
	product_get_sn(deviceinfo->sn);  // ²úÆ·×¢²á·½Ê½ Èç¹ûÊÇsn, ÄÇÃ´ÐèÒª±£ÕÏsnÎ¨Ò»
	product_get_key(deviceinfo->key);
	product_get_model(deviceinfo->model);
	product_get_secret(deviceinfo->secret);
	product_get_type(deviceinfo->type);
	product_get_version(deviceinfo->version);
	product_get_category(deviceinfo->category);
	product_get_manufacturer(deviceinfo->manufacturer);
	product_get_debug_key(deviceinfo->key_sandbox);
	product_get_debug_secret(deviceinfo->secret_sandbox);
	platform_wifi_get_mac(deviceinfo->mac);//²úÆ·×¢²ámacÎ¨Ò» or snÎ¨Ò»  Í³Ò»´óÐ´
	product_get_cid(deviceinfo->cid); // Ê¹ÓÃ½Ó¿Ú»ñÈ¡Î¨Ò»chipid,·ÀÎ±ÔìÉè±¸
	printf("DEV_MODEL:%s \n", deviceinfo->model);
}

// extern PRIVILEGED_DATA TCB_t * volatile pxCurrentTCB[ portNUM_PROCESSORS ] = { NULL };

void alink_demo()
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

	printf("wait main device login");
	/*wait main device login, -1 means wait forever */
	alink_wait_connect(NULL, ALINK_WAIT_FOREVER);

	while (sample_running) {
		printf("%s %d \n", __FUNCTION__, __LINE__);

		alink_device_post_raw_data();
		printf("%s %d \n", __FUNCTION__, __LINE__);
		// int count = pxCurrentTCB[ xPortGetCoreID() ]->uxCriticalNesting;
		// printf("uxCriticalNesting: %d ")
		// platform_msleep(1000);
		vTaskDelay(500/portTICK_RATE_MS);
		printf("%s %d \n", __FUNCTION__, __LINE__);
	}
	printf("=========== alink end ================");
	alink_end();
	platform_mutex_destroy(alink_sample_mutex);
}

