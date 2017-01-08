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
#include "alink_export_rawdata.h"
#include "platform.h"
#include "product.h"
#include "assert.h"


#define SOFTAP_GATEWAY_IP		"172.31.254.250"
#define SOFTAP_TCP_SERVER_PORT		(65125)

#ifndef	info
#define info(format, ...)	printf(format, ##__VA_ARGS__)
#endif

#define	STR_SSID_LEN		(32 + 1)
#define STR_PASSWD_LEN		(64 + 1)
char aws_ssid[STR_SSID_LEN];
char aws_passwd[STR_PASSWD_LEN];
unsigned char aws_bssid[6];

/* json info parser */
int get_ssid_and_passwd(char *msg)
{
	char *ptr, *end, *name;
	int len;

	//ssid
	name = "\"ssid\":";
	ptr = strstr(msg, name);
	if (!ptr) {
		info("%s not found!\n", name);
		goto exit;
	}
	ptr += strlen(name);
	while (*ptr++ == ' ');/* eating the beginning " */
	end = strchr(ptr, '"');
	len = end - ptr;

	assert(len < sizeof(aws_ssid));
	strncpy(aws_ssid, ptr, len);
	aws_ssid[len] = '\0';

	//passwd
	name = "\"passwd\":";
	ptr = strstr(msg, name);
	if (!ptr) {
		info("%s not found!\n", name);
		goto exit;
	}

	ptr += strlen(name);
	while (*ptr++ == ' ');/* eating the beginning " */
	end = strchr(ptr, '"');
	len = end - ptr;

	assert(len < sizeof(aws_passwd));
	strncpy(aws_passwd, ptr, len);
	aws_passwd[len] = '\0';

	//bssid-mac
	name = "\"bssid\":";
	ptr = strstr(msg, name);
	if (!ptr) {
		info("%s not found!\n", name);
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
int aws_softap_tcp_server(void)
{
	struct sockaddr_in server, client;
	socklen_t socklen = sizeof(client);
	int fd = -1, connfd, len, ret;
	char *buf, *msg;
	int opt = 1, buf_size = 512, msg_size = 512;

	info("setup softap & tcp-server\n");

	buf = (char*)malloc(buf_size);
	msg = (char*)malloc(msg_size);
	assert(fd && msg);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd >= 0);

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(SOFTAP_GATEWAY_IP);
	server.sin_port = htons(SOFTAP_TCP_SERVER_PORT);

	// ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	// assert(!ret);

	ret = bind(fd, (struct sockaddr *)&server, sizeof(server));
	assert(!ret);

	ret = listen(fd, 10);
	assert(!ret);

	info("server %x %d created\n", ntohl(server.sin_addr.s_addr),
	     ntohs(server.sin_port));

	connfd = accept(fd, (struct sockaddr *)&client, &socklen);
	assert(connfd > 0);
	info("client %x %d connected!\n", ntohl(client.sin_addr.s_addr),
	     ntohs(client.sin_port));


	len = recvfrom(connfd, buf, buf_size, 0,
	               (struct sockaddr *)&client, &socklen);
	assert(len >= 0);

	buf[len] = 0;
	info("softap tcp server recv: %s\n", buf);

	char product_model[PRODUCT_MODEL_LEN];
	char device_mac[PLATFORM_MAC_LEN];
	product_get_model(product_model);
	platform_wifi_get_mac(device_mac);
	ret = get_ssid_and_passwd(buf);
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
	info("ack %s\n", msg);

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
 * DNS server: 172.31.254.250. 	IMPORTANT!!!  ios depend on it!
 * DHCP: enable
 * SSID: 32 ascii char at most
 * softap timeout: 5min
 */
void aws_softap_setup(void)
{
	/*
	Step1: Softap config
	*/
	char ssid[STR_SSID_LEN] = {0};
	char product_model[PRODUCT_MODEL_LEN];
	product_get_model(product_model);
	//ssid: max 32Bytes(excluding '\0')
	snprintf(ssid, STR_SSID_LEN, "alink_%s", product_model);
	wifi_config_t wifi_config;
	esp_wifi_get_config(WIFI_IF_AP, &wifi_config);
	memcpy(wifi_config.ap.ssid, ssid, sizeof(ssid));
	// memcpy(wifi_config.ap.password, DONGLE_WIFI_PWD, sizeof(DONGLE_WIFI_PWD));
	wifi_config.ap.ssid_len        = strlen(ssid);
	wifi_config.ap.channel         = 6;
	wifi_config.ap.max_connection  = 4;
	// wifi_config.ap.ssid_hidden     = 1;
	// wifi_config.ap.beacon_interval = 100;
	// printf("WiFi SSID: %s", wifi_config.ap.ssid);
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );
	ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_AP, &wifi_config) );
	ESP_ERROR_CHECK( esp_wifi_start() );

	/*
	Step2: Gateway ip config
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

void aws_softap_exit(void)
{
	ESP_ERROR_CHECK( tcpip_adapter_stop(TCPIP_ADAPTER_IF_AP) );
	ESP_ERROR_CHECK( esp_wifi_stop() );
}

int vendor_connect_ap(char *ssid, char *passwd);
void aws_connect_to_ap(char* ssid, char* password)
{
	printf("aws_connect_to ap ssid:%s,password:%s\n", ssid, password);
	vendor_connect_ap(ssid, password);
}

int aws_softap_main(void)
{
	printf("********ENTER SOFTAP MODE******\n");
	/* prepare and setup softap */
	aws_softap_setup();

	/* tcp server to get ssid & passwd */
	aws_softap_tcp_server();

	aws_softap_exit();

	aws_connect_to_ap(aws_ssid, aws_passwd);

	/* after dhcp ready, send notification to APP */
	aws_notify_app();

	return 0;
}

#endif
