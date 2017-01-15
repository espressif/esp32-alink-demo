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
//#include <stdio.h>
//#include <string.h>
#include "product/product.h"

// #define PASS_THROUGH

//TODO: update these product info
#define product_dev_name        "ALINKTEST"
#ifdef PASS_THROUGH
#define product_model		    "ALINKTEST_LIVING_LIGHT_SMARTLED_LUA"
#define product_key             "bIjq3G1NcgjSfF9uSeK2"
#define product_secret		    "W6tXrtzgQHGZqksvJLMdCPArmkecBAdcr2F5tjuF"
#else
#define product_model		    "ALINKTEST_LIVING_LIGHT_SMARTLED"
#define product_key             "ljB6vqoLzmP8fGkE6pon"
#define product_secret		    "YJJZjytOCXDhtQqip4EjWbhR95zTgI92RVjzjyZF"
#endif
#define product_debug_key       "dpZZEpm9eBfqzK7yVeLq"
#define product_debug_secret    "THnfRRsU5vu6g6m9X6uFyAjUWflgZ0iyGjdEneKm"

#define product_dev_sn          "12345678"
#define product_dev_version     "1.0.0"
#define product_dev_type        "LIGHT"
#define product_dev_category    "LIVING"
#define product_dev_manufacturer "ALINKTEST"
#define product_dev_cid         "2D0044000F47333139373038" // ÐèÒªÎ¨Ò» 

/* device info */
#define product_dev_ DEV_CATEGORY "LIVING"
#define DEV_TYPE "LIGHT"
#define DEV_MANUFACTURE "ALINKTEST"


char *product_get_name(char name_str[PRODUCT_NAME_LEN])
{
    return memcpy(name_str, product_dev_name, PRODUCT_NAME_LEN);
}

char *product_get_version(char ver_str[PRODUCT_VERSION_LEN])
{
    return memcpy(ver_str, product_dev_version, PRODUCT_VERSION_LEN);
}

char *product_get_model(char model_str[PRODUCT_MODEL_LEN])
{
    return memcpy(model_str, product_model, PRODUCT_MODEL_LEN);
}

char *product_get_key(char key_str[PRODUCT_KEY_LEN])
{
    return memcpy(key_str, product_key, PRODUCT_KEY_LEN);
}

char *product_get_secret(char secret_str[PRODUCT_SECRET_LEN])
{
    return memcpy(secret_str, product_secret, PRODUCT_SECRET_LEN);
}

char *product_get_debug_key(char key_str[PRODUCT_KEY_LEN])
{
    return memcpy(key_str, product_debug_key, PRODUCT_KEY_LEN);
}

char *product_get_debug_secret(char secret_str[PRODUCT_SECRET_LEN])
{
    return memcpy(secret_str, product_debug_secret, PRODUCT_SECRET_LEN);
}

char *product_get_sn(char sn_str[PRODUCT_SN_LEN])
{
    return memcpy(sn_str, product_dev_sn, PRODUCT_SN_LEN);
}


char *product_get_type(char type_str[STR_NAME_LEN])
{
    return memcpy(type_str, product_dev_type, STR_NAME_LEN);
}

char *product_get_category(char category_str[STR_NAME_LEN])
{
    return memcpy(category_str, product_dev_category, STR_NAME_LEN);
}

char *product_get_manufacturer(char manufacturer_str[STR_NAME_LEN])
{
    return memcpy(manufacturer_str, product_dev_manufacturer, STR_NAME_LEN);
}

char *product_get_cid(char cid_str[STR_CID_LEN])
{
	return memcpy(cid_str, product_dev_cid, STR_CID_LEN);
}



