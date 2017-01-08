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
#include "platform/platform.h"
#include "tcpip_adapter.h"

#define SOMAXCONN 5

#define SOCKET_ERROR (-1)
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

static int network_create_socket( pplatform_netaddr_t netaddr, int type, struct sockaddr_in *paddr, long *psock)
{
    struct hostent *hp;
    uint32_t ip;
    int opt_val = 1;
    printf("free heap_size: %d\n", uxTaskGetStackHighWaterMark(NULL));
    if ((NULL == paddr) || (NULL == psock))
        return -1;

    if (NULL == netaddr->host) {
        ip = htonl(INADDR_ANY);
    } else {
        // if (NULL == (hp = gethostbyname(netaddr->host))){
        //     printf("can't resolute the host address \n");
        //     return -1;
        // }
        // ip = *((uint32_t *)hp->h_addr);

        printf("netaddr->host:%s\n", netaddr->host);
        hp = gethostbyname(netaddr->host);
        if (!hp) {
            printf("can't resolute the host address \n");
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

    // if (0 != setsockopt(*psock, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val)))
    // {
    //     close((int)*psock);
    //     return -1;
    // }

    //paddr->sin_addr.S_un.S_addr = ip;
    printf("ip:%d\n", ip);
    paddr->sin_addr.s_addr = ip;
    paddr->sin_family = AF_INET;
    paddr->sin_port = htons( netaddr->port );
    return 0;
}

void *platform_udp_server_create(_IN_ uint16_t port)
{
    struct sockaddr_in addr;
    long server_socket;
    platform_netaddr_t netaddr = {NULL, port};

    if (0 != network_create_socket(&netaddr, SOCK_DGRAM, &addr, &server_socket))
    {
        return NULL;
    }
    // printf("=== [%s, %d] ===\n", __func__, __LINE__);

    if (-1 == bind(server_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)))
    {
        platform_udp_close((void *)server_socket);
        return NULL;
    }
    // printf("=== [%s, %d] ===\n", __func__, __LINE__);

    return (void *)server_socket;
}

void *platform_udp_client_create(void)
{
    // printf("=== [%s, %d] ===\n", __func__, __LINE__);
    struct sockaddr_in addr;
    long sock;
    platform_netaddr_t netaddr = {NULL, 0};

    if (0 != network_create_socket(&netaddr, SOCK_DGRAM, &addr, &sock))
    {
        return NULL;
    }

    return (void *)sock;
}

void *platform_udp_multicast_server_create(pplatform_netaddr_t netaddr)
{
    // printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int option = 1;
    struct sockaddr_in addr;
    long sock;
    struct ip_mreq mreq;

    platform_netaddr_t netaddr_client = {NULL, netaddr->port};

    memset(&addr, 0, sizeof(addr));
    memset(&mreq, 0, sizeof(mreq));

    if (0 != network_create_socket(&netaddr_client, SOCK_DGRAM, &addr, &sock))
    {
        return NULL;
    }

    /* allow multiple sockets to use the same PORT number */
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option)) < 0)
    {
        printf("Reusing ADDR failed\n");

        platform_udp_close((void *)sock);
        //do something.
        return NULL;
    }

    if (-1 == bind(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)))
    {
        printf("bind error...\n");
        platform_udp_close((void *)sock);
        return NULL;
    }

    mreq.imr_multiaddr.s_addr = inet_addr(netaddr->host);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof(mreq)) < 0)
    {
        printf("setsockopt error \n");
        platform_udp_close((void *)sock);
        return NULL;
    }

    return (void *)sock;
}

void platform_udp_close(void *handle)
{
    close((long)handle);
}


int platform_udp_sendto(
    _IN_ void *handle,
    _IN_ const char *buffer,
    _IN_ uint32_t length,
    _IN_ pplatform_netaddr_t netaddr)
{
    int ret_code;
    struct hostent *hp;
    struct sockaddr_in addr;

    if (NULL == (hp = gethostbyname(netaddr->host)))
    {
        printf("Can't resolute the host address \n");
        return -1;
    }

    addr.sin_addr.s_addr = *((u_long *)(hp->h_addr));
    //addr.sin_addr.S_un.S_addr = *((u_long *)(hp->h_addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons( netaddr->port );

    ret_code = sendto((long)handle,
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
    // printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int ret_code;
    struct sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    ret_code = recvfrom((long)handle, buffer, length, 0, (struct sockaddr *)&addr, &addr_len);
    if (ret_code > 0)
    {
        if (NULL != netaddr)
        {
            netaddr->port = ntohs(addr.sin_port);

            if (NULL != netaddr->host)
            {
                strcpy(netaddr->host, inet_ntoa(addr.sin_addr));
            }
        }
        return ret_code;
    }
    return -1;
}



void *platform_tcp_server_create(_IN_ uint16_t port)
{
    // printf("=== [%s, %d] ===\n", __func__, __LINE__);
    struct sockaddr_in addr;
    long server_socket;
    platform_netaddr_t netaddr = {NULL, port};

    if (0 != network_create_socket(&netaddr, SOCK_STREAM, &addr, &server_socket))
    {
        return NULL;
    }

    if (-1 == bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)))
    {
        platform_tcp_close((void *)server_socket);
        return NULL;
    }

    if (0 != listen(server_socket, SOMAXCONN))
    {
        platform_tcp_close((void *)server_socket);
        return NULL;
    }

    return (void *)server_socket;
}




