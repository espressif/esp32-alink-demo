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



/*
    softap tcp交互部分：目的为APP和设备通过tcp交换ssid和passwd信息
    1）softap流程下 wifi设备开启tcp server，等待APP进行连接
    2）APP连接上设备的tcp server后，发送ssid&passwd&bssid信息给wifi设备，wifi设备进行解析
    3）wifi设备连接通过active scan，获取无线网络加密方式，进行联网
    4）wifi设备联网成功后，发送配网成功的通知给APP。

    //修改记录
    0421：在广播信息里添加sn、mac字段，增加对私有云对接的支持
        绑定过程，设备端等待tcp连接超时时间为2min
    0727： 添加功能说明
    0813： 添加softap dns配置说明
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> /* for strncpy */
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include "lwip/sockets.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_log.h"

#include "alink_export.h"
#include "platform.h"
#include "product.h"

#include "esp_alink.h"
static const char *TAG = "alink_softap";
#define SOFTAP_GATEWAY_IP       "172.31.254.250"
#define SOFTAP_TCP_SERVER_PORT      (65125)
/* json info parser */
static int get_ssid_and_passwd(_IN_ char *msg, _OUT_ wifi_config_t  *wifi_config)
{
    char *ptr, *end, *name;
    int len;

    //ssid
    name = "\"ssid\":";
    ptr = strstr(msg, name);
    if (!ptr) {
        ALINK_LOGI("%s not found!", name);
        goto exit;
    }
    ptr += strlen(name);
    while (*ptr++ == ' ');/* eating the beginning " */
    end = strchr(ptr, '"');
    len = end - ptr;

    // assert(len < sizeof(aws_ssid));
    memcpy(wifi_config->sta.ssid, ptr, len);
    wifi_config->sta.ssid[len] = '\0';

    //passwd
    name = "\"passwd\":";
    ptr = strstr(msg, name);
    if (!ptr) {
        ALINK_LOGI("%s not found!", name);
        goto exit;
    }

    ptr += strlen(name);
    while (*ptr++ == ' ');/* eating the beginning " */
    end = strchr(ptr, '"');
    len = end - ptr;

    // assert(len < sizeof(wifi_config->sta.password));
    memcpy(wifi_config->sta.password, ptr, len);
    wifi_config->sta.password[len] = '\0';

    //bssid-mac
    name = "\"bssid\":";
    ptr = strstr(msg, name);
    if (!ptr) {
        ALINK_LOGI("%s not found!", name);
        goto exit;
    }

    ptr += strlen(name);
    while (*ptr++ == ' ');/* eating the beginning " */
    end = strchr(ptr, '"');
    len = end - ptr;

    return 0;
exit:
    return -1;
}

//setup softap server
static alink_err_t aws_softap_tcp_server(_OUT_ wifi_config_t *wifi_config)
{
    struct sockaddr_in server, client;
    socklen_t socklen = sizeof(client);
    int fd = -1, connfd, len, ret;
    char *buf, *msg;
    int opt = 1, buf_size = 512, msg_size = 512;

    ALINK_LOGI("setup softap & tcp-server\n");

    buf = (char*)malloc(buf_size);
    msg = (char*)malloc(msg_size);
    assert(fd && msg);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(SOFTAP_GATEWAY_IP);
    server.sin_port = htons(SOFTAP_TCP_SERVER_PORT);

    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // assert(!ret);

    ret = bind(fd, (struct sockaddr *)&server, sizeof(server));
    assert(!ret);

    ret = listen(fd, 10);
    assert(!ret);

    ALINK_LOGI("server %x %d created", ntohl(server.sin_addr.s_addr),
               ntohs(server.sin_port));

    connfd = accept(fd, (struct sockaddr *)&client, &socklen);
    assert(connfd > 0);
    ALINK_LOGI("client %x %d connected!", ntohl(client.sin_addr.s_addr),
               ntohs(client.sin_port));


    len = recvfrom(connfd, buf, buf_size, 0,
                   (struct sockaddr *)&client, &socklen);
    assert(len >= 0);

    buf[len] = 0;
    ALINK_LOGI("softap tcp server recv: %s", buf);

    char product_model[PRODUCT_MODEL_LEN];
    char device_mac[PLATFORM_MAC_LEN];
    product_get_model(product_model);
    platform_wifi_get_mac(device_mac);
    ret = get_ssid_and_passwd(buf, wifi_config);
    if (!ret) {
        snprintf(msg, buf_size,
                 "{\"code\":1000, \"msg\":\"format ok\", \"model\":\"%s\", \"mac\":\"%s\"}",
                 product_model, device_mac);
    } else {
        snprintf(msg, buf_size,
                 "{\"code\":2000, \"msg\":\"format error\", \"model\":\"%s\", \"mac\":\"%s\"}",
                 product_model, device_mac);
    }

    len = sendto(connfd, msg, strlen(msg), 0,
                 (struct sockaddr *)&client, socklen);
    assert(len >= 0);
    ALINK_LOGI("ack %s", msg);

    close(connfd);
    close(fd);

    free(buf);
    free(msg);

    return 0;
}


/*
 * wilress params: 11BGN
 * channel: auto, or 1, 6, 11
 * authentication: OPEN
 * encryption: NONE
 * gatewayip: 172.31.254.250, netmask: 255.255.255.0
 * DNS server: 172.31.254.250.  IMPORTANT!!!  ios depend on it!
 * DHCP: enable
 * SSID: 32 ascii char at most
 * softap timeout: 5min
 */
static void aws_softap_setup(void)
{
    /*
    Step1: Softap config
    */
    char ssid[32 + 1] = {0};
    char product_model[PRODUCT_MODEL_LEN];
    product_get_model(product_model);
    //ssid: max 32Bytes(excluding '\0')
    wifi_config_t wifi_config;
    esp_wifi_get_config(WIFI_IF_AP, &wifi_config);
    snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid), "alink_%s", product_model);
    wifi_config.ap.ssid_len        = strlen(ssid);
    wifi_config.ap.channel         = 6;
    wifi_config.ap.max_connection  = 4;
    // wifi_config.ap.ssid_hidden     = 1;
    // wifi_config.ap.beacon_interval = 100;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_AP, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    /*
    Step2: Gateway ip configuration
    */
    tcpip_adapter_ip_info_t ip_info;
    ip_info.ip.addr = ipaddr_addr("172.31.254.250");
    ip_info.gw.addr = ipaddr_addr("172.31.254.250");
    ip_info.netmask.addr = ipaddr_addr("255.255.255.0");

    tcpip_adapter_stop(TCPIP_ADAPTER_IF_AP);
    tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
    uint8_t ap_mac[6] = {0};
    esp_wifi_get_mac(ESP_IF_WIFI_AP, ap_mac);
    tcpip_adapter_start(TCPIP_ADAPTER_IF_AP, ap_mac, &ip_info);
}

static void aws_softap_exit(void)
{
    ESP_ERROR_CHECK( tcpip_adapter_stop(TCPIP_ADAPTER_IF_AP) );
    ESP_ERROR_CHECK( esp_wifi_stop() );
}

alink_err_t aws_softap_init(_OUT_ wifi_config_t * wifi_config)
{
    /* prepare and setup softap */
    aws_softap_setup();

    /* tcp server to get ssid & passwd */
    alink_err_t ret = aws_softap_tcp_server(wifi_config);
    ALINK_ERROR_CHECK(ret != ALINK_OK, ret, "aws_softap_tcp_server, ret:%x", ret);

    aws_softap_exit();
    return ALINK_OK;
}
