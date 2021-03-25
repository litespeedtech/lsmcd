#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_LEN  65536
int sock_fd = -1;
unsigned char buffer[BUFFER_LEN];

int do_connect()
{
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        printf("Error in socket: %s\n", strerror(errno));
        return -1;
    }
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    sa.sin_port = htons(11211);
    if (connect(sock_fd, (const struct sockaddr *)&sa, sizeof(sa)))
    {
        printf("Error in connect: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}


int do_login()
{
/* My login example:
0000: 80210005 00000000 00000013 00000000 .!..............
0010: 00000000 00000000 504c4149 4e007573 ........PLAIN.us
0020: 65720070 61737377 6f7264            er.password
*/
    memset(buffer, 0, 43);
    buffer[0] = 0x80;
    buffer[1] = 0x21;
    buffer[3] = 0x05;
    buffer[11] = 0x13;
    strcpy((char *)&buffer[24], "PLAIN");
    strcpy((char *)&buffer[30], "user");
    strcpy((char *)&buffer[35], "password");
    if (send(sock_fd, buffer, 43, 0) != 43)
    {
        printf("Error in send of login data: %s\n", strerror(errno));
        return -1;
    }
/* My login response example:
0000: 81210000 00000000 0000000d 00000000 .!..............
0010: 00000000 00000000 41757468 656e7469 ........Authenti
0020: 63617465 64                         cated
*/
    ssize_t recvd = recv(sock_fd, buffer, BUFFER_LEN, 0);
    if (recvd != 37)
    {
        printf("Error in login response: %s\n", strerror(errno));
        return -1;
    }
    buffer[37] = 0;
    if (buffer[0] != 0x81 ||
        buffer[1] != 0x21 ||
        buffer[11] != 0x0d ||
        strcmp((char *)&buffer[24], "Authenticated"))
    {
        printf("Error in login response data (turn on the trace)\n");
        return -1;
    }
    return 0;
}


int do_set()
{
/* My set example:
0000: 80010003 08000000 00000010 00000000 ................
0010: 00000000 00000000 00000000 00000000 ................
0020: 6b657976 616c7565                  keyvalue
*/
    memset(buffer, 0, 40);
    buffer[0] = 0x80;
    buffer[1] = 0x01;
    buffer[3] = 0x03;
    buffer[4] = 0x08;
    buffer[11] = 0x10;
    strcpy((char *)&buffer[32], "keyvalue");
    if (send(sock_fd, buffer, 40, 0) != 40)
    {
        printf("Error in send of set data: %s\n", strerror(errno));
        return -1;
    }

/* My set response:
0000: 81010000 00000000 00000000 00000000 ................
0010: 00000000 00006945                  ......iE
*/
    ssize_t recvd = recv(sock_fd, buffer, BUFFER_LEN, 0);
    if (recvd != 24)
    {
        printf("Error in set response: %s\n", strerror(errno));
        return -1;
    }
    buffer[37] = 0;
    if (buffer[0] != 0x81 ||
        buffer[1] != 0x01 ||
        buffer[6] != 0x00 ||
        buffer[7] != 0x00)
    {
        printf("Error in set response data (turn on the trace)\n");
        return -1;
    }
    return 0;
}


int do_get(bool should_fail)
{
/* My get example:
0000: 80000003 00000000 00000003 00000000 ................
0010: 00000000 00000000 6b6579            ........key
*/
    memset(buffer, 0, 27);
    buffer[0] = 0x80;
    buffer[3] = 0x03;
    buffer[11] = 0x03;
    strcpy((char *)&buffer[24], "key");
    if (send(sock_fd, buffer, 27, 0) != 27)
    {
        printf("Error in send of get data: %s\n", strerror(errno));
        return -1;
    }

/* My get response:
0000: 81000000 04000000 00000009 00000000 ................
0010: 00000000 00006945 00000000 76616c75 ......iE....valu
0020: 65                                  e
*/
    ssize_t recvd = recv(sock_fd, buffer, BUFFER_LEN, 0);
    if (recvd != 33)
    {
        if (should_fail)
        {
            printf("get failed as expected, rc: %d\n", buffer[6] * 0x80 + buffer[7]);
            return 0;
        }
        printf("Error in get response: %s\n", strerror(errno));
        return -1;
    }
    buffer[33] = 0;
    if (buffer[0] != 0x81 ||
        buffer[4] != 0x04 ||
        buffer[6] != 0x00 ||
        buffer[7] != 0x00 ||
        buffer[11] != 0x09 ||
        strcmp((char *)&buffer[28], "value"))
    {
        if (should_fail)
        {
            printf("get failed as expected, (data) rc: %d\n", buffer[6] * 0x80 + buffer[7]);
            return 0;
        }
        printf("Error in get response data (turn on the trace)\n");
        return -1;
    }
    return 0;
}


int do_multi1()
{
/* My multiple requests example (2 list_mechs): 
0000: 80200000 00000000 00000000 00010000 
0010: 00000000 00000000 80200000 00000000 
0020: 00000000 00010000 00000000 00000000  
*/
    memset(buffer, 0, 48);
    buffer[0] = 0x80;
    buffer[1] = 0x20;
    buffer[24] = 0x80;
    buffer[25] = 0x20; 
    if (send(sock_fd, buffer, 48, 0) != 48)
    {
        printf("Error in send of multi data1: %s\n", strerror(errno));
        return -1;
    }
/* My LIST_MECHS response 
0000: 81200000 00000000 00000005 00680000 . ...........h..
0010: 00000000 00000000 504c4149 4e       ........PLAIN
*/
    ssize_t recvd = recv(sock_fd, buffer, BUFFER_LEN, 0);
    if (recvd != 29)
    {
        printf("Error in list_mechs response1: %s (recvd: %ld)\n", strerror(errno), recvd);
        return -1;
    }
    if (buffer[0] != 0x81 ||
        buffer[1] != 0x20 ||
        buffer[6] != 0x00 ||
        buffer[7] != 0x00 ||
        buffer[11] != 0x05 ||
        memcmp((char *)&buffer[24], "PLAIN", 5))
    {
        printf("Error in list_mechs response data1 (turn on the trace)\n");
        return -1;
    }

/* My LIST_MECHS response 
0000: 81200000 00000000 00000005 00680000 . ...........h..
0010: 00000000 00000000 504c4149 4e       ........PLAIN
*/
    if (buffer[0] != 0x81 ||
        buffer[1] != 0x20 ||
        buffer[6] != 0x00 ||
        buffer[7] != 0x00 ||
        buffer[11] != 0x05 ||
        memcmp((char *)&buffer[24], "PLAIN", 5))
    {
        printf("Error in list_mechs response data2 (turn on the trace)\n");
        return -1;
    }
    
    return 0;
}


int do_multi2()
{
/* My multiple requests example: 
0000: 80200000 00000000 00000000 00010000 . ..............
0010: 00000000 00000000 80070000 00000000 ................
0020: 00000000 00020000 00000000 00000000 ................
*/
    memset(buffer, 0, 48);
    buffer[0] = 0x80;
    buffer[1] = 0x20;
    buffer[24] = 0x80;
    buffer[25] = 0x07; // QUIT - NO RESPONSE
    if (send(sock_fd, buffer, 48, 0) != 48)
    {
        printf("Error in send of multi data: %s\n", strerror(errno));
        return -1;
    }
/* My LIST_MECHS response 
0000: 81200000 00000000 00000005 00680000 . ...........h..
0010: 00000000 00000000 504c4149 4e       ........PLAIN
*/
    ssize_t recvd = recv(sock_fd, buffer, BUFFER_LEN, 0);
    if (recvd != 29)
    {
        printf("Error in list_mechs response: %s\n", strerror(errno));
        return -1;
    }
    buffer[29] = 0;
    if (buffer[0] != 0x81 ||
        buffer[1] != 0x20 ||
        buffer[6] != 0x00 ||
        buffer[7] != 0x00 ||
        buffer[11] != 0x05 ||
        strcmp((char *)&buffer[24], "PLAIN"))
    {
        printf("Error in list_mechs response data (turn on the trace)\n");
        return -1;
    }
    return 0;
}


int main()
{
    int ret = 1;
    if (do_connect())
        return 1;
    if (!do_login())
    {
        if (!do_set() && !do_get(false) && !do_multi1() && !do_multi2() && do_get(true))
        {
            printf("All tests passed\n");
            ret = 0;
        }
    }
    if (sock_fd >= 0)
        close(sock_fd);
    return ret;
}