void *platform_tcp_server_accept(_IN_ void *server)
{
    // printf("=== [%s, %d] ===\n", __func__, __LINE__);
    struct sockaddr_in addr;
    unsigned int addr_length = sizeof(addr);
    long new_client;

    if ((new_client = accept((long)server, (struct sockaddr*)&addr, &addr_length)) <= 0)
    {
        return NULL;
    }

    return (void *)new_client;
}




void *platform_tcp_client_connect(_IN_ pplatform_netaddr_t netaddr)
{
    // printf("=== [%s, %d] ===\n", __func__, __LINE__);
    struct sockaddr_in addr;
    long sock;

    // printf("=== [%s, %d] ===\n", __func__, __LINE__);
    if (0 != network_create_socket(netaddr, SOCK_STREAM, &addr, &sock))
    {
        return NULL;
    }

    // printf("=== [%s, %d] ===\n", __func__, __LINE__);
    if (-1 == connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)))
    {
        platform_tcp_close((void *)sock);
        return NULL;
    }

    return (void *)sock;
}



int platform_tcp_send(_IN_ void *handle, _IN_ const char *buffer, _IN_ uint32_t length)
{
    // printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int bytes_sent;

    bytes_sent = send((long)handle, buffer, length, 0);
    return bytes_sent > 0 ? bytes_sent : -1;
}



int platform_tcp_recv(_IN_ void *handle, _OUT_ char *buffer, _IN_ uint32_t length)
{
    // printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int bytes_received;

    bytes_received = recv((long)handle, buffer, length, 0);

    return bytes_received > 0 ? bytes_received : -1;
}




void platform_tcp_close(_IN_ void *handle)
{
    // printf("=== [%s, %d] ===\n", __func__, __LINE__);
    close((long)handle);
    //WSACleanup( );
}



int platform_select(void *read_fds[PLATFORM_SOCKET_MAXNUMS],
                    void *write_fds[PLATFORM_SOCKET_MAXNUMS],
                    int timeout_ms)
{
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
    printf("=== [%s, %d] ===\n", __func__, __LINE__);
    printf("tcp_fd: %p, server_cert: %p, server_cert_len: %d\n",
           tcp_fd , server_cert, server_cert_len);
    SSL *ssl;
    int socket = (int)tcp_fd;
    printf("socket: %d\n", socket);
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
    printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int cnt = 0;
    ALINK_ADAPTER_CONFIG_ASSERT(ssl != NULL);
    ALINK_ADAPTER_CONFIG_ASSERT(buffer != NULL);
    cnt = SSL_write((SSL*)ssl, buffer, length);
    return (cnt > 0) ? cnt : -1;
}

int platform_ssl_recv(_IN_ void *ssl, _OUT_ char *buffer, _IN_ int length)
{
    printf("=== [%s, %d] ===\n", __func__, __LINE__);
    int cnt = 0;
    ALINK_ADAPTER_CONFIG_ASSERT(ssl != NULL);
    ALINK_ADAPTER_CONFIG_ASSERT(buffer != NULL);
    cnt = SSL_read((SSL*)ssl, buffer, length);
    return cnt > 0 ? cnt : -1;
}

int platform_ssl_close(_IN_ void *ssl)
{
    int ret = -1;
    // ALINK_ADAPTER_CONFIG_ASSERT(ssl != NULL);
    ret = SSL_shutdown((SSL *)ssl);
    if (ret != 0) {
        return -1;
    }

    int fd = SSL_get_fd((SSL *)ssl);
    printf("=== [%s, %d] === ssl: %p ret: %d fd: %d\n", __func__, __LINE__, ssl, ret, fd);
    // ALINK_ADAPTER_CONFIG_ASSERT(fd <= 0);
    close(fd);

    if (ssl) {
        SSL_free(ssl);
        ssl = NULL;
    }

    if (ctx) {
        SSL_CTX_free(ctx);
        ctx = NULL;
    }

    return 0;
}






