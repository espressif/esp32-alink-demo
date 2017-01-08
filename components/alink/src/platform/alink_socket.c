#if 0
/**********  include  esp32**********/
#include <esp_types.h>
#include "string.h"
/**********  include  lwip**********/
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/netdb.h"
#include "openssl/ssl.h"

#include "adapter_layer_config.h"
#include "platform.h"

#define SOCKET_ERROR (-1)
#define TCP_SERVER_TASK_PRIOTY     (5)
#define TCP_SERVER_TASK_STACK_SIZE (2048)
#define TCP_RECV_BUFFER_LEN (2000)
#define TCP_SERVER_CONNECT_CLIENT_NUM_MAX (5)

#define SOCKET_CHECK_ERROR(func) \
    do{\
        if(func==SOCKET_ERROR){\
            ESP_LOG(ESP_ERROR_LEVEL,"SOCKET");\
        }\
    }while(0)


#define BUFFER_CHECK_ERROR(buffer) \
    do{\
        if(NULL==buffer){\
            ESP_LOG(ESP_ERROR_LEVEL,"BUFFER");}\
    } while (0)


/*---------------------------------------------------------------
                    SOCKET UDP
---------------------------------------------------------------*/



void *platform_udp_server_create(_IN_ uint16_t port)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int32_t server_id = 0;
    int32_t ret = 0;
    struct sockaddr_in remote_info;
    struct sockaddr_in server_info;
    socklen_t len = sizeof(remote_info);
    server_id = socket(AF_INET, SOCK_DGRAM, 0);
    SOCKET_CHECK_ERROR(server_id);
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(port);
    server_info.sin_addr.s_addr = (u32_t)INADDR_ANY; //inet_addr("192.168.1.1");
    server_info.sin_len = sizeof(server_info);
    SOCKET_CHECK_ERROR(bind(server_id, (struct sockaddr*)&server_info, sizeof(server_info)));
    if (-1 != server_id) {
        return (void*)(server_id + 1);
    }
    return NULL;

}
void *platform_udp_client_create(void)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int32_t client_id = 0;
    client_id = socket(AF_INET, SOCK_DGRAM, 0);
    SOCKET_CHECK_ERROR(client_id);
    if (-1 != client_id) {
        return (void*)(client_id + 1);
    }
    return NULL;
}
void *platform_udp_multicast_server_create(_IN_ platform_netaddr_t *netaddr)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int32_t server_id = 0;
    int32_t ret = 0;
    struct sockaddr_in remote_info;
    struct sockaddr_in server_info;
    socklen_t len = sizeof(remote_info);
    server_id = socket(AF_INET, SOCK_DGRAM, 0);
    SOCKET_CHECK_ERROR(server_id);
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(netaddr->port);
    server_info.sin_addr.s_addr = inet_addr(netaddr->host);
    server_info.sin_len = sizeof(server_info);
    SOCKET_CHECK_ERROR(bind(server_id, (struct sockaddr*)&server_info, sizeof(server_info)));
    return (void*)(server_id + 1);
}
void platform_udp_close(_IN_ void *handle)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    ALINK_ADAPTER_CONFIG_ASSERT(handle);
    close(*(int32_t *)(handle));
}

