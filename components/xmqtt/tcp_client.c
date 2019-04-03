#include "tcp_client.h"

struct sockaddr_in serverAddr;
int connect_socket = -1;

static int esp_read(int my_socket, unsigned char *buffer, unsigned int len, unsigned int timeout_ms)
{
    portTickType xTicksToWait = timeout_ms / portTICK_RATE_MS; /* convert milliseconds to ticks */
    xTimeOutType xTimeOut;
    int recvLen = 0;
    int rc = 0;

    struct timeval timeout;
    fd_set fdset;

    FD_ZERO(&fdset);
    FD_SET(my_socket, &fdset);

    timeout.tv_sec = 0;
    timeout.tv_usec = timeout_ms * 1000;

    vTaskSetTimeOutState(&xTimeOut); /* Record the time at which this function was entered. */

    if (select(my_socket + 1, &fdset, NULL, NULL, &timeout) > 0)
    {
        if (FD_ISSET(my_socket, &fdset))
        {
            do
            {
                rc = recv(my_socket, buffer + recvLen, len - recvLen, MSG_DONTWAIT);

                if (rc > 0)
                {
                    recvLen += rc;
                }
                else if (rc <= 0)
                {
                    //recvLen = rc; // 删除 bug
                    //printf("recvLen <0 = %d\r\n",recvLen);
                    break;
                }
            } while (recvLen < len && xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait) == pdFALSE);
        }
    }

    //printf("recvLen = %d\r\n",recvLen);

    return recvLen;
}

static int esp_write(int my_socket, unsigned char *buffer, unsigned int len, unsigned int timeout_ms)
{
    portTickType xTicksToWait = timeout_ms / portTICK_RATE_MS; /* convert milliseconds to ticks */
    xTimeOutType xTimeOut;
    int sentLen = 0;
    int rc = 0;

    struct timeval timeout;
    fd_set fdset;

    FD_ZERO(&fdset);
    FD_SET(my_socket, &fdset);

    timeout.tv_sec = 0;
    timeout.tv_usec = timeout_ms * 1000;

    vTaskSetTimeOutState(&xTimeOut); /* Record the time at which this function was entered. */

    if (select(my_socket + 1, NULL, &fdset, NULL, &timeout) > 0)
    {
        if (FD_ISSET(my_socket, &fdset))
        {
            do
            {
                rc = send(my_socket, buffer + sentLen, len - sentLen, MSG_DONTWAIT);

                if (rc > 0)
                {
                    sentLen += rc;
                }
                else if (rc <= 0)
                {
                    //sentLen = rc;
                    break;
                }
            } while (sentLen < len && xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait) == pdFALSE);
        }
    }

    return sentLen;
}

int tcp_send(int connect_socket, uint8_t *buff, int len, unsigned int timeout_ms)
{
    return esp_write(connect_socket, buff, len, timeout_ms);
}

int tcp_receive(int connect_socket, uint8_t *buff, int len, unsigned int timeout_ms)
{
    return esp_read(connect_socket, buff, len, timeout_ms);
}

int mqtt_tcp_disconnect(void)
{
    shutdown(connect_socket, SHUT_RDWR);
    return close(connect_socket);
}

int mqtt_tcp_connect(const char *host, const int port)
{
    int ret;
    struct addrinfo hints, *addr_list, *cur;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(host, NULL, &hints, &addr_list);
    if (ret != 0)
    {
        printf("DNS parsing failed!\r\n");
        return -1;
    }

    for (cur = addr_list; cur != NULL; cur = cur->ai_next)
    {
        char ip[128];
        inet_ntop(AF_INET, &(((struct sockaddr_in *)cur->ai_addr)->sin_addr), ip, 128);

        //printf("DNS IP: %s:%d\r\n", ip, port);
        connect_socket = socket(AF_INET, SOCK_STREAM, 0); // 创建套接字描述符
        if (connect_socket < 0)
        {
            ret = -1;
            printf("socket created failed!\r\n");
            continue;
        }

        bzero(&serverAddr, sizeof(struct sockaddr_in)); // 清零
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr((const char *)ip);
        //serverAddr.sin_addr.s_addr = inet_addr((const char*)("192.168.1.124"));
        serverAddr.sin_port = htons(port); // 默认以8080端口连接

        if (connect(connect_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == 0)
        {
            //printf("connect server success!\r\n");
            ret = 0;
            break;
        }
        else
        {
            printf("error = %d\r\n", errno); // #define EHOSTUNREACH 113 // No route to host 无法路由到主机
        }
        close(connect_socket);
        ret = -1;
    }
    if (cur == NULL)
        ret = -1;

    freeaddrinfo(addr_list);

    return ret;
}