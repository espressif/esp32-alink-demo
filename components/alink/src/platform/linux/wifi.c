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

#include <stdio.h>
#include <stdlib.h>
#include <netinet/ip.h>       // IP_MAXPACKET (65535)
#include <sys/time.h>
#include <net/ethernet.h>     // ETH_P_ALL
#include <net/if.h>	      // struct ifreq
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <assert.h>

#include "os/platform/platform.h"

#define IFNAME			"eth2"	//TODO: wlan0
#define info(format, ...)	printf(format, ##__VA_ARGS__)

/* socket handler to recv 80211 frame */
static int sock_fd;
/* buffer to hold the 80211 frame */
static char *ether_frame;



//一键配置超时时间, 建议超时时间1-3min, APP侧一键配置1min超时
int platform_awss_get_timeout_interval_ms(void)
{
    return 60 * 1000;
}

//一键配置每个信道停留时间, 建议200ms-400ms
int platform_awss_get_channelscan_interval_ms(void)
{
    return 200;
}


int open_socket(void)
{
	int fd;
#if 0
	if (getuid() != 0)
		err("root privilege needed!\n");
#endif
	//create a raw socket that shall sniff
	fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	assert(fd >= 0);

	struct ifreq ifr;
	int sockopt = 1;

	memset(&ifr, 0, sizeof(ifr));

	/* set interface to promiscuous mode */
	strncpy(ifr.ifr_name, IFNAME, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
		perror("ioctl(SIOCGIFFLAGS)");
		goto exit;
	}
	ifr.ifr_flags |= IFF_PROMISC;
	if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
		perror("ioctl(SIOCSIFFLAGS)");
		goto exit;
	}

	/* allow the socket to be reused - incase connection is closed prematurely */
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) < 0) {
		perror("setsockopt(SO_REUSEADDR)");
		goto exit;
	}

	/* bind to device */
	struct sockaddr_ll ll;

	memset(&ll, 0, sizeof(ll));
	ll.sll_family = PF_PACKET;
	ll.sll_protocol = htons(ETH_P_ALL);
	ll.sll_ifindex = if_nametoindex(IFNAME);
	if (bind(fd, (struct sockaddr *)&ll, sizeof(ll)) < 0) {
		perror("bind[PF_PACKET] failed");
		goto exit;
	}

	return fd;
exit:
	close(fd);
	exit(EXIT_FAILURE);
}

//wifi信道切换，信道1-13
void platform_awss_switch_channel(char primary_channel,
		char secondary_channel, char bssid[ETH_ALEN])
{
#define HAL_PRIME_CHNL_OFFSET_DONT_CARE	0
#define HAL_PRIME_CHNL_OFFSET_LOWER	1
#define HAL_PRIME_CHNL_OFFSET_UPPER	2
	int flag = (primary_channel <= 4
			? HAL_PRIME_CHNL_OFFSET_LOWER : HAL_PRIME_CHNL_OFFSET_UPPER);
	char buf[256];

	snprintf(buf, sizeof(buf), "echo \"%d %d 0\" > /proc/net/rtl8188eu/%s/monitor",
			primary_channel, flag, IFNAME);
	//ret = system(buf);
}

//进入monitor模式, 并做好一些准备工作，如
//设置wifi工作在默认信道6
//若是linux平台，初始化socket句柄，绑定网卡，准备收包
//若是rtos的平台，注册收包回调函数aws_80211_frame_handler()到系统接口
void platform_awss_open_monitor(platform_awss_recv_80211_frame_cb_t cb)
{
	char buf[256];
	int ret;

	snprintf(buf, sizeof(buf), "ifconfig %s up", IFNAME);
	ret = system(buf);

	snprintf(buf, sizeof(buf), "iwconfig %s mode monitor", IFNAME);
	ret = system(buf);

	platform_awss_switch_channel(6, 0, NULL);//switch to channel 6, default


	ether_frame = malloc(IP_MAXPACKET);
	assert(ether_frame);

	//TODO: create thread to recv 80211 frame
	//sock_fd = open_socket();
}

//退出monitor模式，回到station模式, 其他资源回收
void platform_awss_close_monitor(void)
{
	char buf[256];
	int ret;

	snprintf(buf, sizeof(buf), "ifconfig %s up", IFNAME);
	ret = system(buf);

	snprintf(buf, sizeof(buf), "iwconfig %s mode managed", IFNAME);
	ret = system(buf);

	free(ether_frame);
	close(sock_fd);
}

//TODO: platform specific
int connect_to_ap(char *ssid, char *passwd)
{
	char buf[256];
	int ret;

	snprintf(buf, sizeof(buf), "ifconfig %s up", IFNAME);
	ret = system(buf);

	snprintf(buf, sizeof(buf), "iwconfig %s mode managed", IFNAME);
	ret = system(buf);

	snprintf(buf, sizeof(buf), "wpa_cli -i %s status | grep 'wpa_state=COMPLETED'", IFNAME);
	do {
		ret = system(buf);
		usleep(100 * 1000);
	} while (ret);

	snprintf(buf, sizeof(buf), "udhcpc -i %s", IFNAME);
	ret = system(buf);

	//TODO: wait dhcp ready here

	return 0;
}


//TODO-wqb
int platform_alink_version(void) { return 10; }

char *platform_wifi_get_mac(char mac_str[PLATFORM_MAC_LEN])
{
    //TODO:
    strncpy(mac_str, "d8:96:e0:03:04:01", PLATFORM_MAC_LEN);
    return mac_str;
}

int platform_wifi_get_rssi_dbm(void)
{
    return 0;
}


uint32_t platform_wifi_get_ip(char ip_str[PLATFORM_IP_LEN])
{
    //TODO
    return 0;
}