int platform_udp_sendto(
    _IN_ void *handle,
    _IN_ const char *buffer,
    _IN_ uint32_t length,
    _IN_ platform_netaddr_t *netaddr)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    struct sockaddr_in remote_info;
    int socket = *((int *)handle);
    SOCKET_CHECK_ERROR(socket);
    BUFFER_CHECK_ERROR(buffer);
    struct hostent *hp;
    if (NULL == (hp = gethostbyname(netaddr->host)))
    {
        ets_printf("Can't resolute the host address \n");
        return -1;
    }
    int data_len = 0;
    socklen_t len = sizeof(remote_info);
    remote_info.sin_family = AF_INET;
    remote_info.sin_port = htons(netaddr->port);
    remote_info.sin_addr.s_addr =  *((u_long *)(hp->h_addr));; //inet_addr("192.168.1.1");
    remote_info.sin_len = sizeof(remote_info);
    data_len = sendto(socket, buffer, length, 0,
                      (struct sockaddr*)&remote_info, len);
    return data_len;
}
int platform_udp_recvfrom(
    _IN_ void *handle,
    _OUT_ char *buffer,
    _IN_ uint32_t length,
    _OUT_ platform_netaddr_t *netaddr)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    struct sockaddr_in remote_info;
    socklen_t len = sizeof(remote_info);
    int socket = *((int *)handle);
    SOCKET_CHECK_ERROR(socket);
    BUFFER_CHECK_ERROR(buffer);
    int data_len = 0;
    data_len = recvfrom(socket, buffer, length, 0, (struct sockaddr*)&remote_info, &len);
    if (NULL != netaddr) {
        memcpy(netaddr->host, inet_ntoa(remote_info.sin_addr.s_addr), strlen(inet_ntoa(remote_info.sin_addr.s_addr)) + 1);
        netaddr->port = ntohs(remote_info.sin_port);
    }
    return data_len;
}
/*---------------------------------------------------------------
                        SOCKET  TCP
---------------------------------------------------------------*/
void *platform_tcp_server_create(_IN_ uint16_t port)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int tcp_server = -1;
    struct sockaddr_in tcp_server_info;
    int tcp_client = -1;
    struct sockaddr_in tcp_client_info;
    socklen_t len = sizeof(tcp_client_info);
    memset(&tcp_server_info, 0, sizeof(tcp_server_info));
    tcp_server_info.sin_family = AF_INET;
    tcp_server_info.sin_port = htons(port);
    tcp_server_info.sin_addr.s_addr = (u32_t)INADDR_ANY; //inet_addr("192.168.1.1");
    tcp_server_info.sin_len = sizeof(tcp_server_info);
    int ret = 0;
    tcp_server = socket(AF_INET, SOCK_STREAM, 0);
    SOCKET_CHECK_ERROR(tcp_server);
    SOCKET_CHECK_ERROR(bind(tcp_server, (struct sockaddr*)&tcp_server_info, sizeof(tcp_server_info)));
    SOCKET_CHECK_ERROR(listen(tcp_server, TCP_SERVER_CONNECT_CLIENT_NUM_MAX)); //参数5，表示最大连接数为3
    return (void *)(tcp_server + 1);
}

void *platform_tcp_server_accept(_IN_ void *server)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int socket_id = -1;
    int tcp_server = *((int *)server);

    socket_id = accept(tcp_server, NULL, 0);
    if (socket_id != -1) {
        return (void*)(socket_id + 1);
    }
    ESP_LOG(ESP_ERROR_LEVEL, " Tcp Server accept");
    return NULL;
}

void *platform_tcp_client_connect(_IN_ platform_netaddr_t *netaddr)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int32_t client_id = 0;
    int32_t ret = 0;

    struct sockaddr_in remote_info;
    socklen_t len = sizeof(remote_info);
    client_id = socket(AF_INET, SOCK_STREAM, 0);
    SOCKET_CHECK_ERROR(client_id);
    remote_info.sin_family = AF_INET;
    remote_info.sin_port = htons(netaddr->port);
    remote_info.sin_addr.s_addr = inet_addr(netaddr->host); //inet_addr("192.168.1.1");
    remote_info.sin_len = sizeof(remote_info);
    do {
        ret = connect(client_id, (struct sockaddr*)&remote_info, len);
        SOCKET_CHECK_ERROR(ret);
        vTaskDelay(200 / portTICK_RATE_MS); //Wait 200ms
    } while (ret == -1);
    return (void*)(client_id + 1);
}

int platform_tcp_send(_IN_ void *handle, _IN_ const char *buffer, _IN_ uint32_t length)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int data_len = 0;
    int socket = *((int *)handle) - 1;
    SOCKET_CHECK_ERROR(socket);
    BUFFER_CHECK_ERROR(buffer);
    data_len = send(socket, buffer, length, 0);
    return data_len;
}

int platform_tcp_recv(_IN_ void *handle, _OUT_ char *buffer, _IN_ uint32_t length)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int socket = *((int*)(handle)) - 1;
    int data_len = 0;
    SOCKET_CHECK_ERROR(socket);
    BUFFER_CHECK_ERROR(buffer);
    data_len = recv(socket, buffer, length, 0);
    return data_len;
}
void platform_tcp_close(_IN_ void *handle)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int socket = *((int*)(handle)) - 1;
    SOCKET_CHECK_ERROR(socket);
    close(socket);
}


