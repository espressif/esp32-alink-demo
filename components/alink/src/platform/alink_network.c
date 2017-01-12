#include <esp_types.h>
#include "string.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/netdb.h"
#include "esp_system.h"

#include "platform/platform.h"
#include "tcpip_adapter.h"

#define SOMAXCONN 5

static const char *TAG = "alink_network";
#define require_action_exit(con, msg, ...) if(con) {ESP_LOGE(TAG, msg, ##__VA_ARGS__); esp_restart();}
#define require_action_NULL(con, msg, ...) if(con) {ESP_LOGE(TAG, msg, ##__VA_ARGS__); return NULL;}

static int network_create_socket( pplatform_netaddr_t netaddr, int type, struct sockaddr_in *paddr, int *psock)
{
    require_action_exit(netaddr == NULL, "[%s, %d]:Parameter error netaddr == NULL", __func__, __LINE__);
    require_action_exit(paddr == NULL, "[%s, %d]:Parameter error paddr == NULL", __func__, __LINE__);
    require_action_exit(psock == NULL, "[%s, %d]:Parameter error psock == NULL", __func__, __LINE__);

    struct hostent *hp;
    uint32_t ip;

    if (NULL == netaddr->host) {
        ip = htonl(INADDR_ANY);
    } else {
        printf("netaddr->host:%s\n", netaddr->host);
        hp = gethostbyname(netaddr->host);
        if (!hp) {
            ESP_LOGE(TAG, "can't resolute the host address");
            return -1;
        }
        struct ip4_addr *ip4_addr = (struct ip4_addr *)hp->h_addr;
        char ipaddr_tmp[64] = {0};
        sprintf(ipaddr_tmp, IPSTR, IP2STR(ip4_addr));
        printf("ip: %s\n", ipaddr_tmp);
        ip = inet_addr(ipaddr_tmp);
    }

    *psock = socket(AF_INET, type, 0);
    if (*psock < 0) return -1;

    memset(paddr, 0, sizeof(struct sockaddr_in));

    int opt_val = 1;
    if (0 != setsockopt(*psock, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val))) {
        // close((int)*psock);
        ESP_LOGW(TAG, "[%s, %d]:setsockopt SO_REUSEADDR", __func__, __LINE__);
        perror("setsockopt:");
        // return -1;
    }

    printf("ip:%d\n", ip);
    paddr->sin_addr.s_addr = ip;
    paddr->sin_family = AF_INET;
    paddr->sin_port = htons( netaddr->port );
    return 0;
}

void *platform_udp_server_create(_IN_ uint16_t port)
{
    struct sockaddr_in addr;
    int server_socket;
    platform_netaddr_t netaddr = {NULL, port};

    if (0 != network_create_socket(&netaddr, SOCK_DGRAM, &addr, &server_socket)) {
        ESP_LOGE(TAG, "[%s, %d]:create socket", __func__, __LINE__);
        return NULL;
    }

    if (-1 == bind(server_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) {
        platform_udp_close((void *)server_socket);
        ESP_LOGE(TAG, "[%s, %d]:socket bind", __func__, __LINE__);
        perror("socket bind");
        return NULL;
    }

    return (void *)server_socket;
}

void *platform_udp_client_create(void)
{
    struct sockaddr_in addr;
    int sock;
    platform_netaddr_t netaddr = {NULL, 0};

    if (0 != network_create_socket(&netaddr, SOCK_DGRAM, &addr, &sock)) {
        ESP_LOGE(TAG, "[%s, %d]:create socket", __func__, __LINE__);
        perror("create socket");
        return NULL;
    }

    return (void *)sock;
}

void *platform_udp_multicast_server_create(pplatform_netaddr_t netaddr)
{
    require_action_exit(netaddr == NULL, "[%s, %d]:Parameter error netaddr == NULL", __func__, __LINE__);
    int option = 1;
    struct sockaddr_in addr;
    int sock;
    struct ip_mreq mreq;

    platform_netaddr_t netaddr_client = {NULL, netaddr->port};

    memset(&addr, 0, sizeof(addr));
    memset(&mreq, 0, sizeof(mreq));

    if (0 != network_create_socket(&netaddr_client, SOCK_DGRAM, &addr, &sock)) {
        ESP_LOGE(TAG, "[%s, %d]:create socket", __func__, __LINE__);
        return NULL;
    }

    if (-1 == bind(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {
        ESP_LOGE(TAG, "[%s, %d]:socket bind", __func__, __LINE__);
        perror("socket bind");
        platform_udp_close((void *)sock);
        return NULL;
    }

    mreq.imr_multiaddr.s_addr = inet_addr(netaddr->host);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof(mreq)) < 0) {
        ESP_LOGE(TAG, "[%s, %d]:setsockopt IP_ADD_MEMBERSHIP", __func__, __LINE__);
        platform_udp_close((void *)sock);
        return NULL;
    }
    return (void *)sock;
}

void platform_udp_close(void *handle)
{
    close((int)handle);
}


int platform_udp_sendto(
    _IN_ void *handle,
    _IN_ const char *buffer,
    _IN_ uint32_t length,
    _IN_ pplatform_netaddr_t netaddr)
{
    require_action_exit((int)handle < 0, "[%s, %d]:Parameter error handle < 0", __func__, __LINE__);
    require_action_exit(buffer == NULL, "[%s, %d]:Parameter error buffer == NULL", __func__, __LINE__);
    require_action_exit(netaddr == NULL, "[%s, %d]:Parameter error netaddr == NULL", __func__, __LINE__);
    int ret_code;
    struct hostent *hp;
    struct sockaddr_in addr;

    if (NULL == (hp = gethostbyname(netaddr->host))) {
        ESP_LOGE(TAG, "[%s, %d]:gethostbyname", __func__, __LINE__);
        printf("Can't resolute the host address \n");
        return -1;
    }

    addr.sin_addr.s_addr = *((u_int *)(hp->h_addr));
    //addr.sin_addr.S_un.S_addr = *((u_int *)(hp->h_addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons( netaddr->port );

    ret_code = sendto((int)handle,
                      buffer,
                      length,
                      0,
                      (struct sockaddr *)&addr,
                      sizeof(struct sockaddr_in));

    return (ret_code) > 0 ? ret_code : -1;
}


int platform_udp_recvfrom(
    _IN_ void *handle,
    _OUT_ char *buffer,
    _IN_ uint32_t length,
    _OUT_OPT_ pplatform_netaddr_t netaddr)
{
    require_action_exit((int)handle < 0, "[%s, %d]:Parameter error handle < 0", __func__, __LINE__);
    require_action_exit(buffer == NULL, "[%s, %d]:Parameter error buffer == NULL", __func__, __LINE__);
    // require_action_exit(netaddr == NULL, "[%s, %d]:Parameter error netaddr == NULL", __func__, __LINE__);
    int ret_code;
    struct sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    ret_code = recvfrom((int)handle, buffer, length, 0, (struct sockaddr *)&addr, &addr_len);
    if (ret_code > 0) {
        if (NULL != netaddr) {
            netaddr->port = ntohs(addr.sin_port);
            if (NULL != netaddr->host) {
                strcpy(netaddr->host, inet_ntoa(addr.sin_addr));
            }
        }
        return ret_code;
    }

    return -1;
}


void *platform_tcp_server_create(_IN_ uint16_t port)
{
    struct sockaddr_in addr;
    int server_socket;
    platform_netaddr_t netaddr = {NULL, port};

    if (0 != network_create_socket(&netaddr, SOCK_STREAM, &addr, &server_socket)) {
        ESP_LOGE(TAG, "[%s, %d]:create socket", __func__, __LINE__);
        return NULL;
    }

    if (-1 == bind(server_socket, (struct sockaddr *)&addr, sizeof(addr))) {
        ESP_LOGE(TAG, "[%s, %d]:bind", __func__, __LINE__);
        platform_tcp_close((void *)server_socket);
        return NULL;
    }

    if (0 != listen(server_socket, SOMAXCONN)) {
        ESP_LOGE(TAG, "[%s, %d]:listen", __func__, __LINE__);
        platform_tcp_close((void *)server_socket);
        return NULL;
    }

    return (void *)server_socket;
}


void *platform_tcp_server_accept(_IN_ void *server)
{
    require_action_exit(server == NULL, "[%s, %d]:Parameter error server == NULL", __func__, __LINE__);
    struct sockaddr_in addr;
    unsigned int addr_length = sizeof(addr);
    int new_client;

    if ((new_client = accept((int)server, (struct sockaddr*)&addr, &addr_length)) <= 0) {
        ESP_LOGE(TAG, "[%s, %d]:accept", __func__, __LINE__);
        return NULL;
    }

    return (void *)new_client;
}




void *platform_tcp_client_connect(_IN_ pplatform_netaddr_t netaddr)
{
    require_action_exit(netaddr == NULL, "[%s, %d]:Parameter error netaddr == NULL", __func__, __LINE__);
    struct sockaddr_in addr;
    int sock;

    if (0 != network_create_socket(netaddr, SOCK_STREAM, &addr, &sock)) {
        ESP_LOGE(TAG, "[%s, %d]:create socket", __func__, __LINE__);
        return NULL;
    }

    if (-1 == connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {
        ESP_LOGE(TAG, "[%s, %d]:connect", __func__, __LINE__);
        platform_tcp_close((void *)sock);
        return NULL;
    }

    return (void *)sock;
}



int platform_tcp_send(_IN_ void *handle, _IN_ const char *buffer, _IN_ uint32_t length)
{
    require_action_exit((int)handle < 0, "[%s, %d]:Parameter error handle < 0", __func__, __LINE__);
    require_action_exit(buffer == NULL, "[%s, %d]:Parameter error buffer == NULL", __func__, __LINE__);
    int bytes_sent;

    bytes_sent = send((int)handle, buffer, length, 0);
    return bytes_sent > 0 ? bytes_sent : -1;
}



int platform_tcp_recv(_IN_ void *handle, _OUT_ char *buffer, _IN_ uint32_t length)
{
    require_action_exit((int)handle < 0, "[%s, %d]:Parameter error handle < 0", __func__, __LINE__);
    require_action_exit(buffer == NULL, "[%s, %d]:Parameter error buffer == NULL", __func__, __LINE__);
    int bytes_received;

    bytes_received = recv((int)handle, buffer, length, 0);

    return bytes_received > 0 ? bytes_received : -1;
}


void platform_tcp_close(_IN_ void *handle)
{
    close((int)handle);
    //WSACleanup( );
}


int platform_select(void *read_fds[PLATFORM_SOCKET_MAXNUMS],
                    void *write_fds[PLATFORM_SOCKET_MAXNUMS],
                    int timeout_ms)
{
    require_action_exit(read_fds == NULL, "[%s, %d]:Parameter error read_fds == NULL", __func__, __LINE__);
    // require_action_exit(write_fds == NULL, "[%s, %d]:Parameter error write_fds == NULL", __func__, __LINE__);
    int i, ret_code = -1;
    struct timeval timeout_value;
    struct timeval *ptimeval = &timeout_value;
    fd_set *pfd_read_set, *pfd_write_set;

    if (PLATFORM_WAIT_INFINITE == timeout_ms) {
        ptimeval = NULL;
    } else {
        ptimeval->tv_sec = timeout_ms / 1000;
        ptimeval->tv_usec = (timeout_ms % 1000) * 1000;
    }
    pfd_read_set = NULL;
    pfd_write_set = NULL;

    if (NULL != read_fds) {
        pfd_read_set = malloc(sizeof(fd_set));
        if (NULL == pfd_read_set) {
            ESP_LOGE(TAG, "[%s, %d]:pfd_read_set", __func__, __LINE__);
            goto do_exit;
        }

        FD_ZERO(pfd_read_set);

        for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i )
        {
            if ( PLATFORM_INVALID_FD != read_fds[i] )
            {
                FD_SET((int)read_fds[i], pfd_read_set);
            }
        }
    }

    if (NULL != write_fds)
    {
        pfd_write_set = malloc(sizeof(fd_set));
        if (NULL == pfd_write_set) {
            ESP_LOGE(TAG, "[%s, %d]:pfd_write_set", __func__, __LINE__);
            goto do_exit;
        }

        FD_ZERO(pfd_write_set);

        for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i ) {
            if ( PLATFORM_INVALID_FD != write_fds[i] ) {
                FD_SET((int)write_fds[i], pfd_write_set);
            }
        }
    }
    ret_code = select(FD_SETSIZE, pfd_read_set, pfd_write_set, NULL, ptimeval);
    if (ret_code >= 0) {
        if (NULL != read_fds) {
            for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i ) {
                if (PLATFORM_INVALID_FD != read_fds[i]
                        && !FD_ISSET((int)read_fds[i], pfd_read_set)) {

                    read_fds[i] = PLATFORM_INVALID_FD;
                }
            }
        }

        if (NULL != write_fds) {
            for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i ) {
                if (PLATFORM_INVALID_FD != write_fds[i]
                        && !FD_ISSET((int)write_fds[i], pfd_write_set)) {
                    write_fds[i] = PLATFORM_INVALID_FD;
                }
            }
        }
    }

do_exit:
    if (pfd_read_set) free(pfd_read_set);
    if (pfd_write_set) free(pfd_write_set);

    return (ret_code >= 0) ? ret_code : -1;
}