int platform_select(void *read_fds[PLATFORM_SOCKET_MAXNUMS],
                    void *write_fds[PLATFORM_SOCKET_MAXNUMS],
                    int timeout_ms)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int i, ret_code = -1;
    struct timeval timeout_value;
    struct timeval *ptimeval = &timeout_value;
    fd_set *pfd_read_set, *pfd_write_set;

    if (PLATFORM_WAIT_INFINITE == timeout_ms)
    {
        ptimeval = NULL;
    }
    else
    {
        ptimeval->tv_sec = timeout_ms / 1000;
        ptimeval->tv_usec = (timeout_ms % 1000) * 1000;
    }
    pfd_read_set = NULL;
    pfd_write_set = NULL;

    if (NULL != read_fds)
    {
        pfd_read_set = malloc(sizeof(fd_set));
        if (NULL == pfd_read_set)
        {
            goto do_exit;
        }

        FD_ZERO(pfd_read_set);

        for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i )
        {
            if ( PLATFORM_INVALID_FD != read_fds[i] )
            {
                FD_SET((long)read_fds[i], pfd_read_set);
            }
        }
    }

    if (NULL != write_fds)
    {
        pfd_write_set = malloc(sizeof(fd_set));
        if (NULL == pfd_write_set)
        {
            goto do_exit;
        }

        FD_ZERO(pfd_write_set);

        for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i )
        {
            if ( PLATFORM_INVALID_FD != write_fds[i] )
            {
                FD_SET((long)write_fds[i], pfd_write_set);
            }
        }
    }
    ret_code = select(FD_SETSIZE, pfd_read_set, pfd_write_set, NULL, ptimeval);
    if (ret_code >= 0)
    {
        if (NULL != read_fds)
        {
            for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i )
            {
                if (PLATFORM_INVALID_FD != read_fds[i]
                        && !FD_ISSET((long)read_fds[i], pfd_read_set))
                {
                    read_fds[i] = PLATFORM_INVALID_FD;
                }
            }
        }

        if (NULL != write_fds)
        {
            for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i )
            {
                if (PLATFORM_INVALID_FD != write_fds[i]
                        && !FD_ISSET((long)write_fds[i], pfd_write_set))
                {
                    write_fds[i] = PLATFORM_INVALID_FD;
                }
            }
        }
    }

do_exit:
    if (NULL != pfd_read_set)
    {
        free(pfd_read_set);
    }

    if (NULL != pfd_write_set)
    {
        free(pfd_write_set);
    }

    return (ret_code >= 0) ? ret_code : -1;
}

/*---------------------------------------------------------------
                            SSL CODE
---------------------------------------------------------------*/
static SSL_CTX *ctx = NULL;

void *platform_ssl_connect(_IN_ void *tcp_fd, _IN_ const char *server_cert, _IN_ int server_cert_len)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    SSL *ssl;
    int socket = *((int*)tcp_fd);
    int ret = -1;
    ALINK_ADAPTER_CONFIG_ASSERT(tcp_fd != NULL);
    ctx = SSL_CTX_new(TLSv1_1_client_method());
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, socket);
    X509 *ca_cert = d2i_X509(NULL, (unsigned char *)server_cert, server_cert_len);
    ALINK_ADAPTER_CONFIG_ASSERT(ca_cert != NULL);
    ret = SSL_add_client_CA(ssl, ca_cert);
    ALINK_ADAPTER_CONFIG_ASSERT(ret != 0);
    ret = SSL_connect(ssl);
    SOCKET_CHECK_ERROR(ret);
    return (void *)(ssl);
}
int platform_ssl_send(_IN_ void *ssl, _IN_ const char *buffer, _IN_ int length)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int cnt = 0;
    ALINK_ADAPTER_CONFIG_ASSERT(ssl != NULL);
    ALINK_ADAPTER_CONFIG_ASSERT(buffer != NULL);
    cnt = SSL_write((SSL*)ssl, buffer, length);
    return (cnt > 0) ? cnt : -1;
}
int platform_ssl_recv(_IN_ void *ssl, _OUT_ char *buffer, _IN_ int length)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int cnt = 0;
    ALINK_ADAPTER_CONFIG_ASSERT(ssl != NULL);
    ALINK_ADAPTER_CONFIG_ASSERT(buffer != NULL);
    cnt = SSL_read((SSL*)ssl, buffer, length);
    return cnt > 0 ? cnt : -1;
}
int platform_ssl_close(_IN_ void *ssl)
{
    ets_printf("=== [%s, %d] ===\n", __func__, __LINE__);
    ssl = (SSL*)ssl;
    int ret = 0;
    ALINK_ADAPTER_CONFIG_ASSERT(ssl != NULL);
    ret = SSL_shutdown((SSL*)ssl);
    if (ret != 0) {
        return -1;
    }
    SSL_free(ssl);
    ssl = NULL;
    int fd = SSL_get_fd(ssl);
    ALINK_ADAPTER_CONFIG_ASSERT(fd <= 0);
    ret = close(fd);
    if (ret < 0) {
        return -1;
    }
    SSL_CTX_free(ctx);
    ctx = NULL;
    return 0;
}

#endif
